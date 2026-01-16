// ============================================================================
// NiFi Lobby Demo - game.c
// ============================================================================
// Collaborative drawing game logic
// ============================================================================

#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dsnifi9.h>
#include "main.h"

// External game state (defined in main.c)
extern GameState g_state;

// ============================================================================
// INITIALIZATION
// ============================================================================
void Game_Init(GameState* state) {
    // Allocate canvas memory if not already allocated
    if (!state->canvas) {
        state->canvas = (u16*)malloc(CANVAS_SIZE * sizeof(u16));
        if (state->canvas) {
            Game_ClearCanvas(state);
        }
    }

    // Initialize all players' drawing state
    for (int i = 0; i < CLIENT_MAX; i++) {
        state->players[i].drawing.brushColor = 0;  // Black
        state->players[i].drawing.brushSize = 2;   // Medium
        state->players[i].drawing.cursorX = CANVAS_WIDTH / 2;
        state->players[i].drawing.cursorY = CANVAS_HEIGHT / 2;
        state->players[i].drawing.isDrawing = false;
        state->players[i].isReady = false;
    }
}

// ============================================================================
// CLEAR CANVAS
// ============================================================================
void Game_ClearCanvas(GameState* state) {
    if (state->canvas) {
        // Fill with white
        for (int i = 0; i < CANVAS_SIZE; i++) {
            state->canvas[i] = COLOR_WHITE;
        }
    }
}

// ============================================================================
// DRAW PIXEL
// ============================================================================
void Game_DrawPixel(u16* canvas, s16 x, s16 y, u16 color, u8 size) {
    if (!canvas) return;

    // Draw a circle (approximated by filled square) of given size
    for (s16 dy = -size; dy <= size; dy++) {
        for (s16 dx = -size; dx <= size; dx++) {
            s16 px = x + dx;
            s16 py = y + dy;

            // Check bounds
            if (px >= 0 && px < CANVAS_WIDTH && py >= 0 && py < CANVAS_HEIGHT) {
                // Simple circle test
                if (dx * dx + dy * dy <= size * size) {
                    canvas[py * CANVAS_WIDTH + px] = color;
                }
            }
        }
    }
}

// ============================================================================
// DRAW LINE (Bresenham's algorithm)
// ============================================================================
void Game_DrawLine(u16* canvas, s16 x0, s16 y0, s16 x1, s16 y1, u16 color, u8 size) {
    if (!canvas) return;

    s16 dx = abs(x1 - x0);
    s16 dy = abs(y1 - y0);
    s16 sx = (x0 < x1) ? 1 : -1;
    s16 sy = (y0 < y1) ? 1 : -1;
    s16 err = dx - dy;

    while (true) {
        Game_DrawPixel(canvas, x0, y0, color, size);

        if (x0 == x1 && y0 == y1) break;

        s16 e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

// ============================================================================
// HANDLE TOUCH INPUT (Move Dot)
// ============================================================================
void Game_HandleDrawing(GameState* state, touchPosition touch) {
    if (!localClient) return;

    // Find local client's index in the clients array
    int clientIndex = -1;
    for (int i = 0; i < CLIENT_MAX; i++) {
        if (clients[i].clientId == localClient->clientId) {
            clientIndex = i;
            break;
        }
    }
    if (clientIndex < 0) return;

    PlayerDrawing* pd = &state->players[clientIndex].drawing;

    // Update cursor position
    pd->cursorX = touch.px;
    pd->cursorY = touch.py;

    // Broadcast position update so other players can see our dot moving
    Position pos;
    pos.x = pd->cursorX;
    pos.y = pd->cursorY;
    NiFi_BroadcastPosition(pos);
}

// ============================================================================
// BROADCAST DRAW PACKET
// ============================================================================
void Game_BroadcastDrawPixel(s16 x, s16 y, u8 color, u8 clientIndex) {
    if (!localClient) return;

    NiFiPacket packet;
    NiFi_SetPacket(&packet, "DRAW");

    // Pack draw data - send clientId (not index) so all peers can identify correctly
    sprintf(packet.data[0], "%d", x);
    sprintf(packet.data[1], "%d", y);
    sprintf(packet.data[2], "%d", color);
    sprintf(packet.data[3], "%d", localClient->clientId);  // Send clientId

    // Get brush size for this client
    if (clientIndex < CLIENT_MAX && clients[clientIndex].clientId != ID_EMPTY) {
        sprintf(packet.data[4], "%d", g_state.players[clientIndex].drawing.brushSize);
    } else {
        sprintf(packet.data[4], "%d", 2);
    }

    NiFi_SendBroadcast(&packet, NULL);
}

// ============================================================================
// APPLY REMOTE DRAW
// ============================================================================
void Game_ApplyRemoteDraw(GameState* state, s16 x, s16 y, u8 colorIndex, u8 size) {
    if (!state->canvas) return;
    if (colorIndex >= NUM_BRUSH_COLORS) return;

    u16 color = BRUSH_COLORS[colorIndex];
    Game_DrawPixel(state->canvas, x, y, color, size);
}

// ============================================================================
// UPDATE GAME STATE
// ============================================================================
void Game_Update(GameState* state) {
    // Game update logic
    // Canvas is rendered directly via RenderCanvas() in main.c
    (void)state;  // Unused for now
}

// ============================================================================
// HANDLE RECEIVED DRAW PACKET
// ============================================================================
void Game_HandleDrawPacket(GameState* state, NiFiPacket packet) {
    // Parse draw packet data
    s16 x = atoi(packet.data[0]);
    s16 y = atoi(packet.data[1]);
    u8 colorIndex = atoi(packet.data[2]);
    u8 clientId = atoi(packet.data[3]);  // This is clientId, not index
    u8 size = atoi(packet.data[4]);

    // Don't apply our own draw packets (we already drew locally)
    if (localClient && clientId == localClient->clientId) {
        return;
    }

    // Find the client with this clientId in our local clients array
    int clientIndex = -1;
    for (int i = 0; i < CLIENT_MAX; i++) {
        if (clients[i].clientId == clientId) {
            clientIndex = i;
            break;
        }
    }

    // Apply the draw to our canvas
    Game_ApplyRemoteDraw(state, x, y, colorIndex, size > 0 ? size : 2);

    // Update the player's cursor position if we found them
    if (clientIndex >= 0) {
        state->players[clientIndex].drawing.cursorX = x;
        state->players[clientIndex].drawing.cursorY = y;
        state->players[clientIndex].drawing.isDrawing = true;
        // Note: isDrawing will be cleared when touch is released via position update
    }
}
