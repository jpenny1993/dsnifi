# Room Status Implementation Review - Critical Issues & Approach

**Document Version:** 1.0
**Created:** 2025-01-22
**Status:** Pre-Implementation Review
**Reviewers:** Technical Analysis

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Critical Issues Identified](#critical-issues-identified)
3. [Spectator Mode Compatibility](#spectator-mode-compatibility)
4. [Code Compatibility Analysis](#code-compatibility-analysis)
5. [Implementation Risks](#implementation-risks)
6. [Recommended Approach](#recommended-approach)
7. [Testing Considerations](#testing-considerations)
8. [Decision Points](#decision-points)

---

## Executive Summary

### Review Scope

Compared the **room-status-design.md** design specification against the actual **dswifi/arm9/source/nifi_arm9.c** implementation to identify potential issues, conflicts, and risks.

### Overall Assessment

🟡 **MEDIUM RISK** - The plan is sound, with critical spectator mode compatibility required:

- 🔴 **CRITICAL:** MAC filtering MUST check `!IsSpectatorMode` or spectator mode will be completely broken
- ✅ **Good:** Core architecture is compatible with existing code
- ✅ **Good:** MAC persistence is intentional design (returning player feature)
- ⚠️ **Correction Needed:** OnRawPacketReceived() filtering won't work as planned - move to IsPacketIntendedForMe()
- ⚠️ **Correction Needed:** GAME_START auto-detection won't work - use explicit function calls
- ⚠️ **Enhancement Needed:** SetupNiFiClient() should prefer reusing old slots for returning players

### Key Findings

1. **🔴 CRITICAL - Spectator Mode Compatibility:** MAC filtering MUST include `!IsSpectatorMode` check or it will completely break spectator mode functionality
2. **Packet Filtering Location:** OnRawPacketReceived() filtering won't work (packets not decoded yet) - must use IsPacketIntendedForMe()
3. **Game Start Handling:** Cannot auto-detect arbitrary command names - must use explicit NiFi_SetRoomStatus() calls
4. **SetupNiFiClient Enhancement:** Should prefer to reuse returning player's old slot by MAC, with fallback to any empty slot
5. **Timer Frequency:** Currently 240Hz, changing to 60Hz is correct but needs testing
6. **Command Definitions:** Commands are in internal header `nifi_arm9.h`, not public header (by design)

---

## Critical Issues Identified

### Issue 1: OnRawPacketReceived() Filtering Location ⚠️ HIGH PRIORITY

**Current Code (nifi_arm9.c:333-357):**
```c
void OnRawPacketReceived(int packetID, int readlength) {
    // Read packet
    Wifi_RxRawReadPacket(packetID, readlength, ...);

    // Fragment reassembly
    strcat(EncodedPacketBuffer, IncomingData);

    // Process complete packets
    while ((startPosition = ...) > 0 && (endPosition = ...) > 0) {
        ProcessEncodedPacketBuffer(startPosition, endPosition);
        // Packet is STILL ENCODED here: "{TEST;42;POSITION;0;12345;...}"
    }
}
```

**Plan Suggests (Phase 2.11, line 680):**
```c
void OnRawPacketReceived(int packetID, int readlength) {
    // ... existing packet reception and decoding ...

    // NEW: Smart packet filtering for performance
    char* command = decodePacketBuffer[REQUEST_COMMAND_INDEX];  // ❌ Doesn't exist yet!
    char* macAddress = decodePacketBuffer[REQUEST_MAC_INDEX];   // ❌ Not decoded yet!

    // Filter logic...
}
```

**CRITICAL PROBLEM:** Packets are **not decoded yet** in OnRawPacketReceived(). They're still encoded strings at this stage.

**Correct Location:** Add filtering to `IsPacketIntendedForMe()` instead (line 195-214):
```c
bool IsPacketIntendedForMe(char params[READ_PARAM_COUNT][READ_PARAM_LENGTH]) {
    // Packets ARE decoded here ✅
    // params[REQUEST_COMMAND_INDEX] has command
    // params[REQUEST_MAC_INDEX] has MAC

    // ... existing validation ...

    // NEW: Status-based MAC filtering during game
    if (currentRoomStatus == NIFI_ROOM_INGAME_OPEN ||
        currentRoomStatus == NIFI_ROOM_INGAME_CLOSED) {

        char* command = params[REQUEST_COMMAND_INDEX];
        char* macAddress = params[REQUEST_MAC_INDEX];

        // Always allow room discovery
        if (strcmp(command, CMD_ROOM_SEARCH) == 0 ||
            strcmp(command, CMD_ROOM_ANNOUNCE) == 0) {
            return true;
        }

        // Filter unknown MACs during game
        if (IndexOfClientUsingMacAddress(macAddress) == INDEX_UNKNOWN) {
            return false;  // Saves CPU
        }
    }

    // ... rest of existing logic ...
}
```

**Recommendation:**
- ❌ Do NOT add filtering to `OnRawPacketReceived()`
- ✅ Add filtering to `IsPacketIntendedForMe()` instead
- ✅ Update plan Phase 2.11 accordingly

---

### Issue 2: GAME_START Auto-Detection Won't Work ⚠️ HIGH PRIORITY

**Plan Suggests (Phase 2.10, line 656):**
```c
// Automatic detection of game start
if (strcmp(p->command, "GAME_START") == 0) {
    currentRoomStatus = NIFI_ROOM_INGAME_CLOSED;
    // Auto-lock when game starts
}
```

**Problem:** "GAME_START" is **application-defined**, not a library command. Developers might use:
- "GAME_START" or "START_GAME" or "BEGIN" or "GO" or "PLAY"
- The library cannot auto-detect arbitrary command names

**Also:** Plan suggests adding `CMD_GAME_START` as a library command, which would **force developers** to use a specific command name. This violates the library's design philosophy of providing functions/handlers, not dictating command names.

**Correct Approach:** Use explicit function calls (matches library design):

**Host code:**
```c
if (keysdown & KEY_START && NiFi_IsHost()) {
    // Send whatever game packet the developer wants (optional)
    NiFiPacket packet;
    NiFi_SetPacket(&packet, "MYSTART");  // Any name, any length ≤8 chars
    NiFi_SendBroadcast(&packet, NULL);

    // Explicitly lock the room (this broadcasts ROOM_STATUS)
    NiFi_SetRoomStatus(NIFI_ROOM_INGAME_CLOSED);
}
```

**Client code:**
```c
void OnGamePacket(NiFiPacket packet) {
    if (strcmp(packet.command, "MYSTART") == 0) {
        // Handle game start
        // Note: Room status already updated via ROOM_STATUS packet
    }
}
```

**Benefits:**
- ✅ Flexible (any command name works)
- ✅ Explicit behavior (no magic)
- ✅ Consistent with library design (functions + handlers)
- ✅ Developer controls timing

**Recommendation:**
- ❌ Do NOT add auto-detection for "GAME_START"
- ❌ Do NOT add `CMD_GAME_START` library command
- ✅ Use explicit `NiFi_SetRoomStatus()` calls
- ✅ Update plan Phase 2.10 and demo app accordingly

---

### Issue 3: Command Definitions Location 🔧 LOW PRIORITY

**Current Code (dswifi/arm9/source/nifi_arm9.h:32-44):**
```c
#define CMD_ROOM_SEARCH "SCAN"
#define CMD_ROOM_ANNOUNCE "ROOM"
#define CMD_ROOM_JOIN "JOIN"
// ... etc
```

**Problem:** Command definitions are in **internal header** (`nifi_arm9.h`), not public header (`dsnifi9.h`).

**Plan References (Phase 2.6, line 598):**
```c
if (strcmp(p->command, CMD_ROOM_SEARCH) == 0 ||
    strcmp(p->command, CMD_ROOM_ANNOUNCE) == 0) {
    goto process_packet;
}
```

**Why This Works:**
- Implementation code (`nifi_arm9.c`) includes `nifi_arm9.h` (line 4)
- Commands are accessible within library source

**Why This Could Be Confusing:**
- Plan doesn't clarify that commands are internal-only
- Developers might look for `CMD_*` in public header

**Recommendation:**
- Add comment in plan clarifying command location
- Consider documenting command strings ("SCAN", "ROOM") for developers who want to debug

---

### Issue 4: Timer Frequency Change Impact ⚠️ MEDIUM PRIORITY

**Current Code (nifi_arm9.c:1056):**
```c
void NiFi_Init(...) {
    // ...
    timerStart(TimerId, ClockDivider_1024, TIMER_FREQ_1024(240), Timer_Tick);
}
```

**Planned Change:**
```c
timerStart(TimerId, ClockDivider_1024, TIMER_FREQ_1024(60), Timer_Tick);
```

**Impact Analysis:**

| Component | Current Behavior | After Change | Risk Level |
|-----------|------------------|--------------|------------|
| TTL values | 120 ticks = 0.5s | 120 ticks = 2.0s | 🟢 LOW - Actually beneficial (more time for ACKs) |
| WIFI_TTL_RATE | 20 ticks = 83ms | 20 ticks = 333ms | 🟢 LOW - Retry interval increases (better for WiFi congestion) |
| Position updates | If sent every frame @ 60fps = 240Hz OK | If sent every frame @ 60fps = 60Hz will lag | 🟡 MEDIUM - App dependent |
| Packet processing | 240 chances/sec to process | 60 chances/sec to process | 🟢 LOW - 60Hz is sufficient |

**Specific Concern: Position Update Latency**

If application code does this (common pattern):
```c
// Main loop (60fps)
while(1) {
    // Read touchscreen
    Position pos = {touchX, touchY, 0};
    NiFi_BroadcastPosition(pos);  // Queues packet

    // Render
    swiWaitForVBlank();
}
```

**At 240Hz timer:** Packet sent within ~4ms
**At 60Hz timer:** Packet sent within ~16ms

**Latency increase:** 12ms (negligible for most games)

**Recommendation:**
- ✅ Proceed with 60Hz default
- ✅ Document that fast-action games should use `NiFi_SetPacketRate(120)` or `NiFi_SetPacketRate(240)`
- ✅ Add performance note to public header

---

### Issue 5: SetupNiFiClient() Slot Reuse ⚠️ MEDIUM PRIORITY

**Current Code (nifi_arm9.c:150-180):**
```c
u8 SetupNiFiClient(u8 clientId, char macAddress[13], char playerName[10]) {
    // Find ANY empty slot (first with clientId == ID_EMPTY)
    int8 index = IndexOfClientUsingId(ID_EMPTY);

    if (index == INDEX_UNKNOWN) return INDEX_UNKNOWN;

    // Use that slot
    clients[index].clientId = clientId;
    strcpy(clients[index].macAddress, macAddress);
    strcpy(clients[index].playerName, playerName);
    return index;
}
```

**Problem for Returning Players:**
- Player was at `clients[2]`, disconnected → `clients[2].clientId = ID_EMPTY`, MAC persists
- Player rejoins during `INGAME_CLOSED`
- `SetupNiFiClient()` might find `clients[0]` is empty and use that instead
- Result: `clients[2]` has old MAC + ID_EMPTY, `clients[0]` has same MAC + new ID
- **Duplicate MAC in array** (old slot unused, new slot active)

**Design Requirement (Confirmed by Developer):**

**Priority 1:** Prefer to reuse returning player's old slot (by MAC match)
**Priority 2:** Fallback to next available empty slot if old slot occupied
**Constraint:** If room is full, deny join even for returning players

**Enhanced SetupNiFiClient:**
```c
u8 SetupNiFiClient(u8 clientId, char macAddress[13], char playerName[10]) {
    int8 index;

    // PRIORITY 1: Check if this MAC already exists (returning player)
    index = IndexOfClientUsingMacAddress(macAddress);
    if (index != INDEX_UNKNOWN && clients[index].clientId == ID_EMPTY) {
        // Found their old slot and it's empty - reuse it!
        clients[index].clientId = clientId;
        strcpy(clients[index].playerName, playerName);  // Update name if changed
        // Note: MAC already matches, no need to copy
        return index;
    }

    // PRIORITY 2: MAC not found OR old slot occupied - find any empty slot
    index = IndexOfClientUsingId(ID_EMPTY);
    if (index == INDEX_UNKNOWN) return INDEX_UNKNOWN;  // Room full

    // Use the empty slot
    clients[index].clientId = clientId;
    strcpy(clients[index].macAddress, macAddress);
    strcpy(clients[index].playerName, playerName);
    return index;
}
```

**This Handles All Cases:**

| Scenario | Behavior |
|----------|----------|
| New player, room has space | Finds empty slot, uses it ✅ |
| New player, room full | Returns INDEX_UNKNOWN ✅ |
| Returning player, old slot empty | **Reuses exact old slot** ✅ (preferred!) |
| Returning player, old slot occupied | Finds different empty slot ✅ (fallback) |
| Returning player, room full | Returns INDEX_UNKNOWN ✅ |

**Benefits:**
- ✅ Returning players keep their position in the clients array (predictable)
- ✅ Prevents duplicate MACs in the array
- ✅ Fallback ensures joining still works if old slot was taken
- ✅ Compatible with NiFi_CanPlayerJoin() validation

**Recommendation:**
- ✅ Enhance SetupNiFiClient() with MAC-aware slot reuse logic
- ✅ Works seamlessly with room status validation
- ✅ Add to implementation plan Phase 2

---

### Issue 6: MAC Persistence is Intentional Design ✅ CLARIFICATION

**Current Behavior (nifi_arm9.c:860):**
```c
void HandlePacketAsHost(NiFiPacket *p, u8 cIndex) {
    // ...
    if (strcmp(p->command, CMD_ROOM_LEAVE) == 0) {
        // ...
        clients[cIndex].clientId = ID_EMPTY;  // Only clears ID
        return;
        // Note: macAddress and playerName are NOT cleared
    }
}
```

**Design Clarification (From Developer):**

This is **intentional**, not accidental:
- MAC and playerName persist after disconnect
- Enables returning player detection via `IndexOfClientUsingMacAddress()`
- Developers control rejoin behavior through the `clients[]` array
- If developers clear the struct (e.g., `memset(&clients[i], 0, sizeof(NiFiClient))`), rejoin is prevented by design

**This is a Feature:**
- ✅ Intentional design for returning player support
- ✅ Developers have explicit control
- ✅ Compatible with room status plan

**Recommendation:**
- ✅ No changes needed
- ✅ Add documentation comment for clarity:
  ```c
  // Clear client ID but PRESERVE MAC and name for returning player detection
  clients[cIndex].clientId = ID_EMPTY;
  ```

---

### Issue 7: NiFi_CreateRoom() Status Initialization 🔧 LOW PRIORITY

**Current Code (nifi_arm9.c:534-538):**
```c
void NiFi_CreateRoom() {
    if (MyRoomId != ID_ANY) return;
    IsHost = true;
    MyRoomId = RandomByte();
    localClient->clientId = LastClientId = 1;
}
```

**Plan Suggests (Phase 2.5, line 526):**
```c
void NiFi_CreateRoom() {
    // ... existing room creation code ...

    // Set initial status
    currentRoomStatus = NIFI_ROOM_LOBBY_OPEN;
}
```

**Analysis:**

The plan wants to **explicitly** set status when creating a room.

**Current behavior:**
- `currentRoomStatus` is a static variable (will be added in Phase 2.1)
- Static variables initialize to 0 in C
- `NIFI_ROOM_LOBBY_OPEN = 0` (from plan Phase 1)
- ✅ Default initialization is correct even without explicit set

**Recommendation:**
- ✅ Add explicit set for clarity and maintainability
- ✅ Makes intent clear (not relying on implicit initialization)
- ✅ Future-proof if enum values change

---

### Issue 8: Previous Issue 7 - Moved to Issue 2 ✅

(This issue was reorganized and moved to Issue 2 for clarity)

---

## Spectator Mode Compatibility

### Overview

The **spectator mode feature** (documented in `spectator-mode-design.md` and `spectator-mode-implementation-guide.md`) is planned for implementation in the dswifi library. Spectator mode allows a Nintendo DS to passively observe games without joining or transmitting packets.

**CRITICAL:** Room status MAC filtering will **completely break spectator mode** if not implemented with spectator awareness.

---

### Critical Conflict: MAC Filtering vs Spectator Mode ⚠️ HIGHEST PRIORITY

#### The Problem

**Room Status Feature (Phase 2.1, lines 776-791):**
```c
// Status-based MAC filtering during game
if (currentRoomStatus == NIFI_ROOM_INGAME_OPEN ||
    currentRoomStatus == NIFI_ROOM_INGAME_CLOSED) {

    char* command = params[REQUEST_COMMAND_INDEX];
    char* macAddress = params[REQUEST_MAC_INDEX];

    // Always allow room discovery
    if (strcmp(command, CMD_ROOM_SEARCH) == 0 ||
        strcmp(command, CMD_ROOM_ANNOUNCE) == 0) {
        return true;
    }

    // Filter packets from unknown devices (saves ~30% CPU)
    if (IndexOfClientUsingMacAddress(macAddress) == INDEX_UNKNOWN) {
        return false;  // ❌ BLOCKS ALL SPECTATOR PACKETS
    }
}
```

**Why This Breaks Spectator Mode:**
1. Spectator joins mid-game during `NIFI_ROOM_INGAME_CLOSED` state
2. Spectator operates in promiscuous mode (receives all packets)
3. Spectator's MAC is **unknown** to the room (not in `clients[]` array)
4. MAC filter **rejects ALL packets** before spectator can discover clients
5. Spectator's `UpdateSpectatorClientList()` never executes
6. **Result: Spectator sees NOTHING**

**Impact:** 🔴 **BREAKS ALL SPECTATOR MODE FUNCTIONALITY**

---

#### The Solution: Spectator Mode Bypass

**REQUIRED MODIFICATION to Phase 2.1 (line 776):**

```c
// Status-based MAC filtering during game (with spectator mode bypass)
if (!IsSpectatorMode &&  // ← ADD THIS CHECK
    (currentRoomStatus == NIFI_ROOM_INGAME_OPEN ||
     currentRoomStatus == NIFI_ROOM_INGAME_CLOSED)) {

    char* command = params[REQUEST_COMMAND_INDEX];
    char* macAddress = params[REQUEST_MAC_INDEX];

    // Always allow room discovery (needed for rejoining)
    if (strcmp(command, CMD_ROOM_SEARCH) == 0 ||
        strcmp(command, CMD_ROOM_ANNOUNCE) == 0) {
        return true;  // Process discovery, don't filter
    }

    // Filter packets from unknown devices (saves ~30% CPU in active mode)
    if (IndexOfClientUsingMacAddress(macAddress) == INDEX_UNKNOWN) {
        return false;  // Ignore unknown MAC during game
    }
}
// Spectators bypass MAC filtering - they observe all packets from target room
```

**Key Change:** Add `!IsSpectatorMode` condition to bypass MAC filtering entirely for spectators.

---

### Why Spectators Need Bypass

**Spectator Mode Design Principles:**
1. **Passive Observation**: Spectators never transmit (zero network footprint)
2. **Promiscuous Reception**: Spectators receive ALL packets on WiFi channel
3. **Room Filtering**: Spectators filter by target room ID (via `spectatorState.targetRoomId`)
4. **Dynamic Discovery**: Spectators learn about clients by observing their packets

**How Spectators Discover Clients:**
```c
// From spectator-mode-implementation-guide.md Phase 4.1
void UpdateSpectatorClientList(NiFiPacket *packet) {
    if (!IsSpectatorMode) return;

    u8 fromClientId = packet->fromClientId;
    char *macAddress = packet->macAddress;

    // Check if client already known (by MAC address)
    int index = IndexOfClientUsingMacAddress(macAddress);

    if (index == -1) {
        // New client discovered - add to clients[] array
        // This is how spectators build their view of the game
        index = FirstEmptyClientSlot();
        if (index != -1) {
            clients[index].clientId = fromClientId;
            strcpy(clients[index].macAddress, macAddress);
            // Trigger OnClientConnected handler
            if (clientConnectedHandler) {
                clientConnectedHandler(index, clients[index]);
            }
        }
    }
}
```

**Critical Dependency:** This function is called in `ProcessIncomingPackets()` **only if packets pass** `IsPacketIntendedForMe()`. If MAC filtering blocks packets, this never executes.

---

### Packet Filter Order Integration

Both features modify `IsPacketIntendedForMe()`. Here's the **unified filter order** that supports both:

```c
bool IsPacketIntendedForMe(char params[READ_PARAM_COUNT][READ_PARAM_LENGTH]) {
    // 1. Game ID check (existing - unchanged)
    if (strcmp(params[REQUEST_GAMEID_INDEX], GameIdentifier) != 0) {
        return false;
    }

    // 2. Self-packet check (existing - unchanged)
    if (strcmp(params[REQUEST_MAC_INDEX], localClient->macAddress) == 0) {
        return false;
    }

    // 3. ACK handling (existing - unchanged)
    if (params[REQUEST_ISACK_INDEX][0] == '1') {
        MarkOutgoingPacketProcessed(messageId);
        return false;
    }

    // 4. ROOM STATUS MAC FILTERING (with spectator bypass) ✅ NEW
    if (!IsSpectatorMode &&  // ← CRITICAL: Bypass for spectators
        (currentRoomStatus == NIFI_ROOM_INGAME_OPEN ||
         currentRoomStatus == NIFI_ROOM_INGAME_CLOSED)) {

        char* command = params[REQUEST_COMMAND_INDEX];
        char* macAddress = params[REQUEST_MAC_INDEX];

        // Always allow room discovery commands
        if (strcmp(command, CMD_ROOM_SEARCH) == 0 ||
            strcmp(command, CMD_ROOM_ANNOUNCE) == 0) {
            return true;  // Skip MAC filtering for discovery
        }

        // Filter unknown MACs during game (active mode only)
        if (IndexOfClientUsingMacAddress(macAddress) == INDEX_UNKNOWN) {
            return false;
        }
    }

    // 5. Room ID check (with spectator logic) ✅ SPECTATOR AWARE
    u8 pktRoomId;
    sscanf(params[REQUEST_ROOMID_INDEX], "%hhd", &pktRoomId);

    if (IsSpectatorMode) {
        // Spectator: Filter by target room (or accept all during scanning)
        if (spectatorState.targetRoomId != ID_ANY &&
            pktRoomId != spectatorState.targetRoomId) {
            return false;
        }
    } else {
        // Active mode: Filter by assigned room
        if (pktRoomId != MyRoomId) {
            return false;
        }
    }

    // 6. Client ID check (with spectator bypass) ✅ SPECTATOR AWARE
    if (!IsSpectatorMode) {
        u8 pktToClientId;
        sscanf(params[REQUEST_TO_INDEX], "%hhd", &pktToClientId);
        if (pktToClientId != localClient->clientId) {
            return false;
        }
    }
    // Spectators accept packets addressed to ANY client

    // 7. MAC validation (with spectator bypass) ✅ SPECTATOR AWARE
    if (!IsSpectatorMode) {
        if (IndexOfClientUsingMacAddress(params[REQUEST_MAC_INDEX]) == INDEX_UNKNOWN) {
            return false;
        }
    }
    // Spectators accept packets from unknown MACs (they discover dynamically)

    return true;
}
```

**Key Points:**
- MAC filtering **MUST** check `!IsSpectatorMode` before rejecting packets
- Room ID filtering already supports spectator mode (checks `spectatorState.targetRoomId`)
- Client ID filtering already bypasses for spectators
- MAC validation already bypasses for spectators

---

### Implementation Requirements

#### Required Variables

Both features need access to these flags:

```c
// nifi_arm9.c - Global variables (existing from spectator design)
bool IsSpectatorMode = false;  // Set by NiFi_StartSpectating()
SpectatorState spectatorState = {0};  // Target room tracking

// nifi_arm9.c - Global variable (new from room status)
static NiFiRoomStatus currentRoomStatus = NIFI_ROOM_LOBBY_OPEN;
```

**Cross-Feature Initialization:**
- When `NiFi_StartSpectating()` is called, set `IsSpectatorMode = true`
- When `NiFi_StopSpectating()` is called, set `IsSpectatorMode = false`
- Room status logic checks `IsSpectatorMode` before filtering
- Spectator logic ignores `currentRoomStatus` (observes all states)

---

#### Room Status Parsing in Spectator Mode

**Issue:** Spectator room discovery doesn't parse the new `status` field.

**Current Spectator Code (spectator-mode-implementation-guide.md Phase 3.2):**
```c
if (strcmp(packet.command, CMD_ROOM) == 0) {
    NiFiRoom room;
    strcpy(room.macAddress, packet.macAddress);
    sscanf(packet.data[0], "%s", room.roomName);
    sscanf(packet.data[1], "%hhd", &room.memberCount);
    room.roomSize = CLIENT_MAX;
    // ❌ Missing: room.status = ???

    if (roomAnnouncedHandler) {
        roomAnnouncedHandler(room);
    }
}
```

**Required Fix:**
```c
if (strcmp(packet.command, CMD_ROOM) == 0) {
    NiFiRoom room;
    strcpy(room.macAddress, packet.macAddress);
    sscanf(packet.data[0], "%s", room.roomName);
    sscanf(packet.data[1], "%hhd", &room.memberCount);
    room.roomSize = CLIENT_MAX;

    // ✅ Parse status field (with backward compatibility)
    if (strlen(packet.data[4]) > 0) {
        int status;
        sscanf(packet.data[4], "%d", &status);
        room.status = (NiFiRoomStatus)status;
    } else {
        room.status = NIFI_ROOM_LOBBY_OPEN;  // Default for legacy packets
    }

    if (roomAnnouncedHandler) {
        roomAnnouncedHandler(room);
    }
}
```

**Action:** Update `spectator-mode-implementation-guide.md` Phase 3.2 with this parsing logic.

---

### Testing Integration

#### Spectator + Room Status Test Scenarios

Add these tests to both documents:

**Test 7: Spectator + LOBBY_OPEN** (10 min)
1. Device A creates room (status = LOBBY_OPEN)
2. Device B starts spectator mode
3. Device B discovers room via passive listening
4. Device B selects room for observation
5. Verify spectator sees room status correctly

**Pass Criteria:** ✅ Spectator discovers and observes LOBBY_OPEN room

---

**Test 8: Spectator + INGAME_CLOSED** (15 min)
1. Devices A (host), B, C in game (status = INGAME_CLOSED)
2. Device D starts spectator mode
3. Device D discovers room and spectates
4. Verify spectator discovers all clients (A, B, C)
5. Verify spectator receives position updates
6. Verify spectator receives game packets

**Pass Criteria:** ✅ Spectator observes INGAME_CLOSED room without being blocked by MAC filtering

---

**Test 9: Room Status Transition While Spectating** (15 min)
1. Device A creates room (LOBBY_OPEN)
2. Device B starts spectator mode and observes room
3. Device C joins as client
4. Device A sets status to INGAME_CLOSED
5. Verify spectator continues receiving packets
6. Device C leaves and rejoins
7. Verify spectator sees disconnect and rejoin

**Pass Criteria:** ✅ Spectator unaffected by room status changes

---

**Test 10: MAC Filtering Verification** (15 min)
1. Devices A, B in game (INGAME_CLOSED)
2. Device C starts spectator mode (unknown MAC)
3. Device D attempts join (should be blocked - not returning player)
4. Verify:
   - Device C (spectator) receives all packets ✅
   - Device D (new player) join is declined ✅
   - Device A & B don't see spectator's MAC in network traffic ✅

**Pass Criteria:** ✅ MAC filtering blocks new players but NOT spectators

---

### Room Discovery Limitation (Informational)

**Spectator Design Constraint:** Spectators are 100% passive and never send `CMD_ROOM_SEARCH` packets.

**Implication:** Spectators can only discover rooms by "overhearing" `CMD_ROOM_ANNOUNCE` packets sent to other devices.

**Edge Case:** If spectator is the only device looking for a game, they won't discover any rooms (no one is triggering announcements).

**Workaround for Tournament Use:**
- Provide spectators with host MAC address in advance
- Use `NiFi_SpectateRoom()` with known MAC (bypasses discovery)
- Ensure at least one scanning device is present
- Document this limitation in spectator mode documentation

**Not a Bug:** This is intentional design to maintain zero network footprint.

---

### Summary of Required Changes

#### Room Status Implementation (This Document)

1. **Phase 2.1 (line 776):** ✅ Add `!IsSpectatorMode` check to MAC filtering
   ```c
   if (!IsSpectatorMode && (currentRoomStatus == NIFI_ROOM_INGAME_OPEN || ...)) {
   ```

2. **Testing Section:** ✅ Add Tests 7-10 (Spectator integration tests)

3. **Add Note:** ✅ Document that spectator mode will be implemented separately

#### Spectator Mode Implementation (Separate Documents)

1. **Phase 3.2:** ✅ Add room status field parsing to room announcement handler

2. **Phase 6:** ✅ Add integration tests with room status (Tests 7-10)

3. **Design Doc:** ✅ Document room discovery limitation for tournament use

4. **Note:** ✅ Ensure MAC filtering bypass is mentioned in implementation

---

### Implementation Order Recommendation

**Option A: Implement Spectator Mode First (RECOMMENDED)**
1. Implement spectator mode per `spectator-mode-implementation-guide.md`
2. Add placeholder: `static NiFiRoomStatus currentRoomStatus = NIFI_ROOM_LOBBY_OPEN;`
3. Add comments: `// Future: Room status MAC filtering will check !IsSpectatorMode`
4. Test spectator mode thoroughly
5. Implement room status with `!IsSpectatorMode` bypass from day 1

**Advantages:**
- ✅ Simpler feature (no state management complexity)
- ✅ Easier to test in isolation
- ✅ Room status implementation can learn from spectator patterns
- ✅ No risk of breaking spectator mode with later changes

**Option B: Implement Room Status First**
1. Implement room status **WITH** `!IsSpectatorMode` check in place
2. Add placeholder: `bool IsSpectatorMode = false;`
3. Add comments: `// Future: Spectator mode will set this to true`
4. Test room status (IsSpectatorMode always false, so filter works normally)
5. Implement spectator mode, leveraging existing bypass

**Advantages:**
- ✅ More complex feature gets priority
- ✅ Spectator mode implementation is simpler (bypass already exists)

**Recommendation:** **Option A** - Spectator mode first, then room status with awareness built in.

---

### Final Checklist for Developer

Before implementing room status, verify:

- [ ] Read `spectator-mode-design.md` for architecture overview
- [ ] Read `spectator-mode-implementation-guide.md` for implementation details
- [ ] Understand that spectators observe passively in promiscuous mode
- [ ] MAC filtering in `IsPacketIntendedForMe()` MUST check `!IsSpectatorMode`
- [ ] Room announcement parsing must include `status` field
- [ ] Integration tests (Tests 7-10) must pass
- [ ] Document room status feature as "spectator-aware"

---

**🔴 CRITICAL:** Do NOT implement MAC filtering without the `!IsSpectatorMode` bypass. This will completely break spectator mode functionality.

---

## Code Compatibility Analysis

### Functions Referenced in Plan vs Current Implementation

| Function | Status | Location | Notes |
|----------|--------|----------|-------|
| `IndexOfClientUsingMacAddress()` | ✅ EXISTS | nifi_arm9.c:112 | Returns `INDEX_UNKNOWN` if not found |
| `CountActiveClients()` | ✅ EXISTS | nifi_arm9.c:123 | Counts non-empty client slots |
| `NewClientId()` | ✅ EXISTS | nifi_arm9.c:135 | Generates unique IDs 2-126 |
| `SetupNiFiClient()` | ✅ EXISTS | nifi_arm9.c:150 | Returns index or `INDEX_UNKNOWN` |
| `IsHost` | ✅ EXISTS | nifi_arm9.c:30 | Global bool variable |
| `clients[]` | ✅ EXISTS | nifi_arm9.c:35 | Global array, exported in public header |
| `NiFi_SetPacket()` | ✅ EXISTS | Public API | Already implemented |
| `NiFi_SendBroadcast()` | ✅ EXISTS | Public API | Already implemented |
| `NiFi_QueuePacket()` | ✅ EXISTS | Public API | Already implemented |
| `NiFi_CanPlayerJoin()` | ❌ NEW | To be added | **Must implement** |
| `NiFi_SetRoomStatus()` | ❌ NEW | To be added | **Must implement** |
| `NiFi_GetRoomStatus()` | ❌ NEW | To be added | **Must implement** |
| `NiFi_SetPacketRate()` | ❌ NEW | To be added | **Must implement** |

**Summary:** Core helper functions exist ✅, but 4 new API functions must be implemented.

---

### Data Structures

| Structure | Current Definition | Plan's Assumption | Compatible? |
|-----------|-------------------|-------------------|-------------|
| `NiFiClient` | Has `clientId`, `macAddress`, `playerName`, `lastMessageId` | Assumes MAC persists after disconnect | ⚠️ YES (by accident) |
| `NiFiRoom` | Has `macAddress`, `roomName`, `roomSize`, `memberCount` | Wants to add `status` field | ✅ YES (simple addition) |
| `NiFiPacket` | Complete, used for all packets | Expects to send `ROOM_STATUS` packets | ✅ YES (just another command) |

---

### Global Variables

| Variable | Current State | Plan's Change | Risk |
|----------|---------------|---------------|------|
| `IsHost` | bool, line 30 | No change, just read | ✅ SAFE |
| `TimerId` | int, line 28, currently unused after init | Will use for `NiFi_SetPacketRate()` | ✅ SAFE |
| `clients[]` | Exported, line 35 | No change to array, just logic | ✅ SAFE |
| `currentRoomStatus` | ❌ Doesn't exist | **Will add** as static variable | ✅ SAFE (internal only) |

---

## Implementation Risks

### 🔴 HIGH RISK

1. **Spectator Mode Compatibility** (NEW - See dedicated section above)
   - MAC filtering will completely break spectator mode if not implemented with bypass
   - Risk: Spectators unable to discover clients or observe games
   - Mitigation: MUST add `!IsSpectatorMode` check to MAC filtering (line 776)
   - **Required:** Read spectator mode documentation before implementing room status

2. **OnRawPacketReceived() Filtering Location** (Issue #1)
   - Plan's approach won't work at that location (packets not decoded yet)
   - Must move filtering to `IsPacketIntendedForMe()` instead
   - Risk: Breaking packet processing if done wrong

3. **GAME_START Auto-Detection** (Issue #2)
   - Plan assumes library can auto-detect game start commands
   - Risk: Won't work with arbitrary command names
   - Mitigation: Use explicit `NiFi_SetRoomStatus()` calls (matches library design)

### 🟡 MEDIUM RISK

4. **Timer Frequency Impact** (Issue #4)
   - Changing 240Hz → 60Hz affects all timing
   - Risk: Existing games might feel sluggish
   - Mitigation: Test extensively, document performance tuning, allow override

5. **SetupNiFiClient() Slot Reuse** (Issue #5)
   - Current implementation doesn't prefer returning player's old slot
   - Risk: Duplicate MACs in array, suboptimal slot assignment
   - Mitigation: Enhance SetupNiFiClient() to prefer MAC-matched slots with fallback

### 🟢 LOW RISK

6. **Command Location** (Issue #3)
   - Commands are internal, but plan uses them correctly
   - Risk: None (just documentation clarity)

7. **MAC Persistence** (Issue #6)
   - Intentional design, not a bug
   - Risk: None (already working as intended)

8. **CreateRoom Initialization** (Issue #7)
   - Static variable defaults to 0 (correct value)
   - Risk: None (but explicit set is better for clarity)

---

## Recommended Approach

### Phase 0: Pre-Implementation Preparation

**Before writing any code:**

1. ✅ **Create Test Branch**
   ```bash
   cd /mnt/c/nds/repo/dswifi
   git checkout -b feature/room-status
   ```

2. ✅ **Document Current Timer Behavior**
   - Measure actual packet processing time on hardware
   - Baseline: 240Hz performance metrics

3. ✅ **Review Plan with Corrections**
   - Update Phase 2.11 (OnRawPacketReceived filtering)
   - Update Phase 2.10 (GAME_START auto-lock)
   - Add explicit MAC persistence note

---

### Phase 1: Minimal Viable Implementation (1-2 hours)

**Goal:** Get basic room status working without breaking existing functionality.

**Step 1.1: Add Data Structures** (15 min)
```c
// dsnifi9.h - After Position struct
typedef enum {
    NIFI_ROOM_LOBBY_OPEN = 0,
    NIFI_ROOM_LOBBY_CLOSED = 1,
    NIFI_ROOM_INGAME_OPEN = 2,
    NIFI_ROOM_INGAME_CLOSED = 3
} NiFiRoomStatus;

// Modify NiFiRoom struct
typedef struct {
    char macAddress[MAC_ADDRESS_LENGTH];
    char roomName[PROFILE_NAME_LENGTH];
    u8 roomSize;
    u8 memberCount;
    NiFiRoomStatus status;  // NEW
} NiFiRoom;
```

**Step 1.2: Add Function Declarations** (5 min)
```c
// dsnifi9.h - Before #endif
extern void NiFi_SetRoomStatus(NiFiRoomStatus status);
extern NiFiRoomStatus NiFi_GetRoomStatus();
extern bool NiFi_CanPlayerJoin(char macAddress[MAC_ADDRESS_LENGTH]);
```

**Step 1.3: Add Static Variable** (2 min)
```c
// nifi_arm9.c - Near line 30, with other globals
static NiFiRoomStatus currentRoomStatus = NIFI_ROOM_LOBBY_OPEN;
```

**Step 1.4: Implement Core Functions** (30 min)
```c
// nifi_arm9.c - Near end of file

void NiFi_SetRoomStatus(NiFiRoomStatus status) {
    currentRoomStatus = status;

    if (IsHost) {
        NiFiPacket packet;
        NiFi_SetPacket(&packet, "ROOM_STATUS");
        sprintf(packet.data[0], "%d", status);
        NiFi_SendBroadcast(&packet, NULL);
    }
}

NiFiRoomStatus NiFi_GetRoomStatus() {
    return currentRoomStatus;
}

bool NiFi_CanPlayerJoin(char macAddress[MAC_ADDRESS_LENGTH]) {
    u8 activeCount = CountActiveClients();
    int8 existingIndex = IndexOfClientUsingMacAddress(macAddress);

    switch (currentRoomStatus) {
        case NIFI_ROOM_LOBBY_OPEN:
            return (activeCount < CLIENT_MAX);

        case NIFI_ROOM_LOBBY_CLOSED:
            return false;

        case NIFI_ROOM_INGAME_OPEN:
            return (activeCount < CLIENT_MAX);

        case NIFI_ROOM_INGAME_CLOSED:
            if (existingIndex == INDEX_UNKNOWN) {
                return false;
            }
            return (clients[existingIndex].clientId == ID_EMPTY);
    }

    return false;
}
```

**Step 1.5: Update NiFi_CreateRoom** (5 min)
```c
// nifi_arm9.c - In NiFi_CreateRoom()
void NiFi_CreateRoom() {
    if (MyRoomId != ID_ANY) return;
    IsHost = true;
    MyRoomId = RandomByte();
    localClient->clientId = LastClientId = 1;

    // NEW: Set initial status
    currentRoomStatus = NIFI_ROOM_LOBBY_OPEN;
}
```

**Step 1.5.5: Enhance SetupNiFiClient for Slot Reuse** (15 min)
```c
// nifi_arm9.c - Enhance SetupNiFiClient() to prefer returning player's old slot
u8 SetupNiFiClient(u8 clientId, char macAddress[13], char playerName[10]) {
    int8 index;

    // PRIORITY 1: Check if this MAC already exists (returning player)
    index = IndexOfClientUsingMacAddress(macAddress);
    if (index != INDEX_UNKNOWN && clients[index].clientId == ID_EMPTY) {
        // Found their old slot and it's empty - reuse it!
        clients[index].clientId = clientId;
        strcpy(clients[index].playerName, playerName);  // Update name if changed
        // Note: MAC already matches, no need to copy
        return index;
    }

    // PRIORITY 2: MAC not found OR old slot occupied - find any empty slot
    index = IndexOfClientUsingId(ID_EMPTY);
    if (index == INDEX_UNKNOWN) return INDEX_UNKNOWN;  // Room full

    // Use the empty slot
    clients[index].clientId = clientId;
    strcpy(clients[index].macAddress, macAddress);
    strcpy(clients[index].playerName, playerName);
    return index;
}
```

**Step 1.6: Update Join Handler** (20 min)
```c
// nifi_arm9.c - HandlePacketAsHost(), replace CMD_ROOM_JOIN handler
if (strcmp(p->command, CMD_ROOM_JOIN) == 0) {
    // NEW: Validate join based on room status
    if (!NiFi_CanPlayerJoin(p->macAddress)) {
        PrintDebug(DBG_Information, "Join declined - status restriction");
        NiFi_SetPacket(&r, CMD_ROOM_DECLINE_JOIN);
        strcpy(r.data[0], p->macAddress);
        strcpy(r.data[1], localClient->playerName);
        sprintf(r.data[2], "%hhd", CountActiveClients());
        sprintf(r.data[3], "%hhd", CLIENT_MAX);
        NiFi_SendPacket(&r);
        return;
    }

    // Accept join - setup client (keep existing code)
    u8 newClientId = NewClientId();
    u8 newClientIndex = SetupNiFiClient(newClientId, p->macAddress, p->data[1]);
    if (newClientIndex == INDEX_UNKNOWN) {
        PrintDebug(DBG_Error, "Failed to setup client");
        return;
    }

    // ... rest of existing join logic ...
}
```

**Step 1.7: Add MAC Persistence Documentation** (5 min)
```c
// nifi_arm9.c - In CMD_ROOM_LEAVE handler
if (strcmp(p->command, CMD_ROOM_LEAVE) == 0) {
    // ...
    // Clear client ID but PRESERVE MAC and name for returning player detection
    // This enables IndexOfClientUsingMacAddress() to find them later
    // Developers can clear the struct if they want to prevent rejoining
    clients[cIndex].clientId = ID_EMPTY;
    return;
}
```

**Step 1.8: Handle ROOM_STATUS Packets (Client)** (10 min)
```c
// nifi_arm9.c - In HandlePacketAsClient(), add near end
void HandlePacketAsClient(NiFiPacket *p, u8 cIndex) {
    // ... existing handlers ...

    // NEW: Handle room status updates from host
    if (strcmp(p->command, "ROOM_STATUS") == 0) {
        int newStatus;
        sscanf(p->data[0], "%d", &newStatus);
        currentRoomStatus = (NiFiRoomStatus)newStatus;
        return;
    }
}
```

**Step 1.9: Update Room Announcement** (10 min)
```c
// nifi_arm9.c - In HandlePacketAsHost(), CMD_ROOM_SEARCH handler
if (strcmp(p->command, CMD_ROOM_SEARCH) == 0) {
    PrintDebug(DBG_Information, "Announcing presence to searcher");
    NiFi_SetPacket(&r, CMD_ROOM_ANNOUNCE);
    strcpy(r.data[0], p->macAddress);
    strcpy(r.data[1], localClient->playerName);
    sprintf(r.data[2], "%hhd", CountActiveClients());
    sprintf(r.data[3], "%d", CLIENT_MAX);
    sprintf(r.data[4], "%d", currentRoomStatus);  // NEW
    NiFi_SendPacket(&r);
    return;
}
```

**Step 1.10: Parse Status in Announcement Handler** (10 min)
```c
// nifi_arm9.c - In HandlePacketAsSearching(), CMD_ROOM_ANNOUNCE handler
if (strcmp(p->command, CMD_ROOM_ANNOUNCE) == 0) {
    NiFiRoom room;
    strcpy(room.macAddress, p->data[0]);
    strcpy(room.roomName, p->data[1]);
    sscanf(p->data[2], "%hhd", &room.memberCount);
    sscanf(p->data[3], "%hhd", &room.roomSize);

    // NEW: Parse status (with fallback for old packets)
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

**MILESTONE:** At this point, basic room status works! Test before continuing.

---

### Phase 2: Packet Filtering (30 min)

**Goal:** Add MAC-based filtering for performance.

**Step 2.1: Add Filtering to IsPacketIntendedForMe()** (30 min)
```c
// nifi_arm9.c - In IsPacketIntendedForMe(), add AFTER existing validation
bool IsPacketIntendedForMe(char params[READ_PARAM_COUNT][READ_PARAM_LENGTH]) {
    // ... existing game ID, room ID, client ID validation ...

    // NEW: Status-based MAC filtering during game
    if (currentRoomStatus == NIFI_ROOM_INGAME_OPEN ||
        currentRoomStatus == NIFI_ROOM_INGAME_CLOSED) {

        char* command = params[REQUEST_COMMAND_INDEX];
        char* macAddress = params[REQUEST_MAC_INDEX];

        // CRITICAL: Always allow room discovery (needed for rejoining)
        if (strcmp(command, CMD_ROOM_SEARCH) == 0 ||
            strcmp(command, CMD_ROOM_ANNOUNCE) == 0) {
            return true;  // Process discovery, don't filter
        }

        // Filter packets from unknown devices (saves ~30% CPU)
        if (IndexOfClientUsingMacAddress(macAddress) == INDEX_UNKNOWN) {
            return false;  // Ignore unknown MAC during game
        }
    }

    // ... rest of existing validation ...
    return true;  // Or whatever existing return logic
}
```

**Note:** This replaces the plan's Phase 2.11 OnRawPacketReceived() filtering.

**MILESTONE:** Test that unknown MACs are filtered during INGAME states, but discovery still works.

---

### Phase 3: Timer Frequency (30 min)

**Goal:** Change default to 60Hz, add rate control function.

**Step 3.1: Store Timer ID** (5 min)
```c
// nifi_arm9.c - Already exists at line 28!
// No change needed, TimerId is already a global variable
```

**Step 3.2: Change Default Frequency** (5 min)
```c
// nifi_arm9.c - In NiFi_Init()
void NiFi_Init(int wifiChannel, int timerId, char gameIdentifier[GAME_ID_LENGTH]) {
    // ... existing init code ...

    // CHANGE: Start timer at 60Hz instead of 240Hz
    timerStart(TimerId, ClockDivider_1024, TIMER_FREQ_1024(60), Timer_Tick);
    //                                                      ^^
    //                                                      Was 240
}
```

**Step 3.3: Implement SetPacketRate** (15 min)
```c
// dsnifi9.h - Add declaration
extern void NiFi_SetPacketRate(u16 packetsPerSecond);

// nifi_arm9.c - Implement
void NiFi_SetPacketRate(u16 packetsPerSecond) {
    // Validate rate
    if (packetsPerSecond != 30 && packetsPerSecond != 60 &&
        packetsPerSecond != 120 && packetsPerSecond != 240) {
        packetsPerSecond = 60;  // Default fallback
    }

    // Restart timer with new frequency
    timerStop(TimerId);
    timerStart(TimerId, ClockDivider_1024,
               TIMER_FREQ_1024(packetsPerSecond), Timer_Tick);
}
```

**Step 3.4: Test Performance** (5 min)
- Run on hardware with default 60Hz
- Test position updates (should still be smooth)
- Optionally test 120Hz and 240Hz

**MILESTONE:** Timer frequency control works.

---

### Phase 4: Demo Application Updates (30 min)

**Goal:** Update nifitest to demonstrate room status features.

**Step 4.1: Update OnRoomAnnounced** (15 min)
```c
// nifitest/source/main.c - Update OnRoomAnnounced()
void OnRoomAnnounced(NiFiRoom room) {
    // Determine status text and color
    char* statusText;
    char* color;

    switch (room.status) {
        case NIFI_ROOM_LOBBY_OPEN:
            statusText = "Open";
            color = GREEN;
            break;
        case NIFI_ROOM_LOBBY_CLOSED:
            statusText = "Closed";
            color = YELLOW;
            break;
        case NIFI_ROOM_INGAME_OPEN:
            statusText = "In Game (Drop-In)";
            color = CYAN;
            break;
        case NIFI_ROOM_INGAME_CLOSED:
            statusText = "In Game (Locked)";
            color = RED;
            break;
    }

    printf("%sRoom: %s [%s%s%s] (%d/%d)\n",
           WHITE, room.roomName, color, statusText, WHITE,
           room.memberCount, room.roomSize);

    // Auto-join if open or we can rejoin
    NiFi_JoinRoom(room.macAddress);
}
```

**Step 4.2: Add Host Controls** (15 min)
```c
// nifitest/source/main.c - In main loop
if (keysdown & KEY_SELECT && NiFi_IsHost()) {
    NiFiRoomStatus current = NiFi_GetRoomStatus();

    // Toggle status
    if (current == NIFI_ROOM_LOBBY_OPEN) {
        NiFi_SetRoomStatus(NIFI_ROOM_LOBBY_CLOSED);
        printf("%sLobby LOCKED\n", YELLOW);
    }
    else if (current == NIFI_ROOM_LOBBY_CLOSED) {
        NiFi_SetRoomStatus(NIFI_ROOM_LOBBY_OPEN);
        printf("%sLobby OPEN\n", GREEN);
    }
    else if (current == NIFI_ROOM_INGAME_CLOSED) {
        NiFi_SetRoomStatus(NIFI_ROOM_INGAME_OPEN);
        printf("%sDrop-In ENABLED\n", CYAN);
    }
    else if (current == NIFI_ROOM_INGAME_OPEN) {
        NiFi_SetRoomStatus(NIFI_ROOM_INGAME_CLOSED);
        printf("%sDrop-In DISABLED\n", YELLOW);
    }
}

if (keysdown & KEY_START && NiFi_IsHost()) {
    // Start game
    NiFiPacket packet;
    NiFi_SetPacket(&packet, "GAME_START");
    NiFi_SendBroadcast(&packet, NULL);

    // Lock room
    NiFi_SetRoomStatus(NIFI_ROOM_INGAME_CLOSED);
    printf("%sGAME STARTED - Lobby Locked\n", GREEN);
}
```

**MILESTONE:** Demo app shows room status and allows host control.

---

## Testing Considerations

### Unit Test Scenarios (Hardware Required)

**Setup:** 2-3 Nintendo DS consoles with flashcarts

#### Test 1: Basic Status Transitions (15 min)
1. Device A creates room → Status should be LOBBY_OPEN
2. Device B scans → Should see "Open" status
3. Device B joins → Should succeed
4. Device A presses SELECT → Status becomes LOBBY_CLOSED
5. Device C scans → Should see "Closed" status
6. Device C attempts join → Should be declined
7. Device A presses SELECT → Status becomes LOBBY_OPEN
8. Device C attempts join → Should succeed

**Pass Criteria:** All state transitions work correctly

---

#### Test 2: MAC-Based Rejoining (20 min)
1. Devices A (host), B, C in lobby
2. Device A presses START → Status becomes INGAME_CLOSED
3. Device B leaves room (presses RIGHT)
4. Device B scans → Should still see room
5. Device B attempts rejoin → Should succeed (returning player)
6. Device D (new) scans → Should see room as "In Game (Locked)"
7. Device D attempts join → Should be declined (not returning player)

**Pass Criteria:** Returning players can rejoin, new players cannot

---

#### Test 3: Slot Conflict (15 min)
1. Devices A (host), B in lobby
2. Device A starts game → Status = INGAME_CLOSED
3. Device B leaves
4. Device C joins (takes B's slot)
5. Device B attempts rejoin → Should be declined (slot occupied)

**Pass Criteria:** First-come-first-served works correctly

---

#### Test 4: Packet Filtering Performance (10 min)
1. Devices A, B in game (INGAME_CLOSED)
2. Device C nearby, spamming SCAN packets
3. Devices A & B send position updates to each other
4. Observe console output (if debug enabled)

**Pass Criteria:**
- A & B should process each other's position packets
- A & B should ignore C's SCAN packets (filtered)
- Console should NOT show "received unknown packet" spam

---

#### Test 5: Room Discovery During Game (10 min)
1. Devices A (host), B in game (INGAME_CLOSED)
2. Device B leaves, returns to main menu
3. Device B scans for rooms
4. Device A should respond with ROOM_ANNOUNCE (NOT filtered)
5. Device B should see room in scan results

**Pass Criteria:** Room discovery works even during INGAME states

---

#### Test 6: Timer Frequency (15 min)
1. Build with 60Hz (default)
2. Test position updates - should feel smooth
3. Rebuild with NiFi_SetPacketRate(240) in init
4. Test position updates - should feel identical or slightly faster
5. Rebuild with NiFi_SetPacketRate(30) in init
6. Test position updates - may feel slightly delayed

**Pass Criteria:** All frequencies work, no crashes

---

### Edge Cases to Test

| Scenario | Expected Behavior | Risk |
|----------|------------------|------|
| Host leaves during INGAME_CLOSED | Host migration, new host inherits status | 🟡 MEDIUM |
| All clients disconnect, host alone | Host can reset to LOBBY_OPEN | 🟢 LOW |
| JOIN packet received when status = INGAME_CLOSED but clientId != EMPTY | Decline (slot occupied) | 🟡 MEDIUM |
| ROOM_STATUS packet received with invalid value (e.g., 5) | Clamp to valid range or ignore | 🟡 MEDIUM |
| Client receives ROOM_STATUS before JOIN is confirmed | Should buffer or ignore safely | 🟢 LOW |

---

## Decision Points

### Decision Point 1: GAME_START Handling

**Question:** Should the library auto-detect GAME_START or require explicit NiFi_SetRoomStatus()?

**Options:**
- **A:** Auto-detect "GAME_START" command (plan's original approach)
- **B:** Require explicit NiFi_SetRoomStatus() call (recommended)

**Recommendation:** **Option B**

**Rationale:**
- ✅ Flexible (works with any command name)
- ✅ Explicit behavior (no magic)
- ✅ Developer maintains control

**Action:** Update plan Phase 2.10 and Phase 3.2 accordingly.

---

### Decision Point 2: OnRawPacketReceived Filtering

**Question:** Where should MAC filtering be implemented?

**Options:**
- **A:** OnRawPacketReceived() before decoding (plan's original)
- **B:** IsPacketIntendedForMe() after decoding (recommended)

**Recommendation:** **Option B**

**Rationale:**
- ✅ Packet is already decoded at this point
- ✅ Consistent with existing filtering pattern
- ✅ Avoids duplicate decode logic

**Action:** Update plan Phase 2.11 with corrected approach.

---

### Decision Point 3: MAC Persistence Strategy

**Question:** How should MAC addresses persist after disconnect?

**Options:**
- **A:** Rely on current behavior (don't clear struct)
- **B:** Explicitly document "don't clear MAC"
- **C:** Add flag `isReturnable` or similar

**Recommendation:** **Option B**

**Rationale:**
- ✅ Simple (no new code)
- ✅ Documents intent clearly
- ✅ Works with existing code

**Action:** Add comment in CMD_ROOM_LEAVE handler.

---

### Decision Point 4: Backward Compatibility

**Question:** Should old clients (without room status) work with new hosts?

**Options:**
- **A:** Break compatibility (require update)
- **B:** Maintain compatibility (default to LOBBY_OPEN if missing)

**Recommendation:** **Option B** (already in plan)

**Rationale:**
- ✅ Graceful degradation
- ✅ Prevents hard failures
- ✅ Minimal code impact

**Implementation:**
```c
// In CMD_ROOM_ANNOUNCE parser (client side)
if (strlen(p->data[4]) > 0) {
    sscanf(p->data[4], "%d", &status);
} else {
    status = NIFI_ROOM_LOBBY_OPEN;  // Default
}
```

**Action:** Already in plan, ensure implemented.

---

## Summary

### Critical Changes to Plan

1. 🔴 **CRITICAL** Add spectator mode bypass to MAC filtering (NEW)
   ✅ **Add** `!IsSpectatorMode` check to Phase 2.1 (line 776)
   ✅ **Add** spectator integration tests (Tests 7-10)
   ✅ **Read** spectator mode documentation before implementing

2. ❌ **Remove** OnRawPacketReceived() filtering (Phase 2.11)
   ✅ **Add** IsPacketIntendedForMe() filtering instead

3. ❌ **Remove** GAME_START auto-detection (Phase 2.10)
   ✅ **Add** explicit NiFi_SetRoomStatus() calls in demo app (Phase 3.2)

4. ✅ **Enhance** SetupNiFiClient() to prefer returning player's old slot by MAC (Phase 2.1.5)
   - Check for existing MAC first
   - Reuse that slot if empty
   - Fallback to any empty slot if occupied

5. ✅ **Clarify** MAC persistence is intentional design (Phase 2)

6. ✅ **Clarify** command location (Phase 2)

### Implementation Timeline

| Phase | Time Estimate | Risk Level | Can Skip? |
|-------|---------------|------------|-----------|
| 0. Pre-Implementation | 30 min | 🟢 LOW | ❌ No |
| 1. Minimal Viable | 1-2 hours | 🟡 MEDIUM | ❌ No |
| 2. Packet Filtering | 30 min | 🟡 MEDIUM | ✅ Yes (optional optimization) |
| 3. Timer Frequency | 30 min | 🟡 MEDIUM | ✅ Yes (can do later) |
| 4. Demo App | 30 min | 🟢 LOW | ✅ Yes (nice to have) |
| **TOTAL** | **3-4 hours** | | |

### Go/No-Go Recommendation

✅ **GO** - Proceed with implementation

**Conditions:**
1. ✅ Update plan with corrections from this review
2. 🔴 **CRITICAL:** Add `!IsSpectatorMode` bypass to MAC filtering
3. ✅ Read spectator mode documentation before implementing
4. ✅ Implement Phase 1 first, test thoroughly
5. ✅ Add Phases 2-4 incrementally with testing between
6. ✅ Document any deviations from plan

**Confidence Level:** 🟢 **HIGH** (80%)

The core architecture is sound. The spectator mode compatibility is a critical requirement but straightforward to implement (single `!IsSpectatorMode` check). Most other issues are minor implementation details that can be addressed during development.

---

**END OF REVIEW**

*Next Steps:*
1. Review this document with stakeholders
2. **Read spectator mode documentation** (`spectator-mode-design.md` and `spectator-mode-implementation-guide.md`)
3. Update implementation plan with corrections (especially MAC filtering bypass)
4. Create feature branch
5. Begin Phase 0 preparation
