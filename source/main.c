// ============================================================================
// NiFi Lobby Demo - main.c
// ============================================================================
// Comprehensive demo showcasing NiFi's lobby system features
// ============================================================================

#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gl2d.h>
#include <dswifi9.h>
#include <dsnifi9.h>
#include "main.h"

// ============================================================================
// GLOBAL STATE
// ============================================================================
GameState g_state;

// Console for text output (bottom screen)
PrintConsole bottomScreen;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
int GetLocalClientIndex(void);

void HandleMainMenuInput(int keysDown, touchPosition touch);
void HandleLobbySetupInput(int keysDown, touchPosition touch);
void HandleRoomBrowserInput(int keysDown, touchPosition touch);
void HandleLobbyInput(int keysDown, touchPosition touch);
void HandleGameInput(int keysDown, int keysHeld, touchPosition touch);
void HandleSpectatorInput(int keysDown, touchPosition touch);

void RenderCanvas(void);
void UpdateConsole(void);

// ============================================================================
// INITIALIZATION
// ============================================================================
void InitializeApp(void) {
    // Set BOTTOM screen to use 3D engine for GL2D
    lcdMainOnBottom();

    // Initialize video
    videoSetMode(MODE_5_3D);  // Main engine (bottom after swap) for GL2D
    videoSetModeSub(MODE_0_2D);  // Sub engine (top after swap) for console

    // Initialize console on TOP screen for info display
    vramSetBankC(VRAM_C_SUB_BG);
    consoleInit(&bottomScreen, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);
    consoleSelect(&bottomScreen);

    // Initialize GL2D for rendering
    glScreen2D();

    // Initialize game state
    memset(&g_state, 0, sizeof(GameState));
    g_state.currentState = STATE_MAIN_MENU;
    g_state.previousState = STATE_MAIN_MENU;
    g_state.maxPlayers = 4;  // Default to 4 players
    g_state.selectedMenuOption = 0;
    g_state.selectedRoomIndex = 0;
    g_state.roomCount = 0;
    g_state.chatCount = 0;

    // Allocate canvas
    g_state.canvas = (u16*)malloc(CANVAS_SIZE * sizeof(u16));
    if (g_state.canvas) {
        Game_ClearCanvas(&g_state);
    }

    // Initialize network
    Network_Init();

    // Print welcome message
    printf("\x1b[2J");  // Clear console
    printf("=================================\n");
    printf("  NiFi Lobby System Demo\n");
    printf("=================================\n");
    printf("Game ID: %s\n", GAME_IDENTIFIER);
    printf("WiFi Channel: %d\n\n", WIFI_CHANNEL);
    printf("Ready!\n\n");
}

// ============================================================================
// MAIN MENU HANDLING
// ============================================================================
void HandleMainMenuInput(int keysDown, touchPosition touch) {
    if (keysDown & KEY_UP) {
        g_state.selectedMenuOption = (g_state.selectedMenuOption + 2) % 3;
    }
    if (keysDown & KEY_DOWN) {
        g_state.selectedMenuOption = (g_state.selectedMenuOption + 1) % 3;
    }

    if (keysDown & KEY_A) {
        switch (g_state.selectedMenuOption) {
            case 0:  // Host Lobby
                g_state.currentState = STATE_LOBBY_SETUP;
                printf("Lobby Setup - Configure your room\n");
                printf("D-PAD: Adjust max players\n");
                printf("A: Create lobby\n");
                printf("B: Cancel\n\n");
                break;

            case 1:  // Join Lobby
                g_state.currentState = STATE_ROOM_BROWSER;
                g_state.roomCount = 0;
                printf("Room Browser - Scanning...\n");
                printf("A: Rescan\n");
                printf("UP/DOWN: Select room\n");
                printf("X: Join selected\n");
                printf("B: Back\n\n");
                NiFi_ScanRooms();
                break;

            case 2:  // Spectate
                printf("Starting spectator mode...\n");
                if (NiFi_StartSpectating(WIFI_CHANNEL, NIFI_TIMER, GAME_IDENTIFIER)) {
                    g_state.currentState = STATE_SPECTATING;
                    g_state.roomCount = 0;
                    printf("Spectator mode active!\n");
                    printf("B: Cycle rooms\n");
                    printf("A: Watch selected room\n");
                    printf("START: Exit spectator\n\n");
                } else {
                    printf("Failed to start spectator mode\n\n");
                }
                break;
        }
    }
}

// ============================================================================
// LOBBY SETUP HANDLING
// ============================================================================
void HandleLobbySetupInput(int keysDown, touchPosition touch) {
    if (keysDown & KEY_LEFT) {
        if (g_state.maxPlayers > 2) {
            g_state.maxPlayers--;
            printf("Max players: %d\n", g_state.maxPlayers);
        }
    }
    if (keysDown & KEY_RIGHT) {
        if (g_state.maxPlayers < 6) {
            g_state.maxPlayers++;
            printf("Max players: %d\n", g_state.maxPlayers);
        }
    }

    if (keysDown & KEY_A) {
        // Create room
        NiFi_CreateRoom();
        g_state.currentState = STATE_LOBBY;
        printf("Lobby created! Waiting for players...\n");
        printf("L: Lock lobby\n");
        printf("R: Set room status\n");
        printf("START: Start game\n");
        printf("SELECT: Cancel lobby\n\n");
    }

    if (keysDown & KEY_B) {
        g_state.currentState = STATE_MAIN_MENU;
        printf("Cancelled\n\n");
    }
}

// ============================================================================
// ROOM BROWSER HANDLING
// ============================================================================
void HandleRoomBrowserInput(int keysDown, touchPosition touch) {
    if (keysDown & KEY_UP) {
        if (g_state.selectedRoomIndex > 0) {
            g_state.selectedRoomIndex--;
        }
    }
    if (keysDown & KEY_DOWN) {
        if (g_state.selectedRoomIndex < g_state.roomCount - 1) {
            g_state.selectedRoomIndex++;
        }
    }

    if (keysDown & KEY_A) {
        // Rescan
        g_state.roomCount = 0;
        printf("Scanning for rooms...\n");
        NiFi_ScanRooms();
    }

    if (keysDown & KEY_X) {
        // Join selected room
        if (g_state.roomCount > 0 && g_state.selectedRoomIndex < g_state.roomCount) {
            NiFiRoom* room = &g_state.discoveredRooms[g_state.selectedRoomIndex];
            printf("Joining %s...\n", room->roomName);
            NiFi_JoinRoom(room->macAddress);
        }
    }

    if (keysDown & KEY_B) {
        g_state.currentState = STATE_MAIN_MENU;
        printf("Back to main menu\n\n");
    }
}

// ============================================================================
// LOBBY HANDLING
// ============================================================================
void HandleLobbyInput(int keysDown, touchPosition touch) {
    if (NiFi_IsHost()) {
        // Host controls
        if (keysDown & KEY_L) {
            // Toggle lock
            NiFiRoomStatus current = NiFi_GetRoomStatus();
            if (current == NIFI_ROOM_LOBBY_OPEN) {
                NiFi_SetRoomStatus(NIFI_ROOM_LOBBY_CLOSED);
                printf("Lobby locked\n");
            } else if (current == NIFI_ROOM_LOBBY_CLOSED) {
                NiFi_SetRoomStatus(NIFI_ROOM_LOBBY_OPEN);
                printf("Lobby unlocked\n");
            }
        }

        if (keysDown & KEY_R) {
            // Cycle room status
            NiFiRoomStatus current = NiFi_GetRoomStatus();
            NiFiRoomStatus next = (current + 1) % 4;
            NiFi_SetRoomStatus(next);
            const char* names[] = {"LOBBY_OPEN", "LOBBY_CLOSED", "INGAME_OPEN", "INGAME_CLOSED"};
            printf("Status: %s\n", names[next]);
        }

        if (keysDown & KEY_START) {
            // Start game
            Network_SendStartGame();
        }

        if (keysDown & KEY_SELECT) {
            // Cancel lobby
            NiFi_LeaveRoom();
            g_state.currentState = STATE_MAIN_MENU;
            printf("Lobby cancelled\n\n");
        }
    } else {
        // Client controls
        if (keysDown & KEY_A) {
            // Toggle ready
            int clientIndex = GetLocalClientIndex();
            if (clientIndex >= 0) {
                bool ready = !g_state.players[clientIndex].isReady;
                Network_SendReadyStatus(ready);
                printf(ready ? "Ready!\n" : "Not ready\n");
            }
        }

        if (keysDown & KEY_B) {
            // Leave room
            NiFi_LeaveRoom();
            g_state.currentState = STATE_MAIN_MENU;
            printf("Left lobby\n\n");
        }
    }

    // Chat (both host and client)
    if (keysDown & KEY_X) {
        Network_SendChatMessage("Hello!");
        printf("Sent: Hello!\n");
    }
}

// ============================================================================
// HELPER - Get client array index
// ============================================================================
int GetLocalClientIndex(void) {
    if (!localClient) return -1;
    for (int i = 0; i < CLIENT_MAX; i++) {
        if (clients[i].clientId == localClient->clientId) {
            return i;
        }
    }
    return -1;
}

// ============================================================================
// IN-GAME HANDLING
// ============================================================================
void HandleGameInput(int keysDown, int keysHeld, touchPosition touch) {
    if (!localClient) return;

    int clientIndex = GetLocalClientIndex();
    if (clientIndex < 0) return;

    PlayerDrawing* pd = &g_state.players[clientIndex].drawing;

    // Handle touch input - just move the dot
    if (keysHeld & KEY_TOUCH) {
        Game_HandleDrawing(&g_state, touch);
    }

    // Color selection (L/R) - change dot color
    if (keysDown & KEY_L) {
        pd->brushColor = (pd->brushColor + NUM_BRUSH_COLORS - 1) % NUM_BRUSH_COLORS;
        // Broadcast color change
        NiFiPacket packet;
        NiFi_SetPacket(&packet, "COLOR");
        sprintf(packet.data[0], "%d", localClient->clientId);
        sprintf(packet.data[1], "%d", pd->brushColor);
        NiFi_SendBroadcast(&packet, NULL);
    }
    if (keysDown & KEY_R) {
        pd->brushColor = (pd->brushColor + 1) % NUM_BRUSH_COLORS;
        // Broadcast color change
        NiFiPacket packet;
        NiFi_SetPacket(&packet, "COLOR");
        sprintf(packet.data[0], "%d", localClient->clientId);
        sprintf(packet.data[1], "%d", pd->brushColor);
        NiFi_SendBroadcast(&packet, NULL);
    }

    // Exit to lobby (START)
    if (keysDown & KEY_START) {
        g_state.currentState = STATE_LOBBY;
        printf("Returned to lobby\n");
    }
}

// ============================================================================
// SPECTATOR HANDLING
// ============================================================================
void HandleSpectatorInput(int keysDown, touchPosition touch) {
    if (keysDown & KEY_B) {
        // Discover/cycle rooms
        int roomCount = NiFi_GetDiscoveredRooms(g_state.discoveredRooms);
        if (roomCount > 0) {
            g_state.roomCount = roomCount;
            g_state.selectedRoomIndex = (g_state.selectedRoomIndex + 1) % roomCount;
            NiFiRoom* room = &g_state.discoveredRooms[g_state.selectedRoomIndex];
            printf("[%d/%d] %s (%d/%d)\n",
                   g_state.selectedRoomIndex + 1, roomCount,
                   room->roomName, room->memberCount, room->roomSize);
        } else {
            printf("No rooms found\n");
        }
    }

    if (keysDown & KEY_A) {
        // Watch selected room
        if (g_state.roomCount > 0 && g_state.selectedRoomIndex < g_state.roomCount) {
            NiFiRoom* room = &g_state.discoveredRooms[g_state.selectedRoomIndex];
            if (NiFi_SpectateRoom(*room)) {
                printf("Watching: %s\n", room->roomName);
            }
        }
    }

    if (keysDown & KEY_START) {
        // Stop spectating
        NiFi_StopSpectating();
        g_state.currentState = STATE_MAIN_MENU;
        printf("Stopped spectating\n\n");
    }
}

// ============================================================================
// RENDER CANVAS
// ============================================================================
void RenderCanvas(void) {
    if (!g_state.canvas) return;

    // Copy canvas to screen buffer
    u16* vram = (u16*)BG_GFX;
    dmaCopy(g_state.canvas, vram, CANVAS_SIZE * sizeof(u16));
}

// ============================================================================
// UPDATE CONSOLE
// ============================================================================
void UpdateConsole(void) {
    // Switch to bottom screen for debug output
    consoleSelect(&bottomScreen);

    // Display current state info
    static int lastState = -1;
    if (lastState != g_state.currentState) {
        lastState = g_state.currentState;

        // Don't clear too often, just update when state changes
        const char* stateName[] = {
            "MAIN MENU",
            "LOBBY SETUP",
            "ROOM BROWSER",
            "LOBBY",
            "IN-GAME",
            "SPECTATING"
        };

        printf("\n--- %s ---\n", stateName[g_state.currentState]);
    }

    // Show room info if connected
    if (g_state.currentState == STATE_LOBBY || g_state.currentState == STATE_IN_GAME) {
        static int lastMemberCount = -1;
        int memberCount = 0;
        for (int i = 0; i < CLIENT_MAX; i++) {
            if (clients[i].clientId != ID_EMPTY && clients[i].clientId != ID_ANY) {
                memberCount++;
            }
        }

        if (memberCount != lastMemberCount) {
            lastMemberCount = memberCount;
            printf("Players: %d\n", memberCount);
        }
    }
}

// ============================================================================
// MAIN LOOP
// ============================================================================
int main(void) {
    InitializeApp();

    // Main loop
    while (1) {
        scanKeys();
        int pressed = keysDown();
        int held = keysHeld();

        touchPosition touch;
        touchRead(&touch);

        // Handle input based on current state
        switch (g_state.currentState) {
            case STATE_MAIN_MENU:
                HandleMainMenuInput(pressed, touch);
                break;

            case STATE_LOBBY_SETUP:
                HandleLobbySetupInput(pressed, touch);
                break;

            case STATE_ROOM_BROWSER:
                HandleRoomBrowserInput(pressed, touch);
                break;

            case STATE_LOBBY:
                HandleLobbyInput(pressed, touch);
                break;

            case STATE_IN_GAME:
                HandleGameInput(pressed, held, touch);
                break;

            case STATE_SPECTATING:
                HandleSpectatorInput(pressed, touch);
                break;
        }

        // Render based on current state
        switch (g_state.currentState) {
            case STATE_MAIN_MENU:
                UI_DrawMainMenu(&g_state);
                break;

            case STATE_LOBBY_SETUP:
                UI_DrawLobbySetup(&g_state);
                break;

            case STATE_ROOM_BROWSER:
                UI_DrawRoomBrowser(&g_state);
                break;

            case STATE_LOBBY:
                UI_DrawLobby(&g_state);
                break;

            case STATE_IN_GAME:
                UI_DrawGame(&g_state);
                break;

            case STATE_SPECTATING:
                UI_DrawSpectator(&g_state);
                break;
        }

        // Update console (bottom screen)
        UpdateConsole();

        // Wait for VBlank
        swiWaitForVBlank();
    }

    // Cleanup
    if (g_state.canvas) {
        free(g_state.canvas);
    }
    NiFi_Shutdown();

    return 0;
}
