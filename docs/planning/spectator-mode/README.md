# Spectator Mode Feature Documentation

## Overview

**Status:** Documented (Implementation Pending)
**Complexity:** High
**Estimated Implementation Time:** 8-10 hours

Spectator mode allows a Nintendo DS to passively observe an active NiFi game without joining or transmitting any packets. Spectators operate in promiscuous WiFi mode and discover clients dynamically by observing packet traffic.

## Documents in This Folder

### spectator-mode-design.md
**Type:** Design Specification
**Purpose:** Defines WHAT spectator mode is and WHY we're building it

**Key Sections:**
- Executive Summary
- Feature Requirements (FR1-FR6, NFR1-NFR3)
- Technical Architecture with diagrams
- Packet Filtering Strategy
- State Management
- API Design (5 new functions)
- Implementation Plan (6 phases)
- Performance Considerations
- Testing Strategy
- Security and Privacy
- Future Enhancements

**Read this first** to understand the architecture and design decisions.

---

### spectator-mode-implementation-guide.md
**Type:** Step-by-Step Implementation Guide
**Purpose:** Provides EXACT instructions for HOW to implement spectator mode

**Key Sections:**
- Room Status Compatibility (CRITICAL - read this section!)
- Phase 1: Core Infrastructure (2 hours)
- Phase 2: Packet Filtering (1.5 hours)
- Phase 3: Room Discovery (1.5 hours)
- Phase 4: Client Discovery (1.5 hours)
- Phase 5: Transmission Blocking (0.5 hours)
- Phase 6: Testing and Validation (2 hours)
- Troubleshooting Guide
- Quick Reference

**Use this** as your step-by-step guide during implementation.

---

## Key Features

### Core Capabilities
- ✅ **Zero Network Footprint** - Spectators never transmit packets
- ✅ **Room Discovery** - Passive listening for room announcements
- ✅ **Room Targeting** - Select specific room to observe
- ✅ **Client Discovery** - Learn about players by observing traffic
- ✅ **Event Handlers** - OnClientConnected, OnPositionUpdated, OnGamePacket, etc.
- ✅ **Host Migration** - Automatically tracks host changes

### Technical Details
- **Mode:** Promiscuous WiFi mode (receive all packets on channel)
- **Filtering:** By game ID and room ID
- **Battery Impact:** 20-40% higher consumption (acceptable for tournaments)
- **Compatibility:** Works with existing NiFi protocol (no wire format changes)

## Implementation Status

- [x] Design specification completed
- [x] Implementation guide completed
- [ ] Code implementation (pending)
- [ ] Unit tests (pending)
- [ ] Integration tests (pending)
- [ ] Performance validation (pending)

## Dependencies

### Critical Dependency: Room Status Feature

**IMPORTANT:** The room status feature includes MAC-based packet filtering that will **completely break spectator mode** if not implemented correctly.

**Required:** Room status filtering MUST check `!IsSpectatorMode` before rejecting packets from unknown MACs.

**Reference:** See `room-status-review.md` Section "Spectator Mode Compatibility"

**Implementation Order:** Either:
1. Implement spectator mode first, then room status with bypass built in (RECOMMENDED), OR
2. Implement room status with `!IsSpectatorMode` placeholder, then spectator mode

## API Summary

### New Functions

```c
// Initialize spectator mode and start scanning for rooms
bool NiFi_StartSpectating(int wifiChannel, const char *gameId);

// Select a specific room to observe
bool NiFi_SpectateRoom(NiFiRoom room);

// Stop spectator mode and disable WiFi
void NiFi_StopSpectating(void);

// Check if currently in spectator mode
bool NiFi_IsSpectating(void);

// Get the list of discovered rooms during scanning
int NiFi_GetDiscoveredRooms(NiFiRoom *rooms);
```

### Event Handlers (Existing handlers work in spectator mode)

- `NiFi_OnRoomAnnounced()` - Fired during room scanning
- `NiFi_OnClientConnected()` - Fired when client discovered
- `NiFi_OnClientDisconnected()` - Fired when client times out
- `NiFi_OnPositionUpdated()` - Fired for position updates
- `NiFi_OnGamePacket()` - Fired for custom packets
- `NiFi_OnHostMigration()` - Fired when host changes

## Files Modified

### Header Files
- `/mnt/c/nds/repo/dswifi/include/dsnifi9.h` - Public API declarations (5 new functions)
- `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.h` - Internal structures (SpectatorState)

### Source Files
- `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c` - Main implementation (~550 lines changed)

### Critical Functions Modified
- `IsPacketIntendedForMe()` - Bypass client ID check for spectators
- `ProcessIncomingPackets()` - Add spectator client tracking
- `SendAcknowledgement()` - Block in spectator mode
- `NiFi_SendPacket()` - Block in spectator mode
- `NiFi_QueueBroadcast()` - Block in spectator mode

## Testing Plan

### Test Categories
1. **Functional Tests** - Basic lifecycle, room discovery, client discovery
2. **Integration Tests** - Interaction with active players, host migration
3. **Edge Cases** - Packet loss, mid-game entry, room switching
4. **Performance Tests** - Battery consumption, CPU usage
5. **Compatibility Tests** - With room status feature (Tests 7-10 in guide)

### Critical Tests
- **Test 6.10:** Zero network footprint verification (Wireshark capture)
- **Test 6.16:** Spectator + Room Status integration (MAC filtering bypass)

## Known Limitations

1. **Room Discovery Limitation:**
   - Spectators don't send CMD_ROOM_SEARCH packets (passive only)
   - Can only discover rooms by "overhearing" announcements to other devices
   - **Workaround:** Have at least one scanning device present, or provide host MAC in advance

2. **Mid-Game State Sync:**
   - Spectators entering mid-game have incomplete initial state
   - State converges gradually as packets are observed
   - **Enhancement:** Add optional state synchronization packets (future work)

3. **Battery Consumption:**
   - Promiscuous mode increases power draw by 20-40%
   - Expected battery life: 2-3 hours (vs 3-4 hours active mode)
   - **Acceptable:** For tournament/coaching use cases

## Future Enhancements

See design document section "Future Enhancements" for details on:
1. Spectator limit enforcement (optional join protocol)
2. Spectator chat channel
3. State synchronization packets
4. Replay recording to SD card
5. Multi-room spectating

## Quick Start

1. **Read the design document** to understand the architecture
2. **Read the Room Status Compatibility section** in the implementation guide (CRITICAL)
3. **Follow the implementation guide** phase by phase
4. **Test after each phase** using the testing checkpoints
5. **Run integration tests** with room status feature

## Need Help?

- **Design Questions:** See `spectator-mode-design.md`
- **Implementation Questions:** See `spectator-mode-implementation-guide.md`
- **Compatibility Issues:** See `room-status-review.md` (in room-status folder)
- **General Documentation:** See `docs/HOW-TO-DOCUMENT-FEATURES.md`

---

**Last Updated:** 2025-01-22
**Next Steps:** Begin Phase 1 implementation or review room status compatibility
