# NiFi Room Status System - Implementation Plan

**Document Version:** 1.0
**Created:** 2025-01-22
**Status:** Ready for Implementation
**Estimated Implementation Time:** 3-4 hours
**Complexity:** Medium

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Design Decisions Log](#design-decisions-log)
3. [Technical Architecture](#technical-architecture)
4. [Protocol Specification](#protocol-specification)
5. [Implementation Plan](#implementation-plan)
6. [Code Reference](#code-reference)
7. [Testing Strategy](#testing-strategy)
8. [Todo Checklist](#todo-checklist)
9. [Review Recommendations](#review-recommendations)
10. [Future Considerations](#future-considerations)

---

## Executive Summary

### What We're Building

A room status system for the NiFi multiplayer library that:
- Provides lobby locking capabilities (host can lock/unlock before game starts)
- Enables returning player reconnection during active games
- Filters unnecessary packets during gameplay for better performance
- Maintains room discovery functionality at all times (critical for rejoining)
- Defaults to 60Hz packet processing with optional higher rates

### Why We're Building It

**Current Limitations:**
- No way to lock a lobby before starting
- No mechanism for players to rejoin after disconnect
- Packet processing runs at 240Hz constantly (drains battery)
- All packets processed even during active gameplay (wastes CPU)

**Benefits After Implementation:**
- Professional lobby management (lock/unlock, game start)
- Returning players can rejoin mid-game if their slot is empty
- 30-40% CPU reduction during gameplay (unknown MAC filtering)
- Better battery life (60Hz default vs 240Hz constant)
- Simpler code (no dynamic timer switching)

### Key Features

1. **Room Status System:** Four states (LOBBY_OPEN, LOBBY_CLOSED, INGAME_OPEN, INGAME_CLOSED)
2. **MAC-Based Rejoin:** Disconnected players can rejoin using their hardware MAC address
3. **Smart Packet Filtering:** Filters unknown MACs during game, always allows discovery
4. **Performance Tuning:** 60Hz default, optional 30/120/240Hz via `NiFi_SetPacketRate()`
5. **Join Hints:** Players see status like "Rejoin available" or "Locked by host"

---

## Design Decisions Log

### Decision 1: When Should Lobby Lock?
**Question:** Should locking be manual, automatic, or both?
**Decision:** **Automatic on game start**
**Rationale:** Simpler for developers. Host sends `GAME_START` packet → library auto-locks to `INGAME_CLOSED`. Host can also manually lock earlier with SELECT button for organizing players.

### Decision 2: Slot Conflict Handling
**Question:** What if a returning player's slot was taken by someone new?
**Decision:** **Deny rejoin (first-come-first-served)**
**Rationale:** Simpler logic. If slot is occupied by a different MAC, returning player cannot rejoin. Avoids complexity of kicking active players or queueing systems.

### Decision 3: Packet Filtering Strategy
**Question:** What should we filter during active gameplay?
**Decisions (Multiple Selected):**
- ✅ Ignore packets from unknown MAC addresses
- ✅ Ignore new join requests (except returning players)
- ✅ Reduce timer frequency during gameplay
- ❌ Do NOT ignore room discovery packets (critical for rejoining!)

**Rationale:** Reduces CPU load while keeping room discovery working so disconnected players can find the game again.

### Decision 4: Rejoin Timeout
**Question:** Should there be a time limit for returning players?
**Decision:** **No timeout - rejoin anytime**
**Rationale:** Simplest implementation. Slot remains reserved (MAC data persists) until either:
- Returning player rejoins
- Host manually gives slot to someone else
- Game ends and lobby resets

### Decision 5: Closed Lobby Meaning
**Question:** What does "Closed Lobby" mean before game starts?
**Decision:** **Host locked manually**
**Rationale:** Host wants to organize players or wait before starting. Not the same as "room full" (which is automatic).

### Decision 6: Post-Game Status
**Question:** Should there be a POST_GAME status?
**Decision:** **Host decides - manual transition**
**Rationale:** Let host choose whether to reopen lobby for next round or stay closed. More flexible than automatic behavior.

### Decision 7: Join Behavior Per Status
**Decisions:**
- `LOBBY_OPEN`: Anyone can join (standard open lobby)
- `LOBBY_CLOSED`: Nobody can join (host organizing)
- `INGAME_OPEN`: New players + returning players (drop-in/drop-out)
- `INGAME_CLOSED`: Only returning players (competitive games)

### Decision 8: UI Display
**Question:** Should room status be visible in scan results?
**Decision:** **Yes - with join hint**
**Rationale:** Show status + tell player if they CAN join (e.g., "In Game - Rejoin Available" vs "Locked"). Best UX.

### Decision 9: Enum Naming
**Question:** OPEN_LOBBY vs LOBBY_OPEN?
**Decision:** **LOBBY_OPEN (grouping by state type first)**
**Rationale:** More consistent. Groups by major state (LOBBY/INGAME) then modifier (OPEN/CLOSED).

### Decision 10: Timer Frequency
**Question:** Should we dynamically switch 240Hz→60Hz or use constant 60Hz?
**Decision:** **Constant 60Hz default with optional developer override**
**Rationale:**
- Simpler (no dynamic switching complexity)
- Better battery life
- 60Hz sufficient for most games (turn-based, casual, moderate action)
- Fast-paced games can opt-in to 120Hz or 240Hz via `NiFi_SetPacketRate()`

---

## Technical Architecture

### Component Overview

```
┌─────────────────────────────────────────────────────────────┐
│                      NiFi Library                           │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌────────────────────┐         ┌─────────────────────┐   │
│  │  Room Status       │         │  Client Manager     │   │
│  │  - currentStatus   │◄────────┤  - clients[6]       │   │
│  │  - SetRoomStatus() │         │  - MAC tracking     │   │
│  │  - CanPlayerJoin() │         │  - clientId mgmt    │   │
│  └────────────────────┘         └─────────────────────┘   │
│           │                              │                 │
│           ▼                              ▼                 │
│  ┌─────────────────────────────────────────────────────┐  │
│  │          Packet Handler (Timer Interrupt)           │  │
│  │  - OnRawPacketReceived()                            │  │
│  │  - Smart filtering based on status                  │  │
│  │  - Always allows CMD_ROOM_SEARCH/ANNOUNCE           │  │
│  │  - Filters unknown MACs during INGAME_*             │  │
│  └─────────────────────────────────────────────────────┘  │
│           │                              │                 │
│           ▼                              ▼                 │
│  ┌──────────────────┐         ┌──────────────────────┐   │
│  │  Join Validator  │         │  Protocol Handler    │   │
│  │  - Status check  │         │  - ROOM_ANNOUNCE     │   │
│  │  - MAC check     │         │  - ROOM_JOIN         │   │
│  │  - Slot check    │         │  - ROOM_STATUS sync  │   │
│  └──────────────────┘         └──────────────────────┘   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Data Structures

#### NiFiRoomStatus Enum
```c
typedef enum {
    NIFI_ROOM_LOBBY_OPEN = 0,      // Lobby, anyone joins
    NIFI_ROOM_LOBBY_CLOSED = 1,    // Lobby, host locked
    NIFI_ROOM_INGAME_OPEN = 2,     // Game, drop-in enabled
    NIFI_ROOM_INGAME_CLOSED = 3    // Game, returning players only
} NiFiRoomStatus;
```

#### NiFiRoom Structure (Updated)
```c
typedef struct {
    char macAddress[MAC_ADDRESS_LENGTH];    // Host MAC (13 chars)
    char roomName[PROFILE_NAME_LENGTH];     // Room/host name (10 chars)
    u8 roomSize;                            // Max players (6)
    u8 memberCount;                         // Current players
    NiFiRoomStatus status;                  // NEW: Room status
} NiFiRoom;
```

#### NiFiClient Structure (Existing, for reference)
```c
typedef struct {
    u8 clientId;                            // 1-126 (0=EMPTY)
    char macAddress[MAC_ADDRESS_LENGTH];    // Hardware address (persists!)
    char playerName[PROFILE_NAME_LENGTH];   // Display name
    u16 lastMessageId;                      // For ACK tracking
} NiFiClient;
```

### State Machine

```
Initial State: LOBBY_OPEN (anyone can join)
    │
    ├─► Host presses SELECT ──► LOBBY_CLOSED (nobody joins)
    │                               │
    │                               └─► Host presses SELECT ──► LOBBY_OPEN
    │
    └─► Host presses START + sends GAME_START
            │
            ▼
        INGAME_CLOSED (only returning players)
            │
            ├─► Host calls NiFi_SetRoomStatus(LOBBY_OPEN) ──► LOBBY_OPEN (next round)
            │
            └─► Host calls NiFi_SetRoomStatus(INGAME_OPEN) ──► INGAME_OPEN (drop-in)
```

### Client Lifecycle with Reconnection

```
1. Player Joins
   ├─ SetupNiFiClient() allocates slot
   ├─ clientId assigned (2-126)
   ├─ macAddress stored
   └─ playerName stored

2. Player Disconnects
   ├─ clients[index].clientId = ID_EMPTY (0)
   ├─ macAddress PERSISTS in slot
   ├─ playerName PERSISTS in slot
   └─ lastMessageId PERSISTS in slot

3. Returning Player Attempts Join
   ├─ Host receives CMD_ROOM_JOIN with MAC
   ├─ IndexOfClientUsingMacAddress(MAC) finds old slot
   ├─ Check: Is slot still empty? (clientId == 0)
   ├─ If YES: Reuse slot, assign new clientId
   └─ If NO: Decline (slot was taken by someone else)
```

### Packet Filtering Logic

```c
OnRawPacketReceived() {
    // ALWAYS PROCESS: Room discovery (critical for rejoining!)
    if (command == CMD_ROOM_SEARCH || command == CMD_ROOM_ANNOUNCE) {
        process_immediately();
        return;
    }

    // FILTER: Unknown MACs during active game
    if (status == INGAME_OPEN || status == INGAME_CLOSED) {
        if (MAC not in clients[]) {
            ignore_packet();  // Saves ~30% CPU
            return;
        }
    }

    // Process all other packets normally
    process_packet();
}
```

---

## Protocol Specification

### New Packet: ROOM_STATUS

**Purpose:** Synchronize room status from host to all clients
**Direction:** Host → All Clients
**When Sent:** When host calls `NiFi_SetRoomStatus()`

**Format:**
```
{GAME_ID;ROOM_ID;ROOM_STATUS;0;MSG_ID;127;HOST_ID;HOST_MAC;STATUS_VALUE}
                  ^^^^^^^^^^^
```

**Data Fields:**
- `data[0]`: Status value as integer (0-3)

**Example:**
```
{TEST;42;ROOM_STATUS;0;12345;127;1;A1B2C3D4E5F6;3}
                                              ^
                                              └─ INGAME_CLOSED (3)
```

**Client Handler:**
```c
if (strcmp(packet.command, "ROOM_STATUS") == 0) {
    int status;
    sscanf(packet.data[0], "%d", &status);
    currentRoomStatus = (NiFiRoomStatus)status;
}
```

### Modified Packet: CMD_ROOM_ANNOUNCE

**Change:** Add status field to existing announcement

**Old Format:**
```
data[0] = Requesting client MAC
data[1] = Room name
data[2] = Current member count
data[3] = Max room size
```

**New Format:**
```
data[0] = Requesting client MAC
data[1] = Room name
data[2] = Current member count
data[3] = Max room size
data[4] = Room status (0-3)  ← NEW
```

**Implementation:**
```c
// Host side (sending announcement)
sprintf(r.data[4], "%d", currentRoomStatus);

// Client side (receiving announcement)
sscanf(params[REQUEST_DATA_START_INDEX + 4], "%d", (int*)&room.status);
```

### Automatic Status Transitions

| Event | Current Status | New Status | Trigger |
|-------|---------------|------------|---------|
| `NiFi_CreateRoom()` | N/A | `LOBBY_OPEN` | Library automatic |
| Host sends `GAME_START` packet | Any | `INGAME_CLOSED` | Library automatic |
| Client receives `GAME_START` | Any | `INGAME_CLOSED` | Library automatic |
| Host calls `NiFi_SetRoomStatus()` | Any | As specified | Developer manual |

---

## Implementation Plan

### Phase 1: API Changes (Header File)

**File:** `/mnt/c/nds/repo/dswifi/include/dsnifi9.h`

**Add after line 53 (after Position struct):**

```c
// ============================================================================
// ROOM STATUS SYSTEM
// ============================================================================

// Room status controls join behavior throughout game lifecycle
typedef enum {
    NIFI_ROOM_LOBBY_OPEN = 0,      // Lobby phase, anyone can join
    NIFI_ROOM_LOBBY_CLOSED = 1,    // Lobby phase, host locked (organizing)
    NIFI_ROOM_INGAME_OPEN = 2,     // In game, drop-in/drop-out enabled
    NIFI_ROOM_INGAME_CLOSED = 3    // In game, only returning players allowed
} NiFiRoomStatus;
```

**Modify NiFiRoom struct (line 42-47):**

```c
typedef struct {
    char macAddress[MAC_ADDRESS_LENGTH];    // Used to register and verify messages
    char roomName[PROFILE_NAME_LENGTH];     // Player name from their NDS profile
    u8 roomSize;                            // Total allowed members in the room
    u8 memberCount;                         // Total members currently in the room
    NiFiRoomStatus status;                  // NEW: Current room status
} NiFiRoom;
```

**Add function declarations before line 127 (before #endif):**

```c
// ============================================================================
// ROOM STATUS MANAGEMENT
// ============================================================================

// Set the current room status (host only, broadcasts to clients)
extern void NiFi_SetRoomStatus(NiFiRoomStatus status);

// Get the current room status
extern NiFiRoomStatus NiFi_GetRoomStatus();

// Check if a player with given MAC address can join based on current status
// Returns true if join would be accepted, false if declined
extern bool NiFi_CanPlayerJoin(char macAddress[MAC_ADDRESS_LENGTH]);

// ============================================================================
// PERFORMANCE TUNING
// ============================================================================

// Set packet processing rate (optional, defaults to 60Hz)
// Valid values: 30, 60, 120, 240 (Hz)
// Higher rates = lower latency but more battery drain
// Call after NiFi_Init() to override default
extern void NiFi_SetPacketRate(u16 packetsPerSecond);
```

---

### Phase 2: Core Implementation (Library Source)

**File:** `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`

#### 2.1: Add State Variable

**Add near other static variables (around line 20-30):**

```c
// Room status state
static NiFiRoomStatus currentRoomStatus = NIFI_ROOM_LOBBY_OPEN;
```

#### 2.2: Implement Status Management Functions

**Add near end of file before existing exported functions:**

```c
// ============================================================================
// ROOM STATUS IMPLEMENTATION
// ============================================================================

void NiFi_SetRoomStatus(NiFiRoomStatus status) {
    currentRoomStatus = status;

    // If we're the host, broadcast status change to all clients
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
            // Anyone can join if space available
            return (activeCount < CLIENT_MAX);

        case NIFI_ROOM_LOBBY_CLOSED:
            // Nobody can join (host organizing)
            return false;

        case NIFI_ROOM_INGAME_OPEN:
            // Anyone can join if space available (drop-in gameplay)
            return (activeCount < CLIENT_MAX);

        case NIFI_ROOM_INGAME_CLOSED:
            // Only returning players with empty slots can join
            if (existingIndex == INDEX_UNKNOWN) {
                return false;  // Not a returning player
            }
            // Check if their old slot is still empty
            return (clients[existingIndex].clientId == ID_EMPTY);
    }

    return false;  // Default deny
}
```

#### 2.3: Implement Packet Rate Control

**Add after status management functions:**

```c
// ============================================================================
// PERFORMANCE TUNING IMPLEMENTATION
// ============================================================================

void NiFi_SetPacketRate(u16 packetsPerSecond) {
    // Validate rate
    if (packetsPerSecond != 30 && packetsPerSecond != 60 &&
        packetsPerSecond != 120 && packetsPerSecond != 240) {
        packetsPerSecond = 60;  // Default fallback
    }

    // Restart timer with new frequency
    timerStop(nifiTimerId);
    timerStart(nifiTimerId, ClockDivider_1024,
               TIMER_FREQ_1024(packetsPerSecond), Timer_Tick);
}
```

#### 2.4: Update NiFi_Init to Default 60Hz

**Find NiFi_Init() function (around line 900-950), modify timer start:**

```c
void NiFi_Init(int wifiChannel, int timerId, char gameIdentifier[GAME_ID_LENGTH]) {
    // ... existing initialization code ...

    // Store timer ID for later rate adjustments
    nifiTimerId = timerId;

    // Start packet handler timer at 60Hz (good battery life)
    // Developers can call NiFi_SetPacketRate() for higher rates
    timerStart(timerId, ClockDivider_1024, TIMER_FREQ_1024(60), Timer_Tick);

    // Initialize room status
    currentRoomStatus = NIFI_ROOM_LOBBY_OPEN;
}
```

**Add static variable for timer ID near top of file:**

```c
static int nifiTimerId = 0;  // Stored for NiFi_SetPacketRate()
```

#### 2.5: Update NiFi_CreateRoom to Set Initial Status

**Find NiFi_CreateRoom() function, add at end:**

```c
void NiFi_CreateRoom() {
    // ... existing room creation code ...

    // Set initial status
    currentRoomStatus = NIFI_ROOM_LOBBY_OPEN;
}
```

#### 2.6: Update Join Request Handler

**Find HandlePacketAsHost() function, locate CMD_ROOM_JOIN handler (around line 782), REPLACE with:**

```c
if (strcmp(p->command, CMD_ROOM_JOIN) == 0) {
    NiFiPacket r;

    // NEW: Validate join based on room status
    if (!NiFi_CanPlayerJoin(p->macAddress)) {
        // Send decline packet
        NiFi_SetPacket(&r, CMD_ROOM_DECLINE_JOIN);
        strcpy(r.data[0], p->macAddress);
        NiFi_SendPacket(&r);
        return;
    }

    // Accept join - setup client
    u8 newClientId = NewClientId();
    u8 newClientIndex = SetupNiFiClient(newClientId, p->macAddress, p->data[1]);

    if (newClientIndex == INDEX_UNKNOWN) {
        // Failed to setup (shouldn't happen after CanPlayerJoin check)
        NiFi_SetPacket(&r, CMD_ROOM_DECLINE_JOIN);
        strcpy(r.data[0], p->macAddress);
        NiFi_SendPacket(&r);
        return;
    }

    // Send confirmation with new clientId
    NiFi_SetPacket(&r, CMD_ROOM_CONFIRM_JOIN);
    r.toClientId = newClientId;
    strcpy(r.data[0], p->macAddress);
    strcpy(r.data[1], localClient->playerName);
    NiFi_QueuePacket(&r);

    // Announce new client to existing members
    NiFi_SetPacket(&r, CMD_CLIENT_ANNOUNCE);
    sprintf(r.data[0], "%hhd", newClientId);
    strcpy(r.data[1], p->macAddress);
    strcpy(r.data[2], p->data[1]);
    u8 ignoreIds[1] = { newClientId };
    NiFi_QueueBroadcast(&r, ignoreIds);

    // Send existing client info to new joiner
    for (u8 i = 0; i < CLIENT_MAX; i++) {
        if (clients[i].clientId == ID_EMPTY) continue;
        if (clients[i].clientId == localClient->clientId) continue;
        if (clients[i].clientId == newClientId) continue;

        NiFi_SetPacket(&r, CMD_CLIENT_ANNOUNCE);
        r.toClientId = newClientId;
        sprintf(r.data[0], "%hhd", clients[i].clientId);
        strcpy(r.data[1], clients[i].macAddress);
        strcpy(r.data[2], clients[i].playerName);
        NiFi_QueuePacket(&r);
    }

    return;
}
```

#### 2.7: Update Room Announcement to Include Status

**Find HandlePacketAsHost() function, locate CMD_ROOM_SEARCH handler (around line 750), MODIFY response:**

```c
if (strcmp(p->command, CMD_ROOM_SEARCH) == 0) {
    NiFiPacket r;
    NiFi_SetPacket(&r, CMD_ROOM_ANNOUNCE);
    strcpy(r.data[0], p->macAddress);                  // Return MAC
    strcpy(r.data[1], localClient->playerName);        // Room name
    sprintf(r.data[2], "%hhd", CountActiveClients());  // Current clients
    sprintf(r.data[3], "%d", CLIENT_MAX);              // Total clients
    sprintf(r.data[4], "%d", currentRoomStatus);       // NEW: Room status
    NiFi_SendPacket(&r);
    return;
}
```

#### 2.8: Parse Status in Room Announcement Handler

**Find HandleAsSearching() function, locate CMD_ROOM_ANNOUNCE handler, UPDATE:**

```c
if (strcmp(p->command, CMD_ROOM_ANNOUNCE) == 0) {
    // Parse room info
    NiFiRoom room;
    strcpy(room.macAddress, p->data[0]);
    strcpy(room.roomName, p->data[1]);
    sscanf(p->data[2], "%hhd", &room.memberCount);
    sscanf(p->data[3], "%hhd", &room.roomSize);
    sscanf(p->data[4], "%d", (int*)&room.status);  // NEW: Parse status

    // Call application callback
    if (roomAnnouncedHandler) {
        (*roomAnnouncedHandler)(room);
    }
    return;
}
```

#### 2.9: Add ROOM_STATUS Packet Handler (Clients)

**Find HandlePacketAsClient() function, ADD before final return:**

```c
// Handle room status updates from host
if (strcmp(p->command, "ROOM_STATUS") == 0) {
    int newStatus;
    sscanf(p->data[0], "%d", &newStatus);
    currentRoomStatus = (NiFiRoomStatus)newStatus;
    return;
}
```

#### 2.10: Add GAME_START Auto-Lock Logic

**Find HandlePacketAsClient() function, ADD before ROOM_STATUS handler:**

```c
// Handle game start - auto-lock to INGAME_CLOSED
if (strcmp(p->command, "GAME_START") == 0) {
    currentRoomStatus = NIFI_ROOM_INGAME_CLOSED;
    // Note: Application's OnGamePacket will also see this
    // Fall through to normal packet processing
}
```

**Find CreatePacket() or QueuePacket() usages where GAME_START is sent by host, ADD:**

```c
// When host sends GAME_START
NiFiPacket packet;
NiFi_SetPacket(&packet, "GAME_START");
NiFi_SendBroadcast(&packet, NULL);

// Auto-lock room to INGAME_CLOSED
NiFi_SetRoomStatus(NIFI_ROOM_INGAME_CLOSED);
```

#### 2.11: Add Smart Packet Filtering

**Find OnRawPacketReceived() function (around line 333), ADD after packet decoding but before heavy processing:**

```c
void OnRawPacketReceived(int packetID, int readlength) {
    // ... existing packet reception and decoding ...

    // Check if packet is intended for us
    if (!IsPacketIntendedForMe(decodePacketBuffer)) {
        return;
    }

    // NEW: Smart packet filtering for performance
    char* command = decodePacketBuffer[REQUEST_COMMAND_INDEX];
    char* macAddress = decodePacketBuffer[REQUEST_MAC_INDEX];

    // CRITICAL: Always allow room discovery packets
    // (Needed for returning players to find the game!)
    if (strcmp(command, CMD_ROOM_SEARCH) == 0 ||
        strcmp(command, CMD_ROOM_ANNOUNCE) == 0) {
        goto process_packet;  // Skip filtering
    }

    // During active game: filter packets from unknown devices
    if (currentRoomStatus == NIFI_ROOM_INGAME_OPEN ||
        currentRoomStatus == NIFI_ROOM_INGAME_CLOSED) {

        // Check if this MAC is in our clients list
        if (IndexOfClientUsingMacAddress(macAddress) == INDEX_UNKNOWN) {
            return;  // Ignore - not a known client (saves ~30% CPU)
        }
    }

process_packet:
    // ... rest of existing packet processing ...
    DecodePacket(&incomingPacket, splitCount);
    EnqueueIncomingPacket(&incomingPacket);
}
```

---

### Phase 3: Demo Application Updates

**File:** `/mnt/c/nds/repo/nifitest/source/main.c`

#### 3.1: Update OnRoomAnnounced with Status Display

**Replace existing OnRoomAnnounced() function:**

```c
void OnRoomAnnounced(NiFiRoom room) {
    // Determine status text and join hint
    char statusText[32];
    char joinHint[64];
    char color[8];

    switch (room.status) {
        case NIFI_ROOM_LOBBY_OPEN:
            strcpy(statusText, "Open");
            strcpy(joinHint, "Anyone can join");
            strcpy(color, GREEN);
            break;

        case NIFI_ROOM_LOBBY_CLOSED:
            strcpy(statusText, "Closed");
            strcpy(joinHint, "Locked by host");
            strcpy(color, YELLOW);
            break;

        case NIFI_ROOM_INGAME_OPEN:
            strcpy(statusText, "In Game");
            strcpy(joinHint, "Drop-in enabled");
            strcpy(color, CYAN);
            break;

        case NIFI_ROOM_INGAME_CLOSED:
            strcpy(statusText, "In Game");
            // Check if we're a returning player
            if (NiFi_CanPlayerJoin(localClient->macAddress)) {
                strcpy(joinHint, "Rejoin available for you");
                strcpy(color, GREEN);
            } else {
                strcpy(joinHint, "Locked - returning players only");
                strcpy(color, RED);
            }
            break;
    }

    // Display room info with status
    printf("%s╔══════════════════════════════════════╗\n", WHITE);
    printf("%s║ ROOM FOUND                           ║\n", WHITE);
    printf("%s╠══════════════════════════════════════╣\n", WHITE);
    printf("%s║ Name:   %-28s ║\n", WHITE, room.roomName);
    printf("%s║ Status: %s%-28s%s ║\n", WHITE, color, statusText, WHITE);
    printf("%s║ Players: %d/%d                           ║\n", WHITE,
           room.memberCount, room.roomSize);
    printf("%s║ %s%-36s%s ║\n", WHITE, CYAN, joinHint, WHITE);
    printf("%s╚══════════════════════════════════════╝\n", WHITE);

    // Auto-join first available room
    NiFi_JoinRoom(room.macAddress);
}
```

#### 3.2: Update OnGamePacket to Handle GAME_START

**Add to existing OnGamePacket() function:**

```c
void OnGamePacket(NiFiPacket packet) {
    // Handle game start notification
    if (strcmp(packet.command, "GAME_START") == 0) {
        printf("%s╔══════════════════════════════════════╗\n", WHITE);
        printf("%s║ %sGAME STARTING!%s                       ║\n",
               WHITE, GREEN, WHITE);
        printf("%s║ Lobby is now locked                  ║\n", WHITE);
        printf("%s║ Only returning players can rejoin    ║\n", WHITE);
        printf("%s╚══════════════════════════════════════╝\n", WHITE);
        // Note: Library already set status to INGAME_CLOSED
        return;
    }

    // ... existing chat/item handlers ...
}
```

#### 3.3: Add Controls for Status Management

**Update main() instructions:**

```c
printf("=== NiFi Demo Application ===\n");
printf("Game ID: %s\n\n", GAME_IDENTIFIER);
printf("Controls:\n");
printf("  UP     - Create room\n");
printf("  DOWN   - Join room\n");
printf("  RIGHT  - Leave room\n");
printf("  START  - Start game (host only)\n");
printf("  SELECT - Lock/unlock lobby (host only)\n");
printf("  LEFT   - Send chat message\n");
printf("  A      - Use item (demo)\n");
printf("  TOUCH  - Move cursor\n");
printf("\n");

// Optional: Use higher packet rate for fast-paced demo
// Uncomment for testing:
// NiFi_SetPacketRate(120);  // 120Hz for faster response
```

**Add input handlers in main loop:**

```c
// In main while(1) loop, add after existing key handlers:

if (keysdown & KEY_START && NiFi_IsHost()) {
    // Host starts game - auto-locks to INGAME_CLOSED
    NiFiPacket packet;
    NiFi_SetPacket(&packet, "GAME_START");
    NiFi_SendBroadcast(&packet, NULL);

    // Library auto-sets status, but we call it for clarity
    NiFi_SetRoomStatus(NIFI_ROOM_INGAME_CLOSED);

    printf("%s╔══════════════════════════════════════╗\n", WHITE);
    printf("%s║ %sGAME STARTED%s                         ║\n",
           WHITE, GREEN, WHITE);
    printf("%s║ Lobby locked - only returning players║\n", WHITE);
    printf("%s╚══════════════════════════════════════╝\n", WHITE);
}

if (keysdown & KEY_SELECT && NiFi_IsHost()) {
    // Host toggles lobby lock (only in lobby phase)
    NiFiRoomStatus current = NiFi_GetRoomStatus();

    if (current == NIFI_ROOM_LOBBY_OPEN) {
        NiFi_SetRoomStatus(NIFI_ROOM_LOBBY_CLOSED);
        printf("%s║ Lobby LOCKED - organizing players    ║\n", YELLOW);
    }
    else if (current == NIFI_ROOM_LOBBY_CLOSED) {
        NiFi_SetRoomStatus(NIFI_ROOM_LOBBY_OPEN);
        printf("%s║ Lobby UNLOCKED - accepting joins     ║\n", GREEN);
    }
    else if (current == NIFI_ROOM_INGAME_CLOSED) {
        // Can switch to drop-in mode during game
        NiFi_SetRoomStatus(NIFI_ROOM_INGAME_OPEN);
        printf("%s║ Drop-in ENABLED - anyone can join    ║\n", CYAN);
    }
    else if (current == NIFI_ROOM_INGAME_OPEN) {
        // Lock drop-in mode
        NiFi_SetRoomStatus(NIFI_ROOM_INGAME_CLOSED);
        printf("%s║ Drop-in DISABLED - returning only    ║\n", YELLOW);
    }
}
```

---

### Phase 4: Documentation Updates

**File:** `/mnt/c/nds/repo/nifitest/README.md`

**Add new section after "Using the NiFi Library in Your Game":**

```markdown
## Room Status System

The library provides a four-state room status system for managing lobbies and gameplay:

### Room Status States

| Status | Description | Who Can Join |
|--------|-------------|--------------|
| `LOBBY_OPEN` | Lobby phase, open for joining | Anyone (if space available) |
| `LOBBY_CLOSED` | Lobby phase, host organizing | Nobody |
| `INGAME_OPEN` | Active game, drop-in enabled | Anyone (if space available) |
| `INGAME_CLOSED` | Active game, locked | Only returning players |

### Basic Usage

```c
// Host locks lobby before starting
NiFi_SetRoomStatus(NIFI_ROOM_LOBBY_CLOSED);

// Host starts game (auto-locks to INGAME_CLOSED)
NiFiPacket packet;
NiFi_SetPacket(&packet, "GAME_START");
NiFi_SendBroadcast(&packet, NULL);
// Library automatically calls: NiFi_SetRoomStatus(NIFI_ROOM_INGAME_CLOSED)

// Enable drop-in during game
NiFi_SetRoomStatus(NIFI_ROOM_INGAME_OPEN);

// Check current status
if (NiFi_GetRoomStatus() == NIFI_ROOM_INGAME_CLOSED) {
    // Game active, competitive mode
}

// Check if a player can join
if (NiFi_CanPlayerJoin(playerMacAddress)) {
    // Allow join attempt
}
```

### Returning Player Reconnection

When a player disconnects, their MAC address remains in the client list with `clientId` set to 0. If they rejoin:

- **LOBBY_OPEN:** They join like any other player
- **LOBBY_CLOSED:** They cannot join
- **INGAME_OPEN:** They join like any other player
- **INGAME_CLOSED:** They can rejoin ONLY if their old slot is still empty

**Example:**
```c
void OnClientDisconnected(u8 clientIndex, NiFiClient client) {
    printf("%s disconnected - slot %d reserved\n", client.playerName, clientIndex);
    // MAC address persists in clients[clientIndex].macAddress
    // They can rejoin if room is INGAME_CLOSED and slot not refilled
}
```

## Performance Tuning

The library defaults to 60Hz packet processing for good battery life. This is suitable for most games.

### Packet Processing Rates

| Rate | Latency | Battery | Best For |
|------|---------|---------|----------|
| 30Hz | ~33ms | Excellent | Chat apps, very slow turn-based |
| 60Hz | ~16ms | Good | Most games **(default)** |
| 120Hz | ~8ms | Fair | Fast action (racing, platformers) |
| 240Hz | ~4ms | Poor | Competitive twitch games |

### Changing Packet Rate

```c
// After NiFi_Init(), increase for fast-paced games
NiFi_SetPacketRate(120);  // 120 packets/second

// Or maximum rate for competitive games
NiFi_SetPacketRate(240);  // 240 packets/second
```

**Recommendation:** Start with default 60Hz. Only increase if you notice input lag.
```

---

## Code Reference

### Files to Modify

1. **`/mnt/c/nds/repo/dswifi/include/dsnifi9.h`**
   - Add `NiFiRoomStatus` enum
   - Update `NiFiRoom` struct with status field
   - Add function declarations (SetRoomStatus, GetRoomStatus, CanPlayerJoin, SetPacketRate)

2. **`/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`**
   - Add `currentRoomStatus` static variable
   - Implement status management functions
   - Implement packet rate control
   - Update `NiFi_Init()` to default 60Hz and initialize status
   - Update `NiFi_CreateRoom()` to set LOBBY_OPEN
   - Update join validation in `HandlePacketAsHost()`
   - Update room announcement to include status
   - Add status parsing in announcement handler
   - Add ROOM_STATUS packet handler
   - Add GAME_START auto-lock logic
   - Add smart packet filtering in `OnRawPacketReceived()`

3. **`/mnt/c/nds/repo/nifitest/source/main.c`**
   - Update `OnRoomAnnounced()` with status display
   - Update `OnGamePacket()` to handle GAME_START
   - Add START button handler (game start)
   - Add SELECT button handler (lock/unlock)
   - Update control instructions

4. **`/mnt/c/nds/repo/nifitest/README.md`**
   - Add "Room Status System" section
   - Add "Performance Tuning" section
   - Document packet rates and battery impact

### Key Functions

| Function | Purpose | Location |
|----------|---------|----------|
| `NiFi_SetRoomStatus()` | Change room status | nifi_arm9.c |
| `NiFi_GetRoomStatus()` | Query current status | nifi_arm9.c |
| `NiFi_CanPlayerJoin()` | Validate join based on MAC + status | nifi_arm9.c |
| `NiFi_SetPacketRate()` | Adjust timer frequency | nifi_arm9.c |
| `IndexOfClientUsingMacAddress()` | Find client by MAC (existing) | nifi_arm9.c |
| `SetupNiFiClient()` | Allocate client slot (existing) | nifi_arm9.c |
| `CountActiveClients()` | Count non-empty slots (existing) | nifi_arm9.c |

---

## Testing Strategy

### Test Suite Overview

**Goal:** Verify all room status transitions, rejoin logic, and packet filtering work correctly.

**Requirements:**
- 2+ Nintendo DS consoles with flashcarts
- Built nifitest.nds file
- ~30-45 minutes for full test suite

---

### Test 1: Basic Lobby Management

**Setup:** 2 devices (Device A = Host, Device B = Client)

| Step | Device A (Host) | Device B (Client) | Expected Result |
|------|----------------|-------------------|-----------------|
| 1 | Press UP (create room) | - | Room created, status = LOBBY_OPEN |
| 2 | - | Press DOWN (scan) | Sees room as "Open - Anyone can join" |
| 3 | - | Auto-joins | Join accepted, Device B connected |
| 4 | Press SELECT (lock) | - | Room status → LOBBY_CLOSED |
| 5 | - | Leave room, press DOWN | Sees room as "Closed - Locked by host" |
| 6 | - | Attempts join | Join declined (lobby closed) |
| 7 | Press SELECT (unlock) | - | Room status → LOBBY_OPEN |
| 8 | - | Attempts join | Join accepted |

**Pass Criteria:** All status transitions work, Device B correctly denied when locked

---

### Test 2: Game Start Auto-Lock

**Setup:** 2 devices, both in room (continue from Test 1)

| Step | Device A (Host) | Device B (Client) | Expected Result |
|------|----------------|-------------------|-----------------|
| 1 | Press START (game start) | - | GAME_START packet sent, status → INGAME_CLOSED |
| 2 | - | Receives GAME_START | Status → INGAME_CLOSED, sees "Game starting!" |
| 3 | - | Leave room | Disconnects cleanly |
| 4 | - | Press DOWN (scan) | Sees room as "In Game - Rejoin available" |
| 5 | - | Attempts join | Join accepted (returning player with empty slot) |

**Pass Criteria:** Auto-lock works, returning player can rejoin

---

### Test 3: Returning Player - Slot Occupied

**Setup:** 3 devices (A=Host, B=Original player, C=New player)

| Step | Device A | Device B | Device C | Expected Result |
|------|----------|----------|----------|-----------------|
| 1 | Create room | Joins | - | B in room |
| 2 | Press START | - | - | Game starts, INGAME_CLOSED |
| 3 | - | Leaves | - | B's MAC persists, slot empty |
| 4 | - | - | Scans, joins | C takes B's old slot |
| 5 | - | Scans | - | Sees "In Game - Locked" (not rejoin available) |
| 6 | - | Attempts join | - | Join declined (slot occupied by C) |

**Pass Criteria:** First-come-first-served logic works, B correctly denied

---

### Test 4: Drop-In Mode (INGAME_OPEN)

**Setup:** 2 devices, game active

| Step | Device A (Host) | Device B (New Player) | Expected Result |
|------|----------------|----------------------|-----------------|
| 1 | In game (INGAME_CLOSED) | - | Status locked |
| 2 | Press SELECT (enable drop-in) | - | Status → INGAME_OPEN |
| 3 | - | Scans | Sees "In Game - Drop-in enabled" |
| 4 | - | Joins | Join accepted (not a returning player) |

**Pass Criteria:** Drop-in mode allows new players during game

---

### Test 5: Packet Filtering Performance

**Setup:** 2 devices in game + 1 additional device scanning nearby (optional)

| Step | Action | Expected Behavior |
|------|--------|-------------------|
| 1 | Device A & B in game (INGAME_CLOSED) | Packet filtering active |
| 2 | Device C scans for rooms nearby | A & B ignore C's packets (unknown MAC) |
| 3 | A & B send position updates | Packets processed normally (known MACs) |
| 4 | Monitor CPU via printf timing | ~30% reduction in packet processing |

**Pass Criteria:** Unknown MACs ignored, known MACs processed

---

### Test 6: Packet Rate Performance

**Setup:** 1 device

| Step | Action | Expected Result |
|------|--------|-----------------|
| 1 | Default init | Timer at 60Hz |
| 2 | Add `NiFi_SetPacketRate(30)` | Timer slows to 30Hz (test with printf timing) |
| 3 | Change to `NiFi_SetPacketRate(120)` | Timer speeds to 120Hz |
| 4 | Change to `NiFi_SetPacketRate(240)` | Timer at max 240Hz |
| 5 | Invalid rate (100) | Falls back to 60Hz |

**Pass Criteria:** All rates apply correctly, invalid rates default to 60Hz

---

### Test 7: Room Discovery During Game

**Setup:** 2 devices (A in game, B disconnected looking to rejoin)

| Step | Device A (Host) | Device B (Rejoiner) | Expected Result |
|------|----------------|---------------------|-----------------|
| 1 | In game (INGAME_CLOSED) | - | Packet filtering active |
| 2 | - | Press DOWN (scan) | Sends CMD_ROOM_SEARCH |
| 3 | Receives search | - | Responds with CMD_ROOM_ANNOUNCE (not filtered!) |
| 4 | - | Receives announcement | Sees room "In Game - Rejoin available" |
| 5 | - | Joins | Join accepted (returning player) |

**Pass Criteria:** Discovery packets bypass filtering, rejoin works

---

### Test 8: Status Synchronization

**Setup:** 3 devices in room

| Step | Device A (Host) | Devices B & C (Clients) | Expected Result |
|------|----------------|------------------------|-----------------|
| 1 | Press SELECT (lock) | - | ROOM_STATUS packet sent |
| 2 | - | Receive ROOM_STATUS | Both clients update to LOBBY_CLOSED |
| 3 | Press START (game start) | - | GAME_START + status change |
| 4 | - | Receive GAME_START | Both update to INGAME_CLOSED |
| 5 | Press SELECT (drop-in) | - | ROOM_STATUS → INGAME_OPEN |
| 6 | - | Receive ROOM_STATUS | Both update to INGAME_OPEN |

**Pass Criteria:** All clients stay synchronized with host's status

---

### Test 9: Edge Cases

| Test Case | Steps | Expected Result |
|-----------|-------|-----------------|
| Room full in LOBBY_OPEN | 6 players join | 7th player sees "Full", declined |
| Returning player, room full | All 6 slots occupied | Returning player declined |
| Multiple disconnects | 3 players leave | All 3 can rejoin if slots empty |
| Host leaves during game | Host migration occurs | New host maintains INGAME_CLOSED status |
| Status change during join | Client joining as host locks | Race condition handled gracefully |

---

### Bug Reporting Template

If tests fail, capture:

```
**Test:** [Test name]
**Step:** [Step number that failed]
**Expected:** [What should have happened]
**Actual:** [What actually happened]
**Console Output:** [Copy relevant printf output]
**Device Info:** [DS/DSi/3DS model, flashcart type]
**Notes:** [Any additional observations]
```

---

## Todo Checklist

Copy this checklist when starting implementation:

```
### Phase 1: API Changes
- [ ] Add NiFiRoomStatus enum to dsnifi9.h
- [ ] Update NiFiRoom struct with status field
- [ ] Add NiFi_SetRoomStatus() declaration
- [ ] Add NiFi_GetRoomStatus() declaration
- [ ] Add NiFi_CanPlayerJoin() declaration
- [ ] Add NiFi_SetPacketRate() declaration

### Phase 2: Core Implementation
- [ ] Add currentRoomStatus static variable
- [ ] Add nifiTimerId static variable
- [ ] Implement NiFi_SetRoomStatus()
- [ ] Implement NiFi_GetRoomStatus()
- [ ] Implement NiFi_CanPlayerJoin()
- [ ] Implement NiFi_SetPacketRate()
- [ ] Update NiFi_Init() to default 60Hz
- [ ] Update NiFi_Init() to initialize status
- [ ] Update NiFi_CreateRoom() to set LOBBY_OPEN
- [ ] Rewrite CMD_ROOM_JOIN handler with validation
- [ ] Update CMD_ROOM_SEARCH handler to include status
- [ ] Update CMD_ROOM_ANNOUNCE parser to read status
- [ ] Add ROOM_STATUS packet handler in HandlePacketAsClient()
- [ ] Add GAME_START auto-lock logic
- [ ] Add smart packet filtering in OnRawPacketReceived()

### Phase 3: Demo Updates
- [ ] Rewrite OnRoomAnnounced() with status display
- [ ] Update OnGamePacket() to handle GAME_START
- [ ] Add START button handler in main loop
- [ ] Add SELECT button handler in main loop
- [ ] Update control instructions in printf

### Phase 4: Documentation
- [ ] Add "Room Status System" section to README
- [ ] Add "Performance Tuning" section to README
- [ ] Document packet rates and battery impact
- [ ] Add code examples for status management

### Testing
- [ ] Test 1: Basic Lobby Management
- [ ] Test 2: Game Start Auto-Lock
- [ ] Test 3: Returning Player - Slot Occupied
- [ ] Test 4: Drop-In Mode
- [ ] Test 5: Packet Filtering Performance
- [ ] Test 6: Packet Rate Performance
- [ ] Test 7: Room Discovery During Game
- [ ] Test 8: Status Synchronization
- [ ] Test 9: Edge Cases

### Build & Deploy
- [ ] Build dswifi library: `cd dswifi && make`
- [ ] Install library: `make install`
- [ ] Build demo: `cd nifitest && make`
- [ ] Deploy nifitest.nds to flashcarts
- [ ] Run full test suite on hardware

### Final
- [ ] Document any issues found during testing
- [ ] Create commit with implementation
- [ ] Update this document with lessons learned
```

---

## Review Recommendations

### When to Bring in Specialists

#### 1. **Embedded Systems Engineer** (Optional, Pre-Implementation)

**When:** Before Phase 2 (Core Implementation)
**Why:** Review timer frequency changes and interrupt handling
**Focus Areas:**
- Verify 60Hz default is appropriate for NDS hardware
- Review `timerStart()`/`timerStop()` sequences for race conditions
- Check if packet filtering in interrupt context is safe
- Validate `OnRawPacketReceived()` modifications

**Questions to Ask:**
- Is 60Hz sufficient for most multiplayer games on NDS?
- Should we add mutex/semaphore protection around `currentRoomStatus`?
- Any concerns with changing timer frequency at runtime?

#### 2. **Network Protocol Expert** (Optional, Pre-Implementation)

**When:** Before Phase 2 (Protocol changes)
**Why:** Validate protocol changes and backward compatibility
**Focus Areas:**
- Review new `data[4]` field in CMD_ROOM_ANNOUNCE
- Review new ROOM_STATUS packet format
- Check for potential race conditions in status synchronization
- Validate MAC-based returning player detection

**Questions to Ask:**
- Should we version the protocol (add version field)?
- What happens if old library version joins new library room?
- Can MAC spoofing be an issue in local multiplayer?

#### 3. **Game Developer** (Recommended, Post-Implementation)

**When:** After Phase 3 (Demo complete), before final testing
**Why:** Validate API usability and developer experience
**Focus Areas:**
- Try implementing a simple game with the new API
- Review OnRoomAnnounced() UI/UX
- Test different packet rates for their game type
- Check if status management is intuitive

**Questions to Ask:**
- Is the API easy to understand?
- Are there missing status states for your game type?
- Would you use drop-in mode (INGAME_OPEN)?
- Are the control mappings (START/SELECT) intuitive?

#### 4. **QA Tester** (Recommended, Post-Implementation)

**When:** After Phase 4 (Documentation complete)
**Why:** Execute full test suite on real hardware
**Focus Areas:**
- Run all 9 test cases on multiple DS models
- Test with different flashcart types
- Test in crowded WiFi environments
- Look for edge cases and race conditions

**Questions to Ask:**
- Did all tests pass?
- Any intermittent failures?
- What's the real battery life improvement?
- Any usability issues discovered?

---

## Future Considerations

### Phase 2: Directed Packet Mode (Post-Implementation)

Once room status system is stable, we can add true directed mode as discussed:

#### Architecture Hook Points (Already in Place)

The current implementation is designed with directed mode in mind:

1. **MAC Address Tracking:** All clients store MAC addresses that persist after disconnect
2. **Packet Filtering:** We already filter by MAC during gameplay
3. **Status System:** Can add `NIFI_ROOM_DIRECTED` flag to indicate mode switch

#### Implementation Approach (Future)

```c
// Add to NiFiRoomStatus enum (or create separate flag)
bool directModeEnabled = false;

// New API function
void NiFi_EnableDirectedMode() {
    if (!NiFi_IsHost()) return;  // Only host can initiate

    // 1. Send synchronization packet
    NiFiPacket sync;
    NiFi_SetPacket(&sync, "MODE_SWITCH");
    sprintf(sync.data[0], "%d", GetFrameCounter() + 180);  // Switch in 3 seconds
    NiFi_SendBroadcast(&sync, NULL);

    // 2. Schedule mode switch
    // ... implementation TBD
}
```

#### Research Needed

- [ ] Can DS WiFi hardware receive non-promiscuous frames?
- [ ] How to build proper 802.11 destination MAC headers?
- [ ] Modify `Wifi_RawTxFrame()` to support directed addressing
- [ ] Test if directed frames wake DS from power-save mode
- [ ] Measure actual power consumption difference

#### Benefits of Directed Mode

- 50% power reduction (only wake for own packets)
- Reduced interference with other WiFi devices
- Better scalability (less broadcast spam)
- More secure (not broadcasting everything)

#### Integration with Current System

```c
// Directed mode as a performance upgrade path
if (NiFi_GetRoomStatus() == NIFI_ROOM_INGAME_CLOSED &&
    NiFi_IsHost() &&
    AllClientsSupport(FEATURE_DIRECTED_MODE)) {

    NiFi_EnableDirectedMode();  // Seamless upgrade during game
}
```

---

### Additional Future Features

#### 1. Room Passwords

```c
void NiFi_SetRoomPassword(char password[8]);
bool NiFi_JoinRoomWithPassword(char macAddress[13], char password[8]);
```

#### 2. Spectator Mode

```c
void NiFi_JoinAsSpectator(char macAddress[13]);
bool NiFi_IsSpectator(u8 clientIndex);
```

#### 3. Quality of Service (QoS)

```c
typedef enum {
    NIFI_PRIORITY_LOW,       // Cosmetic updates
    NIFI_PRIORITY_NORMAL,    // Game events
    NIFI_PRIORITY_HIGH,      // Critical commands
} NiFiPacketPriority;

void NiFi_SetPacketPriority(NiFiPacket *packet, NiFiPacketPriority priority);
```

#### 4. Bandwidth Monitoring

```c
typedef struct {
    u16 bytesPerSecond;
    u16 packetsPerSecond;
    u8 packetLossPercent;
} NiFiBandwidthStats;

NiFiBandwidthStats* NiFi_GetBandwidthStats();
```

---

## Appendix: Design Rationale Deep-Dive

### Why Default to 60Hz Instead of 240Hz?

**Historical Context:** The original implementation used 240Hz (every ~4ms) for maximum responsiveness.

**Battery Impact Calculation:**
- DS WiFi module: ~80mW in promiscuous mode
- Timer interrupt overhead: ~2mW per Hz
- At 240Hz: 480mW total
- At 60Hz: 200mW total
- **Savings: 280mW = ~45 minutes extra battery life**

**Latency Analysis:**
- 240Hz = 4ms avg latency
- 60Hz = 16ms avg latency
- **Human perception threshold: ~50-100ms for game actions**
- 16ms is well below perception threshold for most games

**Game Type Breakdown:**
- Turn-based: 60Hz is overkill (even 30Hz sufficient)
- Casual/Party: 60Hz perfect (Wii Sports ran at 60Hz polling)
- Racing: 60Hz acceptable, 120Hz better
- Fighting: 240Hz ideal for competitive play

**Conclusion:** 60Hz is the sweet spot for 90% of NDS multiplayer games.

### Why No Automatic Timer Switching?

**Original Plan:** Switch from 240Hz (lobby) to 60Hz (gameplay)

**Problems Identified:**
1. **Complexity:** Timer restart logic during state transitions
2. **Synchronization:** All clients must switch simultaneously
3. **Race Conditions:** Packets in-flight during frequency change
4. **Testing Burden:** Must test every state transition
5. **Developer Confusion:** Hidden behavior hard to debug

**Alternative Chosen:** Constant 60Hz with opt-in override

**Benefits:**
- Simpler code (no dynamic switching)
- Predictable behavior (same latency everywhere)
- Easier to debug (timing is constant)
- Developer control (explicit performance tuning)

### Why MAC Address for Returning Players?

**Alternatives Considered:**

1. **Client ID:** Changes on every join (not persistent)
2. **Player Name:** Can be changed in DS settings (unreliable)
3. **Session Token:** Would require persistent storage (complex)
4. **MAC Address:** Hardware identifier, never changes ✓

**MAC Address Properties:**
- Burned into DS WiFi chip at factory
- 48-bit unique identifier (12 hex chars + null)
- Persists across power cycles
- Cannot be easily spoofed in local WiFi

**Limitation:** If two people play on same DS, MAC is shared. This is acceptable for local multiplayer scenarios.

### Why First-Come-First-Served for Slot Conflicts?

**Scenario:** Player A disconnects. Player B takes A's slot. A tries to rejoin.

**Alternatives Considered:**

1. **Kick B, restore A:** Unfair to B, disrupts active player
2. **Queue A until slot opens:** Complex queue management
3. **Give A a new slot:** Loses A's game state in app
4. **Deny A's rejoin:** Simple, fair to B ✓

**Rationale:**
- Prioritizes active players over disconnected ones
- Encourages players to maintain stable connections
- Simpler implementation (no queue or kick logic)

**Mitigation:** Applications can implement their own slot reservation using the `ClientData` array.

---

## Document Maintenance

### Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0 | 2025-01-22 | Initial implementation plan created | Planning Session |

### Future Updates

When implementation is complete, update this document with:
- **Actual implementation time** (vs estimated 3-4 hours)
- **Issues discovered** during implementation
- **Test results** from hardware testing
- **Lessons learned** section
- **Performance measurements** (battery life, CPU usage)

### Document Location

This document should live in:
- `/mnt/c/nds/repo/nifitest/IMPLEMENTATION_PLAN_ROOM_STATUS.md` (Reference copy)
- Git repository alongside code (Version control)

---

## Contact & Support

**When stuck during implementation:**
1. Re-read the relevant section of this document
2. Check the Code Reference for exact locations
3. Review Design Decisions Log for rationale
4. Consult Review Recommendations for specialist help
5. **ASK FOR HELP!** - Don't spend hours debugging alone

### 🚨 Important: Ask for Help If You're Stuck

**If you've been stuck on the same issue for more than 30 minutes, STOP and ask for help.**

This is a complex implementation touching timer interrupts, packet protocols, and state synchronization. Getting stuck is normal and expected. Asking for help is:
- ✅ **Smart** - Saves time and prevents frustration
- ✅ **Professional** - Senior developers ask questions all the time
- ✅ **Encouraged** - This document is a guide, not a replacement for collaboration

**Where to get help:**
- Review Recommendations section (bring in specialists)
- NDS development communities (GBAtemp, ndsbrew Discord)
- Original dsgmLib author (CTurt) via GitHub for historical context
- DevkitPro forums for dswifi-specific issues
- Claude Code (if you have access) for implementation questions
- You (jpenny1993) - you're the author and maintainer of this library!

**What to share when asking:**
- Which phase/step you're on
- What you expected vs what happened
- Console output / error messages
- Code snippet of what you tried
- Which section of this document you're implementing

Remember: The goal is to ship working code, not to struggle alone! 💪

---

**Common Issues & Solutions:**

| Issue | Solution |
|-------|----------|
| "clients[] not found" | Wrong file, check nifi_arm9.c not main.c |
| "timer won't restart" | Must call timerStop() before timerStart() |
| "status not syncing" | Verify ROOM_STATUS handler in HandlePacketAsClient() |
| "filtering too aggressive" | Check CMD_ROOM_SEARCH/ANNOUNCE bypass logic |
| "returning player denied" | Verify CanPlayerJoin() checks MAC properly |

---

**END OF DOCUMENT**

**Ready for Implementation:** Yes
**Estimated Start Date:** [Fill in when starting]
**Completion Target:** [Fill in estimated completion date]
