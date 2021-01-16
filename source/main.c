#include <nds.h>
#include <dswifi9.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

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
	
	// TODO: find out why this hasn't been exported
	setWirelessMode(WIRELESS_MODE_WIFI);
	
	Wifi_InitDefault(false);
	
	Wifi_SetPromiscuousMode(1);
	
	Wifi_EnableWifi();
	
	Wifi_RawSetPacketHandler(WirelessHandler);
	
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
		iprintf("\n\n\tNiFi received\n\n");
		if (WIFI_ReceivedDataLength == 2 * sizeof(int)) {
			iprintf(WIFI_Buffer);
		}
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
