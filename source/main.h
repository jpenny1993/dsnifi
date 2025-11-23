// ============================================================================
// NiFi Lobby Demo - main.h
// ============================================================================
// Comprehensive demo showcasing NiFi's lobby system features:
//   - Host/Join/Leave lobbies
//   - Lock/Unlock rooms
//   - Collaborative drawing game
//   - Spectator mode for full/locked rooms
//   - Visual UI with proper state management
// ============================================================================

#ifndef MAIN_H_
#define MAIN_H_

#include <nds.h>
#include <dsnifi9.h>

// ============================================================================
// NIFI CONFIGURATION
// ============================================================================
#define GAME_IDENTIFIER "LOBY"
#define NIFI_TIMER 0
#define WIFI_CHANNEL 5

// ============================================================================
// APPLICATION STATES
// ============================================================================
typedef enum {
    STATE_MAIN_MENU,        // Choose Host / Join / Spectate
    STATE_LOBBY_SETUP,      // Configure room settings (host only)
    STATE_ROOM_BROWSER,     // Browse and select available rooms
    STATE_LOBBY,            // In lobby - waiting for game start
    STATE_IN_GAME,          // Playing the collaborative game
    STATE_SPECTATING        // Watching a game as spectator
} AppState;

// ============================================================================
// UI COLORS (RGB15 format)
// ============================================================================
#define COLOR_WHITE        RGB15(31, 31, 31)
#define COLOR_BLACK        RGB15(0, 0, 0)
#define COLOR_GRAY         RGB15(15, 15, 15)
#define COLOR_LIGHT_GRAY   RGB15(20, 20, 20)
#define COLOR_DARK_GRAY    RGB15(8, 8, 8)
#define COLOR_RED          RGB15(31, 0, 0)
#define COLOR_GREEN        RGB15(0, 31, 0)
#define COLOR_BLUE         RGB15(0, 0, 31)
#define COLOR_YELLOW       RGB15(31, 31, 0)
#define COLOR_CYAN         RGB15(0, 31, 31)
#define COLOR_MAGENTA      RGB15(31, 0, 31)
#define COLOR_ORANGE       RGB15(31, 15, 0)
#define COLOR_PURPLE       RGB15(20, 0, 31)
#define COLOR_UI_BG        RGB15(2, 2, 4)
#define COLOR_UI_PANEL     RGB15(5, 5, 10)
#define COLOR_UI_ACCENT    RGB15(10, 20, 31)
#define COLOR_UI_SUCCESS   RGB15(0, 25, 0)
#define COLOR_UI_WARNING   RGB15(31, 20, 0)
#define COLOR_UI_ERROR     RGB15(28, 0, 0)

// Player cursor colors (6 players max)
static const int PLAYER_COLORS[] = {
    RGB15(31, 10, 10),  // Red
    RGB15(10, 31, 10),  // Green
    RGB15(10, 10, 31),  // Blue
    RGB15(31, 31, 10),  // Yellow
    RGB15(31, 10, 31),  // Magenta
    RGB15(10, 31, 31)   // Cyan
};

// Drawing brush colors
static const int BRUSH_COLORS[] = {
    RGB15(0, 0, 0),      // Black
    RGB15(31, 31, 31),   // White
    RGB15(31, 0, 0),     // Red
    RGB15(0, 31, 0),     // Green
    RGB15(0, 0, 31),     // Blue
    RGB15(31, 31, 0),    // Yellow
    RGB15(31, 15, 0),    // Orange
    RGB15(20, 0, 31)     // Purple
};
#define NUM_BRUSH_COLORS 8

// ============================================================================
// DRAWING GAME DATA
// ============================================================================
#define CANVAS_WIDTH 256
#define CANVAS_HEIGHT 192
#define CANVAS_SIZE (CANVAS_WIDTH * CANVAS_HEIGHT)

typedef struct {
    u8 brushColor;      // Index into BRUSH_COLORS
    u8 brushSize;       // 1-5 pixels
    s16 cursorX;        // Current cursor position
    s16 cursorY;
    bool isDrawing;     // Currently drawing?
} PlayerDrawing;

// ============================================================================
// LOBBY DATA
// ============================================================================
#define MAX_CHAT_MESSAGES 10
#define CHAT_MESSAGE_LENGTH 50

typedef struct {
    char playerName[PROFILE_NAME_LENGTH];
    char message[CHAT_MESSAGE_LENGTH];
    u32 timestamp;
} ChatMessage;

typedef struct {
    bool isReady;           // Ready for game start?
    PlayerDrawing drawing;  // Drawing state
} PlayerData;

// ============================================================================
// GLOBAL STATE
// ============================================================================
typedef struct {
    AppState currentState;
    AppState previousState;

    // Lobby browser
    NiFiRoom discoveredRooms[6];
    int roomCount;
    int selectedRoomIndex;

    // Lobby setup
    int maxPlayers;  // 2-6

    // Chat
    ChatMessage chatHistory[MAX_CHAT_MESSAGES];
    int chatCount;

    // Player data
    PlayerData players[CLIENT_MAX];

    // Drawing canvas (shared)
    u16* canvas;  // Allocated dynamically

    // UI state
    int selectedMenuOption;
    bool showHelp;
    u32 messageTimer;
    char statusMessage[64];

    // Touch tracking
    bool wasTouching;
    s16 lastTouchX;
    s16 lastTouchY;

} GameState;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// ui.c
void UI_Init(void);
void UI_DrawMainMenu(GameState* state);
void UI_DrawLobbySetup(GameState* state);
void UI_DrawRoomBrowser(GameState* state);
void UI_DrawLobby(GameState* state);
void UI_DrawGame(GameState* state);
void UI_DrawSpectator(GameState* state);
void UI_ShowMessage(GameState* state, const char* message, u32 duration);

// game.c
void Game_Init(GameState* state);
void Game_Update(GameState* state);
void Game_HandleDrawing(GameState* state, touchPosition touch);
void Game_BroadcastDrawPixel(s16 x, s16 y, u8 color, u8 clientId);
void Game_DrawPixel(u16* canvas, s16 x, s16 y, u16 color, u8 size);
void Game_ClearCanvas(GameState* state);

// network.c
void Network_Init(void);
void Network_SendChatMessage(const char* message);
void Network_SendDrawPacket(s16 x, s16 y, u8 color, u8 size);
void Network_SendReadyStatus(bool ready);
void Network_SendStartGame(void);

#endif // MAIN_H_
