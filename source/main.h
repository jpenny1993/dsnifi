// ============================================================================
// NiFi Demo Application - main.h
// ============================================================================
// Configuration and type definitions for the NiFi demo application
//
// This header defines:
//   - Game configuration (identifier, timer, channel)
//   - ANSI color codes for console output
//   - Application-specific data structures
// ============================================================================

#ifndef MAIN_H_ /* Include guard */
#define MAIN_H_

#include <nds.h>

// ============================================================================
// NIFI CONFIGURATION
// ============================================================================

// Game Identifier: 4-character string that must match between all clients
// Only devices using the same identifier can communicate with each other
// Change this to make your game unique and avoid conflicts with other apps
#define GAME_IDENTIFIER "TEST"

// Hardware Timer: Which NDS timer to use for NiFi packet processing (0-3)
// The timer drives the packet send/receive loop via interrupt
// Make sure this timer isn't used by other parts of your game
#define NIFI_TIMER 0

// WiFi Channel: Which WiFi channel to use for communication (1-13)
// All players must be on the same channel to communicate
// Note: Some regions restrict certain channels
#define WIFI_CHANNEL 10

// ============================================================================
// ANSI COLOR CODES
// ============================================================================
// Used for colorized console output on the Nintendo DS
// These codes work with the standard NDS console library

#define RED     "\x1B[31m"  // Error messages, critical events
#define GREEN   "\x1B[32m"  // Success messages, acknowledgements
#define YELLOW  "\x1B[33m"  // Warnings, received packets
#define BLUE    "\x1B[34m"  // Info messages, sent packets
#define MAGENTA "\x1B[35m"  // Item usage, special events
#define CYAN    "\x1B[36m"  // Chat messages, host migration
#define WHITE   "\x1B[37m"  // Default text color

// ============================================================================
// APPLICATION DATA STRUCTURES
// ============================================================================

// ClientData: Application-specific data stored per connected client
// This is separate from NiFiClient (which is managed by the library)
// Add your own fields here for game-specific player state
typedef struct {
    Position position;  // Player position (synced via NiFi_BroadcastPosition)
    // Add more fields as needed for your game:
    // int health;
    // int score;
    // bool isReady;
    // etc.
} ClientData;

#endif // MAIN_H_