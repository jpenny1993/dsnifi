#ifndef MAIN_H_ /* Include guard */
#define MAIN_H_

#include <nds.h>

#define WIFI_TRANSMIT_RATE 0x0014       // Data transfer rate (2mbits/10)
#define WIFI_TTL 120                    // Number of server ticks a packet should be retryable for
#define WIFI_TTL_RATE 20                // Number of server ticks before a message should be retried

// Text colors
#define RED "\x1B[31m"
#define GREEN "\x1B[32m"
#define YELLOW "\x1B[33m"
#define BLUE "\x1B[34m"
#define MAGENTA "\x1B[35m"
#define CYAN "\x1B[36m"
#define WHITE "\x1B[37m"

#define MAX_REQUEST_LENGTH 256          // Total packet size to read
#define MAX_REQUEST_PARAM_LENGTH 32     // Max length of each parameter in a packet
#define MAX_REQUEST_PARAM_COUNT 14      // Max allowed parameters in a packet, both defined and custom

#define REQUEST_GAMEID_INDEX 0          // Index of unique game identifier
#define REQUEST_ROOMID_INDEX 1          // Index of unique room identifier
#define REQUEST_COMMAND_INDEX 2         // Index of the message type
#define REQUEST_ACK_INDEX 3             // Index of the acknowledgement flag
#define REQUEST_MESSAGEID_INDEX 4       // Index of the message identifier
#define REQUEST_TO_INDEX 5              // Index of the target client ID
#define REQUEST_FROM_INDEX 6            // Index of the sending client ID
#define REQUEST_MAC_INDEX 7             // Index of the sending NDS machine address
#define REQUEST_DATA_START_INDEX 8      // Index of the first data parameter
#define REQUEST_DATA_PARAM_COUNT (MAX_REQUEST_PARAM_COUNT - REQUEST_DATA_START_INDEX) // Max number of custom data parameters in a packet

#define UNKNOWN_INDEX -1
#define IDENTIFIER_EMPTY 0              // Default identifier value
#define IDENTIFIER_ANY 127              // MAX value of a uint8

#define CMD_ROOM_SEARCH "SCAN"          // Request that nearby rooms announce their presence
#define CMD_ROOM_ANNOUNCE "ROOM"        // Announce room presence and info
#define CMD_ROOM_JOIN "JOIN"            // Request to join an existing room
#define CMD_ROOM_CONFIRM_JOIN "ACCEPT"  // Approve a join room request
#define CMD_ROOM_DECLINE_JOIN "DENY"    // Decline the join room request
#define CMD_ROOM_LEAVE "QUIT"           // Announce client disconnect to host
#define CMD_ROOM_DISCONNECTED "LEFT"    // Host announces client disconnect to other clients
#define CMD_HOST_MIGRATE "MIGRATE"      // Announce host migration to one of the clients
#define CMD_HOST_ANNOUNCE "HOST"        // Announce self as new host
#define CMD_CLIENT_ANNOUNCE "CLIENT"    // Annouce  client ID and name
#define CMD_CLIENT_POSITION "POSITION"  // Announce client position
#define CMD_CLIENT_SCORE "SCORE"        // Announce client score
#define CMD_CLIENT_ACTION "ACT"         // Announce client action

#define CLIENT_MAX 6                    // Total room members including the server

// Packet format: {GID;RID;CMD;MID;ACK;TO;FROM;MAC;DATA}
typedef struct {
    bool isProcessed;                   // Message has been acknowledged by other devices
    bool isAcknowledgement;             // Receipt that a message was received
    u16 messageId;                      // Message ID for acknowledgements (up to 65534)
    u8 timeToLive;                      // Time to live before the packet is dropped
    u8 toClientId;                      // Client ID in the joined room to talk to. (127 is ALL)
    u8 fromClientId;                    // Client ID in the joined room the message is coming from
    char command[9];                    // Game Command i.e. client, score, position, action
    char macAddress[13];                // MAC address of the NDS that sent the message
    char data[REQUEST_DATA_PARAM_COUNT][MAX_REQUEST_PARAM_LENGTH]; // Message content, up to 6 params with up to 32 chars each
} Packet;

typedef struct {
    int x;
    int y;
} Position;

typedef struct {
    u8 roomId;                          // Used to identify client in the room (1 - 126)
    char macAddress[13];                // Used to register and verify messages
    char roomName[10];                  // Player name from their NDS profile
    u8 roomSize;                        // Total allowed members in the room
    u8 memberCount;                     // Total members currently in the room
} NiFiRoom;

typedef struct {
    u8 clientId;                        // Used to identify client in the room (1 - 126)
    char macAddress[13];                // Used to register and verify messages
    char playerName[10];                // Player name from their NDS profile
    Position position;                  // Players current position (should let dev handle this)
    u16 lastMessageId;                  // Last received message ID acknowledgement syncing (up to 65534)
} NiFiClient;

#endif // MAIN_H_