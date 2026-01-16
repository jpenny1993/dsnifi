# Room Status System Feature Documentation

## Overview

**Status:** Documented (Implementation Pending)
**Complexity:** Medium-High
**Estimated Implementation Time:** 3-4 hours

The room status system provides game state management for NiFi rooms, controlling when players can join based on the current state (lobby open/closed, in-game with drop-in support). It also includes MAC-based returning player support and performance optimizations through smart packet filtering.

## Documents in This Folder

### room-status-design.md
**Type:** Design Specification
**Purpose:** Detailed design specification and implementation plan for the room status feature

**Key Sections:**
- Executive Summary
- Problem Statement
- Solution Overview
- Technical Design
- Implementation Plan (3 phases)
- Testing Strategy
- Performance Analysis
- Future Enhancements

**Read this first** to understand the feature requirements and design approach.

---

### room-status-review.md
**Type:** Design & Implementation Review
**Purpose:** Critical analysis of the implementation plan, identifying issues and risks

**Key Sections:**
- Executive Summary
- Critical Issues Identified (8 issues)
- **Spectator Mode Compatibility** (CRITICAL - highest priority)
- Code Compatibility Analysis
- Implementation Risks
- Recommended Approach (corrected phases)
- Testing Considerations
- Decision Points

**⚠️ CRITICAL:** This review identifies that MAC filtering will completely break spectator mode if not implemented with `!IsSpectatorMode` bypass. This is the highest priority issue.

**Read this after the design spec** to understand risks and corrections.

---

### room-status-implementation-guide.md
**Type:** Step-by-Step Implementation Guide
**Purpose:** Provides exact code changes and testing procedures

**Key Sections:**
- Implementation phases with exact code
- Testing procedures
- Troubleshooting
- Integration with spectator mode

**Use this** as your implementation guide, incorporating corrections from the review.

---

## Key Features

### Room States

The system defines four room states:

```c
typedef enum {
    NIFI_ROOM_LOBBY_OPEN = 0,      // Accepting new players
    NIFI_ROOM_LOBBY_CLOSED = 1,    // Lobby locked (no joins)
    NIFI_ROOM_INGAME_OPEN = 2,     // Game in progress, drop-in allowed
    NIFI_ROOM_INGAME_CLOSED = 3    // Game in progress, returning players only
} NiFiRoomStatus;
```

### State Transitions

```
LOBBY_OPEN
    ↓ Host locks lobby
LOBBY_CLOSED
    ↓ Game starts
INGAME_CLOSED (or INGAME_OPEN)
    ↓ Game ends
LOBBY_OPEN
```

### Core Capabilities

- ✅ **State Management** - Host controls room state via API
- ✅ **Join Control** - Automatic validation based on current state
- ✅ **Returning Player Support** - MAC-based player identification
- ✅ **Performance Optimization** - MAC filtering saves ~30% CPU during games
- ✅ **Spectator Compatibility** - MAC filtering bypasses spectators
- ✅ **Backward Compatibility** - Old clients default to LOBBY_OPEN

## Implementation Status

- [x] Design specification completed
- [x] Design review completed
- [x] Implementation guide completed
- [ ] Code implementation (pending)
- [ ] Unit tests (pending)
- [ ] Integration tests (pending)
- [ ] Spectator integration tests (pending)

## Dependencies

### Critical Dependency: Spectator Mode Feature

**⚠️ HIGHEST PRIORITY ISSUE:** MAC-based packet filtering MUST check `!IsSpectatorMode` or it will completely break spectator mode functionality.

**The Problem:**
- Room status filters packets from unknown MACs during INGAME states (performance optimization)
- Spectators have unknown MACs (they discover clients dynamically)
- Without bypass, spectators receive ZERO packets and cannot function

**The Solution:**
```c
// In IsPacketIntendedForMe() - REQUIRED MODIFICATION
if (!IsSpectatorMode &&  // ← CRITICAL: Bypass for spectators
    (currentRoomStatus == NIFI_ROOM_INGAME_OPEN ||
     currentRoomStatus == NIFI_ROOM_INGAME_CLOSED)) {

    // Filter unknown MACs (active mode only)
    if (IndexOfClientUsingMacAddress(macAddress) == INDEX_UNKNOWN) {
        return false;
    }
}
// Spectators bypass MAC filtering completely
```

**Implementation Order Options:**
1. **Option A (RECOMMENDED):** Implement spectator mode first, then room status with bypass
2. **Option B:** Implement room status with `bool IsSpectatorMode = false;` placeholder, then spectator mode

**Reference:** See `room-status-review.md` Section "Spectator Mode Compatibility"

## API Summary

### New Functions

```c
// Set current room status (host only)
void NiFi_SetRoomStatus(NiFiRoomStatus status);

// Get current room status
NiFiRoomStatus NiFi_GetRoomStatus(void);

// Check if a player can join based on current status
bool NiFi_CanPlayerJoin(char macAddress[MAC_ADDRESS_LENGTH]);

// Set packet processing rate (optional performance tuning)
void NiFi_SetPacketRate(u16 packetsPerSecond);
```

### Modified Structures

```c
// NiFiRoom struct gains a status field
typedef struct {
    char macAddress[MAC_ADDRESS_LENGTH];
    char roomName[PROFILE_NAME_LENGTH];
    u8 roomSize;
    u8 memberCount;
    NiFiRoomStatus status;  // NEW
} NiFiRoom;
```

## Files Modified

### Header Files
- `/mnt/c/nds/repo/dswifi/include/dsnifi9.h` - Public API (4 new functions, NiFiRoomStatus enum)

### Source Files
- `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c` - Main implementation (~300 lines changed)

### Critical Functions Modified
- `IsPacketIntendedForMe()` - Add MAC filtering with spectator bypass
- `NiFi_CreateRoom()` - Initialize room status to LOBBY_OPEN
- `HandlePacketAsHost()` - Validate joins with `NiFi_CanPlayerJoin()`
- `HandlePacketAsClient()` - Process ROOM_STATUS packets
- `SetupNiFiClient()` - Enhanced to prefer returning player's old slot

## Testing Plan

### Unit Tests
1. **Basic Status Transitions** (15 min) - State changes work correctly
2. **MAC-Based Rejoining** (20 min) - Returning players can rejoin during INGAME_CLOSED
3. **Slot Conflict** (15 min) - First-come-first-served when slot occupied
4. **Packet Filtering Performance** (10 min) - Unknown MACs filtered during game
5. **Room Discovery During Game** (10 min) - Discovery still works in INGAME states
6. **Timer Frequency** (15 min) - Different packet rates work correctly

### Integration Tests
7. **Spectator + LOBBY_OPEN** (10 min) - Spectator discovers and observes open lobby
8. **Spectator + INGAME_CLOSED** (15 min) - Spectator observes game without being blocked
9. **Room Status Transition While Spectating** (15 min) - Spectator unaffected by state changes
10. **MAC Filtering Verification** (15 min) - Filtering blocks new players but NOT spectators

### Critical Tests
- **Test 8:** Verifies MAC filtering doesn't break spectator mode
- **Test 10:** Verifies MAC filtering works correctly for both active and spectator modes

## Critical Issues from Review

### Issue 1: Packet Filtering Location ⚠️ HIGH PRIORITY
- **Problem:** Original plan suggested filtering in `OnRawPacketReceived()` where packets aren't decoded yet
- **Solution:** Filter in `IsPacketIntendedForMe()` instead (packets are decoded there)
- **Action:** Updated in implementation guide

### Issue 2: GAME_START Auto-Detection ⚠️ HIGH PRIORITY
- **Problem:** Cannot auto-detect arbitrary command names like "GAME_START"
- **Solution:** Use explicit `NiFi_SetRoomStatus()` calls instead
- **Action:** Application code must explicitly set status

### Issue 3: Spectator Mode Compatibility 🔴 CRITICAL
- **Problem:** MAC filtering will break spectator mode without bypass
- **Solution:** Add `!IsSpectatorMode` check to filtering logic
- **Action:** MUST implement before deploying room status

### Issue 4: Timer Frequency Impact 🟡 MEDIUM
- **Problem:** Changing from 240Hz to 60Hz affects all timing
- **Solution:** Test thoroughly, allow override via `NiFi_SetPacketRate()`
- **Action:** Measure performance on hardware

### Issue 5: SetupNiFiClient Slot Reuse 🟡 MEDIUM
- **Problem:** Current implementation doesn't prefer returning player's old slot
- **Solution:** Check for MAC match first, reuse that slot if empty, fallback to any slot
- **Action:** Enhanced implementation in guide

## Implementation Risks

### 🔴 HIGH RISK
1. **Spectator Mode Compatibility** - MAC filtering will completely break spectators
   - **Mitigation:** Add `!IsSpectatorMode` bypass (REQUIRED)
   - **Reference:** `room-status-review.md` detailed analysis

### 🟡 MEDIUM RISK
2. **Timer Frequency Impact** - 240Hz → 60Hz affects all timing
   - **Mitigation:** Test extensively, document, allow override
3. **SetupNiFiClient Slot Reuse** - Duplicate MACs possible
   - **Mitigation:** Enhanced slot selection logic

### 🟢 LOW RISK
4. **MAC Persistence** - Intentional design (not a bug)
5. **CreateRoom Initialization** - Explicit set for clarity

## Recommended Implementation Approach

### Phase 0: Pre-Implementation (30 min)
1. Create test branch
2. Document current timer behavior
3. Review plan with corrections from review document

### Phase 1: Minimal Viable Implementation (1-2 hours)
1. Add data structures (NiFiRoomStatus enum, status field)
2. Add function declarations
3. Implement core functions (SetRoomStatus, GetRoomStatus, CanPlayerJoin)
4. Update room creation, join handling
5. **Enhance SetupNiFiClient for slot reuse**

**MILESTONE:** Basic room status works

### Phase 2: Packet Filtering (30 min)
1. Add MAC filtering to IsPacketIntendedForMe()
2. **CRITICAL:** Include `!IsSpectatorMode` check

**MILESTONE:** MAC filtering works, spectators not blocked

### Phase 3: Timer Frequency (30 min)
1. Change default to 60Hz
2. Implement SetPacketRate()

**MILESTONE:** Timer control works

## Quick Start

1. **Read the design specification** (`room-status-design.md`) to understand the feature
2. **Read the review document** (`room-status-review.md`) to understand critical issues (especially spectator compatibility)
3. **Read spectator mode documentation** (`docs/planning/spectator-mode/`) to understand compatibility requirements
4. **Follow the implementation guide** (`room-status-implementation-guide.md`) step by step
5. **Test thoroughly** including integration tests with spectator mode

## Decision Points

### Decision 1: GAME_START Handling
**Recommendation:** Use explicit `NiFi_SetRoomStatus()` calls (not auto-detection)
**Rationale:** Flexible, explicit, matches library design

### Decision 2: Packet Filtering Location
**Recommendation:** Filter in `IsPacketIntendedForMe()` (not `OnRawPacketReceived()`)
**Rationale:** Packets are decoded there, consistent with existing pattern

### Decision 3: MAC Persistence
**Recommendation:** Document current behavior (don't change code)
**Rationale:** Intentional design for returning player support

### Decision 4: Backward Compatibility
**Recommendation:** Maintain compatibility (default to LOBBY_OPEN if missing)
**Rationale:** Graceful degradation, prevents hard failures

## Need Help?

- **Feature Overview:** See `room-status-design.md`
- **Critical Issues:** See `room-status-review.md`
- **Implementation Details:** See `room-status-implementation-guide.md`
- **Spectator Compatibility:** See `spectator-mode/` folder and `room-status-review.md` Section "Spectator Mode Compatibility"
- **General Documentation:** See `docs/HOW-TO-DOCUMENT-FEATURES.md`

---

**Last Updated:** 2025-01-22
**Next Steps:**
1. Read spectator mode documentation
2. Understand `!IsSpectatorMode` bypass requirement
3. Begin Phase 0 preparation
