# ADR 001: NiFi Protocol Implementation for Nintendo DS Local Multiplayer

**Status:** Accepted (Retrospective)

**Date:** 2022-01-15 (Initial Implementation) | 2025-11-22 (Retrospective Documentation)

---

## Retrospective Note

This ADR documents architectural decisions made during the original NiFi protocol implementation (2022-2025). It is written retrospectively to capture the rationale, trade-offs, and lessons learned from building a custom multiplayer protocol for Nintendo DS hardware.

The implementation evolved through 4 major iterations (documented in LESSONS_LEARNED.md), with the final architecture proving stable and performant for real-time local multiplayer games.

---

## Context

### Problem Statement

The Nintendo DS hardware (2004-2011) supports local wireless multiplayer using 802.11b WiFi, but presents unique challenges:

**Hardware Constraints:**
- **4MB RAM total** (limited memory for buffers, state)
- **67 MHz ARM9 CPU** (minimal processing power)
- **2000 mAh battery** (~3-5 hours WiFi usage)
- **WiFi promiscuous mode** (can receive all packets on channel)
- **No infrastructure mode** (ad-hoc P2P only for local multiplayer)

**Nintendo's Official Library Limitations:**
- Nintendo's official wireless library required licensed devkits ($2,000+)
- Not available to homebrew developers
- Closed-source (cannot customize or extend)
- Complex API with steep learning curve

**Existing Solutions Evaluated:**

1. **dswifi Library (Stock)**
   - Provides TCP/IP stack for infrastructure WiFi
   - **Problem:** Too heavy for ad-hoc P2P (full TCP/IP overhead)
   - **Problem:** No built-in game lobby/matchmaking primitives
   - **Problem:** Requires managing sockets, connection state manually

2. **Raw 802.11 Frames**
   - dswifi supports promiscuous mode for raw packet access
   - **Problem:** No reliability layer (packets drop frequently)
   - **Problem:** No fragmentation handling (WiFi splits large packets)
   - **Problem:** No acknowledgment or retry mechanism

3. **UDP Broadcast**
   - Could use UDP over dswifi's TCP/IP stack
   - **Problem:** Still requires infrastructure mode (access point)
   - **Problem:** Broadcast unreliable (no ACKs)
   - **Problem:** Overhead of IP headers, routing, ARP

4. **Existing Homebrew Libraries**
   - Various attempts at DS-to-DS communication
   - **Problem:** Abandoned projects, unmaintained
   - **Problem:** Poor documentation
   - **Problem:** Memory leaks, crashes under load

### Motivating Requirements

**For Homebrew Game Developers:**
1. **Simple API:** Event-driven callbacks, not socket management
2. **Reliable Delivery:** Critical packets must arrive (position, commands)
3. **Low Latency:** Real-time games need < 100ms response times
4. **Low Memory:** Must fit in 4MB alongside game logic and graphics
5. **Battery Efficient:** Support 3+ hours of gameplay
6. **No Infrastructure:** Work without WiFi access point (ad-hoc only)
7. **Lobby System:** Built-in room creation, joining, discovery

**Technical Requirements:**
- Support 2-6 players simultaneously
- Handle packet loss gracefully (wireless is unreliable)
- Detect and recover from disconnections
- Minimize CPU usage (leave headroom for game logic)
- Fixed memory usage (no dynamic allocation in interrupt context)

### Why Not Existing Protocols?

**TCP over dswifi:**
- ❌ Requires infrastructure mode (access point)
- ❌ Heavy overhead (sequence numbers, windowing, congestion control)
- ❌ Connection setup latency (3-way handshake)
- ❌ Overkill for 2-6 local players

**UDP over dswifi:**
- ❌ Still requires infrastructure mode
- ❌ No reliability (must build ACK layer anyway)
- ❌ IP routing overhead unnecessary for ad-hoc

**Custom Protocol Benefits:**
- ✅ Ad-hoc mode (direct device-to-device)
- ✅ Tailored reliability (ACK only critical packets)
- ✅ Game-aware primitives (rooms, clients, positions)
- ✅ Minimal overhead (no IP, no routing, no handshakes)

---

## Decision

We implemented **NiFi (Near Field Communication)**: a custom reliable UDP-like protocol operating directly over raw 802.11 WiFi frames in promiscuous mode.

### Core Architecture Principles

1. **Circular Buffer Design:** Fixed memory usage with automatic overflow handling
2. **TTL-Based Retry Logic:** Soft-state retransmission without complex windowing
3. **Timer-Driven Processing:** Interrupt-driven network I/O decoupled from game loop
4. **MAC-Based Identity:** Hardware addresses as stable client identifiers
5. **Event-Driven Callbacks:** Simple API with 9 hooks for game developers
6. **Host-Authoritative P2P:** Host manages room state, clients react

### Protocol Architecture

#### 1. Packet Format

```
{GAME_ID;ROOM_ID;COMMAND;ACK;MSG_ID;TO_CLIENT;FROM_CLIENT;MAC_ADDRESS;DATA1;DATA2;DATA3;DATA4;DATA5;DATA6}
```

**Field Breakdown:**

| Field | Purpose | Length | Example |
|-------|---------|--------|---------|
| GAME_ID | Application identifier | 4 chars | "RACE" |
| ROOM_ID | Room/session identifier | 1-3 digits | "42" |
| COMMAND | Packet type | 4-8 chars | "JOIN", "POSITION" |
| ACK | Acknowledgment flag | 1 bit | "0" or "1" |
| MSG_ID | Unique message ID | 1-5 digits | "12345" |
| TO_CLIENT | Destination client ID | 1-3 digits | "2" |
| FROM_CLIENT | Source client ID | 1-3 digits | "1" |
| MAC_ADDRESS | Hardware address | 17 chars | "00:1A:2B:3C:4D:5E" |
| DATA1-6 | Payload fields | 32 chars each | "Player1", "128", etc. |

**Total Size:** ~250 bytes worst-case (fits in single WiFi frame)

#### 2. Circular Buffer Architecture

**Problem:** Limited 4MB RAM, unpredictable packet arrival rates

**Solution:** Three fixed-size circular buffers

```c
// Incoming packets (received from network)
static char incomingPackets[INCOMING_PACKET_BUFFER_CAPACITY][INCOMING_PACKET_BUFFER_SIZE];
static u16 ipIndex = 0;  // Current write position
#define INCOMING_PACKET_BUFFER_CAPACITY 12

// Outgoing packets (queued for transmission)
static char outgoingPackets[OUTGOING_PACKET_BUFFER_CAPACITY][OUTGOING_PACKET_BUFFER_SIZE];
static u16 opIndex = 0;  // Current write position
static u16 spIndex = 0;  // Current read position (send)
#define OUTGOING_PACKET_BUFFER_CAPACITY 18

// Acknowledgment tracking
static u16 lastAckReceived[ACK_PACKET_BUFFER_CAPACITY];
static u16 akIndex = 0;  // Current write position
#define ACK_PACKET_BUFFER_CAPACITY 15
```

**Key Property:** Old unprocessed packets are **intentionally overwritten** when buffers fill.

**Rationale:**
- ✅ Real-time games: Stale position data is worse than no data
- ✅ Fixed memory: No heap allocation, no fragmentation
- ✅ Predictable: Buffer exhaustion never crashes, just drops oldest
- ✅ Similar to UDP: Fast and lossy, prioritizing recent events

**Trade-off:** Applications must process packets promptly or accept data loss. This is acceptable and expected for real-time multiplayer.

#### 3. TTL-Based Retry Logic

**Problem:** Packets drop frequently on wireless (30-40% loss observed)

**Solution:** Time-To-Live (TTL) countdown with periodic retries

```c
#define WIFI_TTL 120        // 120 ticks (2 seconds at 60Hz)
#define WIFI_TTL_RATE 20    // Retry every 20 ticks (~333ms at 60Hz)

typedef struct {
    char packet[OUTGOING_PACKET_BUFFER_SIZE];
    u16 ttl;            // Ticks remaining before discard
    u16 messageId;      // For ACK matching
    bool processed;     // ACK received?
} OutgoingPacket;

// In timer interrupt (every 16.67ms at 60Hz):
void Timer_Tick() {
    for (int i = 0; i < OUTGOING_PACKET_BUFFER_CAPACITY; i++) {
        if (outgoingPackets[i].ttl > 0) {
            outgoingPackets[i].ttl--;

            // Retry every WIFI_TTL_RATE ticks
            if (outgoingPackets[i].ttl % WIFI_TTL_RATE == 0) {
                if (!outgoingPackets[i].processed) {
                    // Retransmit packet
                    Wifi_RawTxFrame(outgoingPackets[i].packet, ...);
                }
            }

            // Discard after TTL expires
            if (outgoingPackets[i].ttl == 0) {
                outgoingPackets[i].processed = true;  // Give up
            }
        }
    }
}
```

**Benefits:**
- ✅ No complex windowing or congestion control
- ✅ Soft-state: Packets eventually expire and clean up
- ✅ Adaptive: Retry frequency configurable per game
- ✅ Simple: No sliding windows, cumulative ACKs, or SACKs

**Trade-off:** Higher retry count than TCP (fixed interval, not exponential backoff). Acceptable for low-latency local wireless where congestion is rare.

#### 4. Timer-Driven Network Processing

**Problem:** Game loop runs at variable frame rates (30-60 FPS), network needs consistent timing

**Solution:** Hardware timer interrupt decoupled from main loop

```c
// Hardware timer triggers every 16.67ms (60Hz default)
void Timer_Tick() {
    // 1. Process incoming WiFi packets
    ProcessWiFiInterrupts();

    // 2. Retry outgoing packets (TTL countdown)
    ProcessOutgoingPackets();

    // 3. Fire event callbacks for received packets
    ProcessIncomingPackets();

    // 4. Detect disconnects (timeout-based)
    ProcessDisconnections();
}

// Game loop runs independently at 60 FPS
while(1) {
    scanKeys();
    updateGameLogic();
    renderGraphics();

    // No network code in main loop!
    swiWaitForVBlank();
}
```

**Benefits:**
- ✅ Consistent packet timing regardless of game performance
- ✅ Game logic never blocks on network I/O
- ✅ Network processing budget is predictable
- ✅ Simple game code (just register callbacks)

**Trade-off:** Event handlers run in **interrupt context**, must be fast and simple. Documented clearly in API.

#### 5. MAC-Based Client Identity

**Problem:** Need stable client identifiers that survive disconnects/reconnects

**Solution:** Use hardware MAC addresses as primary identity

```c
typedef struct {
    u8 clientId;                            // Temporary session ID (1-126)
    char macAddress[MAC_ADDRESS_LENGTH];    // Permanent hardware address
    char playerName[PROFILE_NAME_LENGTH];   // Human-readable name
    u16 lastMessageId;                      // For duplicate detection
} NiFiClient;

// MAC addresses persist across disconnects (intentional)
void HandleDisconnect(u8 clientIndex) {
    clients[clientIndex].clientId = ID_EMPTY;  // Clear session ID
    // MAC and playerName PRESERVED for returning player detection
}
```

**Benefits:**
- ✅ Returning player detection (room status feature, ADR 003)
- ✅ Stable identity for tournaments/friends
- ✅ No need for persistent storage or login system

**Trade-off:** MAC addresses are visible in WiFi promiscuous mode (no privacy). Acceptable for local ad-hoc multiplayer.

#### 6. Event-Driven API

**Problem:** Socket APIs are complex (connect, bind, listen, accept, read, write, close)

**Solution:** 9 callback hooks for common multiplayer scenarios

```c
// Room discovery and joining
NiFi_OnRoomAnnounced(OnRoomAnnounced);
NiFi_OnJoinAccepted(OnJoinAccepted);
NiFi_OnJoinDeclined(OnJoinDeclined);

// Player lifecycle
NiFi_OnClientConnected(OnClientConnected);
NiFi_OnClientDisconnected(OnClientDisconnected);
NiFi_OnDisconnected(OnDisconnected);  // You were kicked/disconnected

// Game state
NiFi_OnHostMigration(OnHostMigration);
NiFi_OnPositionUpdated(OnPositionUpdated);  // Real-time position sync
NiFi_OnGamePacket(OnGamePacket);  // Custom game events

// Initialize and create/join room
NiFi_Init(wifiChannel, timerId, "GAME");
NiFi_CreateRoom();  // Host
// or
NiFi_ScanRooms();  // Client (triggers OnRoomAnnounced)
```

**Benefits:**
- ✅ Declarative: "Call me when X happens"
- ✅ Familiar: Similar to GUI event handlers
- ✅ Simple: No state machines, no polling loops
- ✅ Flexible: Opt-in to events you care about

**Trade-off:** Less control than raw sockets. Acceptable for 95% of multiplayer games.

#### 7. Host-Authoritative P2P Architecture

**Problem:** Full peer-to-peer mesh is complex (N² connections)

**Solution:** Star topology with host as authority

```
        Client 2
           |
           |
Client 1 - HOST - Client 3
           |
           |
        Client 4
```

**Host Responsibilities:**
- Accept/decline join requests
- Assign client IDs (2-126, host is always 1)
- Broadcast client announcements
- Manage disconnections
- Trigger host migration if leaving

**Client Responsibilities:**
- Discover rooms (scan for hosts)
- Send join requests
- Acknowledge packets from host
- Broadcast game packets to all

**Benefits:**
- ✅ Simpler than full mesh (6 connections vs 15 for 6 players)
- ✅ Authoritative: Host resolves conflicts
- ✅ Efficient: Broadcast from host reaches all clients once

**Trade-off:** Host has more network traffic and CPU load. Mitigated by small player counts (6 max).

### Command Protocol

**Core Commands (Library-Managed):**

| Command | Direction | Purpose |
|---------|-----------|---------|
| `SCAN` | Client → All | Discover available rooms |
| `ROOM` | Host → Client | Announce room details |
| `JOIN` | Client → Host | Request to join room |
| `ACCEPT` | Host → Client | Join accepted, here's your ID |
| `DECLINE` | Host → Client | Join declined (room full) |
| `CLIENT` | Host → All | Announce new/existing client |
| `LEAVE` | Client → Host | Graceful disconnect |
| `POSITION` | Any → All | Real-time position update |
| `MIGRATE` | Host → All | New host elected |

**Custom Commands (Application-Defined):**
- Developers define any 4-8 character command names
- Examples: "CHAT", "ITEM", "SHOOT", "SCORE", "START"
- Delivered via `OnGamePacket()` callback

### Performance Characteristics

**Measured on Hardware:**

| Metric | Value | Notes |
|--------|-------|-------|
| Latency (position update) | 16-50ms | Average 30ms at 60Hz |
| Packet loss tolerance | 30-40% | Retry logic handles gracefully |
| CPU overhead | 10-15% | At 60Hz, leaves 85%+ for game |
| Memory usage | ~8KB | Fixed (buffers + state) |
| Battery life | 3-5 hours | At 60Hz (vs 2-3 hours at 240Hz) |
| Max players | 6 | Theoretical limit 126 (IDs) |
| Max range | 10-30 meters | Depends on environment |

---

## Consequences

### Positive Consequences

1. **Simple API for Developers**
   - Event-driven callbacks familiar to game developers
   - No socket management or connection state
   - 50-100 lines of code for basic multiplayer
   - Extensive documentation and examples

2. **Reliable on Unreliable Wireless**
   - TTL-based retry handles 30-40% packet loss
   - Soft-state cleanup prevents resource leaks
   - Automatic reconnection detection
   - Graceful degradation under load

3. **Minimal Memory Footprint**
   - Fixed 8KB overhead (circular buffers + state)
   - No dynamic allocation in interrupt context
   - Predictable behavior (no heap fragmentation)
   - Fits alongside game logic and graphics

4. **Low Latency**
   - Direct WiFi frames (no IP routing overhead)
   - Timer-driven processing (no polling delays)
   - Typical 30ms position update latency
   - Suitable for real-time games

5. **Battery Efficient**
   - 60Hz default balances latency and power
   - Configurable packet rate (30/60/120/240 Hz)
   - 3-5 hours gameplay typical
   - MAC filtering optimization (ADR 003) saves 30% CPU

6. **Ad-Hoc Mode (No Infrastructure)**
   - Works without WiFi access point
   - Direct device-to-device communication
   - Portable (parks, cafes, tournaments)
   - Promiscuous mode enables spectator feature (ADR 002)

7. **Extensible Protocol**
   - Custom commands for any game event
   - Room status system (ADR 003)
   - Spectator mode (ADR 002)
   - Future enhancements possible

8. **Proven Stability**
   - 3 years of development and refinement
   - 4 major iterations fixing critical bugs
   - Documented lessons learned
   - Used in multiple homebrew games

### Negative Consequences

1. **Not Truly Peer-to-Peer**
   - **Impact:** Host has higher network load and CPU usage
   - **Scenario:** 6-player game, host processes all join requests
   - **Mitigation:** Small player counts (6 max) make this acceptable
   - **Alternative Rejected:** Full mesh too complex for embedded device

2. **Host Migration Complexity**
   - **Impact:** When host leaves, new host must be elected
   - **Bug History:** Infinite loop race condition (fixed in iteration 4)
   - **Current State:** Stable, but adds protocol complexity
   - **Trade-off:** Worth it for better resilience

3. **Intentional Packet Loss**
   - **Impact:** Circular buffers overwrite old packets when full
   - **Frequency:** "Overwriting packet" warnings in high-traffic scenarios
   - **Rationale:** Stale position data worse than no data (real-time games)
   - **Mitigation:** Document expected behavior, tune buffer sizes

4. **No Encryption or Authentication**
   - **Impact:** Packets visible in promiscuous mode, MAC spoofing possible
   - **Risk:** Low (local ad-hoc, friendly multiplayer only)
   - **Threat Model:** Not designed for adversarial scenarios
   - **Mitigation:** Document security limitations, social controls

5. **Limited to 6 Players**
   - **Impact:** Cannot support large lobbies (10+ players)
   - **Constraint:** Hardware limitations (RAM, CPU, WiFi bandwidth)
   - **Design Decision:** Optimized for 2-6 players (typical for DS)
   - **Alternative Rejected:** Client slots are cheap (~100 bytes), but network traffic scales poorly

6. **Interrupt Context Complexity**
   - **Impact:** Event handlers run in timer interrupt, must be fast
   - **Risk:** Slow handlers block network processing
   - **Documentation:** Clearly documented in API
   - **Best Practice:** Keep handlers simple, defer heavy work to main loop

7. **No Cross-Platform Support**
   - **Impact:** Nintendo DS only (dswifi dependency)
   - **Constraint:** Uses DS-specific WiFi hardware and memory layout
   - **Trade-off:** Optimized for DS over generality
   - **Future:** Could port to 3DS or other WiFi-enabled embedded devices

8. **Debugging Challenges**
   - **Impact:** Wireless protocol errors hard to reproduce
   - **Tools:** Limited debugging on actual hardware
   - **Lessons Learned:** Extensive documentation in LESSONS_LEARNED.md
   - **Mitigation:** Emulator testing, debug logging, packet sniffing (Wireshark)

### Critical Bugs Encountered and Fixed

#### Bug 1: WiFi Buffer Overflow (Iteration 1)
**Symptom:** Random crashes, corrupted packets
**Root Cause:** Interrupt handler too slow, WiFi hardware buffer overflowed
**Fix:** Optimized packet processing, increased buffer size
**Lesson:** Embedded WiFi is unforgiving - timing is critical

#### Bug 2: Message ID Rollover Crash (Iteration 2)
**Symptom:** Crash when message ID exceeded 65,535
**Root Cause:** 16-bit rollover not handled, used as array index
**Fix:** Modulo arithmetic, proper ID wrapping
**Lesson:** Always plan for counter rollover on long-running systems

#### Bug 3: Packet Fragmentation Corruption (Iteration 2)
**Symptom:** Random garbled packets, parsing failures
**Root Cause:** WiFi frames split packets at arbitrary boundaries
**Fix:** Fragment reassembly buffer, start/end delimiters
**Lesson:** Never assume single packet = single WiFi frame

#### Bug 4: Host Migration Infinite Loop (Iteration 3)
**Symptom:** System hang when host disconnected
**Root Cause:** Race condition during host promotion
**Fix:** Proper state machine, atomic flag checks
**Lesson:** Distributed state machines are hard - document carefully

#### Bug 5: ACK Matching Off-By-One (Iteration 4)
**Symptom:** Packets retransmitted unnecessarily
**Root Cause:** Used client ID instead of message ID for ACK matching
**Fix:** Corrected ACK logic, proper message ID comparison
**Lesson:** Simple logic errors in network code cause weird symptoms

**Documentation:** Full debugging journey in LESSONS_LEARNED.md

### Risk Analysis

| Risk | Likelihood | Severity | Mitigation | Status |
|------|-----------|----------|------------|--------|
| Circular buffer exhaustion | Medium | Low | Document expected behavior, tune sizes | ✅ Accepted |
| Host migration failures | Low | High | Extensive testing, state machine docs | ✅ Mitigated |
| Interrupt handler blocking | Medium | Medium | API docs, best practices guide | ✅ Documented |
| MAC spoofing attacks | Low | Low | Document threat model, social controls | ✅ Accepted |
| Battery drain too high | Low | Medium | Configurable packet rate, defaults tested | ✅ Mitigated |
| Hardware compatibility | Very Low | High | Test on multiple DS models | ✅ Validated |

---

## Alternatives Considered

### Alternative 1: UDP over dswifi TCP/IP Stack (Rejected)

**Description:** Use existing UDP protocol over dswifi's full TCP/IP implementation.

**Pros:**
- Standard protocol (well-understood)
- dswifi provides implementation
- Debugging tools available (tcpdump, Wireshark)

**Cons:**
- ❌ Requires infrastructure mode (WiFi access point)
- ❌ IP routing overhead (unnecessary for local P2P)
- ❌ No reliability (still need custom ACK layer)
- ❌ Heavyweight (TCP/IP stack uses significant RAM)
- ❌ Not designed for ad-hoc local multiplayer

**Rejection Reason:** Primary requirement is ad-hoc mode without infrastructure. UDP requires access point.

---

### Alternative 2: TCP over dswifi (Rejected)

**Description:** Use TCP for reliability, dswifi's TCP/IP stack.

**Pros:**
- Reliable delivery built-in
- Congestion control (fairness)
- Well-tested implementation

**Cons:**
- ❌ Requires infrastructure mode
- ❌ Connection setup latency (3-way handshake)
- ❌ Heavy overhead (sequence numbers, windowing, SACKs)
- ❌ Overkill for 2-6 local players
- ❌ Head-of-line blocking (stale data delays new data)

**Rejection Reason:** Too heavy, requires infrastructure, head-of-line blocking bad for real-time games.

---

### Alternative 3: Nintendo's Official Wireless Library (Rejected)

**Description:** Use Nintendo's official DS multiplayer library.

**Pros:**
- Official support
- Optimized for DS hardware
- Used in commercial games

**Cons:**
- ❌ Requires licensed devkit ($2,000+)
- ❌ Not available to homebrew developers
- ❌ Closed-source (cannot customize)
- ❌ Complex API (steep learning curve)

**Rejection Reason:** Not accessible to homebrew community (target audience).

---

### Alternative 4: Full Peer-to-Peer Mesh (Rejected)

**Description:** Every device connects to every other device (N² connections).

**Pros:**
- No single point of failure
- Democratic (no host authority)
- Lower per-device bandwidth (spread across all)

**Cons:**
- ❌ Complex connection management (6 players = 15 connections)
- ❌ Difficult conflict resolution (no authority)
- ❌ Higher total bandwidth (each packet sent N times)
- ❌ State synchronization nightmares
- ❌ Overkill for small lobbies (2-6 players)

**Rejection Reason:** Complexity not justified for small player counts. Host-authoritative simpler and sufficient.

---

### Alternative 5: No Reliability Layer (Pure Broadcast - Rejected)

**Description:** Broadcast packets without ACKs or retries, accept loss.

**Pros:**
- Simple implementation
- Low latency (no retries)
- Minimal CPU overhead

**Cons:**
- ❌ 30-40% packet loss unacceptable for critical events
- ❌ Join requests would fail frequently
- ❌ Disconnects undetected (no heartbeats)
- ❌ Poor player experience

**Rejection Reason:** Wireless too unreliable without retry mechanism. Unacceptable for game-critical packets (join, chat, commands).

---

### Alternative 6: Dynamic Memory Allocation (Rejected)

**Description:** Use malloc/free for packet buffers instead of fixed circular buffers.

**Pros:**
- Flexible memory usage
- No buffer overflow (just allocate more)
- Better memory efficiency (no pre-allocation)

**Cons:**
- ❌ Heap fragmentation (long-running game sessions)
- ❌ Allocation in interrupt context (forbidden)
- ❌ Unpredictable performance (malloc can be slow)
- ❌ Risk of heap exhaustion

**Rejection Reason:** Interrupt context forbids dynamic allocation. Fixed buffers are safer and more predictable.

---

## Implementation Timeline (Retrospective)

### Phase 1: Proof of Concept (January 2022 - March 2022)
- Initial raw WiFi packet transmission
- Basic room creation and joining
- Single-file 849-line main.c implementation
- **Result:** Working but unstable (broken.txt)

### Phase 2: First Refactoring (April 2022 - June 2022)
- Extracted networking into separate functions
- Added retry logic (fixed TTL)
- Circular buffer for incoming packets
- **Result:** More stable (prettygood.txt)
- **Critical Bugs:** WiFi buffer overflow, message ID rollover

### Phase 3: Protocol Formalization (July 2022 - September 2022)
- Defined packet format specification
- Implemented ACK mechanism
- Added host migration
- Fragment reassembly buffer
- **Result:** Production-ready (betterthan_prettygood.txt)
- **Critical Bug:** Host migration infinite loop

### Phase 4: Library Integration (October 2022 - December 2022)
- Migrated code into dswifi fork
- Event-driven callback API
- Client array management
- Position broadcast optimization
- **Result:** Stable library (final architecture)
- **Critical Bug:** ACK matching off-by-one (fixed)

### Phase 5: Documentation and Refinement (2023-2025)
- Comprehensive ARCHITECTURE.md (60 pages)
- LESSONS_LEARNED.md (50 pages)
- Room status system (ADR 003)
- Spectator mode (ADR 002)
- Demo application (nifitest)

**Total Development Time:** ~3 years (part-time)
**Lines of Code:** ~2,000 (library) + ~500 (demo app)
**Test Coverage:** Extensive hardware testing, 10+ test scenarios

---

## Lessons Learned

### Technical Lessons

1. **Circular Buffers are Essential on Embedded Systems**
   - Fixed memory prevents heap fragmentation
   - Overflow behavior must be intentional and documented
   - "Enumerate-don't-iterate" pattern prevents deadlocks

2. **Wireless is Unreliable - Plan for 30-40% Loss**
   - Always implement retry mechanism
   - Soft-state (TTL) simpler than hard-state (windowing)
   - Test with packet loss simulation (emulator)

3. **Interrupt Context has Strict Rules**
   - No dynamic allocation (malloc/free)
   - Keep handlers fast (< 1ms)
   - Defer heavy work to main loop
   - Use atomic operations for shared state

4. **Timing Matters on Real-Time Systems**
   - Timer frequency affects latency and battery
   - 60Hz is sweet spot for most games
   - Profile on actual hardware (emulators lie)

5. **Protocol Evolution is Inevitable**
   - Design for extensibility (custom commands)
   - Backward compatibility from day one
   - Version negotiation is hard - avoid if possible

6. **Debugging Wireless is Hard**
   - Wireshark for packet capture (essential)
   - Debug logging with timestamps
   - State machine diagrams (document expected behavior)
   - Hardware debugging limited (use emulator + real hardware)

### Architectural Lessons

1. **Event-Driven APIs are Beginner-Friendly**
   - Callbacks familiar to game developers
   - Declarative style (vs imperative socket code)
   - Less boilerplate than traditional networking

2. **Host-Authoritative P2P Scales to 6 Players**
   - Simpler than full mesh
   - Authority resolves conflicts
   - Single point of failure mitigated by host migration

3. **MAC Addresses are Good Stable IDs**
   - Hardware guarantees uniqueness
   - Survives disconnects (returning player feature)
   - No need for persistent storage

4. **Document Expected Behaviors, Not Just APIs**
   - Circular buffer overflow is intentional - document it
   - Interrupt context rules - document clearly
   - Packet loss is normal - set expectations

### Project Management Lessons

1. **Iterate on Real Hardware Early**
   - Emulators miss timing issues
   - Battery life only measurable on hardware
   - Range testing requires physical distance

2. **Document as You Go**
   - 3-year project, easy to forget rationale
   - LESSONS_LEARNED.md captured debugging journey
   - ADRs document "why" not just "what"

3. **Test Edge Cases Extensively**
   - Host migration, disconnects, packet loss
   - Full buffers, ID rollover, fragmentation
   - Each bug taught a lesson

4. **Homebrew Community is Valuable**
   - CTurt/dsgmLib provided foundation
   - devkitPro tools essential
   - Community testing and feedback

---

## Related Documents

- [ARCHITECTURE.md](../../ARCHITECTURE.md) - Complete protocol specification (60 pages)
- [LESSONS_LEARNED.md](../../LESSONS_LEARNED.md) - Development journey and debugging stories (50 pages)
- [ADR 002: Spectator Mode](002-spectator-mode.md) - Passive observation feature
- [ADR 003: Room Status System](003-room-status-system.md) - Dynamic join control
- [Protocol Specification](../protocol-specification.md) - Wire format reference
- [README.md](../../README.md) - Getting started guide

---

## Future Enhancements

### Potential Extensions (Not Committed)

1. **Encryption (TLS-Lite)**
   - Shared pre-shared key (PSK) for room
   - Optional feature (adds overhead)
   - Tournament security

2. **Bandwidth Adaptation**
   - Auto-adjust packet rate based on loss
   - Congestion detection
   - Quality-of-service (QoS) hints

3. **Position Interpolation**
   - Client-side prediction for smoother movement
   - Lag compensation techniques
   - Extrapolation on packet loss

4. **Recording and Replay**
   - Save packet stream to SD card
   - Playback for analysis
   - Integration with spectator mode (ADR 002)

5. **Cross-Platform Port**
   - Nintendo 3DS (similar WiFi hardware)
   - Other embedded WiFi devices
   - Abstract hardware layer

6. **Spectator Chat Channel**
   - Separate band for spectators (ADR 002)
   - Does not interfere with gameplay
   - Optional feature

---

## Success Metrics (Achieved)

| Metric | Target | Actual | Status |
|--------|--------|--------|--------|
| Simple API | < 100 LOC for basic multiplayer | 50-80 LOC | ✅ Exceeded |
| Reliable delivery | Handle 30%+ packet loss | Handles 40% | ✅ Exceeded |
| Low latency | < 100ms position updates | 30ms average | ✅ Exceeded |
| Low memory | < 10KB overhead | ~8KB | ✅ Met |
| Battery life | 3+ hours | 3-5 hours | ✅ Met |
| Max players | 6 simultaneous | 6 validated | ✅ Met |
| Stability | Zero crashes in 1-hour session | Zero crashes | ✅ Met |
| Developer adoption | Used in 3+ homebrew games | 5+ games | ✅ Exceeded |

---

## Retrospective Assessment

### What Went Well

- ✅ Event-driven API is intuitive and productive
- ✅ Circular buffers proved correct design choice
- ✅ TTL-based retry is simple and effective
- ✅ Extensive documentation captured knowledge
- ✅ 3 years of refinement resulted in stable library
- ✅ Extensible design enabled room status and spectator features

### What Could Be Improved

- ⚠️ Earlier documentation would have helped onboarding
- ⚠️ More emulator testing before hardware testing
- ⚠️ Formal protocol specification sooner (not retroactively)
- ⚠️ Unit tests (difficult on embedded hardware)

### Would We Do It Again?

**Yes.** The custom protocol was the right decision for this use case:
- Ad-hoc mode requirement eliminated TCP/UDP
- Simple API made multiplayer accessible to homebrew developers
- Performance and battery life meet targets
- Extensible design supports new features (spectator, room status)

---

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-11-22 | Retrospective ADR documenting 2022-2025 implementation |

---

**END OF ADR 001**
