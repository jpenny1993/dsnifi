// ============================================================================
// NiFi Lobby Demo - network.c
// ============================================================================
// Network helper functions and NiFi event handlers
// ============================================================================

#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <dsnifi9.h>
#include "main.h"

// External game state (defined in main.c)
extern GameState g_state;

// Forward declarations
void Game_HandleDrawPacket(GameState* state, NiFiPacket packet);

// ============================================================================
// NIFI EVENT HANDLERS
// ============================================================================

void OnRoomAnnounced(NiFiRoom room) {
    // Check if room already exists (by MAC address)
    for (int i = 0; i < g_state.roomCount; i++) {
        if (strcmp(g_state.discoveredRooms[i].macAddress, room.macAddress) == 0) {
            // Update existing room (status/members may have changed)
            g_state.discoveredRooms[i] = room;
            return;
        }
    }

    // Add new room if not full
    if (g_state.roomCount < 6) {
        g_state.discoveredRooms[g_state.roomCount] = room;
        g_state.roomCount++;
    }
}

void OnJoinAccepted(NiFiRoom room) {
    // Transition to lobby
    g_state.currentState = STATE_LOBBY;

    // Show message
    char msg[64];
    snprintf(msg, sizeof(msg), "Joined %s", room.roomName);
    UI_ShowMessage(&g_state, msg, 180);  // 3 seconds
}

void OnJoinDeclined(NiFiRoom room) {
    // Show error message
    UI_ShowMessage(&g_state, "Room is full or locked", 180);
}

void OnClientConnected(u8 clientIndex, NiFiClient client) {
    // Initialize player data
    if (clientIndex < CLIENT_MAX) {
        g_state.players[clientIndex].isReady = false;
        g_state.players[clientIndex].drawing.brushColor = 0;
        g_state.players[clientIndex].drawing.brushSize = 2;
        g_state.players[clientIndex].drawing.cursorX = CANVAS_WIDTH / 2;
        g_state.players[clientIndex].drawing.cursorY = CANVAS_HEIGHT / 2;
        g_state.players[clientIndex].drawing.isDrawing = false;
    }

    // Show notification
    char msg[64];
    snprintf(msg, sizeof(msg), "%s joined", client.playerName);
    UI_ShowMessage(&g_state, msg, 120);
}

void OnClientDisconnected(u8 clientIndex, NiFiClient client) {
    // Clear player data for this client
    if (clientIndex < CLIENT_MAX) {
        g_state.players[clientIndex].isReady = false;
        g_state.players[clientIndex].drawing.isDrawing = false;
        g_state.players[clientIndex].drawing.cursorX = 0;
        g_state.players[clientIndex].drawing.cursorY = 0;
    }

    // Show notification
    char msg[64];
    snprintf(msg, sizeof(msg), "%s left", client.playerName);
    UI_ShowMessage(&g_state, msg, 120);
}

void OnDisconnected() {
    // Return to main menu
    g_state.currentState = STATE_MAIN_MENU;
    UI_ShowMessage(&g_state, "Disconnected from room", 180);
}

void OnHostMigration(u8 clientIndex, NiFiClient client) {
    // Show notification
    char msg[64];
    snprintf(msg, sizeof(msg), "%s is now host", client.playerName);
    UI_ShowMessage(&g_state, msg, 180);
}

void OnPositionUpdated(Position position, u8 clientIndex, NiFiClient client) {
    // Update cursor position (used for drawing and general cursor tracking)
    if (clientIndex < CLIENT_MAX) {
        g_state.players[clientIndex].drawing.cursorX = position.x;
        g_state.players[clientIndex].drawing.cursorY = position.y;
        // Note: isDrawing state is managed separately via DRAW packets
    }
}

void OnGamePacket(NiFiPacket packet) {
    // Handle custom packets
    if (strcmp(packet.command, "DRAW") == 0) {
        Game_HandleDrawPacket(&g_state, packet);
    }
    else if (strcmp(packet.command, "READY") == 0) {
        // Player ready status
        u8 clientId = atoi(packet.data[0]);
        bool ready = (atoi(packet.data[1]) != 0);

        // Find the client with this clientId and update their ready status
        for (int i = 0; i < CLIENT_MAX; i++) {
            if (clients[i].clientId == clientId) {
                g_state.players[i].isReady = ready;
                break;
            }
        }
    }
    else if (strcmp(packet.command, "START") == 0) {
        // Game starting - transition to in-game
        g_state.currentState = STATE_IN_GAME;
        Game_Init(&g_state);
        UI_ShowMessage(&g_state, "Game started!", 120);
    }
    else if (strcmp(packet.command, "CHAT") == 0) {
        // Chat message
        if (g_state.chatCount < MAX_CHAT_MESSAGES) {
            ChatMessage* msg = &g_state.chatHistory[g_state.chatCount++];
            // Intentionally truncate if name/message is too long
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(msg->playerName, PROFILE_NAME_LENGTH, "%s", packet.data[0]);
            snprintf(msg->message, CHAT_MESSAGE_LENGTH, "%s", packet.data[1]);
            #pragma GCC diagnostic pop
            msg->timestamp = 0;  // Could use timer
        }
    }
    else if (strcmp(packet.command, "CLEAR") == 0) {
        // Clear canvas
        Game_ClearCanvas(&g_state);
        UI_ShowMessage(&g_state, "Canvas cleared", 60);
    }
    else if (strcmp(packet.command, "DRAWEND") == 0) {
        // Player stopped drawing
        u8 clientId = atoi(packet.data[0]);
        // Find the client with this clientId and clear their drawing flag
        for (int i = 0; i < CLIENT_MAX; i++) {
            if (clients[i].clientId == clientId) {
                g_state.players[i].drawing.isDrawing = false;
                break;
            }
        }
    }
    else if (strcmp(packet.command, "COLOR") == 0) {
        // Player changed color
        u8 clientId = atoi(packet.data[0]);
        u8 color = atoi(packet.data[1]);
        // Find the client with this clientId and update their color
        for (int i = 0; i < CLIENT_MAX; i++) {
            if (clients[i].clientId == clientId) {
                g_state.players[i].drawing.brushColor = color;
                break;
            }
        }
    }
    else if (strcmp(packet.command, "CURSOR") == 0) {
        // Player cursor position (sent as part of full game state)
        u8 clientId = atoi(packet.data[0]);
        s16 x = atoi(packet.data[1]);
        s16 y = atoi(packet.data[2]);
        // Find the client with this clientId and update their cursor
        for (int i = 0; i < CLIENT_MAX; i++) {
            if (clients[i].clientId == clientId) {
                g_state.players[i].drawing.cursorX = x;
                g_state.players[i].drawing.cursorY = y;
                break;
            }
        }
    }
}

void OnDebugOutput(int type, char* message) {
    // Filter out raw packets (too verbose)
    if (type == NIFI_DBG_RawPacket) {
        return;
    }

    // Format debug messages with readable type labels
    char formattedMsg[DEBUG_MESSAGE_LENGTH];
    const char* prefix;

    switch (type) {
        case NIFI_DBG_Information:
            prefix = "I";
            break;
        case NIFI_DBG_Error:
            prefix = "E";
            break;
        case NIFI_DBG_SentPacket:
            prefix = "S";
            break;
        case NIFI_DBG_SentAcknowledgement:
            prefix = "SA";
            break;
        case NIFI_DBG_ReceivedPacket:
            prefix = "R";
            break;
        case NIFI_DBG_ReceivedAcknowledgement:
            prefix = "RA";
            break;
        default:
            prefix = "?";
            break;
    }

    // Format: TYPE | message (truncated to fit)
    snprintf(formattedMsg, sizeof(formattedMsg), "%s | %s", prefix, message);
    UI_AddDebugMessage(&g_state, formattedMsg, type);
}

void OnFullGameStateRequested(char macAddress[MAC_ADDRESS_LENGTH]) {
    // Host received a request for full game state from a spectator
    // Send all relevant game state using custom packets

    printf("Sending full game state to spectator\n");

    // Send complete state for each client together (prevents partial loading)
    for (int i = 0; i < CLIENT_MAX; i++) {
        if (clients[i].clientId == ID_EMPTY || clients[i].clientId == ID_ANY) {
            continue;
        }

        // 1. Announce client (so spectator adds them to clients[])
        NiFiPacket announcePacket;
        NiFi_SetPacket(&announcePacket, "CLIENT");
        sprintf(announcePacket.data[0], "%d", clients[i].clientId);
        strcpy(announcePacket.data[1], clients[i].macAddress);
        strcpy(announcePacket.data[2], clients[i].playerName);
        NiFi_SendBroadcast(&announcePacket, NULL);

        // 2. Send player color
        NiFiPacket colorPacket;
        NiFi_SetPacket(&colorPacket, "COLOR");
        sprintf(colorPacket.data[0], "%d", clients[i].clientId);
        sprintf(colorPacket.data[1], "%d", g_state.players[i].drawing.brushColor);
        NiFi_SendBroadcast(&colorPacket, NULL);

        // 3. Send player cursor position
        NiFiPacket posPacket;
        NiFi_SetPacket(&posPacket, "CURSOR");
        sprintf(posPacket.data[0], "%d", clients[i].clientId);
        sprintf(posPacket.data[1], "%d", g_state.players[i].drawing.cursorX);
        sprintf(posPacket.data[2], "%d", g_state.players[i].drawing.cursorY);
        NiFi_SendBroadcast(&posPacket, NULL);
    }

    // Note: Canvas state is sent via continuous DRAW packets during gameplay
    // If you need to send the full canvas, you could add a "CANVAS" packet type
    // that sends compressed canvas data or key pixels
}

// ============================================================================
// NETWORK HELPER FUNCTIONS
// ============================================================================

void Network_Init(void) {
    // Register all event handlers
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
    NiFi_OnFullGameStateRequested(OnFullGameStateRequested);

    // Initialize NiFi
    NiFi_Init(WIFI_CHANNEL, NIFI_TIMER, GAME_IDENTIFIER);
}

void Network_SendChatMessage(const char* message) {
    if (!localClient) return;

    NiFiPacket packet;
    NiFi_SetPacket(&packet, "CHAT");

    strncpy(packet.data[0], localClient->playerName, READ_PARAM_LENGTH - 1);
    packet.data[0][READ_PARAM_LENGTH - 1] = '\0';
    strncpy(packet.data[1], message, READ_PARAM_LENGTH - 1);
    packet.data[1][READ_PARAM_LENGTH - 1] = '\0';

    NiFi_SendBroadcast(&packet, NULL);

    // Add to local chat history
    if (g_state.chatCount < MAX_CHAT_MESSAGES) {
        ChatMessage* msg = &g_state.chatHistory[g_state.chatCount++];
        snprintf(msg->playerName, PROFILE_NAME_LENGTH, "%s", localClient->playerName);
        snprintf(msg->message, CHAT_MESSAGE_LENGTH, "%s", message);
        msg->timestamp = 0;
    }
}

void Network_SendDrawPacket(s16 x, s16 y, u8 color, u8 size) {
    if (!localClient) return;

    NiFiPacket packet;
    NiFi_SetPacket(&packet, "DRAW");

    sprintf(packet.data[0], "%d", x);
    sprintf(packet.data[1], "%d", y);
    sprintf(packet.data[2], "%d", color);
    sprintf(packet.data[3], "%d", localClient->clientId);
    sprintf(packet.data[4], "%d", size);

    NiFi_SendBroadcast(&packet, NULL);
}

void Network_SendReadyStatus(bool ready) {
    if (!localClient) return;

    NiFiPacket packet;
    NiFi_SetPacket(&packet, "READY");

    // Send clientId (not index) - it's unique across all peers
    sprintf(packet.data[0], "%d", localClient->clientId);
    sprintf(packet.data[1], "%d", ready ? 1 : 0);

    NiFi_SendBroadcast(&packet, NULL);

    // Update local state - find our index
    for (int i = 0; i < CLIENT_MAX; i++) {
        if (clients[i].clientId == localClient->clientId) {
            g_state.players[i].isReady = ready;
            break;
        }
    }
}

void Network_SendStartGame(void) {
    if (!NiFi_IsHost()) return;

    NiFiPacket packet;
    NiFi_SetPacket(&packet, "START");

    NiFi_SendBroadcast(&packet, NULL);

    // Transition locally
    g_state.currentState = STATE_IN_GAME;
    Game_Init(&g_state);
}
