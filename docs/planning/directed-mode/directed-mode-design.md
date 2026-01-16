# Directed Mode (Power Saving) - Design Specification

**Document Version:** 1.0
**Created:** 2026-01-16
**Updated:** 2026-01-16
**Status:** Design Phase
**Estimated Implementation Time:** 12-20 hours
**Complexity:** High

> **Cross-Feature Note:** This feature is **INCOMPATIBLE with Spectator Mode**. Spectators rely on promiscuous mode to eavesdrop on all game traffic. Directed mode must be opt-in and cannot be used when spectators are present.

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Current Architecture Analysis](#current-architecture-analysis)
3. [Technical Investigation](#technical-investigation)
4. [Proposed Architecture](#proposed-architecture)
5. [Spectator Mode Incompatibility](#spectator-mode-incompatibility)
6. [Implementation Plan](#implementation-plan)
7. [Performance Considerations](#performance-considerations)
8. [Testing Strategy](#testing-strategy)
9. [Risk Analysis](#risk-analysis)
10. [Appendix](#appendix)

---

## Executive Summary

### What We're Building

**Directed Mode** is an optional power-saving WiFi mode that switches from promiscuous packet reception (receive ALL WiFi traffic) to directed packet reception (receive only packets addressed to your MAC address).

Currently, NiFi operates in **promiscuous mode** where the DS WiFi hardware captures every 802.11 frame on channel 10. The application layer (`IsPacketIntendedForMe()`) then filters packets in software. This works but has two costs:

1. **Power consumption**: WiFi chip is constantly active processing all traffic
2. **CPU overhead**: Software must parse and filter every packet on the channel

In **directed mode**, the WiFi hardware itself filters packets, only waking the CPU for packets addressed to the device's MAC address. This enables hardware-level power management and reduces CPU load.

### Why We're Building It

**Use Cases:**
- **Extended play sessions**: Competitive games where battery life matters
- **Tournament settings**: Multiple DS consoles playing for hours
- **Casual games**: Turn-based games where latency isn't critical

**Current Limitations:**
- WiFi module runs at ~80mW constantly in promiscuous mode
- CPU processes every packet on channel 10 (potentially hundreds per second in busy environments)
- No hardware power management possible (chip can't sleep)
- Battery drain even when no game packets are being sent

**Benefits After Implementation:**
- ~50% power reduction (estimated 40mW in directed mode)
- 30-60 minutes additional battery life per charge
- Reduced CPU load (hardware filters instead of software)
- Better scalability in busy WiFi environments

---

## Current Architecture Analysis

### Promiscuous Mode Flow

```
┌─────────────────────────────────────────────────────────────────────┐
│                     WiFi Hardware (MM3218)                          │
│                                                                     │
│   Channel 10 ──▶ [Receive ALL 802.11 frames] ──▶ ARM7 Buffer       │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼ (All packets)
┌─────────────────────────────────────────────────────────────────────┐
│                         ARM9 (NiFi Layer)                           │
│                                                                     │
│   RawPacketHandler() ──▶ IsPacketIntendedForMe() ──▶ ProcessPacket │
│                              │                                      │
│                              ├── Check Game ID                      │
│                              ├── Check Room ID                      │
│                              ├── Check Client ID                    │
│                              └── Check MAC Address                  │
│                                                                     │
│   Most packets REJECTED here (software filtering)                   │
└─────────────────────────────────────────────────────────────────────┘
```

### Key Code Locations

| File | Function | Lines | Purpose |
|------|----------|-------|---------|
| `dswifi/arm9/source/wifi_arm9.c` | `Wifi_SetPromiscuousMode()` | 471-473 | Enable/disable promiscuous mode |
| `dswifi/arm9/source/nifi_arm9.c` | `IsPacketIntendedForMe()` | 195-297 | Software packet filtering |
| `dswifi/arm9/source/nifi_arm9.c` | `RawPacketHandler()` | 391-435 | Raw packet reception callback |
| `dswifi/include/dswifi9.h` | `Wifi_RawSetPacketHandler()` | 319-321 | Set packet handler callback |

### Current Packet Header Structure

NiFi packets are broadcast with destination MAC `FF:FF:FF:FF:FF:FF` (broadcast):

```
802.11 Frame Header (Current):
┌─────────────────────────────────────────────────────────────┐
│ Frame Control │ Duration │ Dest MAC (BROADCAST) │ Src MAC  │
│    2 bytes    │ 2 bytes  │   FF:FF:FF:FF:FF:FF  │ 6 bytes  │
└─────────────────────────────────────────────────────────────┘
                               │
                               └── All devices receive this
```

---

## Technical Investigation

### Current PACKET_MODE_NIFI Analysis

**Key Finding:** `PACKET_MODE_NIFI` hardcodes broadcast MAC addresses!

```c
// dswifi/arm9/source/wifi_arm9.c:385
// MACs.
memset(framehdr + 8, 0xFF, 18);  // Forces ALL packets to broadcast!
```

This means every NiFi packet is sent to `FF:FF:FF:FF:FF:FF` regardless of the intended recipient. The `toClientId` field in the NiFi protocol is only used for application-layer filtering, not 802.11 frame addressing.

### Packet Modes Comparison

| Mode | Destination MAC | Use Case |
|------|-----------------|----------|
| `PACKET_MODE_WIFI` | Caller specifies | Standard WiFi |
| `PACKET_MODE_NIFI` | Hardcoded broadcast (0xFF) | Current NiFi (all broadcast) |
| `PACKET_MODE_NIFI_DIRECT` | Actual client MAC | **NEW: Directed mode** |

### Frame Structure (PACKET_MODE_NIFI)

```c
// dswifi/arm9/source/wifi_arm9.c:363-430
framehdr[6] = 0x0208;           // Frame control (data frame)
framehdr[7] = 0;                // Duration
memset(framehdr + 8, 0xFF, 18); // 3x MAC addresses = broadcast
// ... LLC header with protocol 0x08FE
// ... payload data
```

**802.11 MAC Address Fields (18 bytes total):**
- Bytes 8-13: Destination MAC (currently FF:FF:FF:FF:FF:FF)
- Bytes 14-19: Source MAC (currently FF:FF:FF:FF:FF:FF)
- Bytes 20-25: BSSID (currently FF:FF:FF:FF:FF:FF)

### Promiscuous Mode Note

NiFi currently does NOT explicitly call `Wifi_SetPromiscuousMode()`. It uses:
- `Wifi_SetRawPacketMode(PACKET_MODE_NIFI)` - for transmission
- `Wifi_RawSetPacketHandler()` - for reception

The raw packet handler receives all packets on the channel (effectively promiscuous). Future complex NiFi implementations may want explicit promiscuous mode control, but current implementation works without it.

### DS WiFi Hardware Capabilities

**Chipset:** Mitsumi MM3218 (DS Lite) / MM3155 (Original DS)

**Known Capabilities:**
- 802.11b only (1-2 Mbps)
- Raw frame transmission via `Wifi_RawTxFrame()`
- Raw packet reception via handler callback
- Hardware WEP encryption

**Unknown/Needs Testing:**
- [ ] Does hardware deliver unicast frames addressed to our MAC when not using raw handler?
- [ ] Power consumption difference between broadcast and unicast reception?
- [ ] Can we use `Wifi_SetPromiscuousMode(0)` to enable hardware MAC filtering?

---

## Proposed Architecture

### New Packet Mode: PACKET_MODE_NIFI_DIRECT

The cleanest solution is to add a new packet mode that uses actual MAC addresses instead of hardcoded broadcast:

```c
// dswifi/include/dswifi9.h - Add new mode
enum WIFI_PACKET_MODE {
    PACKET_MODE_WIFI,
    PACKET_MODE_NIFI,           // Existing: broadcast all packets
    PACKET_MODE_NIFI_DIRECT,    // NEW: unicast to specific MACs
};
```

### Mode Switching Flow

```
┌─────────────────────────────────────────────────────────────────────┐
│                        ROOM LIFECYCLE                                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   CREATE/JOIN ──▶ LOBBY ──▶ LOCK ROOM ──▶ IN-GAME                  │
│        │            │            │            │                     │
│        │            │            │            ▼                     │
│        │            │            │   ┌─────────────────┐            │
│        │            │            └──▶│ Switch to       │            │
│        │            │                │ NIFI_DIRECT     │            │
│        │            │                │ (optional)      │            │
│        │            │                └─────────────────┘            │
│        │            │                                               │
│        ▼            ▼                                               │
│   PACKET_MODE_NIFI (broadcast)     PACKET_MODE_NIFI_DIRECT         │
│   - Room discovery works           - Only known clients             │
│   - New players can join           - Power saving enabled           │
│   - Spectators can watch           - NO spectators allowed          │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### State Machine

```
                    ┌──────────────────────┐
                    │  PACKET_MODE_NIFI    │ ◀── Default (broadcast)
                    │  (Spectators OK)     │
                    └──────────┬───────────┘
                               │
                               │ Room locked + Host calls NiFi_EnableDirectedMode()
                               │ (Only if no spectators)
                               ▼
                    ┌──────────────────────┐
                    │  MODE_SWITCH_PENDING │
                    │  (Sync all clients)  │
                    └──────────┬───────────┘
                               │
                               │ All clients ACK + timer expires
                               ▼
                    ┌────────────────────────────┐
                    │  PACKET_MODE_NIFI_DIRECT   │
                    │  (Unicast to known MACs)   │
                    │  (NO spectators)           │
                    └──────────┬────────────────┘
                               │
                               │ Room unlocked OR new player rejoins
                               ▼
                    ┌──────────────────────┐
                    │  PACKET_MODE_NIFI    │
                    └──────────────────────┘
```

### Transmission Changes

**PACKET_MODE_NIFI (Current - Broadcast):**
```c
// All 3 MAC fields set to broadcast
memset(framehdr + 8, 0xFF, 18);
```

**PACKET_MODE_NIFI_DIRECT (New - Unicast):**
```c
// Set actual MAC addresses
memcpy(framehdr + 8, destMac, 6);       // Destination MAC
memcpy(framehdr + 11, localMac, 6);     // Source MAC
memcpy(framehdr + 14, bssidMac, 6);     // BSSID (could use host MAC)
```

### NiFi_SendBroadcast Behavior Change

In directed mode, `NiFi_SendBroadcast()` already iterates through clients - it just needs to use their actual MACs:

```c
void NiFi_SendBroadcast(NiFiPacket *packet, u8 ignoreClientIds[]) {
    for (u8 i = 0; i < CLIENT_MAX; i++) {
        if (clients[i].clientId == ID_EMPTY) continue;
        if (clients[i].clientId == localClient->clientId) continue;
        // ... ignore list check ...

        packet->toClientId = clients[i].clientId;

        if (wirelessMode == PACKET_MODE_NIFI_DIRECT) {
            // Unicast: frame addressed to this specific client's MAC
            NiFi_SendPacket_Directed(packet, clients[i].macAddress);
        } else {
            // Broadcast: frame goes to FF:FF:FF:FF:FF:FF (current behavior)
            NiFi_SendPacket(packet);
        }
    }
}
```

### Reception Consideration

Transmission is straightforward with PACKET_MODE_NIFI_DIRECT. Reception is the open question:

**Option A: Hardware MAC Filtering (Ideal)**
- Disable raw packet handler
- Hardware only delivers frames addressed to our MAC
- Power savings from hardware sleep
- **Needs testing to verify DS WiFi supports this**

**Option B: Software MAC Filtering (Fallback)**
- Keep raw packet handler
- Filter by destination MAC in `IsPacketIntendedForMe()`
- Less power savings (CPU still processes all packets)
- **Guaranteed to work**

**Recommendation:** Implement transmission first (PACKET_MODE_NIFI_DIRECT), then investigate hardware reception filtering as a separate optimization.

---

## Spectator Mode Incompatibility

### Why Directed Mode Breaks Spectators

Spectator mode works by **eavesdropping** on broadcast traffic:

```
PROMISCUOUS MODE:
┌─────────┐         ┌─────────┐         ┌─────────┐
│  Host   │ ──────▶ │ Player  │         │Spectator│
│         │ broadcast│         │         │  (sees) │
└─────────┘    │    └─────────┘         └────▲────┘
               │                              │
               └──────────────────────────────┘
                    Broadcast reaches everyone

DIRECTED MODE:
┌─────────┐         ┌─────────┐         ┌─────────┐
│  Host   │ ──────▶ │ Player  │         │Spectator│
│         │ unicast │         │         │ (blind) │
└─────────┘ to P1   └─────────┘         └─────────┘
                                              │
                    Unicast only reaches      │
                    destination MAC     ──────┘
                                        CANNOT SEE
```

**Impact:**
- Spectators receive ZERO game traffic in directed mode
- Room discovery still works (SCAN/ROOM_ANNOUNCE remain broadcast)
- Once game starts in directed mode, spectators are blind

### Compatibility Strategy

**Option 1: Mutual Exclusion (Recommended)**
- Directed mode and spectator mode cannot coexist
- If spectators are present, directed mode is blocked
- If directed mode is active, spectators cannot join

**Option 2: Hybrid Mode**
- Host sends critical updates as broadcast (for spectators)
- Host sends high-frequency updates as unicast (for players)
- Complexity: High, potential inconsistency

**Option 3: Spectator Relay**
- One player acts as relay for spectators
- Receives unicast, rebroadcasts for spectators
- Doubles traffic, negates power savings

**Recommendation:** Option 1 - Keep modes mutually exclusive for simplicity.

### API Design for Mutual Exclusion

```c
// Returns false if spectators are present
bool NiFi_EnableDirectedMode(void) {
    if (!NiFi_IsHost()) {
        PrintDebug(DBG_Error, "Only host can enable directed mode");
        return false;
    }

    if (spectatorsPresent) {  // Need way to detect this
        PrintDebug(DBG_Error, "Cannot enable directed mode: spectators present");
        return false;
    }

    // Initiate mode switch...
    return true;
}

// Spectator join blocked if directed mode active
void NiFi_JoinRoom(NiFiRoom room) {
    if (IsSpectatorMode && room.directedModeActive) {
        PrintDebug(DBG_Error, "Cannot spectate: room using directed mode");
        return;
    }
    // ...
}
```

---

## Implementation Plan

### Phase 1: Add PACKET_MODE_NIFI_DIRECT (4-6 hours)

**Objective:** Create new packet mode that sends unicast frames to specific MACs.

**Files Modified:**
- `dswifi/include/dswifi9.h` - Add enum value
- `dswifi/arm9/source/wifi_arm9.h` - Add enum value
- `dswifi/arm9/source/wifi_arm9.c` - Implement directed frame building

**Tasks:**

1. **Add new enum value:**
```c
// dswifi/include/dswifi9.h
enum WIFI_PACKET_MODE {
    PACKET_MODE_WIFI,
    PACKET_MODE_NIFI,
    PACKET_MODE_NIFI_DIRECT,  // NEW
};
```

2. **Add destination MAC storage and setter:**
```c
// dswifi/arm9/source/wifi_arm9.c
static u8 directedDestMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Set destination MAC for directed mode (MAC string format: "A1B2C3D4E5F6")
void Wifi_SetDirectedDestMac(char *macString) {
    // Parse MAC string to bytes (NiFi stores MACs as strings)
    for (int i = 0; i < 6; i++) {
        char hex[3] = { macString[i*2], macString[i*2+1], 0 };
        directedDestMac[i] = strtol(hex, NULL, 16);
    }
}
```

3. **Modify Wifi_RawTxFrame for PACKET_MODE_NIFI_DIRECT:**
```c
// dswifi/arm9/source/wifi_arm9.c:363+
else if (wirelessMode == PACKET_MODE_NIFI_DIRECT) {
    // Same frame structure as PACKET_MODE_NIFI but with actual MACs
    int base, framelen, hdrlen, writelen;
    // ... same setup code ...

    // Use actual destination MAC instead of broadcast (0xFF)
    memcpy(framehdr + 8, directedDestMac, 6);           // Destination MAC
    memcpy(framehdr + 11, WifiData->MacAddr, 6);        // Source MAC (our hardware MAC)
    memset(framehdr + 14, 0xFF, 6);                     // BSSID (keep as broadcast for ad-hoc)

    // ... rest same as PACKET_MODE_NIFI
}
```

**Note:** NiFi stores MAC addresses as strings (e.g., "A1B2C3D4E5F6") so `Wifi_SetDirectedDestMac` parses the string to bytes.

### Phase 2: NiFi Layer Integration (4-6 hours)

**Files Modified:**
- `dswifi/arm9/source/nifi_arm9.c` - Modify NiFi_SendPacket to use mode
- `dswifi/arm9/source/nifi_arm9.h` - Add internal state
- `dswifi/include/dsnifi9.h` - Add public API

**Key Insight:** The MAC addresses are already stored in `clients[].macAddress` - they're the real hardware MACs. We just need to pass them to the frame builder instead of using broadcast.

**Tasks:**

1. **Add directed mode state:**
```c
// dswifi/arm9/source/nifi_arm9.c
static bool DirectedModeEnabled = false;
```

2. **Modify NiFi_SendPacket to handle both modes:**
```c
void NiFi_SendPacket(NiFiPacket *packet) {
    // ... existing spectator mode checks ...

    char outgoingBuffer[RAW_PACKET_LENGTH];
    int packetLength = WritePacketToBuffer(packet, outgoingBuffer);

    if (DirectedModeEnabled && packet->toClientId != ID_ANY) {
        // Directed mode: look up destination MAC from toClientId
        int8 clientIndex = IndexOfClientUsingId(packet->toClientId);
        if (clientIndex != INDEX_UNKNOWN) {
            Wifi_SetDirectedDestMac(clients[clientIndex].macAddress);
            Wifi_SetRawPacketMode(PACKET_MODE_NIFI_DIRECT);
        }
    } else {
        // Broadcast mode (default)
        Wifi_SetRawPacketMode(PACKET_MODE_NIFI);
    }

    Wifi_RawTxFrame(packetLength, WIFI_TRANSMIT_RATE, (unsigned short *)outgoingBuffer);
}
```

3. **NiFi_SendBroadcast remains unchanged:**
```c
// NiFi_SendBroadcast already iterates through clients and sets toClientId
// NiFi_SendPacket will handle directed vs broadcast based on mode
void NiFi_SendBroadcast(NiFiPacket *packet, u8 ignoreClientIds[]) {
    if (IsSpectatorMode) return;

    for (u8 i = 0; i < CLIENT_MAX; i++) {
        if (clients[i].clientId == ID_EMPTY) continue;
        if (clients[i].clientId == localClient->clientId) continue;
        // ... ignore list check ...

        packet->toClientId = clients[i].clientId;
        NiFi_SendPacket(packet);  // Handles mode internally
    }
}
```

**Note:** The MAC addresses in `clients[]` are already the real hardware MACs - they're captured during room discovery and join. No conversion needed.

### Phase 3: Mode Switching Protocol (4-6 hours)

**Files Modified:**
- `dswifi/arm9/source/nifi_arm9.c` - Mode switch logic
- `dswifi/arm9/source/nifi_arm9.h` - Command constants

**New Protocol Commands:**
```c
#define CMD_DIRECT_MODE_REQUEST "DIRECT"   // Host initiates switch
#define CMD_DIRECT_MODE_ACK "DIrack"       // Client acknowledges
#define CMD_DIRECT_MODE_OFF "DIROFF"       // Return to broadcast mode
```

**Synchronization Protocol:**
```
Host                          Client 1                    Client 2
  │                               │                           │
  │──DIRECT (switch at T+3s)─────▶│                           │
  │──DIRECT (switch at T+3s)──────────────────────────────────▶
  │                               │                           │
  │◀─────────DIRACT───────────────│                           │
  │◀──────────────────────────────────────────DIRACT──────────│
  │                               │                           │
  │         [T+3s: All switch DirectedModeEnabled = true]     │
  │                               │                           │
  │◀═══════ UNICAST TRAFFIC ═════▶│◀═════ UNICAST TRAFFIC ═══▶│
```

**Public API:**
```c
// dswifi/include/dsnifi9.h

/// Enable directed mode (unicast to known MACs only)
/// @return true if switch initiated, false if blocked (spectators present, not host, etc.)
/// @note Only callable by host when room is locked (INGAME_OPEN or INGAME_CLOSED)
extern bool NiFi_EnableDirectedMode(void);

/// Disable directed mode (return to broadcast)
/// @note Called automatically when room unlocks or new player joins
extern void NiFi_DisableDirectedMode(void);

/// Check if directed mode is active
extern bool NiFi_IsDirectedMode(void);
```

### Phase 4: Spectator Mode Integration (2-4 hours)

**Tasks:**
1. Block `NiFi_EnableDirectedMode()` if spectators present (host can't detect spectators easily - document this)
2. Include directed mode status in ROOM_ANNOUNCE packets
3. Block spectator join if room announces directed mode active
4. Document mutual exclusion clearly in API

**Room Announce Extension:**
```c
// Current: {ROOM;127;ROOM;...;mac;name;count;max;status}
// Extended: {ROOM;127;ROOM;...;mac;name;count;max;status;directedMode}
//                                                         0 or 1
```

### Phase 5: Reception Investigation (Optional - Future)

**Objective:** Investigate if hardware MAC filtering can provide additional power savings.

**Tasks:**
1. Test `Wifi_SetPromiscuousMode(0)` with directed transmissions
2. Measure if DS still receives unicast frames addressed to its MAC
3. Measure power consumption difference
4. If successful, integrate hardware filtering into directed mode

**Note:** Phase 5 is optional. Directed transmission alone provides value (cleaner protocol, foundation for future optimizations). Hardware reception filtering is a bonus if achievable.

---

## Performance Considerations

### Power Consumption

| Mode | WiFi Power | CPU Load | Battery Impact |
|------|------------|----------|----------------|
| Promiscuous | ~80mW | High (filter all packets) | ~3-4 hours play |
| Directed | ~40mW (estimated) | Low (hardware filters) | ~5-6 hours play |

**Expected Savings:** 40-50% power reduction, 30-60 minutes additional battery life

### Network Overhead

| Mode | Packets per "Broadcast" | Notes |
|------|-------------------------|-------|
| Promiscuous | 1 (true broadcast) | Simple, efficient |
| Directed | N (one per client) | More TX, but HW can sleep between |

**Trade-off:** More transmissions, but hardware power savings outweigh transmission cost for games with 2-4 players.

### Latency

| Mode | Expected Latency | Notes |
|------|------------------|-------|
| Promiscuous | ~1-5ms | Single broadcast |
| Directed | ~2-10ms | Sequential unicasts |

**Impact:** Slightly higher latency in directed mode. Not significant for most games. Competitive/fast-paced games should stay in promiscuous mode.

---

## Testing Strategy

### Hardware Verification Tests

**Test 1: Unicast Reception**
1. DS-A sends unicast frame to DS-B's MAC
2. DS-B in non-promiscuous mode
3. Verify DS-B receives frame
4. Verify DS-C (different MAC) does NOT receive frame

**Test 2: Power Measurement**
1. Run game in promiscuous mode for 10 minutes
2. Note battery indicator / measure current if possible
3. Run same game in directed mode for 10 minutes
4. Compare results

### Protocol Tests

**Test 3: Mode Switch Synchronization**
1. Host initiates directed mode switch
2. Verify all clients receive MODESWT
3. Verify all clients send MODEACK
4. Verify synchronized switch occurs
5. Verify game continues normally

**Test 4: Late Joiner Handling**
1. Game in directed mode
2. New player attempts to join
3. Verify mode switches back to promiscuous
4. Verify new player joins successfully

**Test 5: Spectator Blocking**
1. Game in directed mode
2. Spectator attempts to join
3. Verify spectator is blocked with appropriate error
4. Verify game continues unaffected

### Stress Tests

**Test 6: Rapid Mode Switching**
1. Rapidly toggle directed mode
2. Verify no packet loss
3. Verify no desync

---

## Risk Analysis

### High Risk

| Risk | Impact | Mitigation |
|------|--------|------------|
| DS hardware doesn't support directed reception | Feature impossible | Phase 1 investigation before committing |
| Mode switch desync | Players disconnect | Robust sync protocol with timeouts |
| Spectators accidentally blocked | Bad UX | Clear error messages, room status indicator |

### Medium Risk

| Risk | Impact | Mitigation |
|------|--------|------------|
| Latency increase unacceptable | Fast games unusable | Make directed mode opt-in, document limitations |
| Power savings less than expected | Reduced value | Measure before full implementation |

### Low Risk

| Risk | Impact | Mitigation |
|------|--------|------------|
| Protocol complexity | Maintenance burden | Clean separation, good documentation |

---

## Appendix

### Code Locations Summary

**dswifi Repository:**

| File | Purpose |
|------|---------|
| `arm9/source/wifi_arm9.c` | WiFi hardware interface, promiscuous mode control |
| `arm9/source/nifi_arm9.c` | NiFi protocol, packet handling, mode management |
| `arm9/source/nifi_arm9.h` | Internal constants and structures |
| `include/dswifi9.h` | Public WiFi API |
| `include/dsnifi9.h` | Public NiFi API |

**Critical Functions:**

| Function | File | Lines | Purpose |
|----------|------|-------|---------|
| `Wifi_SetPromiscuousMode()` | wifi_arm9.c | 471-473 | Toggle promiscuous mode |
| `Wifi_RawTxFrame()` | wifi_arm9.c | ~500 | Send raw 802.11 frame |
| `IsPacketIntendedForMe()` | nifi_arm9.c | 195-297 | Software packet filtering |
| `NiFi_SendPacket()` | nifi_arm9.c | 472-495 | Send NiFi packet |
| `NiFi_SendBroadcast()` | nifi_arm9.c | 535-556 | Broadcast to all clients |

### Open Questions

1. **Hardware:** Does `Wifi_SetPromiscuousMode(0)` actually enable directed reception, or does it just disable all reception?

2. **Frame Format:** What exact 802.11 frame format is needed for unicast? Does DS WiFi hardware require specific flags?

3. **Power Measurement:** How can we accurately measure DS WiFi power consumption? Battery indicator is imprecise.

4. **Spectator Detection:** How does host know if spectators are present? Spectators don't send packets. May need periodic "spectator ping" in promiscuous mode.

---

## Conclusion

**Directed Mode** via `PACKET_MODE_NIFI_DIRECT` provides a clean path to unicast transmission, with potential power savings from hardware MAC filtering as a future optimization.

**Key Benefits:**
- Cleaner protocol (packets addressed to actual destinations)
- Foundation for hardware power savings (if reception filtering works)
- Natural integration with room locking (switch after room locked)
- Reduced broadcast spam in busy WiFi environments

**Key Challenges:**
- Completely incompatible with Spectator Mode (mutual exclusion required)
- Requires synchronized mode switching across all clients
- Hardware reception filtering unverified (Phase 5 investigation)
- More transmissions for broadcasts (N unicasts vs 1 broadcast)

**Implementation Strategy:**
1. **Phases 1-4:** Implement `PACKET_MODE_NIFI_DIRECT` for transmission - this is straightforward and provides immediate value
2. **Phase 5 (Optional):** Investigate hardware reception filtering for additional power savings

**Recommendation:** Proceed with Phases 1-4. The transmission changes are well-understood and don't require hardware investigation. Reception optimization can be explored later as a separate enhancement.

**Integration Point:** Switch to directed mode after room is locked (`NIFI_ROOM_INGAME_OPEN` or `NIFI_ROOM_INGAME_CLOSED`), when all clients are known and no new players are expected.

---

**Last Updated:** 2026-01-16
**Author:** NiFi Development Team
