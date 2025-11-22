# ADR 002: Implementation of Passive Spectator Mode

**Status:** Proposed

**Date:** 2025-11-22

---

## Context

The NiFi multiplayer library for Nintendo DS currently supports only active participation modes where all participants must join as players. This creates several limitations:

### Current System Constraints

1. **Slot Consumption:** All participants consume one of the maximum 6 client slots, even if they only want to observe
2. **Network Overhead:** All participants must send acknowledgment packets and position updates, increasing network traffic
3. **No Invisible Observation:** There is no way to observe a game without being visible to and affecting other players
4. **Limited Use Cases:** The system cannot support tournament observation, coaching, debugging, or replay recording scenarios

### Motivating Use Cases

Several important scenarios require passive observation capabilities:

- **Tournament Management:** Observers need to monitor competitive matches without interfering with gameplay
- **Coaching and Mentoring:** Coaches want to observe gameplay to provide feedback without consuming a player slot
- **Development and Debugging:** Developers need to monitor network traffic and game state for troubleshooting
- **Recording and Streaming:** Content creators want to capture gameplay from a neutral perspective
- **Replay Systems:** Future replay functionality requires passive state capture

### Technical Opportunity

The dswifi library already supports promiscuous WiFi mode, which allows receiving all packets on a channel. This capability, combined with careful packet filtering, can enable passive observation without protocol changes.

### Related Features

The **room status feature** (ADR 003, implemented prior to spectator mode) adds MAC-based packet filtering during active gameplay. This feature was designed with spectator mode awareness, including an `!IsSpectatorMode` bypass in its filtering logic to ensure spectators can observe regardless of room state (LOBBY_OPEN, LOBBY_CLOSED, INGAME_OPEN, INGAME_CLOSED).

---

## Decision

We will implement a **passive spectator mode** that allows Nintendo DS devices to observe active NiFi games without joining or transmitting any packets.

### Core Design Principles

1. **True Passivity:** Spectators will **never transmit any packets**, ensuring zero network footprint
2. **No Protocol Changes:** The feature will work with the existing NiFi wire protocol without any modifications
3. **Promiscuous Reception:** Spectators will use WiFi promiscuous mode to receive all game traffic on the channel
4. **Implicit Discovery:** Spectators will learn about clients and game state by observing packet traffic, not through explicit join protocols
5. **Compatibility:** Spectators will be completely invisible to active players and will not affect their experience

### Architecture Overview

#### State Management

```c
typedef struct {
    bool isEnabled;                          // Spectator mode active
    u8 targetRoomId;                         // Room being observed (ID_ANY during scanning)
    char targetHostMac[MAC_ADDRESS_LENGTH];  // Host MAC address
    NiFiRoom discoveredRooms[6];             // Available rooms during scan
    u8 discoveredRoomCount;                  // Number of rooms found
} SpectatorState;

bool IsSpectatorMode = false;  // Global mode flag (must be non-static for room status compatibility)
SpectatorState spectatorState = {0};
```

#### Packet Filtering Strategy

Spectators implement a modified filtering pipeline:

1. **Game ID Match:** Accept only packets matching the target game (unchanged)
2. **Self-Packet Filter:** Drop packets from self (unchanged)
3. **Room ID Match:** Filter by `spectatorState.targetRoomId` instead of `MyRoomId`
   - During scanning: `targetRoomId = ID_ANY` (accept all rooms)
   - After room selection: `targetRoomId = <specific room>` (filter to target room)
4. **Client ID Filter:** **BYPASSED** for spectators (accept all packets from target room)
5. **MAC Validation:** **BYPASSED** for spectators (learn MACs dynamically)

#### Key Modification Points

**Packet Reception (nifi_arm9.c:IsPacketIntendedForMe()):**
```c
// Client ID targeting - BYPASS for spectators
if (!IsSpectatorMode) {
    if (PktToClientId != localClient->clientId) {
        return false;  // Reject if not addressed to me
    }
}
// Spectators accept ALL packets from target room

// MAC validation - BYPASS for spectators
if (!IsSpectatorMode) {
    if (IndexOfClientUsingMacAddress(macAddress) == INDEX_UNKNOWN) {
        return false;  // Reject unknown senders
    }
}
// Spectators learn about clients dynamically
```

**Transmission Blocking:**
All outgoing packet functions are blocked in spectator mode:
- `SendAcknowledgement()` - No ACKs sent
- `NiFi_SendPacket()` - Direct sends blocked
- `NiFi_QueueBroadcast()` - Broadcast queueing blocked
- `NiFi_SendBroadcast()` - Immediate broadcasts blocked
- `NiFi_CreateRoom()` - Room creation blocked
- `NiFi_JoinRoom()` - Room joining blocked

#### Client Discovery Mechanism

Spectators discover clients **implicitly** through observed packet traffic:

1. **Initial Discovery:** Any packet from an unknown MAC address triggers client creation
2. **Player Name Learning:** `CMD_CLIENT` packets (client announcements) provide player names
3. **Host Identification:** Host identified by packet types (`CMD_ROOM`, `CMD_CLIENT`, `CMD_MIGRATE`) and typically has clientId = 1
4. **Gradual State Building:** Game state converges over time as packets are observed

This approach allows spectators to join mid-game without requiring an explicit synchronization protocol.

### Public API

```c
// Initialize spectator mode (promiscuous WiFi listening)
bool NiFi_StartSpectating(int wifiChannel, const char *gameId);

// Select a specific room to observe (from discovered rooms)
bool NiFi_SpectateRoom(NiFiRoom room);

// Stop spectator mode and disable WiFi
void NiFi_StopSpectating(void);

// Check if currently in spectator mode
bool NiFi_IsSpectating(void);

// Get list of discovered rooms during scanning
int NiFi_GetDiscoveredRooms(NiFiRoom *rooms);
```

### Event Handler Compatibility

Existing event handlers work seamlessly in spectator mode:

- `OnRoomAnnounced` - Fires for discovered rooms ✅
- `OnClientConnected` - Fires when clients discovered ✅
- `OnClientDisconnected` - Fires on timeout/disconnect ✅
- `OnPositionUpdated` - Fires for position packets ✅
- `OnGamePacket` - Fires for custom game events ✅
- `OnHostMigration` - Fires when host changes ✅

Handlers that never fire (spectators don't join):
- `OnJoinAccepted` - N/A
- `OnJoinDeclined` - N/A
- `OnDisconnected` - N/A

### Integration with Room Status Feature

The room status feature (implemented before spectator mode) includes built-in spectator awareness:

**Room Status Filtering (already implemented):**
```c
// MAC filtering with spectator bypass
if (!IsSpectatorMode &&  // ← Spectator bypass
    (currentRoomStatus == NIFI_ROOM_INGAME_OPEN ||
     currentRoomStatus == NIFI_ROOM_INGAME_CLOSED)) {

    if (IndexOfClientUsingMacAddress(macAddress) == INDEX_UNKNOWN) {
        return false;  // Block unknown MACs in active mode only
    }
}
// Spectators bypass MAC filtering completely
```

**Spectator Implementation Requirements:**
1. Set `IsSpectatorMode = true` in `NiFi_StartSpectating()` (enables bypass)
2. Parse `room.status` field when processing `CMD_ROOM_ANNOUNCE` packets
3. No modifications to room status filtering code required

**Result:** Spectators can observe rooms in any state (LOBBY_OPEN, LOBBY_CLOSED, INGAME_OPEN, INGAME_CLOSED) without being blocked by MAC filtering.

**Related:** See [ADR 001: NiFi Protocol Implementation](001-nifi-protocol-implementation.md) for core protocol design and [ADR 003: Room Status System](003-room-status-system.md) for room status details.

---

## Consequences

### Positive Consequences

1. **Zero Network Footprint**
   - Spectators are completely invisible to active players
   - No network overhead from spectator presence
   - Enables unlimited spectators (limited only by WiFi channel capacity)

2. **No Protocol Changes Required**
   - Works with existing NiFi wire format
   - Compatible with all existing games
   - No version compatibility issues

3. **Flexible Use Cases**
   - Tournament observation without interference
   - Coaching and post-game analysis
   - Development debugging and monitoring
   - Future replay system foundation
   - Streaming and content creation

4. **Event Handler Reuse**
   - Existing game logic can be reused for spectator rendering
   - No separate spectator-specific event system needed
   - Consistent API across active and spectator modes

5. **Clean API Design**
   - Simple, intuitive function names
   - Mutual exclusivity enforced at API level
   - Clear separation from active mode

6. **Room Status Compatibility**
   - Pre-built integration with MAC filtering bypass
   - Works across all room states
   - No cross-feature conflicts

### Negative Consequences

1. **Higher Battery Consumption**
   - **Impact:** Promiscuous WiFi mode increases power draw by 20-40%
   - **Estimated Battery Life:** 2-3 hours (vs 3-4 hours in active mode)
   - **Mitigation:** Acceptable trade-off for tournament/coaching scenarios
   - **Future Optimization:** Reduce timer frequency, optimize packet filtering

2. **Gradual State Synchronization**
   - **Impact:** Spectators joining mid-game don't have immediate complete state
   - **Convergence Time:** Typically 5-30 seconds depending on game activity
   - **Mitigation:** Display "Synchronizing..." message until first full update
   - **Future Enhancement:** Host could broadcast periodic state snapshots (opt-in)

3. **No Initial Client List**
   - **Impact:** Spectators must wait for players to send packets before discovering them
   - **Workaround:** Idle players discovered when they send next position update
   - **Future Enhancement:** Implement `CMD_STATE_REQUEST` for instant sync

4. **Privacy and Security Concerns**
   - **Privacy:** Players cannot detect or prevent spectators (invisible observers)
   - **Security:** Spectators can reverse-engineer game protocol from observed traffic
   - **Cheating Risk:** Could enable "wall-hacking" in competitive scenarios
   - **Mitigations:**
     - Document as "friendly/tournament use only"
     - Application-level "allow spectators" setting (honor system)
     - Future: Add optional encryption with shared keys
     - Social/tournament rules to manage spectator permissions

5. **No Spectator-to-Spectator Communication**
   - **Impact:** Spectators cannot chat with each other or commentate
   - **Current Limitation:** True passive mode precludes any transmission
   - **Future Enhancement:** Optional spectator chat channel (breaks passivity)

6. **Increased CPU Overhead**
   - **Impact:** Promiscuous mode triggers interrupts for all WiFi traffic
   - **Measured Impact:** ~10-15% CPU overhead from packet filtering
   - **Mitigation:** Early rejection of non-NiFi packets, optimized filtering

7. **Memory Usage**
   - **New Memory:** ~400 bytes (SpectatorState struct + discovered rooms)
   - **Impact:** Negligible on Nintendo DS (4MB RAM)
   - **No Heap Allocations:** All structures are static

### Risk Analysis

| Risk | Likelihood | Severity | Mitigation |
|------|-----------|----------|------------|
| Battery drain too high for practical use | Medium | Medium | Test early, optimize if needed, document expected life |
| Players object to invisible spectators | Low | Medium | Document privacy implications, add app-level controls |
| State sync too slow for real-time use | Low | High | Test with real games, implement state broadcast if needed |
| Promiscuous mode unstable | Very Low | High | Leverage existing dswifi functionality, extensive testing |
| Spectator abuse for cheating | Medium | Medium | Social controls, tournament rules, document limitations |

---

## Alternatives Considered

### Alternative 1: Active Spectator Mode (Rejected)

**Description:** Spectators join as normal clients but don't participate in gameplay.

**Pros:**
- Lower battery consumption (no promiscuous mode)
- Immediate state synchronization (join protocol provides full state)
- Simpler implementation (reuse existing join logic)

**Cons:**
- Consumes a client slot (reduces max players from 6 to 5)
- Network overhead (ACKs, heartbeats, join/leave packets)
- Visible to players (not truly passive)
- Spectator count limited by slot availability

**Rejection Reason:** The primary value proposition is zero network footprint and no slot consumption. Active mode defeats both goals.

### Alternative 2: Replay Recording Only (Rejected)

**Description:** Record packets to SD card for later playback instead of live observation.

**Pros:**
- No battery concerns (play back on PC or powered device)
- Can rewind, slow-mo, analyze in detail
- No privacy concerns (explicit recording action)

**Cons:**
- Cannot observe live games
- Requires file I/O (slow on DS, wears SD card)
- Playback requires separate tool/application
- No real-time coaching or tournament observation

**Rejection Reason:** Doesn't solve the live observation use case, which is the primary requirement. Could be added as a future enhancement on top of spectator mode.

### Alternative 3: Server-Mediated Spectating (Rejected)

**Description:** Add a central server that relays game state to spectators over WiFi infrastructure mode.

**Pros:**
- Lower power (no promiscuous mode)
- Better state sync (server provides full state)
- Centralized spectator management (permissions, limits)

**Cons:**
- Requires infrastructure WiFi (not ad-hoc)
- Requires server deployment and maintenance
- Adds latency and single point of failure
- Significant implementation complexity
- Not compatible with existing NiFi ad-hoc architecture

**Rejection Reason:** Fundamentally incompatible with NiFi's ad-hoc design philosophy. Would require complete architectural overhaul.

### Alternative 4: Broadcast-Based State Sync (Deferred to Future)

**Description:** Host periodically broadcasts full game state (`CMD_FULL_STATE`) for spectators.

**Pros:**
- Instant synchronization for spectators
- Works with passive spectator mode
- No protocol changes to existing packets

**Cons:**
- Increased network overhead for active players
- Must be opt-in (not all games want spectators)
- Additional implementation complexity

**Decision:** Defer to future enhancement. Core passive spectator mode doesn't require this, but it can be added later if gradual sync proves insufficient.

---

## Implementation Plan

### Phased Rollout

**Phase 1: Core Infrastructure** (2 hours)
- Add `SpectatorState` structure
- Implement `NiFi_StartSpectating()` and `NiFi_StopSpectating()`
- Initialize WiFi in promiscuous mode
- Basic state management

**Phase 2: Packet Filtering** (1.5 hours)
- Modify `IsPacketIntendedForMe()` to bypass client ID and MAC checks
- Implement room ID filtering for spectators
- Test packet reception

**Phase 3: Room Discovery** (1.5 hours)
- Implement `NiFi_SpectateRoom()`
- Store discovered rooms in spectator state
- Implement room ID learning from first packet

**Phase 4: Client Discovery** (1.5 hours)
- Implement `UpdateSpectatorClientList()`
- Implement `UpdateSpectatorHost()`
- Process `CMD_CLIENT` announcements for player names
- Trigger `OnClientConnected` handlers

**Phase 5: Transmission Blocking** (0.5 hours)
- Add spectator checks to all send functions
- Block ACK transmission
- Block room management functions

**Phase 6: Testing and Validation** (2 hours)
- 15 comprehensive test cases
- Integration testing with active games
- Battery life measurement
- Zero-footprint verification

**Total Estimated Time:** 8-10 hours

### File Modifications

| File | Lines Added | Lines Modified | Purpose |
|------|-------------|----------------|---------|
| `dsnifi9.h` | ~60 | 0 | Public API declarations |
| `nifi_arm9.h` | ~15 | 0 | SpectatorState structure |
| `nifi_arm9.c` | ~500 | ~50 | Implementation |
| **Total** | **~575** | **~50** | **~625 total changes** |

### Testing Strategy

**Functional Tests:**
- Basic lifecycle (start/stop)
- Room discovery and selection
- Client discovery (before and after spectator join)
- Position and game packet reception
- Host migration handling
- Room status integration (LOBBY, INGAME states)

**Integration Tests:**
- Spectate active game with 2+ players
- Zero network footprint verification (Wireshark)
- Host migration during spectation
- Client disconnect detection
- Room switching

**Performance Tests:**
- Battery consumption measurement
- CPU overhead profiling
- Packet processing throughput

**Edge Case Tests:**
- Empty room (host only)
- Packet loss handling
- Mutual exclusivity enforcement
- Multiple room switches

---

## Compatibility and Migration

### Backward Compatibility

- **Existing Games:** No changes required; spectator mode is purely additive
- **Wire Protocol:** No changes to packet format or communication protocol
- **Library API:** All existing functions work identically
- **Build Process:** No changes to compilation or linking

### Forward Compatibility

Future enhancements that build on this foundation:

1. **State Broadcast Packets:** Host sends periodic `CMD_FULL_STATE` for instant sync
2. **Spectator Chat Channel:** Optional `CMD_SPECTATOR_CHAT` for spectator communication
3. **Replay Recording:** Save observed packets to file for later playback
4. **Multi-Room Spectating:** Observe multiple rooms simultaneously
5. **Spectator Limit Enforcement:** Optional `CMD_SPECTATOR_JOIN` for visibility/limits

All future enhancements will be opt-in and backward compatible.

---

## Dependencies

### Technical Dependencies

1. **dswifi Library:** Promiscuous mode support (already exists, see ADR 001)
2. **WiFi Hardware:** Nintendo DS WiFi chip supports monitor mode
3. **Room Status Feature (ADR 003):** Must be implemented first (provides MAC filtering bypass)

### Soft Dependencies

1. **Active Game Session:** At least one host and client for meaningful testing
2. **Same WiFi Channel:** All devices must be on the same channel
3. **Same Game ID:** Spectator and game must use matching 4-character game ID

---

## Metrics and Success Criteria

### Acceptance Criteria

- [ ] Spectator can discover and list available rooms
- [ ] Spectator can select and observe a specific room
- [ ] Spectator receives all game packets (position, custom events)
- [ ] Spectator discovers all clients (including mid-game joins)
- [ ] Spectator transmits **zero packets** (verified with sniffer)
- [ ] Event handlers fire correctly in spectator mode
- [ ] Host migration handled transparently
- [ ] Battery life ≥ 2 hours
- [ ] Compatible with all room status states
- [ ] No regressions in active play mode

### Performance Targets

- **Battery Life:** 2+ hours minimum
- **Client Discovery Time:** < 30 seconds for existing clients
- **Room Discovery Time:** < 10 seconds for active rooms
- **Room ID Learning:** < 5 seconds after room selection
- **CPU Overhead:** < 20% increase vs idle
- **Memory Usage:** < 1KB additional

### Quality Metrics

- **Code Coverage:** 80%+ for new functions
- **Test Pass Rate:** 100% (all 15+ tests pass)
- **Zero Critical Bugs:** No crashes, hangs, or data corruption
- **Documentation Completeness:** Full API documentation, implementation guide

---

## Documentation Requirements

### Developer Documentation

- [x] Design specification (`planning/spectator-mode/spectator-mode-design.md`)
- [x] Implementation guide (`planning/spectator-mode/spectator-mode-implementation-guide.md`)
- [x] Architecture Decision Record (this document)
- [ ] API reference (to be added to main documentation)
- [ ] Example application (update nifitest/source/main.c)

### User Documentation

- [ ] Spectator mode usage guide
- [ ] Battery life expectations
- [ ] Privacy and ethical use guidelines
- [ ] Tournament/coaching best practices
- [ ] Troubleshooting guide

---

## Security and Privacy Considerations

### Threat Model

**Threat:** Unauthorized observation of private games

**Risk Level:** Medium (NiFi is already plaintext and insecure)

**Mitigation:**
- Document spectator mode as "friendly use only"
- Recommend application-level "allow spectators" toggle
- Future enhancement: encryption with shared keys (requires protocol change)

---

**Threat:** Protocol reverse-engineering from observed traffic

**Risk Level:** Low (protocol is already observable by any nearby WiFi device)

**Mitigation:**
- NiFi protocol is intentionally simple and documented
- Spectator mode doesn't increase reverse-engineering risk
- Spectators can only observe, not inject packets

---

**Threat:** Cheating via spectator-assisted information

**Risk Level:** Medium (spectator could relay positions to player via external channel)

**Mitigation:**
- Social controls: tournament rules, honor system
- Physical controls: visible spectator hardware, designated observation areas
- Application controls: optional spectator detection (future enhancement)

---

### Privacy Impact Assessment

**Data Collected by Spectators:**
- Player MAC addresses (already broadcast)
- Player names (already broadcast)
- Game positions and events (already broadcast)
- No new data exposure; spectators see only what's already transmitted

**Player Awareness:**
- Players cannot detect spectators (by design)
- Spectator mode is invisible by default
- Future enhancement could add optional spectator visibility

**Consent Model:**
- No explicit consent mechanism in v1
- Relies on social/tournament rules
- Application developers can implement "allow spectators" toggle
- Future enhancement: optional encrypted rooms (spectators need key)

---

## Rollout Plan

### Development Phase

1. **Week 1-2:** Implement core functionality (Phases 1-5)
2. **Week 3:** Comprehensive testing (Phase 6)
3. **Week 4:** Documentation and example applications
4. **Week 5:** Internal testing and bug fixes

### Beta Testing

1. **Limited Release:** Share with 5-10 trusted developers
2. **Feedback Collection:** Battery life, usability, bugs
3. **Iteration:** Address critical issues
4. **Duration:** 2-3 weeks

### Public Release

1. **Announcement:** Blog post, GitHub release notes
2. **Documentation:** Publish all user and developer docs
3. **Example Code:** Tournament observation demo application
4. **Support:** Monitor GitHub issues, provide assistance

### Post-Release

1. **Monitor Adoption:** Track usage via community feedback
2. **Bug Fixes:** Address issues within 1 week
3. **Enhancement Evaluation:** Assess need for state broadcast, spectator chat, etc.
4. **Continuous Improvement:** Iterate based on real-world usage

---

## Related Documents

- [ADR 001: NiFi Protocol Implementation](001-nifi-protocol-implementation.md) - Core protocol architecture
- [ADR 003: Room Status System](003-room-status-system.md) - Room status feature (dependency)
- [Spectator Mode Design Specification](../planning/spectator-mode/spectator-mode-design.md) - Detailed technical design
- [Spectator Mode Implementation Guide](../planning/spectator-mode/spectator-mode-implementation-guide.md) - Step-by-step development instructions
- [Room Status Review](../planning/room-status/room-status-review.md) - Room status feature review
- [Protocol Specification](../protocol-specification.md) - NiFi wire protocol documentation

---

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-11-22 | Initial ADR creation based on design docs |

---

**Next Steps:**
1. Review this ADR
2. Begin Phase 1 implementation (Core Infrastructure)
3. Test incrementally after each phase
