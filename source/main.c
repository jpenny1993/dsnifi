#include <nds.h>
#include <dswifi9.h>

#include <stdio.h>
#include <string.h>

touchPosition touchXY;
char WIFI_Incoming_Buffer[4096];
size_t WIFI_Incoming_Data_Length = 0;
bool WIFI_Data_IsReceieved = false;

#define WIFI_Data (WIFI_Incoming_Buffer + 32)
#define WIFI_Data_Size (2 * sizeof(int))

void WirelessHandler(int packetID, int readlength) {
	// Read incoming data
	Wifi_RxRawReadPacket(packetID, readlength, (unsigned short *)WIFI_Incoming_Buffer);
	WIFI_Incoming_Data_Length = readlength - 32;

	// Identify if wifi packet was intended for the DS
	// Ideally you want to be checking for a message prefix, and maybe include a checksum of your message
	// But for now we're just confirming that only x and y coordinates were sent
	WIFI_Data_IsReceieved = WIFI_Incoming_Data_Length == WIFI_Data_Size;
}

void SendWirelessData(unsigned short *buffer, int length) {
	if (Wifi_RawTxFrame(length, 0x0014, buffer) != 0) {
		iprintf("Error calling RawTxFrame\n");
	}
}

void nifiInit(void) {
	iprintf("\nNiFi initiating\n\n");
	
	// Changes how packets are handled
	// this seems to modify the frame much like Wifi_TransmitFunction()
	Wifi_SetRawPacketMode(PACKET_MODE_NIFI);
	
	// Init FIFO without automatic settings
	Wifi_InitDefault(false);
	
	// Allow packet sniffing of all wifi data... surely this is actually the cause for NiFi?
	Wifi_SetPromiscuousMode(1);
	
	// Enable wireless radio antenna
	Wifi_EnableWifi();
	
	// Configure custom packet handler for when 
	Wifi_RawSetPacketHandler(WirelessHandler);
	
	// Force specific channel for communication
	Wifi_SetChannel(10);
	
	iprintf("NiFi initiated\n");
}

void gameLoop(void) {
	int heldDown = keysHeld();
	bool isStylusHeld = heldDown & KEY_TOUCH;

	if (isStylusHeld) {
		// Get stylus location
		touchRead(&touchXY);
		
		// print at using ansi escape sequence \x1b[line;columnH 
		iprintf("\x1b[16;0HTouch x = %04X\n", touchXY.px);
		iprintf("Touch y = %04X\n", touchXY.py);

		// Buffer must be aligned to 2 bytes, so use unsigned short
		unsigned short outgoing_buffer[WIFI_Data_Size / sizeof(unsigned short)];
		outgoing_buffer[0] = touchXY.px;
		outgoing_buffer[1] = touchXY.py;

		// Send outgoing packet
		SendWirelessData(outgoing_buffer, WIFI_Data_Size);
	}

	if (WIFI_Data_IsReceieved) {
		// Process your data packet here
		unsigned short x = (unsigned short)WIFI_Data[0 * sizeof(unsigned short)];
		unsigned short y = (unsigned short)WIFI_Data[1 * sizeof(unsigned short)];
		iprintf("\x1b[7;0HNiFi x = %04X, y = %04X\n", x, y);

		// Reset once data received
		WIFI_Data_IsReceieved = false;
	}
}

//---------------------------------------------------------------------------------
int main(void) {
//---------------------------------------------------------------------------------
	consoleDemoInit();
	nifiInit();

	while(1) {
		swiWaitForVBlank();
		
		// Read the controller input state
		scanKeys();

		int keys = keysDown();
		if(keys & KEY_START) break;

		gameLoop();
	}
}
