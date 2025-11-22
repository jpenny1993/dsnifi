# NiFi Spectator Mode - Step-by-Step Implementation Guide

**Document Version:** 1.1
**Created:** 2025-11-22
**Updated:** 2025-11-22 (Added room status compatibility)
**Purpose:** Detailed implementation instructions for spectator mode feature
**Prerequisites:**
- Read `spectator-mode-design.md` first for architecture overview
- **IMPORTANT:** Read [Room Status Compatibility](#room-status-compatibility) section before starting implementation
- Familiarize yourself with `room-status-review.md` to understand existing MAC filtering

---

## Table of Contents

1. [Overview](#overview)
2. [Room Status Compatibility](#room-status-compatibility)
3. [Phase 1: Core Infrastructure](#phase-1-core-infrastructure)
4. [Phase 2: Packet Filtering](#phase-2-packet-filtering)
5. [Phase 3: Room Discovery](#phase-3-room-discovery)
6. [Phase 4: Client Discovery](#phase-4-client-discovery)
7. [Phase 5: Transmission Blocking](#phase-5-transmission-blocking)
8. [Phase 6: Testing and Validation](#phase-6-testing-and-validation)
9. [Troubleshooting Guide](#troubleshooting-guide)

---

## Overview

This guide provides **exact code changes** for implementing spectator mode. Each phase includes:
- ✅ Specific file locations and line numbers
- ✅ Complete before/after code comparisons
- ✅ Integration points with existing code
- ✅ Testing checkpoints to verify correctness
- ✅ Expected behavior after each phase

**Estimated Total Time:** 8-10 hours (including testing)

**Implementation Order:** Follow phases sequentially. Each phase builds on the previous one.

---

## Room Status Compatibility

### ⚠️ IMPORTANT: Room Status Feature Already Exists

**By the time you implement spectator mode, the room status feature will already be deployed.**

The room status feature (documented in `room-status-review.md`) adds MAC-based packet filtering to improve performance during games. **This filtering was designed with spectator mode awareness built in.**

---

### What Room Status Does

Room status adds four states to rooms:
- `NIFI_ROOM_LOBBY_OPEN` - Lobby accepting new players
- `NIFI_ROOM_LOBBY_CLOSED` - Lobby locked (no joins)
- `NIFI_ROOM_INGAME_OPEN` - Game in progress, drop-in allowed
- `NIFI_ROOM_INGAME_CLOSED` - Game in progress, only returning players can rejoin

**Key Feature:** During `INGAME_OPEN` and `INGAME_CLOSED` states, the library filters out packets from unknown MAC addresses to save CPU (~30% performance improvement).

---

### Why This Matters for Spectator Mode

**The MAC filter checks `!IsSpectatorMode` before filtering:**

```c
// In IsPacketIntendedForMe() - Already implemented in room status
if (!IsSpectatorMode &&  // ← Spectator mode bypass
    (currentRoomStatus == NIFI_ROOM_INGAME_OPEN ||
     currentRoomStatus == NIFI_ROOM_INGAME_CLOSED)) {

    // Always allow room discovery
    if (strcmp(command, CMD_ROOM_SEARCH) == 0 ||
        strcmp(command, CMD_ROOM_ANNOUNCE) == 0) {
        return true;
    }

    // Filter unknown MACs (active mode only)
    if (IndexOfClientUsingMacAddress(macAddress) == INDEX_UNKNOWN) {
        return false;  // Blocks unknown MACs in active mode
    }
}
// Spectators bypass this completely - they observe all packets
```

**What This Means:**
- ✅ **Your spectator implementation will work correctly** as long as you set `IsSpectatorMode = true`
- ✅ The room status code already knows to bypass filtering for spectators
- ✅ You don't need to modify the room status filtering logic
- ✅ Spectators can observe rooms in any state (LOBBY, INGAME, OPEN, CLOSED)

---

### Implementation Requirements

#### 1. Set the IsSpectatorMode Flag

**In Phase 1 (Step 1.2), when adding global variables:**

```c
// nifi_arm9.c - Near line 30, with other globals
bool IsSpectatorMode = false;  // ← MUST be global, not static
SpectatorState spectatorState = {0};
```

**⚠️ CRITICAL:** `IsSpectatorMode` must be **global** (not static) so the room status code can check it.

**In NiFi_StartSpectating():**
```c
bool NiFi_StartSpectating(int wifiChannel, const char *gameId) {
    // ... validation ...

    IsSpectatorMode = true;  // ← MUST SET THIS

    // ... rest of initialization ...
}
```

**In NiFi_StopSpectating():**
```c
void NiFi_StopSpectating(void) {
    if (!IsSpectatorMode) return;

    IsSpectatorMode = false;  // ← MUST CLEAR THIS

    // ... rest of cleanup ...
}
```

#### 2. Parse Room Status Field

**In Phase 3 (Step 3.2), when parsing room announcements:**

The `NiFiRoom` struct now has a `status` field that you need to parse:

```c
// In HandlePacketAsSearching(), CMD_ROOM_ANNOUNCE handler
if (strcmp(p->command, CMD_ROOM_ANNOUNCE) == 0) {
    NiFiRoom room;
    strcpy(room.macAddress, p->data[0]);
    strcpy(room.roomName, p->data[1]);
    sscanf(p->data[2], "%hhd", &room.memberCount);
    sscanf(p->data[3], "%hhd", &room.roomSize);

    // ✅ NEW: Parse status field (added by room status feature)
    if (strlen(p->data[4]) > 0) {
        int status;
        sscanf(p->data[4], "%d", &status);
        room.status = (NiFiRoomStatus)status;
    } else {
        room.status = NIFI_ROOM_LOBBY_OPEN;  // Default for legacy
    }

    if (roomAnnouncedHandler) {
        (*roomAnnouncedHandler)(room);
    }
    return;
}
```

**Update the guide at Phase 3, Step 3.2 (around line 735) with this parsing logic.**

#### 3. No Changes Needed to Packet Filtering

**Good news:** The packet filtering modifications in Phase 2 (Step 2.3 and 2.4) work correctly with room status.

Your code:
```c
// Spectator mode: Accept ALL packets from target room (bypass client ID check)
if (!IsSpectatorMode) {
    if (PktToClientId != localClient->clientId) {
        return false;
    }
}
```

Room status code:
```c
// Status-based MAC filtering (with spectator bypass)
if (!IsSpectatorMode && (currentRoomStatus == NIFI_ROOM_INGAME_OPEN || ...)) {
    // Filter unknown MACs
}
```

Both features use the same `!IsSpectatorMode` pattern, so they compose correctly.

---

### Testing with Room Status

Add these test scenarios to Phase 6:

**Test 6.16: Spectator + Room Status States** (20 min)

Prerequisites: Room status feature already implemented

1. **Test LOBBY_OPEN:**
   - Device A creates room (status = LOBBY_OPEN)
   - Device B starts spectator mode
   - Device B discovers room
   - Verify room.status == NIFI_ROOM_LOBBY_OPEN
   - Verify spectator observes room correctly

2. **Test INGAME_CLOSED:**
   - Devices A (host), B, C in game (status = INGAME_CLOSED)
   - Device D starts spectator mode
   - Device D discovers room (should see status = INGAME_CLOSED)
   - Verify spectator discovers all clients (A, B, C)
   - Verify spectator receives position updates
   - **Critical:** Spectator's unknown MAC should NOT be filtered

3. **Test Status Transitions:**
   - Device A creates room (LOBBY_OPEN)
   - Device B spectates
   - Device C joins as client
   - Device A sets status to INGAME_CLOSED
   - Verify spectator continues receiving packets
   - Device C leaves and rejoins (returning player)
   - Verify spectator sees both events

4. **Test MAC Filtering Bypass:**
   - Devices A, B in game (INGAME_CLOSED)
   - Device C spectates (unknown MAC)
   - Device D tries to join (new player, should be blocked)
   - Verify:
     - Device C receives all packets ✅
     - Device D join is declined ✅
     - Device C never transmitted packets ✅

**Pass Criteria:** All tests pass, spectator works in all room states

---

### Troubleshooting

**Problem: Spectator doesn't discover clients during INGAME_CLOSED**

**Cause:** `IsSpectatorMode` not set to `true`, so MAC filtering blocks packets.

**Solution:** Verify `IsSpectatorMode = true` in `NiFi_StartSpectating()`.

---

**Problem: Room status field is always 0 (LOBBY_OPEN)**

**Cause:** Not parsing `p->data[4]` in room announcement handler.

**Solution:** Add status parsing code from section above.

---

### Cross-Feature Variables

Both features share these variables:

```c
// From spectator mode (you will add these)
extern bool IsSpectatorMode;  // In nifi_arm9.h
bool IsSpectatorMode = false; // In nifi_arm9.c

// From room status (already exists)
static NiFiRoomStatus currentRoomStatus = NIFI_ROOM_LOBBY_OPEN;  // In nifi_arm9.c
```

**Important:** Your `IsSpectatorMode` must be **non-static** so room status code can read it.

---

### Summary

✅ **What room status already did for you:**
- Added `!IsSpectatorMode` bypass to MAC filtering
- Added `status` field to `NiFiRoom` struct
- Added `NiFiRoomStatus` enum to public API
- Tested that spectators work in all room states

✅ **What you need to do:**
- Set `IsSpectatorMode = true` in `NiFi_StartSpectating()`
- Set `IsSpectatorMode = false` in `NiFi_StopSpectating()`
- Parse `room.status` field in room announcement handler
- Add integration tests (Test 6.16)

✅ **What works automatically:**
- Packet filtering bypass (already implemented)
- Room discovery in all states
- Client discovery regardless of room status
- MAC filtering doesn't block spectators

**You don't need to modify the room status code.** Just set the `IsSpectatorMode` flag correctly and parse the status field.

---

## Phase 1: Core Infrastructure

**Goal:** Add spectator state management and basic API functions.

**Estimated Time:** 2 hours

**Files Modified:**
1. `/mnt/c/nds/repo/dswifi/include/dsnifi9.h` (public API)
2. `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.h` (internal structures)
3. `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c` (implementation)

---

### Step 1.1: Add Spectator State Structure (nifi_arm9.h)

**File:** `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.h`

**Location:** After line 92 (after `#define WIFI_FRAME_OFFSET 16`)

**Add the following:**

```c
// ============================================================================
// SPECTATOR MODE STRUCTURES
// ============================================================================

/**
 * Spectator mode state tracking.
 */
typedef struct {
    bool isEnabled;                          // Spectator mode active
    u8 targetRoomId;                         // Room being observed (0 = none)
    char targetHostMac[MAC_ADDRESS_LENGTH];  // Host MAC address
    NiFiRoom discoveredRooms[6];             // Available rooms during scan
    u8 discoveredRoomCount;                  // Number of rooms found
} SpectatorState;
```

**Testing Checkpoint:** Compile the project. Should succeed with no errors.

```bash
cd /mnt/c/nds/repo/dswifi
make clean
make
```

---

### Step 1.2: Add Global Spectator State Variable (nifi_arm9.c)

**File:** `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`

**Location:** After line 50 (after global variables like `IsHost`, `localClient`, etc.)

**Find this section (around line 50):**
```c
bool IsHost = false;
NiFiClient *localClient;
NiFiClient *host = NULL;
```

**Add immediately after:**
```c
// Spectator mode state
bool IsSpectatorMode = false;
SpectatorState spectatorState = {0};
```

**Testing Checkpoint:** Compile again. Should succeed.

---

### Step 1.3: Add Public API Declarations (dsnifi9.h)

**File:** `/mnt/c/nds/repo/dswifi/include/dsnifi9.h`

**Location:** After line 126 (after existing function declarations, before `#ifdef __cplusplus`)

**Add the following:**

```c
// ============================================================================
// SPECTATOR MODE API
// ============================================================================

/**
 * Initializes NiFi in spectator mode and starts scanning for rooms.
 *
 * @param wifiChannel WiFi channel to listen on (1-13)
 * @param gameId 4-character game identifier (must match target game)
 * @return true if spectator mode started successfully, false on error
 *
 * @note Spectator mode is mutually exclusive with active play mode.
 *       Do NOT call NiFi_Init() and NiFi_StartSpectating() together.
 * @note Battery consumption will be higher due to promiscuous mode.
 */
bool NiFi_StartSpectating(int wifiChannel, const char *gameId);

/**
 * Selects a specific room to spectate.
 *
 * @param room Room to observe (obtained from OnRoomAnnounced handler)
 * @return true if room targeting succeeded, false on error
 *
 * @note Must call NiFi_StartSpectating() first.
 * @note OnRoomAnnounced handler will fire for each discovered room.
 */
bool NiFi_SpectateRoom(NiFiRoom room);

/**
 * Stops spectator mode and disables WiFi.
 *
 * @note Clears all client state and event handlers.
 * @note Call NiFi_Init() to switch back to active mode.
 */
void NiFi_StopSpectating(void);

/**
 * Checks if currently in spectator mode.
 *
 * @return true if spectating, false otherwise
 */
bool NiFi_IsSpectating(void);

/**
 * Gets the list of discovered rooms during scanning.
 *
 * @param rooms Output buffer for room list (must be size [6])
 * @return Number of rooms discovered (0-6)
 *
 * @note Only valid after calling NiFi_StartSpectating().
 * @note Room list updates as OnRoomAnnounced handler fires.
 */
int NiFi_GetDiscoveredRooms(NiFiRoom *rooms);
```

**Testing Checkpoint:** Compile. Should succeed with no warnings.

---

### Step 1.4: Implement NiFi_IsSpectating() (nifi_arm9.c)

**File:** `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`

**Location:** After existing `NiFi_Shutdown()` function (around line 1079)

**Add the following:**

```c
// ============================================================================
// SPECTATOR MODE IMPLEMENTATION
// ============================================================================

bool NiFi_IsSpectating(void) {
    return IsSpectatorMode;
}
```

**Testing Checkpoint:** Compile and verify no errors.

---

### Step 1.5: Implement NiFi_StartSpectating() (nifi_arm9.c)

**File:** `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`

**Location:** After `NiFi_IsSpectating()` function

**Add the following complete implementation:**

```c
bool NiFi_StartSpectating(int wifiChannel, const char *gameId) {
    // Validate parameters
    if (wifiChannel < 1 || wifiChannel > 13) {
        if (debugMessageHandler) {
            debugMessageHandler(NIFI_ERROR, "Invalid WiFi channel (must be 1-13)");
        }
        return false;
    }

    if (gameId == NULL || strlen(gameId) != 4) {
        if (debugMessageHandler) {
            debugMessageHandler(NIFI_ERROR, "Invalid game ID (must be 4 characters)");
        }
        return false;
    }

    // Prevent starting spectator mode if already initialized
    if (IsHost || localClient->clientId != ID_EMPTY) {
        if (debugMessageHandler) {
            debugMessageHandler(NIFI_ERROR, "Cannot start spectator mode: NiFi already initialized");
        }
        return false;
    }

    // Initialize spectator state
    memset(&spectatorState, 0, sizeof(SpectatorState));
    spectatorState.isEnabled = true;
    spectatorState.targetRoomId = ID_ANY;  // Scanning mode
    spectatorState.discoveredRoomCount = 0;
    IsSpectatorMode = true;

    // Copy game identifier
    strncpy(GameIdentifier, gameId, 4);
    GameIdentifier[4] = '\0';

    // Initialize WiFi hardware
    Wifi_InitDefault(false);  // Don't auto-connect to AP
    Wifi_SetRawPacketMode(PACKET_MODE_NIFI);
    Wifi_RawSetPacketHandler(OnRawPacketReceived);
    Wifi_SetChannel(wifiChannel);
    Wifi_SetPromiscuousMode(1);  // Enable promiscuous mode for spectators
    Wifi_EnableWifi();

    // Initialize client array
    localClient = &clients[0];
    localClient->clientId = ID_EMPTY;  // Spectator has no client ID
    strcpy(localClient->macAddress, GetMacAddressString());
    strcpy(localClient->playerName, GetProfileName());
    localClient->lastMessageId = 0;

    // Clear other client slots
    for (int i = 1; i < CLIENT_MAX; i++) {
        clients[i].clientId = ID_EMPTY;
        memset(clients[i].macAddress, 0, MAC_ADDRESS_LENGTH);
        memset(clients[i].playerName, 0, PROFILE_NAME_LENGTH);
        clients[i].lastMessageId = 0;
    }

    // Set room ID to scanning mode
    MyRoomId = ID_ANY;
    IsHost = false;
    host = NULL;

    // Initialize packet buffers (same as normal mode)
    ipIndex = 0;
    akIndex = 0;
    opIndex = 0;
    spIndex = 0;
    memset(IncomingPackets, 0, sizeof(IncomingPackets));
    memset(OutgoingPackets, 0, sizeof(OutgoingPackets));
    memset(EncodedPacketBuffer, 0, sizeof(EncodedPacketBuffer));

    if (debugMessageHandler) {
        char msg[64];
        sprintf(msg, "Spectator mode started on channel %d for game '%s'", wifiChannel, gameId);
        debugMessageHandler(NIFI_INFO, msg);
    }

    return true;
}
```

**Key Points:**
- Validates parameters (channel 1-13, game ID is 4 chars)
- Prevents starting if already in active mode
- Initializes WiFi in promiscuous mode
- Sets `IsSpectatorMode = true` and `MyRoomId = ID_ANY`
- Initializes client array with spectator as clients[0]
- Clears packet buffers

**Testing Checkpoint:** Compile. Should succeed. We'll test runtime behavior after Phase 2.

---

### Step 1.6: Implement NiFi_StopSpectating() (nifi_arm9.c)

**File:** `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`

**Location:** After `NiFi_StartSpectating()` function

**Add the following:**

```c
void NiFi_StopSpectating(void) {
    if (!IsSpectatorMode) {
        return;  // Not in spectator mode, nothing to do
    }

    if (debugMessageHandler) {
        debugMessageHandler(NIFI_INFO, "Stopping spectator mode");
    }

    // Disable promiscuous mode
    Wifi_SetPromiscuousMode(0);

    // Disable WiFi hardware
    Wifi_DisableWifi();

    // Clear spectator state
    memset(&spectatorState, 0, sizeof(SpectatorState));
    IsSpectatorMode = false;

    // Clear room and client state
    MyRoomId = ID_ANY;
    IsHost = false;
    host = NULL;

    // Clear client array
    for (int i = 0; i < CLIENT_MAX; i++) {
        clients[i].clientId = ID_EMPTY;
        memset(clients[i].macAddress, 0, MAC_ADDRESS_LENGTH);
        memset(clients[i].playerName, 0, PROFILE_NAME_LENGTH);
        clients[i].lastMessageId = 0;
    }

    localClient = NULL;

    // Clear packet buffers
    ipIndex = 0;
    akIndex = 0;
    opIndex = 0;
    spIndex = 0;
    memset(IncomingPackets, 0, sizeof(IncomingPackets));
    memset(OutgoingPackets, 0, sizeof(OutgoingPackets));
    memset(EncodedPacketBuffer, 0, sizeof(EncodedPacketBuffer));
}
```

**Key Points:**
- Disables promiscuous mode
- Disables WiFi hardware
- Clears all state (spectator, clients, packets)
- Safe to call even if not in spectator mode

**Testing Checkpoint:** Compile. Should succeed.

---

### Step 1.7: Implement NiFi_GetDiscoveredRooms() (nifi_arm9.c)

**File:** `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`

**Location:** After `NiFi_StopSpectating()` function

**Add the following:**

```c
int NiFi_GetDiscoveredRooms(NiFiRoom *rooms) {
    if (!IsSpectatorMode || rooms == NULL) {
        return 0;
    }

    // Copy discovered rooms to output buffer
    int count = spectatorState.discoveredRoomCount;
    if (count > 6) count = 6;  // Safety check

    for (int i = 0; i < count; i++) {
        memcpy(&rooms[i], &spectatorState.discoveredRooms[i], sizeof(NiFiRoom));
    }

    return count;
}
```

**Testing Checkpoint:** Compile. Should succeed.

---

### Step 1.8: Implement NiFi_SpectateRoom() (Stub for Now)

**File:** `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`

**Location:** After `NiFi_GetDiscoveredRooms()` function

**Add the following stub (we'll complete this in Phase 3):**

```c
bool NiFi_SpectateRoom(NiFiRoom room) {
    if (!IsSpectatorMode) {
        if (debugMessageHandler) {
            debugMessageHandler(NIFI_ERROR, "Not in spectator mode");
        }
        return false;
    }

    // TODO: Complete in Phase 3
    if (debugMessageHandler) {
        char msg[64];
        sprintf(msg, "Spectating room: %s (ID %d)", room.roomName, room.roomSize);
        debugMessageHandler(NIFI_INFO, msg);
    }

    return true;
}
```

**Testing Checkpoint:** Full compile and link test.

```bash
cd /mnt/c/nds/repo/dswifi
make clean
make
```

Expected: No errors, no warnings. Library compiles successfully.

---

### Phase 1 Completion Checklist

Before proceeding to Phase 2, verify:

- [ ] All files compile without errors
- [ ] `NiFi_IsSpectating()` returns false by default
- [ ] New API functions are declared in `dsnifi9.h`
- [ ] `SpectatorState` struct is defined in `nifi_arm9.h`
- [ ] Global variables `IsSpectatorMode` and `spectatorState` are declared

**Expected State:** Library compiles, but spectator mode doesn't work yet (packet filtering not implemented).

---

## Phase 2: Packet Filtering

**Goal:** Modify packet filtering to accept packets in spectator mode.

**Estimated Time:** 1.5 hours

**Files Modified:**
1. `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c` (IsPacketIntendedForMe function)

---

### Step 2.1: Understand Current Filtering Logic

**File:** `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`

**Function:** `IsPacketIntendedForMe()` (lines 175-228)

**Current Filtering Pipeline:**
1. Line 180: Game ID match check → DROP if mismatch
2. Line 184: Self-packet check → DROP if from self
3. Line 188-191: ACK processing → Special handling
4. Line 195: Room ID check → DROP if wrong room
5. **Line 202-205: Client ID targeting → DROP if not addressed to me** ⚠️ THIS IS THE KEY BLOCKER
6. Line 207: MAC validation → DROP if unknown sender

**What We'll Change:** Add spectator mode bypass at line 202 to skip client ID filtering.

---

### Step 2.2: Backup Original Function

Before making changes, document the original logic:

**Original Code (lines 202-205):**
```c
// Ignore game packets that aren't directed at me
sscanf(params[REQUEST_TO_INDEX], "%hhd", &PktToClientId);
if (PktToClientId != localClient->clientId) {
    return false;
}
```

This rejects packets where `TO_CLIENT` field doesn't match your client ID. Spectators need to see ALL packets, so we'll bypass this check.

---

### Step 2.3: Modify Client ID Filtering for Spectators

**File:** `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`

**Location:** Line 202 (inside `IsPacketIntendedForMe()` function)

**Find this code block (around lines 202-205):**
```c
// Ignore game packets that aren't directed at me
sscanf(params[REQUEST_TO_INDEX], "%hhd", &PktToClientId);
if (PktToClientId != localClient->clientId) {
    return false;
}
```

**Replace with:**
```c
// Ignore game packets that aren't directed at me
sscanf(params[REQUEST_TO_INDEX], "%hhd", &PktToClientId);

// Spectator mode: Accept ALL packets from target room (bypass client ID check)
if (!IsSpectatorMode) {
    if (PktToClientId != localClient->clientId) {
        return false;
    }
}
// If IsSpectatorMode == true, we skip the clientId check and accept the packet
```

**Key Changes:**
- Added `if (!IsSpectatorMode)` wrapper around client ID check
- Spectators bypass this filter entirely
- Normal mode behavior unchanged (check still runs)

---

### Step 2.4: Relax MAC Address Validation for Spectators

**File:** `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`

**Location:** Line 207 (inside `IsPacketIntendedForMe()` function)

**Find this code block (around line 207):**
```c
if (IndexOfClientUsingMacAddress(params[REQUEST_MACADDRESS_INDEX]) == -1) {
    return false;  // Unknown sender
}
```

**Replace with:**
```c
// In spectator mode, we learn about clients dynamically, so skip MAC validation initially
if (!IsSpectatorMode) {
    if (IndexOfClientUsingMacAddress(params[REQUEST_MACADDRESS_INDEX]) == -1) {
        return false;  // Unknown sender
    }
}
// Spectators accept packets from unknown MACs (they'll learn about clients later)
```

**Rationale:** Spectators don't know client MACs initially. They discover clients by observing traffic. This filter would block all packets at first, so we disable it for spectators.

---

### Step 2.5: Add Room ID Validation for Spectator Target

**File:** `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`

**Location:** Line 195 (inside `IsPacketIntendedForMe()` function, room ID check)

**Find this code block (around lines 195-200):**
```c
// Ignore packets from other rooms
sscanf(params[REQUEST_ROOMID_INDEX], "%hhd", &PktRoomId);
if (PktRoomId != MyRoomId) {
    return false;
}
```

**Replace with:**
```c
// Ignore packets from other rooms
sscanf(params[REQUEST_ROOMID_INDEX], "%hhd", &PktRoomId);

if (IsSpectatorMode) {
    // Spectator mode: Filter by target room ID (or accept all during scanning)
    if (spectatorState.targetRoomId != ID_ANY && PktRoomId != spectatorState.targetRoomId) {
        return false;  // Wrong room
    }
    // If targetRoomId == ID_ANY (scanning), accept all room announcements
} else {
    // Normal mode: Filter by your assigned room ID
    if (PktRoomId != MyRoomId) {
        return false;
    }
}
```

**Key Changes:**
- Spectators use `spectatorState.targetRoomId` instead of `MyRoomId`
- During scanning (`targetRoomId == ID_ANY`), accept all rooms
- After selecting room, filter to only that room

---

### Step 2.6: Testing Checkpoint - Compile and Verify

**Compile the library:**
```bash
cd /mnt/c/nds/repo/dswifi
make clean
make
```

**Expected Result:** No errors, no warnings.

**Code Review Checklist:**
- [ ] Client ID filter wrapped in `if (!IsSpectatorMode)`
- [ ] MAC validation wrapped in `if (!IsSpectatorMode)`
- [ ] Room ID check uses `spectatorState.targetRoomId` for spectators
- [ ] Original active mode behavior preserved (no regressions)

---

### Step 2.7: Testing - Runtime Verification

**Create a test application** (or modify existing `main.c`):

```c
// Minimal test in /mnt/c/nds/repo/nifitest/source/main.c
void TestSpectatorFiltering() {
    printf("Testing spectator mode...\n");

    // Register debug handler
    NiFi_SetDebugOutput(OnDebugOutput);

    // Start spectator mode
    if (!NiFi_StartSpectating(10, "TEST")) {
        printf("ERROR: Failed to start spectator mode\n");
        return;
    }

    printf("Spectator mode active: %s\n", NiFi_IsSpectating() ? "YES" : "NO");

    // Wait and listen for packets
    printf("Listening for 10 seconds...\n");
    for (int i = 0; i < 600; i++) {  // 10 seconds at 60 FPS
        swiWaitForVBlank();
    }

    // Stop spectator mode
    NiFi_StopSpectating();
    printf("Spectator mode stopped\n");
}
```

**Expected Behavior:**
- `NiFi_IsSpectating()` returns `true` after starting
- WiFi hardware initializes (no errors)
- Debug messages show packets being received (if other DS devices are nearby)
- `NiFi_IsSpectating()` returns `false` after stopping

**Verification:**
- Check debug output for "Spectator mode started" message
- Verify promiscuous mode is enabled (more packets received than normal)
- Confirm no crashes or hangs

---

### Phase 2 Completion Checklist

Before proceeding to Phase 3, verify:

- [ ] Packet filtering modified to support spectator mode
- [ ] Client ID check bypassed for spectators
- [ ] MAC validation relaxed for spectators
- [ ] Room ID filtering uses spectator target room
- [ ] Library compiles without errors
- [ ] Test application starts/stops spectator mode successfully
- [ ] No regressions in normal (active) mode

**Expected State:** Spectators can receive packets from any room, but room discovery isn't implemented yet.

---

## Phase 3: Room Discovery

**Goal:** Enable spectators to scan for and select rooms to observe.

**Estimated Time:** 1.5 hours

**Files Modified:**
1. `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c` (NiFi_SpectateRoom, room announcement handling)

---

### Step 3.1: Understand Room Discovery Protocol

**How Room Discovery Works in Active Mode:**

1. Client calls `NiFi_ScanRooms()` → broadcasts `CMD_ROOM_SEARCH` packet
2. Host receives search, responds with `CMD_ROOM_ANNOUNCE` packet
3. Client's `OnRoomAnnounced` handler fires with room details
4. Client calls `NiFi_JoinRoom(macAddress)` to join

**How Spectator Mode Differs:**

1. Spectator starts with `targetRoomId = ID_ANY` (scanning mode)
2. Spectator listens passively for `CMD_ROOM_ANNOUNCE` packets (NO broadcast sent)
3. Spectator's `OnRoomAnnounced` handler fires for each discovered room
4. Spectator calls `NiFi_SpectateRoom(room)` to select target

**Key Difference:** Spectators don't send `CMD_ROOM_SEARCH` packets. They only listen.

---

### Step 3.2: Modify Room Announcement Processing

**File:** `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`

**Function:** `ProcessSearchingPacket()` (around lines 745-766)

This function handles `CMD_ROOM_ANNOUNCE` packets when you're searching for rooms.

**Find the room announcement handling code (around line 751):**

```c
if (strcmp(packet.command, CMD_ROOM) == 0) {
    // Parse room announcement
    NiFiRoom room;
    strcpy(room.macAddress, packet.macAddress);
    sscanf(packet.data[0], "%s", room.roomName);
    sscanf(packet.data[1], "%hhd", &room.memberCount);
    room.roomSize = CLIENT_MAX;

    // Fire handler
    if (roomAnnouncedHandler) {
        roomAnnouncedHandler(room);
    }
}
```

**Replace with:**

```c
if (strcmp(packet.command, CMD_ROOM) == 0) {
    // Parse room announcement
    NiFiRoom room;
    strcpy(room.macAddress, packet.macAddress);
    sscanf(packet.data[0], "%s", room.roomName);
    sscanf(packet.data[1], "%hhd", &room.memberCount);
    room.roomSize = CLIENT_MAX;

    // Store in spectator discovered rooms list (if in spectator mode)
    if (IsSpectatorMode) {
        bool alreadyDiscovered = false;

        // Check if room already in list (by MAC address)
        for (int i = 0; i < spectatorState.discoveredRoomCount; i++) {
            if (strcmp(spectatorState.discoveredRooms[i].macAddress, room.macAddress) == 0) {
                // Update existing entry (member count may have changed)
                spectatorState.discoveredRooms[i].memberCount = room.memberCount;
                alreadyDiscovered = true;
                break;
            }
        }

        // Add new room if not already discovered and space available
        if (!alreadyDiscovered && spectatorState.discoveredRoomCount < 6) {
            memcpy(&spectatorState.discoveredRooms[spectatorState.discoveredRoomCount],
                   &room, sizeof(NiFiRoom));
            spectatorState.discoveredRoomCount++;
        }
    }

    // Fire handler (for both active and spectator mode)
    if (roomAnnouncedHandler) {
        roomAnnouncedHandler(room);
    }
}
```

**Key Changes:**
- Added spectator-specific logic to store discovered rooms
- Prevents duplicates (compares MAC addresses)
- Updates member count if room already known
- Fires handler for both modes

---

### Step 3.3: Complete NiFi_SpectateRoom() Implementation

**File:** `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`

**Location:** Find the stub we created in Phase 1 (after `NiFi_GetDiscoveredRooms()`)

**Replace the stub with:**

```c
bool NiFi_SpectateRoom(NiFiRoom room) {
    if (!IsSpectatorMode) {
        if (debugMessageHandler) {
            debugMessageHandler(NIFI_ERROR, "Not in spectator mode");
        }
        return false;
    }

    // Validate room parameter
    if (strlen(room.macAddress) != MAC_ADDRESS_LENGTH - 1) {  // 12 chars (no null terminator)
        if (debugMessageHandler) {
            debugMessageHandler(NIFI_ERROR, "Invalid room MAC address");
        }
        return false;
    }

    // Store target room details
    spectatorState.targetRoomId = ID_ANY;  // We'll learn the room ID from packets
    strncpy(spectatorState.targetHostMac, room.macAddress, MAC_ADDRESS_LENGTH);

    // Extract room ID from next packet (we don't know it yet from announcement)
    // Room ID will be set dynamically when we receive first packet from this room

    if (debugMessageHandler) {
        char msg[64];
        sprintf(msg, "Targeting room: %s (MAC: %.12s)", room.roomName, room.macAddress);
        debugMessageHandler(NIFI_INFO, msg);
    }

    return true;
}
```

**Key Points:**
- Stores target host MAC address
- Sets `targetRoomId = ID_ANY` initially (we'll learn it from packets)
- Validates MAC address format

---

### Step 3.4: Add Room ID Learning Logic

When spectator targets a room, we don't know the room ID yet (only MAC address). We need to learn it from the first packet we receive from that host.

**File:** `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`

**Location:** Inside `ProcessIncomingPackets()` function (around line 880), after packet decoding but before dispatching to handlers.

**Find this section (around line 900):**
```c
// Process the packet based on command type
if (packet.isAcknowledgement) {
    // Handle ACK
    MarkOutgoingPacketProcessed(packet.messageId);
} else {
    // Data packet - send ACK and dispatch
    SendAcknowledgement(&packet);
    // ... dispatch to handlers ...
}
```

**Add BEFORE the ACK handling:**

```c
// Spectator mode: Learn room ID from first packet from target host
if (IsSpectatorMode && spectatorState.targetRoomId == ID_ANY) {
    // Check if this packet is from our target host
    if (strcmp(packet.macAddress, spectatorState.targetHostMac) == 0) {
        // Extract room ID from packet
        u8 pktRoomId;
        sscanf(params[REQUEST_ROOMID_INDEX], "%hhd", &pktRoomId);

        if (pktRoomId != ID_ANY && pktRoomId != ID_EMPTY) {
            spectatorState.targetRoomId = pktRoomId;

            if (debugMessageHandler) {
                char msg[64];
                sprintf(msg, "Learned room ID: %d from host", pktRoomId);
                debugMessageHandler(NIFI_INFO, msg);
            }
        }
    }
}

// ... continue with existing ACK handling ...
```

**Explanation:**
- After targeting a room, spectator waits for first packet from that host
- Extracts room ID from packet header
- Updates `spectatorState.targetRoomId` for filtering
- All future packets filtered to only this room

---

### Step 3.5: Testing Checkpoint - Room Discovery

**Test Application:**

```c
// Add to /mnt/c/nds/repo/nifitest/source/main.c
int roomsFound = 0;

void OnRoomDiscoveredHandler(NiFiRoom room) {
    roomsFound++;
    printf("Room %d: %s (%d/%d players)\n",
           roomsFound, room.roomName, room.memberCount, room.roomSize);
    printf("  MAC: %.12s\n", room.macAddress);
}

void TestRoomDiscovery() {
    printf("=== Testing Room Discovery ===\n");

    // Register handlers
    NiFi_SetDebugOutput(OnDebugOutput);
    NiFi_OnRoomAnnounced(OnRoomDiscoveredHandler);

    // Start spectator mode (scanning)
    if (!NiFi_StartSpectating(10, "TEST")) {
        printf("ERROR: Failed to start spectator mode\n");
        return;
    }

    printf("Scanning for rooms for 10 seconds...\n");

    // Wait for room announcements
    for (int i = 0; i < 600; i++) {  // 10 seconds
        swiWaitForVBlank();

        // Check for user input to select first room
        scanKeys();
        if ((keysDown() & KEY_A) && roomsFound > 0) {
            NiFiRoom rooms[6];
            int count = NiFi_GetDiscoveredRooms(rooms);

            if (count > 0) {
                printf("\nSpectating first room...\n");
                if (NiFi_SpectateRoom(rooms[0])) {
                    printf("Successfully targeted room\n");

                    // Wait 5 seconds observing
                    for (int j = 0; j < 300; j++) {
                        swiWaitForVBlank();
                    }
                }
                break;
            }
        }
    }

    printf("\nRooms discovered: %d\n", roomsFound);

    // Stop spectator mode
    NiFi_StopSpectating();
    printf("Test complete\n");
}
```

**Test Prerequisites:**
- At least 1 other Nintendo DS running NiFi in host mode (creating a room)
- Both devices on same WiFi channel (10)
- Both using same game ID ("TEST")

**Expected Behavior:**
1. Spectator mode starts successfully
2. `OnRoomAnnounced` handler fires for each nearby room
3. Room names and member counts displayed
4. Press A to spectate first room
5. `NiFi_SpectateRoom()` returns true
6. Debug message shows "Learned room ID: X from host"

**Verification:**
- [ ] At least 1 room discovered (if host nearby)
- [ ] `NiFi_GetDiscoveredRooms()` returns correct count
- [ ] Room MAC addresses are valid (12 hex characters)
- [ ] After targeting room, room ID learned from first packet

---

### Phase 3 Completion Checklist

Before proceeding to Phase 4, verify:

- [ ] Room announcement handling updated for spectator mode
- [ ] `NiFi_SpectateRoom()` fully implemented
- [ ] Room ID learning logic added to packet processing
- [ ] `spectatorState.discoveredRooms[]` populated correctly
- [ ] Test application discovers rooms and selects target successfully

**Expected State:** Spectators can discover rooms and select a target. Room filtering works. Client discovery not yet implemented.

---

## Phase 4: Client Discovery

**Goal:** Enable spectators to learn about clients by observing packet traffic.

**Estimated Time:** 1.5 hours

**Files Modified:**
1. `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c` (add client tracking functions)

---

### Step 4.1: Add Helper Function - UpdateSpectatorClientList()

**File:** `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`

**Location:** After the spectator mode API functions (after `NiFi_SpectateRoom()`)

**Add the following helper function:**

```c
/**
 * Updates spectator client list based on observed packet traffic.
 * Called for every packet processed in spectator mode.
 */
void UpdateSpectatorClientList(NiFiPacket *packet) {
    if (!IsSpectatorMode) return;

    u8 fromClientId = packet->fromClientId;
    char *macAddress = packet->macAddress;

    // Ignore packets with invalid client IDs
    if (fromClientId == ID_EMPTY || fromClientId == ID_ANY) {
        return;
    }

    // Check if client already known (by MAC address)
    int index = IndexOfClientUsingMacAddress(macAddress);

    if (index == -1) {
        // New client discovered - find empty slot
        index = -1;
        for (int i = 1; i < CLIENT_MAX; i++) {  // Skip clients[0] (spectator itself)
            if (clients[i].clientId == ID_EMPTY) {
                index = i;
                break;
            }
        }

        if (index == -1) {
            // No empty slots (shouldn't happen with 6-player limit)
            if (debugMessageHandler) {
                debugMessageHandler(NIFI_ERROR, "Client array full, cannot add new client");
            }
            return;
        }

        // Add new client to array
        clients[index].clientId = fromClientId;
        strncpy(clients[index].macAddress, macAddress, MAC_ADDRESS_LENGTH);
        clients[index].lastMessageId = packet->messageId;

        // Player name unknown initially (will learn from CMD_CLIENT packets)
        memset(clients[index].playerName, 0, PROFILE_NAME_LENGTH);

        if (debugMessageHandler) {
            char msg[64];
            sprintf(msg, "Discovered client ID %d (MAC: %.12s)", fromClientId, macAddress);
            debugMessageHandler(NIFI_INFO, msg);
        }

        // Trigger OnClientConnected handler
        if (clientConnectedHandler) {
            clientConnectedHandler(index, clients[index]);
        }

    } else {
        // Client already known - update client ID if changed (rejoin scenario)
        if (clients[index].clientId != fromClientId) {
            if (debugMessageHandler) {
                char msg[64];
                sprintf(msg, "Client ID changed: %d -> %d (MAC: %.12s)",
                        clients[index].clientId, fromClientId, macAddress);
                debugMessageHandler(NIFI_INFO, msg);
            }
            clients[index].clientId = fromClientId;
        }

        // Update last message ID
        clients[index].lastMessageId = packet->messageId;
    }
}
```

**Key Points:**
- Called for every packet in spectator mode
- Discovers new clients and adds to `clients[]` array
- Updates client IDs if client rejoins with different ID
- Triggers `OnClientConnected` handler for new clients
- Player names initially empty (learned later from `CMD_CLIENT` packets)

---

### Step 4.2: Add Helper Function - UpdateSpectatorHost()

**File:** `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`

**Location:** After `UpdateSpectatorClientList()` function

**Add the following:**

```c
/**
 * Updates spectator host pointer based on observed traffic.
 * Host is typically client ID 1, but can change during migration.
 */
void UpdateSpectatorHost(NiFiPacket *packet) {
    if (!IsSpectatorMode) return;

    // Host is identified by:
    // 1. Sender of CMD_CLIENT packets (client announcements)
    // 2. Sender of CMD_ROOM packets (room announcements)
    // 3. Sender of CMD_MIGRATE packets (during migration)
    // 4. Typically has clientId == 1

    bool isHostPacket = (strcmp(packet->command, CMD_ROOM) == 0 ||
                         strcmp(packet->command, CMD_CLIENT) == 0 ||
                         strcmp(packet->command, CMD_MIGRATE) == 0 ||
                         packet->fromClientId == 1);

    if (isHostPacket) {
        int hostIndex = IndexOfClientUsingId(packet->fromClientId);

        if (hostIndex != -1 && host != &clients[hostIndex]) {
            host = &clients[hostIndex];

            if (debugMessageHandler) {
                char msg[64];
                sprintf(msg, "Host identified: Client ID %d (MAC: %.12s)",
                        host->clientId, host->macAddress);
                debugMessageHandler(NIFI_INFO, msg);
            }
        }
    }
}
```

**Key Points:**
- Identifies host by packet type (room management packets)
- Host typically has client ID 1
- Updates `host` global pointer
- Handles host migration automatically

---

### Step 4.3: Add Helper Function - ProcessSpectatorClientAnnouncement()

When a new client joins the game, the host broadcasts a `CMD_CLIENT` packet with player name and details. Spectators need to process these to learn player names.

**File:** `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`

**Location:** After `UpdateSpectatorHost()` function

**Add the following:**

```c
/**
 * Processes CMD_CLIENT packets to learn player names in spectator mode.
 */
void ProcessSpectatorClientAnnouncement(NiFiPacket *packet) {
    if (!IsSpectatorMode) return;

    // CMD_CLIENT format:
    // data[0] = clientId (as string)
    // data[1] = playerName
    // data[2] = macAddress

    u8 announcedClientId;
    char playerName[PROFILE_NAME_LENGTH];
    char macAddress[MAC_ADDRESS_LENGTH];

    sscanf(packet->data[0], "%hhd", &announcedClientId);
    strncpy(playerName, packet->data[1], PROFILE_NAME_LENGTH - 1);
    playerName[PROFILE_NAME_LENGTH - 1] = '\0';
    strncpy(macAddress, packet->data[2], MAC_ADDRESS_LENGTH - 1);
    macAddress[MAC_ADDRESS_LENGTH - 1] = '\0';

    // Find client in array
    int index = IndexOfClientUsingId(announcedClientId);

    if (index != -1) {
        // Update player name
        strncpy(clients[index].playerName, playerName, PROFILE_NAME_LENGTH);

        if (debugMessageHandler) {
            char msg[64];
            sprintf(msg, "Learned player name: %s (ID %d)", playerName, announcedClientId);
            debugMessageHandler(NIFI_INFO, msg);
        }
    } else {
        // Client not yet in array - add now with full details
        index = -1;
        for (int i = 1; i < CLIENT_MAX; i++) {
            if (clients[i].clientId == ID_EMPTY) {
                index = i;
                break;
            }
        }

        if (index != -1) {
            clients[index].clientId = announcedClientId;
            strncpy(clients[index].macAddress, macAddress, MAC_ADDRESS_LENGTH);
            strncpy(clients[index].playerName, playerName, PROFILE_NAME_LENGTH);
            clients[index].lastMessageId = 0;

            if (debugMessageHandler) {
                char msg[64];
                sprintf(msg, "Added client from announcement: %s (ID %d)", playerName, announcedClientId);
                debugMessageHandler(NIFI_INFO, msg);
            }

            // Trigger OnClientConnected handler
            if (clientConnectedHandler) {
                clientConnectedHandler(index, clients[index]);
            }
        }
    }
}
```

**Key Points:**
- Parses `CMD_CLIENT` packets to extract player details
- Updates player names for existing clients
- Adds new clients with full details if not yet discovered
- Handles mid-game join scenarios

---

### Step 4.4: Integrate Client Discovery into Packet Processing

**File:** `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`

**Location:** Inside `ProcessIncomingPackets()` function (around line 920), after processing the packet but before dispatching to command-specific handlers.

**Find this section (around line 920-925):**
```c
// Data packet - send ACK and dispatch
SendAcknowledgement(&packet);

// Dispatch to command-specific handler
if (strcmp(packet.command, CMD_POSITION) == 0) {
    ProcessPositionPacket(&packet);
} else if (strcmp(packet.command, CMD_CLIENT) == 0) {
    ProcessClientPacket(&packet);
}
// ... more command handlers ...
```

**Add BEFORE the command dispatching:**

```c
// Spectator mode: Update client list from observed traffic
if (IsSpectatorMode) {
    UpdateSpectatorClientList(&packet);
    UpdateSpectatorHost(&packet);

    // Handle client announcements specially
    if (strcmp(packet.command, CMD_CLIENT) == 0) {
        ProcessSpectatorClientAnnouncement(&packet);
    }
}

// Data packet - send ACK and dispatch (unless spectator mode)
if (!IsSpectatorMode) {
    SendAcknowledgement(&packet);  // Spectators don't send ACKs
}

// Dispatch to command-specific handler (both modes)
if (strcmp(packet.command, CMD_POSITION) == 0) {
    ProcessPositionPacket(&packet);
} else if (strcmp(packet.command, CMD_CLIENT) == 0) {
    ProcessClientPacket(&packet);
}
// ... continue with existing handlers ...
```

**Key Changes:**
- Added spectator client tracking before command dispatch
- Moved ACK sending inside `if (!IsSpectatorMode)` block
- Command handlers still fire normally (so `OnPositionUpdated`, `OnGamePacket`, etc. work)

---

### Step 4.5: Testing Checkpoint - Client Discovery

**Test Application:**

```c
// Add to /mnt/c/nds/repo/nifitest/source/main.c
int clientsDiscovered = 0;

void OnClientDiscoveredHandler(u8 index, NiFiClient client) {
    clientsDiscovered++;
    printf("Client %d joined:\n", clientsDiscovered);
    printf("  ID: %d\n", client.clientId);
    printf("  Name: %s\n", strlen(client.playerName) > 0 ? client.playerName : "(unknown)");
    printf("  MAC: %.12s\n", client.macAddress);
}

void OnPositionHandler(Position pos, u8 index, NiFiClient client) {
    printf("Client %d moved: (%d, %d, %d)\n", client.clientId, pos.x, pos.y, pos.z);
}

void TestClientDiscovery() {
    printf("=== Testing Client Discovery ===\n");

    // Register handlers
    NiFi_SetDebugOutput(OnDebugOutput);
    NiFi_OnRoomAnnounced(OnRoomDiscoveredHandler);
    NiFi_OnClientConnected(OnClientDiscoveredHandler);
    NiFi_OnPositionUpdated(OnPositionHandler);

    // Start spectator mode
    if (!NiFi_StartSpectating(10, "TEST")) {
        printf("ERROR: Failed to start spectator mode\n");
        return;
    }

    printf("Scanning for rooms...\n");

    // Wait for first room announcement
    bool roomSelected = false;
    for (int i = 0; i < 600; i++) {  // 10 seconds
        swiWaitForVBlank();

        if (!roomSelected) {
            NiFiRoom rooms[6];
            int count = NiFi_GetDiscoveredRooms(rooms);

            if (count > 0) {
                printf("\nSpectating room: %s\n", rooms[0].roomName);
                NiFi_SpectateRoom(rooms[0]);
                roomSelected = true;
            }
        }
    }

    if (!roomSelected) {
        printf("No rooms found\n");
        NiFi_StopSpectating();
        return;
    }

    // Observe for 30 seconds
    printf("\nObserving game for 30 seconds...\n");
    for (int i = 0; i < 1800; i++) {  // 30 seconds
        swiWaitForVBlank();
    }

    printf("\nClients discovered: %d\n", clientsDiscovered);

    // Print final client list
    printf("\nFinal client list:\n");
    for (int i = 1; i < CLIENT_MAX; i++) {
        if (clients[i].clientId != ID_EMPTY) {
            printf("  [%d] %s (ID %d)\n", i, clients[i].playerName, clients[i].clientId);
        }
    }

    // Stop spectator mode
    NiFi_StopSpectating();
    printf("Test complete\n");
}
```

**Test Prerequisites:**
- 2+ Nintendo DS devices running NiFi game (host + clients)
- Active gameplay with movement/packets being sent
- Same WiFi channel and game ID

**Expected Behavior:**
1. Spectator discovers room and selects it
2. `OnClientConnected` fires for each player
3. Client IDs and MAC addresses displayed
4. Player names appear when `CMD_CLIENT` packets observed
5. `OnPositionUpdated` fires for player movement
6. Host pointer correctly identifies host (client ID 1)

**Verification:**
- [ ] All active players discovered (client count matches)
- [ ] Player names learned from `CMD_CLIENT` packets
- [ ] Position updates trigger handler correctly
- [ ] Host pointer set correctly

---

### Phase 4 Completion Checklist

Before proceeding to Phase 5, verify:

- [ ] `UpdateSpectatorClientList()` implemented and called
- [ ] `UpdateSpectatorHost()` implemented and called
- [ ] `ProcessSpectatorClientAnnouncement()` implemented
- [ ] Client discovery integrated into packet processing
- [ ] Test application discovers all clients correctly
- [ ] Player names learned from announcements

**Expected State:** Spectators can observe games, see all players, track positions, and receive game packets. Only transmission blocking remains.

---

## Phase 5: Transmission Blocking

**Goal:** Ensure spectators never send any packets (zero network footprint).

**Estimated Time:** 0.5 hours

**Files Modified:**
1. `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c` (block outgoing packets)

---

### Step 5.1: Block SendAcknowledgement()

**File:** `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`

**Function:** `SendAcknowledgement()` (around line 264)

**Current Code (beginning of function):**
```c
void SendAcknowledgement(NiFiPacket *packet) {
    // ... ACK sending logic ...
}
```

**Add guard at the very beginning:**
```c
void SendAcknowledgement(NiFiPacket *packet) {
    // Spectators never send ACKs (passive observation only)
    if (IsSpectatorMode) {
        return;
    }

    // ... existing ACK sending logic ...
}
```

---

### Step 5.2: Block NiFi_SendPacket()

**File:** `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`

**Function:** `NiFi_SendPacket()` (around line 409)

**Add guard at the beginning:**
```c
void NiFi_SendPacket(NiFiPacket *packet, NiFiClient *client) {
    // Spectators cannot send packets
    if (IsSpectatorMode) {
        if (debugMessageHandler) {
            debugMessageHandler(NIFI_ERROR, "Cannot send packets in spectator mode");
        }
        return;
    }

    // ... existing send logic ...
}
```

---

### Step 5.3: Block NiFi_QueueBroadcast()

**File:** `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`

**Function:** `NiFi_QueueBroadcast()` (around line 451)

**Add guard at the beginning:**
```c
void NiFi_QueueBroadcast(NiFiPacket *packet, u8 ignoreClientIds[]) {
    // Spectators cannot broadcast
    if (IsSpectatorMode) {
        if (debugMessageHandler) {
            debugMessageHandler(NIFI_ERROR, "Cannot broadcast in spectator mode");
        }
        return;
    }

    // ... existing broadcast logic ...
}
```

---

### Step 5.4: Block NiFi_SendBroadcast()

**File:** `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`

**Function:** `NiFi_SendBroadcast()` (around line 480)

**Add guard at the beginning:**
```c
void NiFi_SendBroadcast(NiFiPacket *packet, u8 ignoreClientIds[]) {
    // Spectators cannot broadcast
    if (IsSpectatorMode) {
        if (debugMessageHandler) {
            debugMessageHandler(NIFI_ERROR, "Cannot broadcast in spectator mode");
        }
        return;
    }

    // ... existing broadcast logic ...
}
```

---

### Step 5.5: Block Room Management Functions

**File:** `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`

**Functions to block:**
- `NiFi_CreateRoom()` (line ~534)
- `NiFi_ScanRooms()` (line ~552)
- `NiFi_JoinRoom()` (line ~564)
- `NiFi_LeaveRoom()` (line ~591)

**Add guard to each function:**
```c
// Example for NiFi_CreateRoom()
void NiFi_CreateRoom(const char *roomName) {
    // Cannot create room in spectator mode
    if (IsSpectatorMode) {
        if (debugMessageHandler) {
            debugMessageHandler(NIFI_ERROR, "Cannot create room in spectator mode");
        }
        return;
    }

    // ... existing logic ...
}

// Repeat for NiFi_ScanRooms(), NiFi_JoinRoom(), NiFi_LeaveRoom()
```

---

### Step 5.6: Verify ProcessOutgoingPackets() Behavior

**File:** `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`

**Function:** `ProcessOutgoingPackets()` (around line 945)

This function processes the outgoing packet buffer. Since spectators never add packets to the outgoing buffer, this function should naturally have nothing to process.

**No code changes needed**, but verify the logic:
- Outgoing buffer (`OutgoingPackets[]`) should remain empty in spectator mode
- No packets added because all send functions blocked
- No performance impact (empty buffer processed quickly)

---

### Step 5.7: Testing Checkpoint - Zero Network Footprint

**Test with Packet Sniffer (Wireshark):**

If you have Wireshark or similar WiFi sniffer available:

1. Start spectator mode on DS
2. Note spectator's MAC address
3. Begin packet capture on WiFi channel 10
4. Observe game for 5 minutes
5. Filter captures by spectator's MAC address
6. **Expected Result:** ZERO packets transmitted from spectator MAC

**Test Without Sniffer (Application Level):**

```c
// Add to /mnt/c/nds/repo/nifitest/source/main.c
void TestTransmissionBlocking() {
    printf("=== Testing Transmission Blocking ===\n");

    NiFi_SetDebugOutput(OnDebugOutput);

    if (!NiFi_StartSpectating(10, "TEST")) {
        printf("ERROR: Failed to start spectator mode\n");
        return;
    }

    // Attempt operations that should be blocked
    printf("\nAttempting blocked operations...\n");

    // Try to create room (should fail)
    NiFi_CreateRoom("TestRoom");
    printf("CreateRoom called\n");

    // Try to join room (should fail)
    char fakeMac[13] = "AABBCCDDEEFF";
    NiFi_JoinRoom(fakeMac);
    printf("JoinRoom called\n");

    // Try to send broadcast (should fail)
    NiFiPacket packet;
    NiFi_SetPacket(&packet, "TEST");
    NiFi_SendBroadcast(&packet, NULL);
    printf("SendBroadcast called\n");

    // Try to send position (should fail)
    Position pos = {100, 100, 0};
    NiFi_BroadcastPosition(pos);
    printf("BroadcastPosition called\n");

    printf("\nCheck debug output for error messages.\n");
    printf("All operations should have been blocked.\n");

    // Wait to observe any leakage
    printf("\nObserving for 10 seconds...\n");
    for (int i = 0; i < 600; i++) {
        swiWaitForVBlank();
    }

    NiFi_StopSpectating();
    printf("Test complete\n");
}
```

**Expected Behavior:**
- All send/broadcast/room functions log error messages
- No packets transmitted (verify with debug output)
- No crashes or hangs

**Verification:**
- [ ] `CreateRoom` blocked with error message
- [ ] `JoinRoom` blocked with error message
- [ ] `SendBroadcast` blocked with error message
- [ ] `BroadcastPosition` blocked with error message
- [ ] No ACKs sent (even when receiving packets)
- [ ] Spectator MAC never appears in transmitted packets

---

### Phase 5 Completion Checklist

Before proceeding to Phase 6, verify:

- [ ] All send functions blocked with spectator mode guard
- [ ] ACK transmission disabled in spectator mode
- [ ] Room management functions blocked
- [ ] Test confirms zero packets transmitted
- [ ] Error messages logged for blocked operations

**Expected State:** Spectator mode fully functional with zero network footprint. Ready for comprehensive testing.

---

## Phase 6: Testing and Validation

**Goal:** Comprehensive testing of all spectator mode features.

**Estimated Time:** 2 hours

**Test Categories:**
1. Functional tests (feature correctness)
2. Integration tests (interaction with active players)
3. Edge case tests (host migration, disconnects, etc.)
4. Performance tests (battery, CPU usage)

---

### Test 6.1: Basic Spectator Lifecycle

**Test:** Start, spectate, stop spectator mode.

**Steps:**
1. Start spectator mode with `NiFi_StartSpectating(10, "TEST")`
2. Verify `NiFi_IsSpectating()` returns `true`
3. Wait 5 seconds
4. Stop spectator mode with `NiFi_StopSpectating()`
5. Verify `NiFi_IsSpectating()` returns `false`

**Expected Result:**
- No crashes or errors
- WiFi hardware initializes and shuts down cleanly
- Spectator state cleared after stop

**Pass Criteria:** ✅ All operations succeed without errors

---

### Test 6.2: Room Discovery

**Test:** Scan for and discover active rooms.

**Prerequisites:** 1+ NDS running NiFi in host mode

**Steps:**
1. Start spectator mode
2. Register `OnRoomAnnounced` handler
3. Wait 10 seconds
4. Call `NiFi_GetDiscoveredRooms()`
5. Verify at least 1 room discovered

**Expected Result:**
- `OnRoomAnnounced` fires for each nearby room
- Room name, MAC, and member count populated correctly
- Discovered rooms list matches handler callbacks

**Pass Criteria:** ✅ At least 1 room discovered with valid data

---

### Test 6.3: Room Selection and Targeting

**Test:** Select a specific room to spectate.

**Steps:**
1. Discover rooms (Test 6.2)
2. Call `NiFi_SpectateRoom(rooms[0])`
3. Verify function returns `true`
4. Wait for debug message "Learned room ID: X from host"
5. Verify `spectatorState.targetRoomId` set correctly

**Expected Result:**
- Room targeting succeeds
- Room ID learned from first packet
- Subsequent packets filtered to only this room

**Pass Criteria:** ✅ Room ID learned within 5 seconds

---

### Test 6.4: Client Discovery (Join Before Spectator)

**Test:** Discover clients that joined before spectator started.

**Prerequisites:** 2+ NDS in active game (host + 1 client)

**Steps:**
1. Start game with 2 players
2. Start spectator and select room
3. Register `OnClientConnected` handler
4. Wait 30 seconds
5. Verify both clients discovered

**Expected Result:**
- `OnClientConnected` fires for each client
- Client IDs, MACs, and names populated
- Position updates received for both clients

**Pass Criteria:** ✅ All clients discovered within 30 seconds

---

### Test 6.5: Client Discovery (Join After Spectator)

**Test:** Discover new client that joins mid-observation.

**Prerequisites:** 1 NDS in host mode, 1 spectator, 1 client to join late

**Steps:**
1. Start host (alone)
2. Start spectator and select room
3. Register `OnClientConnected` handler
4. Have 2nd player join game
5. Verify spectator sees new client

**Expected Result:**
- `OnClientConnected` fires when new player joins
- New client details populated correctly
- Position updates received immediately

**Pass Criteria:** ✅ New client discovered within 5 seconds of join

---

### Test 6.6: Position Updates

**Test:** Receive and process position updates from all clients.

**Steps:**
1. Spectate active game with 2+ players
2. Register `OnPositionUpdated` handler
3. Have players move around
4. Verify handler fires for each player

**Expected Result:**
- `OnPositionUpdated` fires for each position change
- Position data accurate (x, y, z values)
- Handler receives correct client index and details

**Pass Criteria:** ✅ All position updates received

---

### Test 6.7: Custom Game Packets

**Test:** Receive custom game events (chat, scores, etc.).

**Prerequisites:** Game that sends custom packets

**Steps:**
1. Spectate active game
2. Register `OnGamePacket` handler
3. Have players trigger custom events (chat message, score change)
4. Verify handler receives packets

**Expected Result:**
- `OnGamePacket` fires for custom packets
- Packet data correct and complete
- All custom commands received

**Pass Criteria:** ✅ All custom packets received

---

### Test 6.8: Host Migration

**Test:** Spectator handles host migration correctly.

**Steps:**
1. Spectate game with 3+ players
2. Have host leave (triggers migration)
3. Verify spectator updates host pointer
4. Verify spectator continues receiving packets from new host

**Expected Result:**
- `OnHostMigration` handler fires (if registered)
- `host` pointer updated to new host
- No packet loss or disconnection
- New host's room ID learned automatically

**Pass Criteria:** ✅ Host migration handled seamlessly

---

### Test 6.9: Client Disconnect

**Test:** Spectator detects when client leaves.

**Steps:**
1. Spectate game with 2+ players
2. Register `OnClientDisconnected` handler
3. Have non-host client leave
4. Verify handler fires

**Expected Result:**
- `OnClientDisconnected` fires for leaving client
- Client removed from active list
- No errors or crashes

**Pass Criteria:** ✅ Disconnect detected and handled

---

### Test 6.10: Transmission Blocking (Zero Footprint)

**Test:** Verify spectator never transmits packets.

**Method 1: Wireshark Capture**
1. Start spectator mode, note MAC address
2. Capture WiFi traffic on channel 10 for 5 minutes
3. Filter by spectator MAC address
4. Verify 0 transmitted packets

**Method 2: Application Test**
1. Run Test 5.7 (transmission blocking)
2. Verify all blocked operations log errors
3. Verify no ACKs sent

**Expected Result:**
- Zero packets transmitted
- All send operations blocked
- Error messages logged for blocked operations

**Pass Criteria:** ✅ Absolutely zero packets transmitted

---

### Test 6.11: Battery Consumption

**Test:** Measure battery drain rate in spectator mode.

**Steps:**
1. Fully charge NDS
2. Run spectator mode for 1 hour
3. Measure battery percentage drop
4. Compare to active mode baseline

**Expected Result:**
- Higher drain than active mode (due to promiscuous mode)
- Acceptable for tournament/coaching use (2-3 hours total life)

**Pass Criteria:** ✅ 2+ hours battery life

---

### Test 6.12: Mutual Exclusivity

**Test:** Verify spectator and active modes are mutually exclusive.

**Steps:**
1. Start active mode with `NiFi_Init()`
2. Attempt `NiFi_StartSpectating()`
3. Verify operation fails with error
4. Stop active mode, start spectator mode
5. Attempt `NiFi_Init()`
6. Verify operation fails with error

**Expected Result:**
- Cannot start spectator mode while in active mode
- Cannot start active mode while in spectator mode
- Error messages logged

**Pass Criteria:** ✅ Mutual exclusivity enforced

---

### Test 6.13: Multiple Room Switches

**Test:** Switch between different rooms during observation.

**Steps:**
1. Start spectator mode
2. Spectate room A for 10 seconds
3. Call `NiFi_GetDiscoveredRooms()` again
4. Switch to room B with `NiFi_SpectateRoom(rooms[1])`
5. Verify packets now filtered to room B

**Expected Result:**
- Room switch succeeds
- Packets from room A stopped
- Packets from room B received
- Client list updated for new room

**Pass Criteria:** ✅ Room switch works correctly

---

### Test 6.14: Packet Loss Handling

**Test:** Spectator handles packet loss gracefully.

**Steps:**
1. Spectate game at edge of WiFi range (weak signal)
2. Observe for 5 minutes
3. Verify no crashes or hangs
4. Position updates may be choppy but functional

**Expected Result:**
- No crashes from lost packets
- State gradually updates as packets received
- No retry mechanisms (expected behavior)

**Pass Criteria:** ✅ No crashes or errors from packet loss

---

### Test 6.15: Edge Case - Empty Room

**Test:** Spectate room with no active players (host only).

**Steps:**
1. Start host alone (no clients)
2. Start spectator and select room
3. Verify spectator sees host
4. Have client join later
5. Verify spectator sees new client

**Expected Result:**
- Host discovered correctly
- New client discovered when joins
- No errors or crashes

**Pass Criteria:** ✅ Empty room handled correctly

---

### Performance Profiling

**Optional: CPU Usage Measurement**

Add performance counters to measure overhead:

```c
// In nifi_arm9.c, add counters
static u32 spectatorPacketsProcessed = 0;
static u32 spectatorClientsDiscovered = 0;

// Increment in UpdateSpectatorClientList()
spectatorPacketsProcessed++;

// Print stats periodically
void NiFi_PrintSpectatorStats() {
    if (IsSpectatorMode && debugMessageHandler) {
        char msg[128];
        sprintf(msg, "Spectator Stats: %lu packets, %lu clients",
                spectatorPacketsProcessed, spectatorClientsDiscovered);
        debugMessageHandler(NIFI_INFO, msg);
    }
}
```

Call `NiFi_PrintSpectatorStats()` from main loop every 10 seconds.

---

### Phase 6 Completion Checklist

After completing all tests, verify:

- [ ] All 15 tests passed
- [ ] No crashes, hangs, or errors
- [ ] Zero network footprint confirmed
- [ ] Battery life acceptable (2+ hours)
- [ ] All event handlers work correctly
- [ ] Host migration handled seamlessly
- [ ] Packet loss doesn't cause issues
- [ ] Room switching works
- [ ] Mutual exclusivity enforced

**Final Validation:**
- [ ] Library compiles without warnings
- [ ] Example application runs successfully
- [ ] Documentation complete and accurate
- [ ] Code reviewed for edge cases

**Implementation Complete!** 🎉

---

## Troubleshooting Guide

### Problem: Spectator doesn't discover any rooms

**Possible Causes:**
1. No active hosts on the WiFi channel
2. Wrong WiFi channel (host and spectator on different channels)
3. Wrong game ID (doesn't match host)
4. Promiscuous mode not enabled

**Solutions:**
- Verify host is broadcasting room announcements
- Check WiFi channel matches (use same channel for all devices)
- Verify game ID is exactly 4 characters and matches
- Confirm `Wifi_SetPromiscuousMode(1)` called in `NiFi_StartSpectating()`

---

### Problem: Spectator discovers rooms but doesn't learn room ID

**Possible Causes:**
1. No packets received from target host after selection
2. Room ID learning logic not triggered
3. Host MAC address mismatch

**Solutions:**
- Verify `UpdateSpectatorClientList()` is called in packet processing
- Add debug output to room ID learning code
- Confirm target host MAC matches packets received

---

### Problem: Spectator discovers some clients but not all

**Possible Causes:**
1. Clients not sending packets (idle)
2. `UpdateSpectatorClientList()` not called for all packets
3. Client array full (more than 6 clients)

**Solutions:**
- Wait longer for idle clients to send position updates
- Verify `UpdateSpectatorClientList()` called in main packet loop
- Check client count (NiFi supports max 6 clients)

---

### Problem: OnPositionUpdated handler not firing

**Possible Causes:**
1. Handler not registered
2. Position packets filtered out
3. `ProcessPositionPacket()` not called for spectators

**Solutions:**
- Verify `NiFi_OnPositionUpdated()` called before starting spectator mode
- Check packet filtering allows `CMD_POSITION` packets
- Ensure command dispatcher calls `ProcessPositionPacket()` in spectator mode

---

### Problem: Spectator transmits packets (non-zero footprint)

**Possible Causes:**
1. ACK transmission not blocked
2. Send functions not blocked
3. Promiscuous mode capturing own transmitted packets

**Solutions:**
- Verify all send functions have `if (IsSpectatorMode) return;` guard
- Check `SendAcknowledgement()` blocked in spectator mode
- Use Wireshark to confirm zero transmitted packets

---

### Problem: Spectator crashes when host migrates

**Possible Causes:**
1. Host pointer invalid after migration
2. Room ID changes not handled
3. Client array corruption

**Solutions:**
- Verify `UpdateSpectatorHost()` updates host pointer correctly
- Check room ID learning handles migration packets
- Add null pointer checks before dereferencing `host`

---

### Problem: High battery drain

**Expected Behavior:** Promiscuous mode increases battery consumption by 20-40%.

**Optimizations:**
- Reduce timer frequency (60Hz → 30Hz) for spectators
- Filter packets earlier (reject non-NiFi packets faster)
- Sleep during VBlank periods

---

### Problem: Compilation errors

**Common Issues:**
1. Missing header includes
2. Undefined variables
3. Function signature mismatches

**Solutions:**
- Ensure all files include necessary headers
- Verify `IsSpectatorMode` declared as `extern` in header
- Check function signatures match declarations in `dsnifi9.h`

---

## Appendix: Quick Reference

### Key Files Modified

| File | Purpose | Major Changes |
|------|---------|---------------|
| `dsnifi9.h` | Public API | Added 5 new functions |
| `nifi_arm9.h` | Internal structures | Added `SpectatorState` struct |
| `nifi_arm9.c` | Implementation | Added ~500 lines of code |

### Key Functions Added

| Function | Purpose |
|----------|---------|
| `NiFi_StartSpectating()` | Initialize spectator mode |
| `NiFi_StopSpectating()` | Shutdown spectator mode |
| `NiFi_IsSpectating()` | Check if in spectator mode |
| `NiFi_SpectateRoom()` | Target specific room |
| `NiFi_GetDiscoveredRooms()` | Get room list |
| `UpdateSpectatorClientList()` | Discover clients from packets |
| `UpdateSpectatorHost()` | Identify host |
| `ProcessSpectatorClientAnnouncement()` | Learn player names |

### Key Modifications

| Function | Modification |
|----------|--------------|
| `IsPacketIntendedForMe()` | Bypass client ID check for spectators |
| `ProcessSearchingPacket()` | Store discovered rooms |
| `ProcessIncomingPackets()` | Call spectator update functions |
| `SendAcknowledgement()` | Block in spectator mode |
| `NiFi_SendPacket()` | Block in spectator mode |
| `NiFi_QueueBroadcast()` | Block in spectator mode |

### Estimated Line Count

- **New code:** ~500 lines
- **Modified code:** ~50 lines
- **Total changes:** ~550 lines

### Build Commands

```bash
# Build dswifi library
cd /mnt/c/nds/repo/dswifi
make clean
make

# Build test application
cd /mnt/c/nds/repo/nifitest
make clean
make

# Deploy to NDS (with flashcart or emulator)
cp nifitest.nds /path/to/flashcart/
```

---

**END OF IMPLEMENTATION GUIDE**

This guide provides complete step-by-step instructions for implementing spectator mode. Follow phases sequentially and use testing checkpoints to verify correctness at each stage.
