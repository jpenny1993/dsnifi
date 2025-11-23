// ============================================================================
// NiFi Demo Application - main.c
// ============================================================================
// Demonstrates the NiFi (Near Field Communication) library for Nintendo DS
//
// This application shows how to:
//   - Create and join wireless rooms for local multiplayer
//   - Broadcast position updates in real-time
//   - Send custom game packets (chat messages, item usage, etc.)
//   - Handle network events (connections, disconnections, host migration)
//   - Render player positions using GL2D
//
// NiFi is a fork of the dsgmLib wireless system, integrated into dswifi.
// It uses promiscuous WiFi mode to enable DS-to-DS communication without
// a traditional access point, making it ideal for local multiplayer games.
//
// ARCHITECTURE NOTES:
// - Event handlers run in interrupt context from the hardware timer
// - Packet queues use circular buffers that wrap around when full
// - Old unprocessed packets are overwritten by design (limited NDS RAM)
// - This is optimal for real-time games where old data becomes stale
// - Similar to UDP philosophy: lossy but fast, prioritizing recent events
//
// Original NiFi concept: CTurt/dsgmLib
// Modified for dswifi by: jpenny1993
// ============================================================================

#include <nds.h>
#include <dswifi9.h>
#include <dsnifi9.h>
#include <stdio.h>
#include <string.h>
#include <gl2d.h>
#include "main.h"

// Application state: stores position data for all connected clients
ClientData players[CLIENT_MAX];

// Spectator mode state
bool inSpectatorMode = false;
NiFiRoom discoveredRooms[6];
int currentSpectatorRoomIndex = 0;

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================
// Get human-readable name for room status
const char* GetRoomStatusName(NiFiRoomStatus status) {
	switch (status) {
		case NIFI_ROOM_LOBBY_OPEN: return "LOBBY_OPEN";
		case NIFI_ROOM_LOBBY_CLOSED: return "LOBBY_CLOSED";
		case NIFI_ROOM_INGAME_OPEN: return "INGAME_OPEN";
		case NIFI_ROOM_INGAME_CLOSED: return "INGAME_CLOSED";
		default: return "UNKNOWN";
	}
}

// ============================================================================
// DEBUG OUTPUT HANDLER
// ============================================================================
// Called whenever the NiFi library generates a debug message
// Useful for troubleshooting packet transmission and network issues
// Parameters:
//   type    - Message type (Info, Error, Sent, Received, Acknowledgement)
//   message - The debug message string
void OnDebugOutput(int type, char* message) {
	switch (type) {
		case NIFI_DBG_Information:					printf(WHITE);		break;
		case NIFI_DBG_Error: 						printf(RED);		break;
//		case NIFI_DBG_RawPacket:					printf(MAGENTA);	break;
		case NIFI_DBG_SentPacket:					printf(BLUE);		break;
		case NIFI_DBG_SentAcknowledgement:		printf(CYAN);		break;
		case NIFI_DBG_ReceivedPacket:				printf(YELLOW);	break;
		case NIFI_DBG_ReceivedAcknowledgement:	printf(GREEN);		break;
		default: break;
	}
	printf(message);
	printf("\n");
}

// ============================================================================
// ROOM DISCOVERY HANDLER
// ============================================================================
// Called when a nearby room broadcasts its presence during NiFi_ScanRooms()
// This fires once per discovered room within WiFi range
// Parameters:
//   room - Contains room details (macAddress, roomName, memberCount, roomSize)
// Use Case: Display available rooms to the user or implement auto-join logic
void OnRoomAnnounced(NiFiRoom room) {
	printf("%sROOM FOUND: %s (%d/%d players) [%s]\n", WHITE,
	       room.roomName, room.memberCount, room.roomSize,
	       GetRoomStatusName(room.status));

	// In spectator mode, just display the room
	// In active mode, auto-join the first available room (for testing)
	if (!inSpectatorMode) {
		NiFi_JoinRoom(room.macAddress);
	}
}

// ============================================================================
// JOIN ACCEPTED HANDLER
// ============================================================================
// Called when the room host accepts your join request
// At this point you are officially part of the room and can send/receive packets
// Parameters:
//   room - Contains details about the room you just joined
// Use Case: Transition to lobby/game screen, initialize player data
void OnJoinAccepted(NiFiRoom room) {
	printf("%sJOINED ROOM: %s\n", WHITE, room.roomName);
	printf("%sYou are client ID: %d\n", WHITE, localClient->clientId);
}

// ============================================================================
// JOIN DECLINED HANDLER
// ============================================================================
// Called when your join request is rejected (usually because room is full)
// Parameters:
//   room - Contains details about the room that rejected you
// Use Case: Show error message, continue scanning for other rooms
void OnJoinDeclined(NiFiRoom room) {
	printf("%sROOM FULL: %s\n", RED, room.roomName);
}

// ============================================================================
// CLIENT CONNECTED HANDLER
// ============================================================================
// Called when a new player joins the room (or when you join and learn about existing players)
// This fires for each player already in the room when you join
// Parameters:
//   clientIndex - Array index in clients[] where this client is stored
//   client      - Client info (clientId, macAddress, playerName)
// Use Case: Initialize player-specific game data, spawn player avatar
void OnClientConnected(u8 clientIndex, NiFiClient client) {
	printf("%sCLIENT CONNECTED: %s (ID: %d)\n", WHITE,
	       client.playerName, client.clientId);

	// Initialize player data
	players[clientIndex].position.x = 0;
	players[clientIndex].position.y = 0;
	players[clientIndex].position.z = 0;
}

// ============================================================================
// CLIENT DISCONNECTED HANDLER
// ============================================================================
// Called when a player leaves the room or times out
// The client data will be cleared after this callback returns
// Parameters:
//   clientIndex - Array index in clients[] for the disconnecting client
//   client      - Client info before removal
// Use Case: Remove player avatar, redistribute items, show notification
void OnClientDisconnected(u8 clientIndex, NiFiClient client) {
	printf("%sCLIENT DISCONNECTED: %s (ID: %d)\n", YELLOW,
	       client.playerName, client.clientId);
}

// ============================================================================
// LOCAL DISCONNECT HANDLER
// ============================================================================
// Called when YOU are disconnected from the room (kicked, host left, etc.)
// After this fires, you are no longer in any room
// Use Case: Return to main menu, show disconnection reason
void OnDisconnected() {
	printf("%sYOU WERE DISCONNECTED FROM THE ROOM\n", RED);
}

// ============================================================================
// HOST MIGRATION HANDLER
// ============================================================================
// Called when room leadership transfers to a new host
// This happens when the current host leaves or due to room ID conflicts
// Parameters:
//   clientIndex - Array index of the new host in clients[]
//   client      - Client info for the new host
// Use Case: Update UI to show new host, pause game during transition
void OnHostMigration(u8 clientIndex, NiFiClient client) {
	printf("%sNEW HOST: %s (ID: %d)\n", CYAN,
	       client.playerName, client.clientId);
}

// ============================================================================
// POSITION UPDATE HANDLER
// ============================================================================
// Called when any client (including yourself) broadcasts position data
// This is a specialized packet type for common real-time position syncing
// Parameters:
//   position    - The new position (x, y, z coordinates)
//   clientIndex - Array index in clients[] for the client that moved
//   client      - Client info for the moving client
// Use Case: Update player avatar position, interpolate movement
// Note: This fires for ALL clients including yourself (echoed from host)
void OnPositionUpdated(Position position, u8 clientIndex, NiFiClient client) {
	players[clientIndex].position.x = position.x;
	players[clientIndex].position.y = position.y;
	players[clientIndex].position.z = position.z;

	// Uncomment to debug position updates
	// printf("%s%s moved to (%d,%d,%d)\n", WHITE,
	//        client.playerName, position.x, position.y, position.z);
}

// ============================================================================
// CUSTOM GAME PACKET HANDLER
// ============================================================================
// Called when a custom game packet is received (anything not handled above)
// This is your main hook for implementing custom game logic
// Parameters:
//   packet - Contains command string and up to 6 data parameters
// Use Case: Chat messages, game state updates, ability activations, item pickups
// Example Commands: "CHAT_MSG", "ITEM_USE", "GAME_START", "PLAYER_SCORE"
void OnGamePacket(NiFiPacket packet) {
	// Example: Handle different game events based on command
	if (strcmp(packet.command, "CHAT_MSG") == 0) {
		// packet.data[0] = player name
		// packet.data[1] = message text
		printf("%s[CHAT] %s: %s\n", CYAN, packet.data[0], packet.data[1]);
	}
	else if (strcmp(packet.command, "ITEM_USE") == 0) {
		// packet.data[0] = item ID
		printf("%s%s used item %s\n", MAGENTA,
		       clients[packet.fromClientId].playerName, packet.data[0]);
	}
	else {
		// Unknown custom event
		printf("%sCUSTOM EVENT: %s\n", WHITE, packet.command);
	}
}

// ============================================================================
// MAIN FUNCTION
// ============================================================================
// Entry point for the NiFi demo application
// This demonstrates how to use the NiFi library for local multiplayer on NDS
//
// NiFi Setup Process:
//   1. Initialize hardware (video, console, GL, screen swap)
//   2. Register event handlers (all handlers are optional, but recommended)
//   3. Call NiFi_Init() with channel, timer, and game identifier
//   4. Main loop: handle input, process events, render
//   5. Call NiFi_Shutdown() on exit (if applicable)
//
// Important Notes:
//   - All event handlers run in interrupt context from the timer
//   - Keep handler code fast and simple to avoid blocking network updates
//   - The game identifier must match between all clients to communicate
//   - WiFi channel should be the same for all players (1-13)
int main(void)
{
	// ========================================================================
	// HARDWARE INITIALIZATION
	// ========================================================================
	videoSetMode(MODE_5_3D); // Enable 3D engine
	consoleDemoInit();        // Initialize console for printf output
	glScreen2D();             // Initialize GL for 2D rendering
	lcdMainOnBottom();        // Swap screens (console on bottom)

	// ========================================================================
	// REGISTER EVENT HANDLERS
	// ========================================================================
	// All handlers are optional, but registering them allows you to respond
	// to network events. Handlers are called from interrupt context.
	NiFi_SetDebugOutput(OnDebugOutput);
	NiFi_OnRoomAnnounced(OnRoomAnnounced);
	NiFi_OnJoinAccepted(OnJoinAccepted);
	NiFi_OnJoinDeclined(OnJoinDeclined);
	NiFi_OnClientConnected(OnClientConnected);
	NiFi_OnClientDisconnected(OnClientDisconnected);
	NiFi_OnDisconnected(OnDisconnected);
	NiFi_OnHostMigration(OnHostMigration);
	NiFi_OnPositionUpdated(OnPositionUpdated);
	NiFi_OnGamePacket(OnGamePacket);

	// ========================================================================
	// USER INSTRUCTIONS
	// ========================================================================
	printf("=== NiFi Demo Application ===\n");
	printf("Game ID: %s\n\n", GAME_IDENTIFIER);
	printf("ACTIVE MODE:\n");
	printf("  UP    - Create room\n");
	printf("  DOWN  - Join room\n");
	printf("  RIGHT - Leave room\n");
	printf("  LEFT  - Send chat message\n");
	printf("  A     - Use item (demo)\n");
	printf("  L/R   - Room status (host)\n");
	printf("  X     - Show room status\n");
	printf("  TOUCH - Move cursor\n");
	printf("\nSPECTATOR MODE:\n");
	printf("  SELECT - Start spectating\n");
	printf("  START  - Stop spectating\n");
	printf("  B      - Cycle rooms\n");
	printf("  Y      - Join room\n");
	printf("\n");

	// ========================================================================
	// INITIALIZE NIFI
	// ========================================================================
	// Parameters:
	//   wifiChannel    - WiFi channel (1-13), all players must use same channel
	//   timerId        - Hardware timer to use (0-3), NIFI_TIMER is defined in main.h
	//   gameIdentifier - 4-char game ID, must match for players to communicate
	//
	// Note: Currently NiFi_Init() doesn't return error codes, but ensure:
	//   - DevkitPro and dswifi library are properly installed
	//   - Hardware timer isn't already in use
	//   - Game identifier is exactly 4 characters
	NiFi_Init(5, NIFI_TIMER, GAME_IDENTIFIER);

	printf("NiFi initialized on channel 5\n");
	printf("Waiting for input...\n\n");

	// Pretend this isn't an infinite loop
	while (1)
	{
		scanKeys();
		touchPosition touchXY;
		touchRead(&touchXY);
		int keys = keysHeld();
		int keysdown = keysDown();

		if (keys & KEY_TOUCH && !inSpectatorMode) // STYLUS POSITION (not in spectator mode)
		{
			Position *pos = &(players[0].position);
			pos->x = touchXY.px;
			pos->y = touchXY.py;
			pos->z = 0;
			NiFi_BroadcastPosition(*pos);
		}

		if (keysdown & KEY_UP) // HOST A ROOM
		{
			NiFi_CreateRoom();
		}

		if (keysdown & KEY_DOWN) // SEARCH & JOIN FIRST ROOM
		{
			NiFi_ScanRooms();
		}

		if (keysdown & KEY_RIGHT) // LEAVE ROOM
		{
			NiFi_LeaveRoom();
		}

		if (keysdown & KEY_LEFT) // SEND CHAT MESSAGE (DEMO)
		{
			// Example: Send a custom chat message packet
			NiFiPacket chatPacket;
			NiFi_SetPacket(&chatPacket, "CHAT_MSG");

			// Set packet data
			strcpy(chatPacket.data[0], localClient->playerName); // Sender name
			strcpy(chatPacket.data[1], "Hello from NiFi!");       // Message text

			// Broadcast to all clients
			NiFi_SendBroadcast(&chatPacket, NULL);

			printf("%sSent chat message\n", CYAN);
		}

		if (keysdown & KEY_A) // USE ITEM (DEMO)
		{
			// Example: Send a custom item usage packet
			NiFiPacket itemPacket;
			NiFi_SetPacket(&itemPacket, "ITEM_USE");

			// Set packet data
			sprintf(itemPacket.data[0], "%d", 42); // Item ID

			// Broadcast to all clients
			NiFi_SendBroadcast(&itemPacket, NULL);

			printf("%sUsed item #42\n", MAGENTA);
		}

		// ====================================================================
		// ROOM STATUS CONTROLS (Host only)
		// ====================================================================
		if (keysdown & KEY_L) // PREVIOUS ROOM STATUS
		{
			if (NiFi_IsHost()) {
				NiFiRoomStatus current = NiFi_GetRoomStatus();
				NiFiRoomStatus newStatus = (current == 0) ? NIFI_ROOM_INGAME_CLOSED : (current - 1);
				NiFi_SetRoomStatus(newStatus);
				printf("%sRoom status: %s\n", CYAN, GetRoomStatusName(newStatus));
			} else {
				printf("%sOnly host can change room status\n", RED);
			}
		}

		if (keysdown & KEY_R) // NEXT ROOM STATUS
		{
			if (NiFi_IsHost()) {
				NiFiRoomStatus current = NiFi_GetRoomStatus();
				NiFiRoomStatus newStatus = (current == NIFI_ROOM_INGAME_CLOSED) ? 0 : (current + 1);
				NiFi_SetRoomStatus(newStatus);
				printf("%sRoom status: %s\n", CYAN, GetRoomStatusName(newStatus));
			} else {
				printf("%sOnly host can change room status\n", RED);
			}
		}

		if (keysdown & KEY_X) // SHOW ROOM STATUS
		{
			NiFiRoomStatus status = NiFi_GetRoomStatus();
			printf("%sCurrent room status: %s\n", WHITE, GetRoomStatusName(status));
			printf("%sYou are %s\n", WHITE, NiFi_IsHost() ? "HOST" : "CLIENT");
		}

		// ====================================================================
		// SPECTATOR MODE CONTROLS
		// ====================================================================
		if (keysdown & KEY_SELECT) // START SPECTATOR MODE
		{
			if (!inSpectatorMode && !NiFi_IsSpectating()) {
				printf("%sStarting spectator mode...\n", CYAN);
				if (NiFi_StartSpectating(5, NIFI_TIMER, GAME_IDENTIFIER)) {
					inSpectatorMode = true;
					currentSpectatorRoomIndex = 0;
					printf("%sSpectator mode active!\n", GREEN);
					printf("%sPress B to discover rooms\n", WHITE);
				} else {
					printf("%sFailed to start spectator mode\n", RED);
				}
			} else {
				printf("%sAlready in spectator mode\n", YELLOW);
			}
		}

		if (keysdown & KEY_START) // STOP SPECTATOR MODE
		{
			if (inSpectatorMode && NiFi_IsSpectating()) {
				NiFi_StopSpectating();
				inSpectatorMode = false;
				printf("%sStopped spectator mode\n", YELLOW);
			}
		}

		if (keysdown & KEY_B) // DISCOVER/CYCLE ROOMS IN SPECTATOR MODE
		{
			if (inSpectatorMode && NiFi_IsSpectating()) {
				int roomCount = NiFi_GetDiscoveredRooms(discoveredRooms);
				if (roomCount > 0) {
					// Cycle to next room
					currentSpectatorRoomIndex = (currentSpectatorRoomIndex + 1) % roomCount;
					NiFiRoom room = discoveredRooms[currentSpectatorRoomIndex];
					printf("%s[%d/%d] %s (%d/%d) [%s]\n", CYAN,
					       currentSpectatorRoomIndex + 1, roomCount,
					       room.roomName, room.memberCount, room.roomSize,
					       GetRoomStatusName(room.status));
				} else {
					printf("%sNo rooms discovered yet\n", YELLOW);
				}
			}
		}

		if (keysdown & KEY_Y) // JOIN SPECTATOR ROOM
		{
			if (inSpectatorMode && NiFi_IsSpectating()) {
				int roomCount = NiFi_GetDiscoveredRooms(discoveredRooms);
				if (roomCount > 0 && currentSpectatorRoomIndex < roomCount) {
					NiFiRoom room = discoveredRooms[currentSpectatorRoomIndex];
					if (NiFi_SpectateRoom(room)) {
						printf("%sNow spectating: %s\n", GREEN, room.roomName);
					} else {
						printf("%sFailed to spectate room\n", RED);
					}
				} else {
					printf("%sNo rooms available\n", YELLOW);
				}
			}
		}

		// Start drawing 2D
		glBegin2D();

		// Draw client stylus position
		for (int i = 0; i < CLIENT_MAX; i++)
		{
			if (clients[i].clientId == ID_EMPTY) continue;
			if (clients[i].clientId == ID_ANY) continue;

			// Change the color based on mode
			int color;
			if (inSpectatorMode) {
				// In spectator mode, all players are gray except host (yellow)
				color = (clients[i].clientId == 1)
					? RGB15(31, 31, 0)   // Yellow for host
					: RGB15(20, 20, 20); // Gray for other players
			} else {
				// In active mode, local player is blue, others are white
				color = (clients[i].clientId == localClient->clientId)
					? RGB15(0, 10, 31)   // Blue for local player
					: RGB15(31, 31, 31); // White for other players
			}

			// draw a box at the client position
			glBoxFilled(players[i].position.x - 3, players[i].position.y - 3,
							players[i].position.x + 3, players[i].position.y + 3,
							color);
		}

		// Draw spectator mode indicator in top-left corner
		if (inSpectatorMode) {
			glBoxFilled(2, 2, 60, 12, RGB15(31, 0, 31)); // Purple box
		}

		glEnd2D();
		glFlush(0);

		swiWaitForVBlank();
	}

	// Clean up code when stopping
	NiFi_Shutdown();
}
