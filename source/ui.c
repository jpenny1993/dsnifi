// ============================================================================
// NiFi Lobby Demo - ui.c
// ============================================================================
// UI rendering functions for all application states
// ============================================================================

#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <gl2d.h>
#include <dsnifi9.h>
#include "main.h"

// Console initialization flag
static bool consoleInitialized = false;

// Current screen for console output (shared with main.c)
extern PrintConsole bottomScreen;

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Print formatted text at specific position on top screen console
void PrintAt(int x, int y, const char* format, ...) {
    char buffer[128];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // Print to current console (top screen)
    printf("\x1b[%d;%dH%s", y, x, buffer);
}

// Clear the text console
void ClearText(void) {
    printf("\x1b[2J");  // Clear screen
}

// Draw a filled rounded rectangle (approximation using boxes)
void DrawPanel(int x, int y, int w, int h, int color) {
    glBoxFilled(x, y, x + w, y + h, color);
}

// Simple 5x7 pixel font bitmaps for capital letters
// Each character is stored as 7 bytes, each byte represents a row
static const u8 font5x7[][7] = {
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // A
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}, // B
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, // C
    {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}, // D
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, // E
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}, // F
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}, // G
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // H
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, // I
    {0x0F, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C}, // J
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, // K
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}, // L
    {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}, // M
    {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}, // N
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // O
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}, // P
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}, // Q
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}, // R
    {0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E}, // S
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, // T
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // U
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}, // V
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11}, // W
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}, // X
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}, // Y
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}, // Z
};

// Draw a single character at position with given color
void DrawChar(int x, int y, char c, int color) {
    if (c >= 'a' && c <= 'z') c -= 32; // Convert to uppercase
    if (c >= 'A' && c <= 'Z') {
        const u8* glyph = font5x7[c - 'A'];
        // Draw each pixel as a single pixel for compact text
        for (int row = 0; row < 7; row++) {
            for (int col = 0; col < 5; col++) {
                if (glyph[row] & (1 << (4 - col))) {
                    glBox(x + col, y + row, x + col, y + row, color);
                }
            }
        }
    } else if (c == ' ') {
        // Space - do nothing
    }
}

// Draw text string at position
void DrawText(int x, int y, const char* text, int color) {
    int cx = x;
    while (*text) {
        DrawChar(cx, y, *text, color);
        cx += 7; // Character width (5) + 2 pixel spacing
        text++;
    }
}

// Draw button
void DrawButton(int x, int y, int w, int h, const char* text, bool selected, bool disabled) {
    int bgColor = disabled ? COLOR_DARK_GRAY : (selected ? COLOR_UI_ACCENT : COLOR_UI_PANEL);
    int borderColor = selected ? COLOR_CYAN : COLOR_GRAY;
    int textColor = disabled ? COLOR_GRAY : COLOR_WHITE;

    // Background
    glBoxFilled(x, y, x + w, y + h, bgColor);

    // Border
    glBox(x, y, x + w, y + h, borderColor);

    // Text centered in button
    int textLen = strlen(text);
    int textWidth = textLen * 7; // Character spacing
    int textX = x + (w - textWidth) / 2;
    int textY = y + (h - 7) / 2;  // Character height is 7 pixels
    DrawText(textX, textY, text, textColor);
}

// Draw status badge (for room browser)
void DrawStatusBadge(int x, int y, NiFiRoomStatus status) {
    int color;
    int size = 8;

    switch (status) {
        case NIFI_ROOM_LOBBY_OPEN:
            color = COLOR_GREEN;  // Open - green circle
            glBoxFilled(x, y, x + size, y + size, color);
            break;
        case NIFI_ROOM_LOBBY_CLOSED:
            color = COLOR_YELLOW;  // Locked - yellow square
            glBoxFilled(x, y, x + size, y + size, color);
            break;
        case NIFI_ROOM_INGAME_OPEN:
            color = COLOR_CYAN;  // In game (open) - cyan
            glBoxFilled(x, y, x + size, y + size, color);
            break;
        case NIFI_ROOM_INGAME_CLOSED:
            color = COLOR_RED;  // In game (closed) - red
            glBoxFilled(x, y, x + size, y + size, color);
            break;
    }
}

// Draw player indicator (small colored box with name)
void DrawPlayerIndicator(int x, int y, const char* name, int color, bool isHost, bool isReady) {
    int boxSize = 16;

    // Player color box
    glBoxFilled(x, y, x + boxSize, y + boxSize, color);

    // Ready indicator (green checkmark approximation)
    if (isReady) {
        glBoxFilled(x + boxSize - 6, y, x + boxSize, y + 6, COLOR_GREEN);
    }

    // Host indicator (crown approximation)
    if (isHost) {
        glBoxFilled(x, y - 6, x + 6, y, COLOR_YELLOW);
    }
}

// ============================================================================
// MAIN MENU
// ============================================================================
void UI_DrawMainMenu(GameState* state) {
    glBegin2D();

    // Background
    glBoxFilled(0, 0, 256, 192, COLOR_UI_BG);

    // Title area
    DrawPanel(20, 20, 216, 40, COLOR_UI_PANEL);

    // Menu options (buttons)
    DrawButton(50, 80, 156, 25, "HOST LOBBY", state->selectedMenuOption == 0, false);
    DrawButton(50, 110, 156, 25, "JOIN LOBBY", state->selectedMenuOption == 1, false);
    DrawButton(50, 140, 156, 25, "SPECTATE", state->selectedMenuOption == 2, false);

    glEnd2D();
    glFlush(0);

    // Draw text labels on console
    ClearText();
    PrintAt(9, 4, "NiFi Lobby Demo");
    PrintAt(11, 11, "HOST LOBBY");
    PrintAt(11, 15, "JOIN LOBBY");
    PrintAt(12, 19, "SPECTATE");
    PrintAt(3, 23, "D-PAD: Select  A: Confirm");
}

// ============================================================================
// LOBBY SETUP (Host configures room)
// ============================================================================
void UI_DrawLobbySetup(GameState* state) {
    glBegin2D();

    // Background
    glBoxFilled(0, 0, 256, 192, COLOR_UI_BG);

    // Panel
    DrawPanel(20, 20, 216, 152, COLOR_UI_PANEL);

    // Max players indicator (visual boxes showing 2-6)
    int startX = 60;
    int startY = 80;
    for (int i = 0; i < 6; i++) {
        int color = (i < state->maxPlayers) ? COLOR_GREEN : COLOR_DARK_GRAY;
        glBoxFilled(startX + i * 25, startY, startX + i * 25 + 20, startY + 20, color);
    }

    // Buttons
    DrawButton(50, 120, 70, 25, "-", false, false);
    DrawButton(136, 120, 70, 25, "+", false, false);
    DrawButton(70, 150, 116, 20, "CREATE", false, false);

    glEnd2D();
    glFlush(0);

    // Draw text labels
    ClearText();
    PrintAt(8, 4, "Create Lobby");
    PrintAt(7, 8, "Max Players: %d", state->maxPlayers);
    PrintAt(9, 16, "-      +");
    PrintAt(12, 20, "CREATE");
    PrintAt(3, 23, "LEFT/RIGHT: Adjust  A: Create");
}

// ============================================================================
// ROOM BROWSER
// ============================================================================
void UI_DrawRoomBrowser(GameState* state) {
    glBegin2D();

    // Background
    glBoxFilled(0, 0, 256, 192, COLOR_UI_BG);

    // Title bar
    DrawPanel(10, 10, 236, 25, COLOR_UI_PANEL);

    // Room list
    int yOffset = 45;
    for (int i = 0; i < state->roomCount && i < 6; i++) {
        NiFiRoom* room = &state->discoveredRooms[i];
        bool selected = (i == state->selectedRoomIndex);

        // Room panel
        int panelColor = selected ? COLOR_UI_ACCENT : COLOR_UI_PANEL;
        DrawPanel(15, yOffset, 226, 22, panelColor);

        // Status badge
        DrawStatusBadge(20, yOffset + 7, room->status);

        // Border
        glBox(15, yOffset, 241, yOffset + 22, selected ? COLOR_CYAN : COLOR_GRAY);

        yOffset += 25;
    }

    // No rooms message area
    if (state->roomCount == 0) {
        DrawPanel(60, 80, 136, 30, COLOR_UI_PANEL);
    }

    // Buttons
    DrawButton(15, 165, 70, 22, "SCAN", false, false);
    DrawButton(93, 165, 70, 22, "JOIN", false, state->roomCount == 0);
    DrawButton(171, 165, 70, 22, "BACK", false, false);

    glEnd2D();
    glFlush(0);

    // Draw text labels
    ClearText();
    PrintAt(8, 2, "Available Rooms");

    // Room names
    int textY = 7;
    for (int i = 0; i < state->roomCount && i < 6; i++) {
        NiFiRoom* room = &state->discoveredRooms[i];
        const char* status = (room->status == NIFI_ROOM_LOBBY_OPEN) ? "Open" :
                            (room->status == NIFI_ROOM_LOBBY_CLOSED) ? "Lock" :
                            (room->status == NIFI_ROOM_INGAME_OPEN) ? "Game" : "Full";
        PrintAt(4, textY, "%s %s (%d/%d)", status, room->roomName, room->memberCount, room->roomSize);
        textY += 3;
    }

    if (state->roomCount == 0) {
        PrintAt(8, 12, "No rooms found");
    }

    PrintAt(3, 22, "SCAN   JOIN   BACK");
    PrintAt(2, 23, "UP/DOWN: Select  X: Join");
}

// ============================================================================
// LOBBY (Waiting room)
// ============================================================================
void UI_DrawLobby(GameState* state) {
    glBegin2D();

    // Background
    glBoxFilled(0, 0, 256, 192, COLOR_UI_BG);

    // Top panel - room info
    DrawPanel(10, 10, 236, 30, COLOR_UI_PANEL);

    // Player list area
    DrawPanel(10, 45, 236, 75, COLOR_UI_PANEL);

    // Draw connected players
    int playerX = 20;
    int playerY = 55;
    for (int i = 0; i < CLIENT_MAX; i++) {
        if (clients[i].clientId != ID_EMPTY && clients[i].clientId != ID_ANY) {
            int colorIndex = (clients[i].clientId - 1) % 6;
            bool isHost = (clients[i].clientId == 1);
            bool isReady = state->players[i].isReady;

            DrawPlayerIndicator(playerX, playerY, clients[i].playerName,
                              PLAYER_COLORS[colorIndex], isHost, isReady);

            playerX += 35;
            if (playerX > 200) {
                playerX = 20;
                playerY += 25;
            }
        }
    }

    // Chat area
    DrawPanel(10, 125, 236, 35, COLOR_UI_PANEL);

    // Button bar (host vs client)
    if (NiFi_IsHost()) {
        DrawButton(15, 165, 50, 22, "LOCK", false, false);
        DrawButton(70, 165, 50, 22, "START", false, false);
        DrawButton(125, 165, 50, 22, "CHAT", false, false);
        DrawButton(180, 165, 56, 22, "CANCEL", false, false);
    } else {
        DrawButton(15, 165, 70, 22, "READY", false, false);
        DrawButton(90, 165, 70, 22, "CHAT", false, false);
        DrawButton(165, 165, 71, 22, "LEAVE", false, false);
    }

    glEnd2D();
    glFlush(0);

    // Draw text labels
    ClearText();
    // Room status
    NiFiRoomStatus status = NiFi_GetRoomStatus();
    const char* statusText = (status == NIFI_ROOM_LOBBY_OPEN) ? "OPEN" :
                            (status == NIFI_ROOM_LOBBY_CLOSED) ? "LOCKED" :
                            (status == NIFI_ROOM_INGAME_OPEN) ? "IN GAME" : "CLOSED";
    PrintAt(10, 3, "Lobby - %s", statusText);

    // Player names
    int textX = 3;
    int textY = 7;
    for (int i = 0; i < CLIENT_MAX; i++) {
        if (clients[i].clientId != ID_EMPTY && clients[i].clientId != ID_ANY) {
            const char* ready = state->players[i].isReady ? "*" : " ";
            const char* host = (clients[i].clientId == 1) ? "H" : " ";
            PrintAt(textX, textY, "%s%s%.8s", host, ready, clients[i].playerName);
            textX += 10;
            if (textX > 25) {
                textX = 3;
                textY += 2;
            }
        }
    }

    // Buttons
    if (NiFi_IsHost()) {
        PrintAt(3, 22, "L:Lock R:Status START:Begin");
    } else {
        PrintAt(3, 22, "A:Ready X:Chat B:Leave");
    }
}

// ============================================================================
// IN-GAME (Moving Dots)
// ============================================================================
void UI_DrawGame(GameState* state) {
    glBegin2D();

    // Background - simple gradient or solid color
    glBoxFilled(0, 0, 256, 192, RGB15(8, 8, 12));  // Dark blue-gray

    // Draw player dots for connected clients
    for (int i = 0; i < CLIENT_MAX; i++) {
        // Only draw dot if client is actually connected
        if (clients[i].clientId != ID_EMPTY && clients[i].clientId != ID_ANY) {
            PlayerDrawing* pd = &state->players[i].drawing;

            // Skip if dot is at origin (likely uninitialized)
            if (pd->cursorX == 0 && pd->cursorY == 0) continue;

            // Get dot color based on player's selected color
            u16 dotColor = BRUSH_COLORS[pd->brushColor];

            // Draw a larger dot (12x12) for each player
            int dotSize = 6;
            glBoxFilled(pd->cursorX - dotSize, pd->cursorY - dotSize,
                       pd->cursorX + dotSize, pd->cursorY + dotSize,
                       dotColor);

            // Draw white border around dot
            glBox(pd->cursorX - dotSize - 1, pd->cursorY - dotSize - 1,
                 pd->cursorX + dotSize + 1, pd->cursorY + dotSize + 1,
                 COLOR_WHITE);
        }
    }

    // Color palette (bottom right corner)
    int paletteX = 200;
    int paletteY = 150;
    int swatchSize = 12;

    // Background for palette
    DrawPanel(paletteX - 5, paletteY - 5, 56, 30, COLOR_UI_PANEL);

    // Find local client index
    int localClientIndex = -1;
    if (localClient) {
        for (int i = 0; i < CLIENT_MAX; i++) {
            if (clients[i].clientId == localClient->clientId) {
                localClientIndex = i;
                break;
            }
        }
    }

    // Color swatches (4x2 grid)
    for (int i = 0; i < NUM_BRUSH_COLORS && i < 8; i++) {
        int x = paletteX + (i % 4) * (swatchSize + 2);
        int y = paletteY + (i / 4) * (swatchSize + 2);
        glBoxFilled(x, y, x + swatchSize, y + swatchSize, BRUSH_COLORS[i]);

        // Selection indicator
        if (localClientIndex >= 0 && state->players[localClientIndex].drawing.brushColor == i) {
            glBox(x - 1, y - 1, x + swatchSize + 1, y + swatchSize + 1, COLOR_YELLOW);
        }
    }

    glEnd2D();
    glFlush(0);

    // Draw text labels
    ClearText();
    PrintAt(1, 23, "Touch:Move L/R:Color START:Exit");
}

// ============================================================================
// SPECTATOR MODE
// ============================================================================
void UI_DrawSpectator(GameState* state) {
    glBegin2D();

    // Background
    glBoxFilled(0, 0, 256, 192, RGB15(8, 8, 12));  // Dark blue-gray

    // Draw player dots
    for (int i = 0; i < CLIENT_MAX; i++) {
        // Only draw dot if client is actually connected
        if (clients[i].clientId != ID_EMPTY && clients[i].clientId != ID_ANY) {
            PlayerDrawing* pd = &state->players[i].drawing;

            // Skip if dot is at origin (likely uninitialized)
            if (pd->cursorX == 0 && pd->cursorY == 0) continue;

            // Get dot color
            u16 dotColor = BRUSH_COLORS[pd->brushColor];

            // Draw dot
            int dotSize = 6;
            glBoxFilled(pd->cursorX - dotSize, pd->cursorY - dotSize,
                       pd->cursorX + dotSize, pd->cursorY + dotSize,
                       dotColor);

            // Draw white border
            glBox(pd->cursorX - dotSize - 1, pd->cursorY - dotSize - 1,
                 pd->cursorX + dotSize + 1, pd->cursorY + dotSize + 1,
                 COLOR_WHITE);
        }
    }

    // Spectator indicator banner (top)
    DrawPanel(50, 2, 156, 15, RGB15(15, 0, 15));  // Purple banner

    // Room info (bottom)
    DrawPanel(10, 170, 236, 18, COLOR_UI_PANEL);

    glEnd2D();
    glFlush(0);

    // Draw text labels
    ClearText();
    PrintAt(9, 2, "SPECTATING");
    if (state->roomCount > 0 && state->selectedRoomIndex < state->roomCount) {
        NiFiRoom* room = &state->discoveredRooms[state->selectedRoomIndex];
        PrintAt(2, 23, "Watching: %s", room->roomName);
    }
    PrintAt(2, 22, "B:Cycle START:Exit");
}

// ============================================================================
// MESSAGE POPUP
// ============================================================================
void UI_ShowMessage(GameState* state, const char* message, u32 duration) {
    strncpy(state->statusMessage, message, 63);
    state->statusMessage[63] = '\0';
    state->messageTimer = duration;
}

// ============================================================================
// INITIALIZATION
// ============================================================================
void UI_Init(void) {
    // GL2D is already initialized in main.c for bottom screen
    // Console is already on top screen (bottomScreen variable from main.c)

    // Just mark as initialized
    consoleInitialized = true;

    // Clear the top screen
    ClearText();
    PrintAt(0, 0, "NiFi Lobby Demo");
    PrintAt(0, 1, "==================");
}
