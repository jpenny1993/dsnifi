#include <nds.h>
#include <dswifi9.h>

#include <stdio.h>
#include <string.h>

touchPosition touchXY;
char WIFI_Buffer[4096];
size_t WIFI_ReceivedDataLength = 0;
bool Wifi_ReceivedData = false;

void WirelessHandler(int packetID, int readlength) {
	Wifi_RxRawReadPacket(packetID, readlength, (unsigned short *)WIFI_Buffer);
	
	WIFI_ReceivedDataLength = readlength - 32;
	Wifi_ReceivedData = true;
}

void SendWirelessData(unsigned short *buffer, int length) {
	if (Wifi_RawTxFrame(length, 0x0014, buffer) != 0) {
		iprintf("Error calling RawTxFrame\n");
	}
}

void nifiInit(void) {
	iprintf("\n\n\tNiFi initiating\n\n");
	
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
	// Buffer must be aligned to 2 bytes, so use unsigned short
	unsigned short buffer[(2 * sizeof(int)) / sizeof(unsigned short)];
	
	// Get stylus location
	touchRead(&touchXY);

	// print at using ansi escape sequence \x1b[line;columnH 
	iprintf("\x1b[16;0HTouch x = %04X, %04X\n", touchXY.rawx, touchXY.px);
	iprintf("Touch y = %04X, %04X\n", touchXY.rawy, touchXY.py);

	// Send Outgoing Packets
	// if (DSGM_held.Stylus) {
		buffer[0] = touchXY.px;
		buffer[1] = touchXY.py;
		SendWirelessData(buffer, 2 * sizeof(int));
	// }
	
	// Handle Incoming Packets
	if (Wifi_ReceivedData) {
		// This will happen from pretty much anything, my house is pretty noisey
		iprintf("\n\n\tNiFi received\n\n");
		if (WIFI_ReceivedDataLength == 2 * sizeof(int)) {
			// TODO: get data to be received consitently and printed out visible
			unsigned short x = (unsigned short)WIFI_Buffer[0 * sizeof(unsigned short)];
			unsigned short y = (unsigned short)WIFI_Buffer[1 * sizeof(unsigned short)];
			iprintf("\x1b[7;0HNiFi x = %04X, y = %04X\n", x, y);
		}

		// Reset once data received
		Wifi_ReceivedData = false;
	}
}

//---------------------------------------------------------------------------------
int main(void) {
//---------------------------------------------------------------------------------
	consoleDemoInit();
	nifiInit();

	while(1) {
		swiWaitForVBlank();
			int keys = keysDown();
			if(keys & KEY_START) break;

		gameLoop();
	}
}
