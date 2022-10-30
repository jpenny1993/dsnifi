#include <nds.h>
#include <dswifi9.h>
#include <stdio.h>
#include <gl2d.h>
#include "main.h"

char gameIdentifier[5] = "TEST";
u8 myRoomId = IDENTIFIER_ANY;

bool IsHost = false;
u16 CurrentMessageId = 65000;

/// Raw WiFi data, contains packets from any nearby wireless devices
int WiFi_FrameOffset = 32;           // The size of a WiFi frame
int WiFi_ReceivedLength = 0;         // The size of the incoming data from the WiFi receiver
char WiFi_ReceivedBuffer[1024] = ""; // A raw buffer of the incoming data from the WiFi receiver (DO NOT USE DIRECTLY!)
#define IncomingData (WiFi_ReceivedBuffer + WiFi_FrameOffset) // Safe accessor for WiFi_ReceivedBuffer array

char decodePacketBuffer[MAX_REQUEST_PARAM_COUNT][MAX_REQUEST_PARAM_LENGTH] = {0}; // The decode buffer for identified packets
char outgoingPacketBuffer[MAX_REQUEST_LENGTH]; // The send buffer for the currently outgoing packet

Packet incomingPacket; // The incoming packet used by the packet decoder
Packet acknowledgementPacket; // The outgoing packet used to prepare acknowledgements

// Unprocessed incoming NiFi data, stored as a string
char EncodedPacketBuffer[1024] = ""; // Packets waiting to be processed
char TempBuffer[1024] = "";          // Incomplete packets that couldn't be processed yet

u8 ipIndex = 0, opIndex = 0, akIndex = 0, spIndex = 0;
Packet IncomingPackets[12], OutgoingPackets[18];

u8 lastClientId;
NiFiClient clients[CLIENT_MAX];
NiFiClient *localClient = &clients[0];
NiFiClient *host = NULL;

u8 CountActiveClients() {
	u8 counter = 0;
	for (u8 i = 0; i < CLIENT_MAX; i++) {
		if (clients[i].clientId != 0)
			counter++;
	}
	return counter;
}

int8 IndexOfClientUsingId(u8 clientId) {
	if (clientId == IDENTIFIER_ANY) return UNKNOWN_INDEX;
	for (int8 index = 0; index < CLIENT_MAX; index++)
	{
		if (clients[index].clientId != clientId) continue;
		return index;
	}
	return UNKNOWN_INDEX;
}

int8 IndexOfClientUsingMacAddress(char macAddress[13]) {
	for (int8 index = 0; index < CLIENT_MAX; index++)
	{
		if (clients[index].clientId == IDENTIFIER_EMPTY) continue;
		if (strcmp(clients[index].macAddress, macAddress) != 0) continue;
		return index;
	}
	return UNKNOWN_INDEX;
}

u8 GenerateNewClientId() {
	bool used = false;
	do {
		// Increment
		lastClientId++;
		// Reset to first client
		if (lastClientId >= IDENTIFIER_ANY) lastClientId = 2;
		used = IndexOfClientUsingId(lastClientId) != UNKNOWN_INDEX;
	} while (used); // Retry until unique client ID
	return lastClientId;
}

bool TrySetupNiFiClient(u8 clientId, char macAddress[13], char playerName[10]) {
	// Skip existing clients
	int8 clientIndex = IndexOfClientUsingId(clientId);
	if (clientIndex != UNKNOWN_INDEX) return false;
	// Skip duplicate attempts to join
	clientIndex = IndexOfClientUsingMacAddress(macAddress);
	if (clientIndex != UNKNOWN_INDEX) return false;
	// Back out of responding when the room is full room
	clientIndex = IndexOfClientUsingId(IDENTIFIER_EMPTY);
	if (clientIndex == UNKNOWN_INDEX) return false;
	// Set player data on the empty client
	clients[clientIndex].clientId = clientId;
	strncpy(clients[clientIndex].macAddress, macAddress, sizeof(clients[clientIndex].macAddress));
	strncpy(clients[clientIndex].playerName, playerName, sizeof(clients[clientIndex].playerName));
	return true;
}

// Returns a value between 1 and 127
u8 RandomByte() {
	return (rand() % 126 + 1) & 0xff;
}

void GetProfileName(char *username) {
	memset(username, 0, sizeof(*username));
	int usernameLength = PersonalData->nameLen;

	// Fallback for no profile name
	if (usernameLength <= 0) {
		strcpy(username,  "Player");
		return;
	}

	// Cast chars from utf-16 name stored on DS
	for(int i = 0; i < usernameLength; i++) {
		username[i] = (char)PersonalData->name[i] & 255;
	}
}

bool IsPacketIntendedForMe(char (*params)[MAX_REQUEST_PARAM_LENGTH]) {
   // Ignore messages from other applications
   if (strcmp(params[REQUEST_GAMEID_INDEX], gameIdentifier) != 0) {
      return false;
	}
	// Ignore packets from myself
   if (strcmp(params[REQUEST_MAC_INDEX], localClient->macAddress) == 0) {
       return false;
	}
	// Only accept ACKs to my own packets
	bool isAck = strcmp(params[REQUEST_ACK_INDEX], "1") == 0; 
   if (isAck && strcmp(params[REQUEST_DATA_START_INDEX], localClient->macAddress) == 0) {
       return true;
	}
	u8 pktRoomId; // 1-126 is room data, 127 is a scan
	sscanf(params[REQUEST_ROOMID_INDEX], "%hhd", &pktRoomId);
	// Only accept instruction packets for the room I'm inside
	if (!isAck && pktRoomId == myRoomId && myRoomId != IDENTIFIER_ANY) {
		// Accept scans and announcements from new rooms using the same ID
		if (IsHost && (strcmp(params[REQUEST_COMMAND_INDEX], CMD_ROOM_SEARCH) == 0 ||
						  strcmp(params[REQUEST_COMMAND_INDEX], CMD_ROOM_ANNOUNCE) == 0)) {
			return true;
		}
		// Ignore game packets that aren't directed at me
		u8 toClientId;
		sscanf(params[REQUEST_TO_INDEX], "%hhd", &toClientId);
		if (toClientId != localClient->clientId) {
			return false;
		}
		// Ignore packets from unknown clients
		return IndexOfClientUsingMacAddress(params[REQUEST_MAC_INDEX]) != UNKNOWN_INDEX;
	}
	// Accept searches and join requests for the room I'm hosting
	if (IsHost && pktRoomId == IDENTIFIER_ANY) {
		return strcmp(params[REQUEST_COMMAND_INDEX], CMD_ROOM_SEARCH) == 0 ||
				 (strcmp(params[REQUEST_COMMAND_INDEX], CMD_ROOM_JOIN) == 0 &&
				  strcmp(params[REQUEST_DATA_START_INDEX], localClient->macAddress) == 0);
	}
	// Accept room announcements and join responses when I'm looking for a room
	if (myRoomId == IDENTIFIER_ANY && pktRoomId != myRoomId &&
	    strcmp(params[REQUEST_DATA_START_INDEX], localClient->macAddress) == 0) {
		if (strcmp(params[REQUEST_COMMAND_INDEX], CMD_ROOM_CONFIRM_JOIN) == 0) {
			// Copy room ID early, then handle the confirm packet later as a client
			myRoomId = pktRoomId;
			return true;
		}
		return strcmp(params[REQUEST_COMMAND_INDEX], CMD_ROOM_ANNOUNCE) == 0 ||
				 strcmp(params[REQUEST_COMMAND_INDEX], CMD_ROOM_CONFIRM_JOIN) == 0 ||
				 strcmp(params[REQUEST_COMMAND_INDEX], CMD_ROOM_DECLINE_JOIN) == 0;
	}
   return false;
}

void WritePacketToBuffer(Packet *packet, char buffer[MAX_REQUEST_LENGTH])
{
	memset(buffer, 0, MAX_REQUEST_LENGTH);
	int pos = 0;
	buffer[pos++] = '{';
	pos += sprintf(&buffer[pos], "%s", gameIdentifier);
	pos += sprintf(&buffer[pos], ";%hhd", myRoomId);
	pos += sprintf(&buffer[pos], ";%s", packet->command);
	pos += sprintf(&buffer[pos], ";%d", packet->isAcknowledgement);
	pos += sprintf(&buffer[pos], ";%d", packet->messageId);
	pos += sprintf(&buffer[pos], ";%hhd", packet->toClientId);
	pos += sprintf(&buffer[pos], ";%hhd", packet->fromClientId);
	pos += sprintf(&buffer[pos], ";%s", localClient->macAddress);
   for (int i = 0; i < REQUEST_DATA_PARAM_COUNT; i++) {
		if (strlen(packet->data[i]) > 0)
			pos += sprintf(&buffer[pos], ";%s", packet->data[i]);
   }
	buffer[pos++] = '}';
}

void CreatePacket(Packet *packet, char commandCode[9])
{
	// {GID;RID;CMD;MID;ACK;TO;FROM;MAC;DATA}
	// GID and RID will only be included during send
	packet->isProcessed = false;
	packet->timeToLive = WIFI_TTL;
	memset(packet->command, 0, sizeof(packet->command));
	strcpy(packet->command, commandCode);
   packet->messageId = ++CurrentMessageId;
   packet->isAcknowledgement = false;
	memset(packet->macAddress, 0, sizeof(packet->macAddress));
	strcpy(packet->macAddress, localClient->macAddress);
	packet->toClientId = IDENTIFIER_ANY;
	packet->fromClientId = localClient->clientId;
	memset(packet->data, 0, sizeof(packet->data));
	if (CurrentMessageId == 65534) {
		CurrentMessageId = 0; // MessageID rollover
	}
}

void DecodePacket(Packet *packet, u8 readParams) {
	// {GID;RID;CMD;ACK;MID;TO;FROM;MAC;DATA}
	// We can ignore GID and RID at this point
	packet->isProcessed = false;

	// Read the packet headers
   strcpy(packet->command, decodePacketBuffer[REQUEST_COMMAND_INDEX]);
	packet->isAcknowledgement = strcmp(decodePacketBuffer[REQUEST_ACK_INDEX], "1") == 0;
	sscanf(decodePacketBuffer[REQUEST_MESSAGEID_INDEX], "%hd", &packet->messageId);
	sscanf(decodePacketBuffer[REQUEST_TO_INDEX], "%hhd", &packet->toClientId);
	sscanf(decodePacketBuffer[REQUEST_FROM_INDEX], "%hhd", &packet->fromClientId);
   strcpy(packet->macAddress, decodePacketBuffer[REQUEST_MAC_INDEX]);

	// Zero the message content
	memset(packet->data, 0, sizeof(packet->data));

	// Copy the received message content
	for (int paramIndex = REQUEST_DATA_START_INDEX; paramIndex < MAX_REQUEST_PARAM_COUNT; paramIndex++) {
		if (paramIndex > readParams) break;
		if (strlen(decodePacketBuffer[paramIndex]) > 0) {
			int dataIndex = paramIndex - REQUEST_DATA_START_INDEX;
			strcpy(packet->data[dataIndex], decodePacketBuffer[paramIndex]);
		}
	}
}

void SendPacket(Packet *packet) {
   WritePacketToBuffer(packet, outgoingPacketBuffer);
	// printf(packet->isAcknowledgement ? CYAN : BLUE);
	// printf(outgoingPacketBuffer);
	// printf("\n");
	Wifi_RawTxFrame(strlen(outgoingPacketBuffer) + 1, WIFI_TRANSMIT_RATE, (unsigned short *)outgoingPacketBuffer);
}

void SendAcknowledgement(Packet *receivedPacket)
{
	// {GID;RID;CMD;MID;ACK;TO;FROM;MAC;DATA}
	// GID and RID only included during send
	acknowledgementPacket.isProcessed = false;
	acknowledgementPacket.timeToLive = WIFI_TTL;
	memset(acknowledgementPacket.command, 0, sizeof(acknowledgementPacket.command));
	strcpy(acknowledgementPacket.command, receivedPacket->command);
   acknowledgementPacket.messageId = receivedPacket->messageId;
   acknowledgementPacket.isAcknowledgement = true;
	memset(acknowledgementPacket.macAddress, 0, sizeof(acknowledgementPacket.macAddress));
	strcpy(acknowledgementPacket.macAddress, localClient->macAddress);
	acknowledgementPacket.toClientId = receivedPacket->fromClientId;
	acknowledgementPacket.fromClientId = localClient->clientId;
	memset(acknowledgementPacket.data, 0, sizeof(acknowledgementPacket.data));
	strcpy(acknowledgementPacket.data[0], receivedPacket->macAddress); // Include MAC of sender for validation on received
	SendPacket(&acknowledgementPacket);
}

void QueuePacket(Packet *packet) {
	// Warn if needed
	if (!OutgoingPackets[opIndex].isProcessed) {
		printf("%sOverwriting outgoing packet %hhd!\n", RED, opIndex);
	}

	// Copy packet onto array
	OutgoingPackets[opIndex].isProcessed = packet->isProcessed;
   OutgoingPackets[opIndex].isAcknowledgement = packet->isAcknowledgement;
   OutgoingPackets[opIndex].messageId = packet->messageId;
	OutgoingPackets[opIndex].timeToLive = packet->timeToLive;
	OutgoingPackets[opIndex].toClientId = packet->toClientId;
	OutgoingPackets[opIndex].fromClientId = packet->fromClientId;
	memset(OutgoingPackets[opIndex].command, 0, sizeof(OutgoingPackets[opIndex].command));
	strncpy(OutgoingPackets[opIndex].command, packet->command, sizeof(OutgoingPackets[opIndex].command));
	memset(OutgoingPackets[opIndex].macAddress, 0, sizeof(OutgoingPackets[opIndex].macAddress));
	strncpy(OutgoingPackets[opIndex].macAddress, packet->macAddress, sizeof(OutgoingPackets[opIndex].macAddress));
	memset(OutgoingPackets[opIndex].data, 0, sizeof(OutgoingPackets[opIndex].data));
	for (u8 di = 0; di < REQUEST_DATA_PARAM_COUNT; di++) {
		strncpy(OutgoingPackets[opIndex].data[di], packet->data[di], MAX_REQUEST_PARAM_LENGTH);
	}

	// Find next available index to write a packet
	u8 arraySize = (sizeof(OutgoingPackets) / sizeof(Packet));
	u8 counter = 0;
	do {
		opIndex += 1;
		if (opIndex == arraySize) opIndex = 0;
		if (OutgoingPackets[opIndex].isProcessed) break;
	} while(counter++ < arraySize);
}

void EnqueueIncomingPacket(Packet *packet) {
	// Warn if needed
	if (!IncomingPackets[ipIndex].isProcessed) {
		printf("%sOverwriting incoming packet %d!\n", RED, ipIndex);
	}

	// Copy packet onto array
	IncomingPackets[ipIndex].isProcessed = packet->isProcessed;
   IncomingPackets[ipIndex].isAcknowledgement = packet->isAcknowledgement;
   IncomingPackets[ipIndex].messageId = packet->messageId;
	IncomingPackets[ipIndex].timeToLive = packet->timeToLive;
	IncomingPackets[ipIndex].toClientId = packet->toClientId;
	IncomingPackets[ipIndex].fromClientId = packet->fromClientId;
	memset(IncomingPackets[ipIndex].command, 0, sizeof(IncomingPackets[ipIndex].command));
	strncpy(IncomingPackets[ipIndex].command, packet->command, sizeof(IncomingPackets[ipIndex].command));
	memset(IncomingPackets[ipIndex].macAddress, 0, sizeof(IncomingPackets[ipIndex].macAddress));
	strncpy(IncomingPackets[ipIndex].macAddress, packet->macAddress, sizeof(IncomingPackets[ipIndex].macAddress));
	memset(IncomingPackets[ipIndex].data, 0, sizeof(IncomingPackets[ipIndex].data));
	for (u8 di = 0; di < REQUEST_DATA_PARAM_COUNT; di++) {
		strncpy(IncomingPackets[ipIndex].data[di], packet->data[di], MAX_REQUEST_PARAM_LENGTH);
	}

	// Find next available index to write a packet
	u8 arraySize = (sizeof(IncomingPackets) / sizeof(Packet));
	u8 counter = 0;
	do {
		ipIndex += 1;
		if (ipIndex == arraySize) ipIndex = 0;
		if (IncomingPackets[ipIndex].isProcessed) break;
	} while(counter++ < arraySize);
}

void ProcessEncodedPacketBuffer(int startPosition, int endPosition)
{
	// Get the string packet inbetween the curly braces
	char currentPacket[MAX_REQUEST_LENGTH] = "";
	strncpy(currentPacket, EncodedPacketBuffer + startPosition, endPosition - startPosition);
	char printBuffer[MAX_REQUEST_LENGTH];
	strcpy(printBuffer, currentPacket);
	//printf("%s%s\n",RED, currentPacket);

	// Get the index of the parameter delimiter
	char *ptr = strtok(currentPacket, ";");
	int splitCount = 0;

	// Copy the value of each parameter into an array for decoding
	memset(decodePacketBuffer, 0, sizeof(decodePacketBuffer));
	while (ptr != NULL)
	{
		strcpy(decodePacketBuffer[splitCount], ptr);
		splitCount++;
		ptr = strtok(NULL, ";");
	}

	// Decode packet and copy onto processing buffer for later
	if (IsPacketIntendedForMe(decodePacketBuffer))
	{
		DecodePacket(&incomingPacket, splitCount);
		//printf("%s{%s}\n", (incomingPacket.isAcknowledgement ? GREEN : YELLOW), printBuffer);
		EnqueueIncomingPacket(&incomingPacket);
	}
}

void OnRawPacketReceived(int packetID, int readlength)
{
	// This function needs to complete as fast as possible
	// We should not be forwarding packets into game code from this routine
   Wifi_RxRawReadPacket(packetID, readlength, (unsigned short *)WiFi_ReceivedBuffer);
   WiFi_ReceivedLength = readlength - WiFi_FrameOffset;

   // Ensure that all data contained in the packet are ASCII characters
   int receivedDataLength = strlen(IncomingData);
   if (WiFi_ReceivedLength != receivedDataLength + 1)
   {
      return;
   }

   // It's not guaranteed that we will receive a full packet in a single read
   strcat(EncodedPacketBuffer, IncomingData);

	int startPosition, endPosition;
   while ((startPosition = strstr(EncodedPacketBuffer, "{") - EncodedPacketBuffer + 1) > 0 &&
          (endPosition = strstr(EncodedPacketBuffer + startPosition, "}") - EncodedPacketBuffer) > 0)
   {
      ProcessEncodedPacketBuffer(startPosition, endPosition);

		// Zero the temp buffer
		memset(TempBuffer, 0, sizeof(TempBuffer));

      // Add all remaining characters after current data packet back onto the data buffer
      strcat(TempBuffer, EncodedPacketBuffer + endPosition + 1);
      strcpy(EncodedPacketBuffer, TempBuffer);
   }
}

void QueueBroadcast(Packet *p, u8 ignoreClientIds[]) {
	bool contains;
	for (u8 i = 0; i < CLIENT_MAX; i++) {
		if (clients[i].clientId == IDENTIFIER_EMPTY) continue;
		if (clients[i].clientId == localClient->clientId) continue;
		if (ignoreClientIds != NULL) {
			contains = false;
			for (u8 j = 0; j < (sizeof(*ignoreClientIds) / sizeof(u8)); j++) {
				if (clients[i].clientId == ignoreClientIds[j]) {
					contains = true;
					break;
				}
			}
			if (contains) continue;
		}
		p->toClientId = clients[i].clientId;
		QueuePacket(p);
	}
}

void HandlePacketAsClient(Packet *p, u8 cIndex) {
	if (strcmp(p->command, CMD_CLIENT_POSITION) == 0) {
		if (cIndex == UNKNOWN_INDEX) return;
		sscanf(p->data[0], "%d", &(clients[cIndex].position.x));
		sscanf(p->data[1], "%d", &(clients[cIndex].position.y));
		return;
	}
	// When the host updates you on client changes
	if (strcmp(p->command, CMD_CLIENT_ANNOUNCE) == 0) {
		u8 clientId;
		sscanf(p->data[0], "%hhd", &clientId);
		if (TrySetupNiFiClient(clientId, p->data[1], p->data[2])) {
			printf("%sID: %hhd, C:%s, DN:%s\n", WHITE, clientId, p->data[1], p->data[2]);	
		}
		return;
	}
	// When I've been disconnected, or I've been notified of someone else disconnecting
	if (strcmp(p->command, CMD_ROOM_DISCONNECTED) == 0) {
		u8 disconnectedId;
		sscanf(p->data[0], "%hhd", &disconnectedId);
		printf("Server has announced client %d disconnected\n", disconnectedId);
		printf("I'm %d \n", localClient->clientId);
		printf("It really was %s\n", p->data[0]);
		if (disconnectedId == localClient->clientId) {
			printf("It's me, better reset my values\n");
			myRoomId = IDENTIFIER_ANY;
			for(u8 i = 0; i < CLIENT_MAX; i++)
				clients[i].clientId = IDENTIFIER_EMPTY;
			return;
		}
		u8 dcIndex = IndexOfClientUsingId(disconnectedId);
		printf("Removing the client, their index is %d\n", dcIndex);
		if (dcIndex == UNKNOWN_INDEX) return; 
		clients[dcIndex].clientId = IDENTIFIER_EMPTY;
		return;
	}
	// When the host accepts you into the room, setup their client
	if (strcmp(p->command, CMD_ROOM_CONFIRM_JOIN) == 0) {
		// Set my client ID
		localClient->clientId = p->toClientId;
		// Setup room HOST
		if (!TrySetupNiFiClient(p->fromClientId, p->macAddress, p->data[1])) return;
		host = &clients[IndexOfClientUsingId(p->fromClientId)];
		for (int x = 0; x < CLIENT_MAX; x++)
		{
			if (clients[x].clientId == IDENTIFIER_EMPTY) continue;
			printf("%sID: %hhd, C:%s, DN:%s\n", WHITE, clients[x].clientId, clients[x].macAddress, clients[x].playerName);	
		}
		return;
	}
	// When the host migrates due to room ID conflict, go along with it
	if (strcmp(p->command, CMD_HOST_MIGRATE) == 0) { // TODO FIX DISCONNECT AND HOST MIGRATION
		printf("Migrate to new Host\n");
		if (cIndex == UNKNOWN_INDEX) return;
		sscanf(p->data[0], "%hhd", &myRoomId);
		printf("New Room ID is %d\n", myRoomId);
		u8 newHostId;
		sscanf(p->data[1], "%hhd", &newHostId);
		printf("New Host ID is %d\n", newHostId);
		u8 hostIndex = IndexOfClientUsingId(newHostId);
		if (hostIndex != UNKNOWN_INDEX)
			host = &clients[hostIndex];
		IsHost = hostIndex == 0;
		printf("Am I the new Host? %d\n", IsHost);
		return;
	}
}

void HandlePacketAsHost(Packet *p, u8 cIndex) {
	Packet r;
	// When client is searching for a room, announce room presence
	if (strcmp(p->command, CMD_ROOM_SEARCH) == 0) {
		CreatePacket(&r, CMD_ROOM_ANNOUNCE);
		strcpy(r.data[0], p->macAddress);                  // Return MAC
		strcpy(r.data[1], localClient->playerName);        // Room name
		sprintf(r.data[2], "%hhd", CountActiveClients());  // Current clients
		sprintf(r.data[3], "%d", CLIENT_MAX);              // Total clients
		SendPacket(&r);
		return;
	}
	// When the client is attempting to join the room, only allow if space
	if (strcmp(p->command, CMD_ROOM_JOIN) == 0) {  
		if (CountActiveClients() < CLIENT_MAX) {
			u8 newClientId = GenerateNewClientId();
			if (!TrySetupNiFiClient(newClientId, p->macAddress, p->data[1]))
				return;
			// Queue join confirmation
			CreatePacket(&r, CMD_ROOM_CONFIRM_JOIN);
			r.toClientId = newClientId; 
			strcpy(r.data[0], p->macAddress);
			strcpy(r.data[1], localClient->playerName);
			QueuePacket(&r);
			// New client announcement
			CreatePacket(&r, CMD_CLIENT_ANNOUNCE);
			sprintf(r.data[0], "%hhd", newClientId);
			strcpy(r.data[1], p->macAddress);
			strcpy(r.data[2], p->data[1]);
			u8 ignoreIds[1] = { newClientId };
			QueueBroadcast(&r, ignoreIds);
			for (u8 i = 0; i < CLIENT_MAX; i++) {
				if (clients[i].clientId == IDENTIFIER_EMPTY) continue;
				if (clients[i].clientId == localClient->clientId) continue;
				if (clients[i].clientId == newClientId) continue;
				// Existing client announcement
				CreatePacket(&r, CMD_CLIENT_ANNOUNCE);
				r.toClientId = newClientId;
				sprintf(r.data[0], "%hhd", clients[i].clientId);
				strcpy(r.data[1], clients[i].macAddress);
				strcpy(r.data[2], clients[i].playerName);
				QueuePacket(&r);
				printf("%sID: %hhd, C:%s, DN:%s\n", WHITE, clients[i].clientId, clients[i].macAddress, clients[i].playerName);
			}
		} else if (IndexOfClientUsingMacAddress(p->macAddress) == UNKNOWN_INDEX) {
			printf("%sDeclined new client\n", YELLOW);
			Packet r;
			CreatePacket(&r, CMD_ROOM_DECLINE_JOIN);
			SendPacket(&r);
		}
		return;
	}
	// When another room accounces it's presence, migrate room ID and retry
	if (strcmp(p->command, CMD_ROOM_ANNOUNCE) == 0) {
		u8 newRoomId = RandomByte();
		if (strcmp(p->data[0], localClient->macAddress) != 0) {
			// Somehow 2 rooms with the same ID have come near each other
			// Perform a host migration to the existing clients
			printf("%sROOMID CONFLICT! HOST MIGRATION NECESSARY\n", WHITE);
			Packet r;
			CreatePacket(&r, CMD_HOST_MIGRATE);
			sprintf(r.data[0], "%hhd", newRoomId);
			sprintf(r.data[1], "%hhd", localClient->clientId);
			QueuePacket(&r);
		}
		// Room ID is taken try another
		myRoomId = newRoomId;
		Packet r;
		CreatePacket(&r, CMD_ROOM_SEARCH);
		QueuePacket(&r); // Maybe only queue if ACK is required?
		return;
	}
	// When a client disconnects from the room, mark them as empty
	if (strcmp(p->command, CMD_ROOM_LEAVE) == 0) {
		if (cIndex == UNKNOWN_INDEX) return;
		printf("Client %d has announce they are leaving\n", p->fromClientId);
		CreatePacket(&r, CMD_ROOM_DISCONNECTED);
		sprintf(r.data[0] , "%hhd", p->fromClientId);
		QueueBroadcast(&r, NULL);
		printf("removing client index %d\n", cIndex);
		clients[cIndex].clientId = IDENTIFIER_EMPTY;
		return;
	}
	// Update client position
	if (strcmp(p->command, CMD_CLIENT_POSITION) == 0) {
		if (cIndex == UNKNOWN_INDEX) return;
		sscanf(p->data[0], "%d", &(clients[cIndex].position.x));
		sscanf(p->data[1], "%d", &(clients[cIndex].position.y));
		return;
	}
}

void HandleAsSearching(Packet *p) {
	if (strcmp(p->command, CMD_ROOM_ANNOUNCE) == 0) {
		printf("%sFOUND: %s, (%s/%s)\n", GREEN, p->data[1], p->data[2], p->data[3]);
		printf(WHITE);
		Packet r; // Response
		CreatePacket(&r, CMD_ROOM_JOIN);
		strcpy(r.data[0], p->macAddress); // Room MAC
		strcpy(r.data[1], localClient->playerName); // My display name
		SendPacket(&r);
		return;
	}
	if (strcmp(p->command, CMD_ROOM_DECLINE_JOIN) == 0) {
		printf("%sRoom full\n", RED);
		printf(WHITE);
		return;
	}
}

void CompleteAcknowledgedPacket(Packet *p) {
	for (u8 oi = 0; oi < (sizeof(OutgoingPackets) / sizeof(Packet)); oi++) {
		if (OutgoingPackets[oi].isProcessed) continue;
		if (OutgoingPackets[oi].messageId != p->messageId) continue;
		if (OutgoingPackets[oi].toClientId != p->fromClientId) continue;
		if (strcmp(OutgoingPackets[oi].command, p->command) != 0) continue;
		OutgoingPackets[oi].isProcessed = true; // Mark as acknowledged
		//printf("%sACK Confirmed\n", MAGENTA);
		break;
	}
}

void Timer_Tick() {
	u8 arraySize = sizeof(IncomingPackets) / sizeof(Packet);
	u8 counter = 0;
	bool enumerate = false;
	do {
		if (akIndex == ipIndex) break;
		if (enumerate) {
			// Increment onto next packet
			akIndex += 1;
			// Return to the start of the array
			if (akIndex == arraySize) akIndex = 0;
			enumerate = false;
		}
		Packet *p = &IncomingPackets[akIndex];
		// Ignore processed packets
		if (p->isProcessed) {
			enumerate = true;
			continue;
		}
		// Mark packet as processed
		p->isProcessed = true;
		// Mark acknowledged outgoing packets as processed
		if (p->isAcknowledgement) {
			CompleteAcknowledgedPacket(p);
			enumerate = true;
			continue;
		}
		// Send acknowledgement ASAP for instruction packets
		SendAcknowledgement(p);
		if (myRoomId == IDENTIFIER_ANY) {
			HandleAsSearching(p);
			enumerate = true;
			continue;
		}

		// Skip late messages that are considered out of date
		u8 cIndex = UNKNOWN_INDEX;
		if (p->fromClientId != IDENTIFIER_ANY  &&
			 p->fromClientId != IDENTIFIER_EMPTY) {
			cIndex = IndexOfClientUsingId(p->fromClientId);
			NiFiClient *client = &clients[cIndex];
			if (p->messageId < client->lastMessageId &&     // Ignore old messages
				client->lastMessageId - p->messageId < 500) // Unless MsgId Rollover
			{
				enumerate = true;
				continue;
			}

			// Update the last message ID on the client
			client->lastMessageId = p->messageId;
		}

		if (IsHost) HandlePacketAsHost(p, cIndex);
		else HandlePacketAsClient(p, cIndex);
		enumerate = true;
	} while (++counter < arraySize);


	// Bizzare loop that works really well for resending packets
	// Retries the same send index until the counter times out
	// Once the packet is sent, then we increment to the next index
	// If we're making packets fast, then we can process the full buffer in 1 server tick
	arraySize = sizeof(OutgoingPackets) / sizeof(Packet);
	counter = 0;
	enumerate = false;
	 do {
		if (spIndex == opIndex) break;
		if (enumerate) {
			// Increment onto next packet
			spIndex += 1;
			// Return to the start of the array
			if (spIndex == arraySize) spIndex = 0;
			enumerate = false;
		}
		// Ignore processed packets
		if (OutgoingPackets[spIndex].isProcessed) {
			enumerate = true;
			continue;
		}
		// Countdown until packet should be dropped
		OutgoingPackets[spIndex].timeToLive -= 1;
		// Drop packet and add timeout to strike target client
		if (OutgoingPackets[spIndex].timeToLive == 0) {
			OutgoingPackets[spIndex].isProcessed = true;
			printf("%sPACKET DROPPED!\n", RED);
			enumerate = true;
			continue;
		}
		// Choose when to send the packet, currently up to 3 times, the 4th attempt will timeout first
		if ((OutgoingPackets[spIndex].timeToLive % WIFI_TTL_RATE) == 0) {
			SendPacket(&OutgoingPackets[spIndex]);
			enumerate = true;
		}
	} while (++counter < arraySize);
}

void initialise() {
   // Changes how incoming packets are handled
   Wifi_SetRawPacketMode(PACKET_MODE_NIFI);

   // Init WiFi without automatic settings
   Wifi_InitDefault(false);
   Wifi_EnableWifi();

   // Configure a custom packet handler
   Wifi_RawSetPacketHandler(OnRawPacketReceived);

   // Force specific channel for communication
   Wifi_SetChannel(10);

   // Get MAC address of the Nintendo DS
   u8 macAddressUnsigned[6];
   Wifi_GetData(WIFIGETDATA_MACADDRESS, 6, macAddressUnsigned);

   // Convert unsigned values to hexa values
   sprintf(localClient->macAddress, "%02X%02X%02X%02X%02X%02X", 
		macAddressUnsigned[0], macAddressUnsigned[1], macAddressUnsigned[2], 
		macAddressUnsigned[3], macAddressUnsigned[4], macAddressUnsigned[5]);

	GetProfileName(localClient->playerName);

	// Start packet handler timer
	timerStart(0, ClockDivider_1024, TIMER_FREQ_1024(240), Timer_Tick);
}

int main(void)
{
	videoSetMode(MODE_5_3D); // Enable 3D engine
	consoleDemoInit();
	glScreen2D(); //  Initialize GL
	lcdMainOnBottom(); // Swap screens

	// reset buffers
	for (int i = 0 ; i < (sizeof(IncomingPackets) / sizeof(Packet)); i++) {
		IncomingPackets[i].isProcessed = true;
	}

	for (int i = 0 ; i < (sizeof(OutgoingPackets) / sizeof(Packet)); i++) {
		OutgoingPackets[i].isProcessed = true;
	}

	for (int i = 0 ; i < CLIENT_MAX; i++) {
		clients[i].clientId = IDENTIFIER_EMPTY;
		clients[i].lastMessageId = 0;
		clients[i].position.x = 0;
		clients[i].position.y = 0;
	}

	initialise(); // nifi startup

	printf("Press UP to create a new room\n");
	printf("Press DOWN to join the first available room\n");
	printf("Press RIGHT to leave a joined room\n");
	printf("Hosts will negociate when their room ID conflicts\n");
	printf("Leaving as the host will perform a host migration\n");

	while (1)
	{
		scanKeys();
		touchPosition touchXY;
		touchRead(&touchXY);
		int keys = keysHeld();
		int keysdown = keysDown();

		if (keys & KEY_TOUCH) // STYLUS POSITION
		{
			localClient->position.x = touchXY.px;
			localClient->position.y = touchXY.py;
			Packet pk;
			CreatePacket(&pk, CMD_CLIENT_POSITION);
			sprintf(pk.data[0], "%d", localClient->position.x);
			sprintf(pk.data[1], "%d",localClient->position.y);
			QueueBroadcast(&pk, NULL);
		}

		if (keysdown & KEY_UP) // HOST A ROOM
		{
			IsHost = true;
			myRoomId = RandomByte();
			printf(YELLOW);
			printf("HOSTING A ROOM %d as %s\n", myRoomId, localClient->playerName);
			Packet pk;
			localClient->clientId = lastClientId = 1;
			CreatePacket(&pk, CMD_ROOM_SEARCH); // Search for other rooms using the same room ID
			SendPacket(&pk);
		}

		if (keysdown & KEY_DOWN) // SEARCH & JOIN FIRST ROOM
		{
			Packet pk;
			myRoomId = IDENTIFIER_ANY;
			localClient->clientId = IDENTIFIER_ANY;
			CreatePacket(&pk, CMD_ROOM_SEARCH);
			SendPacket(&pk);
		}

		if (keysdown & KEY_RIGHT) // LEAVE ROOM
		{
			Packet pk;
			u8 hostId = host->clientId;
			if (IsHost && CountActiveClients() > 1) {
				for (u8 i = 1; i < CLIENT_MAX; i++) {
					if (clients[i].clientId != IDENTIFIER_EMPTY) {
						hostId = clients[i].clientId;
						break;
					}
				}
				printf("Creating host migration packet\n");
				CreatePacket(&pk, CMD_HOST_MIGRATE);
				sprintf(pk.data[0], "%hhd", myRoomId);
				sprintf(pk.data[1], "%hhd", hostId);
				printf("Changing my host\n");
				host = &clients[hostId];
				IsHost = false;
				printf("Queuing broadcast\n");
				QueueBroadcast(&pk, NULL);
			}

			printf("Sending host a disconnect message\n");
			CreatePacket(&pk, CMD_ROOM_LEAVE);
			pk.toClientId = hostId;
			QueuePacket(&pk);
		}

		// printf(WHITE);
		// printf("\x1B[0;0H M:%s R:%d C:%d   ",
		// 	localClient->macAddress,
		// 	myRoomId,
		// 	localClient->clientId);
		// printf("\n");

		// Start drawing 2D
		glBegin2D();

		// Draw client stylus position
		for (int i = 0; i < CLIENT_MAX; i++)
		{
			if (clients[i].clientId == IDENTIFIER_EMPTY) continue;

			// Change the color of the local player
			int color = (clients[i].clientId == localClient->clientId)
				? RGB15(0, 10, 31)
				: RGB15(31, 31, 31);

			// draw a box at the client position
			glBoxFilled(clients[i].position.x - 3, clients[i].position.y - 3,
							clients[i].position.x + 3, clients[i].position.y + 3,
							color);
		}

		glEnd2D();
		glFlush(0);

		swiWaitForVBlank();
	}
}
