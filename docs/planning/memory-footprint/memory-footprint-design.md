# Memory Footprint Optimization - Design Specification

**Version:** 1.0
**Date:** 2025-11-22
**Status:** Design Phase
**Complexity:** Medium
**Estimated Implementation:** 4-8 hours

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Current State Analysis](#current-state-analysis)
3. [Optimization Goals](#optimization-goals)
4. [Proposed Changes](#proposed-changes)
5. [Trade-Off Analysis](#trade-off-analysis)
6. [Use Case Validation](#use-case-validation)
7. [Backward Compatibility](#backward-compatibility)
8. [Success Criteria](#success-criteria)
9. [Related Documents](#related-documents)

---

## Executive Summary

The NiFi library currently uses **~14.4 KB** of static memory for packet buffers and protocol management. While this represents only 0.35% of the Nintendo DS's 4MB RAM, reducing this footprint by 37-53% would provide game developers with additional memory for sprites, maps, audio, and gameplay logic.

This design proposes a **multi-phase optimization strategy** that reduces memory usage to **7-9 KB** through:
- **Phase 1** (Recommended): Parameter length reduction (32→16 chars) - **5.4 KB saved**
- **Phase 2** (Optional): Buffer capacity tuning - **2.2 KB additional saved**

The optimizations maintain protocol compatibility while introducing breaking changes for games that rely on large data payloads (>16 characters per parameter).

---

## Current State Analysis

### Memory Breakdown (Total: ~14.4 KB)

Based on analysis of `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c` and `/mnt/c/nds/repo/dswifi/include/dsnifi9.h`:

| Component | Size | Percentage | Notes |
|-----------|------|-----------|-------|
| **Outgoing Packet Buffer** | 6,876 bytes | 47.7% | 18 slots × 382 bytes/packet |
| **Incoming Packet Buffer** | 4,584 bytes | 31.8% | 12 slots × 382 bytes/packet |
| **WiFi Receive Buffers** | 3,072 bytes | 21.3% | 3 × 1,024 byte buffers |
| **Decode Buffer** | 448 bytes | 3.1% | 14 params × 32 chars |
| **Client Array** | 156 bytes | 1.1% | 6 clients × 26 bytes |
| **Handlers & State** | ~120 bytes | 0.8% | Function pointers & flags |
| **TOTAL** | **~14,400 bytes** | **100%** | **0.35% of 4MB RAM** |

### Packet Structure Analysis

Each `NiFiPacket` structure (defined in `dsnifi9.h`) contains:

```c
typedef struct {
    bool isProcessed;               // 1 byte
    bool isAcknowledgement;         // 1 byte
    u16 messageId;                  // 2 bytes
    u8 timeToLive;                  // 1 byte
    u8 toClientId;                  // 1 byte
    u8 fromClientId;                // 1 byte
    char command[COMMAND_LENGTH];   // 9 bytes
    char macAddress[MAC_ADDRESS_LENGTH]; // 13 bytes
    char data[6][READ_PARAM_LENGTH]; // 6 × 32 = 192 bytes ← KEY TARGET
} NiFiPacket;
```

**Total per packet**: ~222 bytes (padded to ~382 bytes with alignment)

### Critical Finding

**The `data[6][READ_PARAM_LENGTH]` array accounts for 192 bytes per packet** (86% of packet size). With 30 total buffered packets (12 incoming + 18 outgoing), this represents **5,760 bytes of parameter storage**.

---

## Optimization Goals

### Primary Goal
Reduce NiFi library memory footprint by **40-50%** through targeted optimizations with minimal impact on functionality.

### Secondary Goals
1. Maintain protocol compatibility with existing deployments
2. Preserve network reliability characteristics
3. Avoid breaking core library functionality
4. Provide opt-in migration path for games

### Target Memory Usage

| Scenario | Memory Usage | Savings | Status |
|----------|--------------|---------|--------|
| **Current** | 14.4 KB | - | Baseline |
| **Phase 1 Target** | 9.0 KB | 5.4 KB (37.5%) | Recommended |
| **Phase 2 Target** | 7.7 KB | 6.7 KB (46.5%) | Optional |
| **Aggressive Target** | 6.8 KB | 7.6 KB (52.8%) | High Risk |

---

## Proposed Changes

### Phase 1: Parameter Length Reduction ⭐ **RECOMMENDED**

**File**: `/mnt/c/nds/repo/dswifi/include/dsnifi9.h`

```c
// Current (Line ~7)
#define READ_PARAM_LENGTH 32U

// Proposed
#define READ_PARAM_LENGTH 16U
```

**Impact**:
- **Memory Saved**: 5,400 bytes (37.5% reduction)
- **New Footprint**: ~9,000 bytes
- **Risk Level**: **LOW**
- **Breaking Change**: **YES** (affects games using >16 char parameters)

**Rationale**:
- Most game data fits in 16 characters (scores, IDs, positions, short messages)
- Packet overhead is dominated by data payload
- Single constant change with massive impact
- Easy to test and validate

---

### Phase 2: Outgoing Buffer Capacity Reduction (Optional)

**File**: `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`

```c
// Current (Line ~26)
#define OUTGOING_PACKET_BUFFER_CAPACITY 18

// Proposed
#define OUTGOING_PACKET_BUFFER_CAPACITY 12
```

**Impact**:
- **Memory Saved**: 1,332 bytes (9.2% additional)
- **New Footprint**: ~7,700 bytes (combined with Phase 1)
- **Risk Level**: **MEDIUM**
- **Breaking Change**: **NO** (internal buffer management)

**Rationale**:
- Reduces retry slots from 18 to 12 (still 6 retries with 2-second TTL)
- May impact high-latency scenarios or lossy WiFi
- Acceptable for most turn-based and strategy games
- Can be tuned per-game if needed

---

### Phase 3: Incoming Buffer Capacity Reduction (High Risk)

**File**: `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`

```c
// Current (Line ~25)
#define INCOMING_PACKET_BUFFER_CAPACITY 12

// Proposed
#define INCOMING_PACKET_BUFFER_CAPACITY 8
```

**Impact**:
- **Memory Saved**: 888 bytes (6.2% additional)
- **New Footprint**: ~6,800 bytes (combined with Phase 1+2)
- **Risk Level**: **HIGH**
- **Breaking Change**: **NO**

**Rationale**:
- Reduces circular buffer overflow tolerance
- Higher risk of dropped packets during bursts
- Only recommended for low-traffic games
- **NOT RECOMMENDED** for general use

---

## Trade-Off Analysis

### Phase 1: Parameter Length (32→16)

#### Advantages ✅
- **Massive memory savings**: 5.4 KB (37.5%)
- **Simple change**: Single constant modification
- **Easy validation**: String truncation observable
- **Backward wire-compatible**: Older clients parse 16-char fields normally

#### Disadvantages ⚠️
- **Breaking change**: Games using >16 chars must update
- **Data limitations**: Cannot send long strings (chat messages, JSON)
- **Migration effort**: Existing games need protocol version check

#### Use Cases Affected
**Still Supported** (✅ 16 chars sufficient):
- Player scores: `"1234"` (4 chars)
- Player IDs: `"PLAYER_01"` (9 chars)
- Coordinates: `"128,64,32"` (9 chars)
- Short commands: `"START_GAME"` (10 chars)
- Item codes: `"ITEM_SWORD_05"` (14 chars)

**No Longer Supported** (❌ Requires >16 chars):
- Long chat messages: `"Hello everyone, ready to start?"` (32 chars)
- Full JSON payloads: `{"x":128,"y":64,"hp":100}` (24 chars)
- Large data blobs: Base64-encoded sprites, maps

#### Mitigation Strategies
1. **Multi-packet approach**: Split large data across multiple packets
2. **Abbreviations**: Use compact encoding (e.g., `"P1:128,64"` instead of `"Player1 at position 128,64"`)
3. **Custom serialization**: Binary encoding instead of text
4. **Documentation**: Clearly document 16-char limit in API docs

---

### Phase 2: Outgoing Buffer Reduction (18→12)

#### Advantages ✅
- **Additional savings**: 1.3 KB
- **Still 6 retries**: Maintains reliability for most cases
- **No API changes**: Internal buffer management

#### Disadvantages ⚠️
- **Reduced retry capacity**: High-traffic games may exhaust slots
- **Latency sensitivity**: Slower ACKs mean fewer available slots
- **Burst handling**: Large simultaneous broadcasts may overflow

#### Risk Scenarios
**Low Risk** (✅ Works well):
- Turn-based games (low packet rate)
- Puzzle games (infrequent updates)
- Strategy games (player-paced)

**Medium Risk** (⚠️ Test carefully):
- Real-time action games (frequent position updates)
- Racing games (constant velocity broadcasts)
- Fighting games (combo/input synchronization)

**High Risk** (❌ Not recommended):
- MMO-style games (many players, high traffic)
- Fast-paced shooters (60Hz position updates)
- Games with complex state synchronization

---

### Phase 3: Incoming Buffer Reduction (12→8)

#### Advantages ✅
- **Additional savings**: 0.9 KB
- **Marginal improvement**: Completes optimization

#### Disadvantages ⚠️
- **High overflow risk**: Circular buffer wraps faster
- **Burst packet loss**: Multiple simultaneous senders problematic
- **Processing delays**: Slow handlers cause overruns

**Recommendation**: **DO NOT IMPLEMENT** unless memory is absolutely critical and game traffic is predictable.

---

## Use Case Validation

### Test Case 1: Racing Game (Real-Time)
**Typical Data Payloads**:
- Position: `"128,64"` (6 chars) ✅
- Velocity: `"5.2,-3.1"` (8 chars) ✅
- Lap/Time: `"3/125.45"` (8 chars) ✅
- Item pickup: `"BOOST"` (5 chars) ✅

**Verdict**: **Phase 1 SAFE** | **Phase 2 MEDIUM RISK** (high packet rate)

---

### Test Case 2: Turn-Based RPG
**Typical Data Payloads**:
- Action: `"ATTACK"` (6 chars) ✅
- Target: `"ENEMY_02"` (8 chars) ✅
- Damage: `"42"` (2 chars) ✅
- Status: `"POISONED"` (8 chars) ✅

**Verdict**: **Phase 1 SAFE** | **Phase 2 SAFE** (low packet rate)

---

### Test Case 3: Multiplayer Chat/Social
**Typical Data Payloads**:
- Short message: `"Hey! Ready?"` (12 chars) ✅
- Long message: `"Anyone want to trade items?"` (28 chars) ❌

**Verdict**: **Phase 1 RISKY** (need multi-packet chat)

---

### Test Case 4: Puzzle Game (Casual)
**Typical Data Payloads**:
- Move: `"R3C2"` (4 chars) ✅
- Score: `"1500"` (4 chars) ✅
- Combo: `"5x"` (2 chars) ✅

**Verdict**: **Phase 1 SAFE** | **Phase 2 SAFE** (very low packet rate)

---

## Backward Compatibility

### Wire Protocol Compatibility

**Question**: Can old clients (32-char) talk to new clients (16-char)?

**Answer**: **YES** ✅

**Explanation**:
- Protocol uses semicolon-delimited strings: `{GID;RID;CMD;...;DATA1;DATA2;...}`
- Parsing reads until `;` delimiter, not fixed 32-char boundaries
- Old client sends 32-char field: `"PLAYER_NAME_VERY_LONG_STRING"`
- New client receives and parses: Reads up to `;` (uses first 16 chars if truncated)
- New client sends 16-char field: `"PLAYER_NAME____"` (padded or truncated)
- Old client receives and parses: Works normally (compatible)

### Breaking Changes

**What Breaks**:
1. Games that send >16 char data in `NiFiPacket.data[]` fields
2. Custom protocols expecting full 32-char capacity
3. Base64-encoded data or JSON payloads

**What Doesn't Break**:
- Position updates (coordinates)
- Scores and stats
- Player IDs and names (up to 10 chars already)
- Commands (already limited to 8 chars)
- MAC addresses (13 chars, separate field)

### Migration Path

**For New Games**:
- Use 16-char parameter design from start
- Document in API that `READ_PARAM_LENGTH = 16`

**For Existing Games**:
1. **Option A**: Accept truncation (safest, least work)
2. **Option B**: Split large data across multiple packets
3. **Option C**: Compress/abbreviate data to fit 16 chars
4. **Option D**: Stay on old library version (pre-optimization)

---

## Success Criteria

### Functional Requirements
- [ ] All existing nifitest demo features work correctly
- [ ] Position updates transmit and receive successfully
- [ ] Custom game packets delivered reliably
- [ ] Room creation, joining, and disconnection functional
- [ ] Host migration works without errors

### Performance Requirements
- [ ] Memory usage reduced by ≥5 KB (Phase 1 target)
- [ ] Packet transmission latency unchanged (±5ms tolerance)
- [ ] No increase in packet loss rate
- [ ] Circular buffer overflow rate ≤5% (Phase 2 only)

### Compatibility Requirements
- [ ] Old clients (32-char) can join new host (16-char) rooms
- [ ] New clients (16-char) can join old host (32-char) rooms
- [ ] Mixed client versions in same room communicate correctly
- [ ] No protocol-level errors or crashes

### Testing Requirements
- [ ] Unit tests for parameter length validation
- [ ] Integration tests with 2-6 clients
- [ ] Stress tests with high packet rates
- [ ] Long-running stability tests (1+ hour sessions)
- [ ] Memory profiling confirms savings

---

## Related Documents

- [Memory Footprint Review](memory-footprint-review.md) - Risk assessment and testing strategy
- [Memory Footprint Implementation Guide](memory-footprint-implementation-guide.md) - Step-by-step instructions
- [ADR 004: Memory Footprint Optimization](../../architecture/adr/004-memory-footprint-optimization.md) - Architectural rationale
- [ADR 001: NiFi Protocol Implementation](../../architecture/adr/001-nifi-protocol-implementation.md) - Core protocol design
- [ARCHITECTURE.md](../../../ARCHITECTURE.md) - Protocol specification

---

**Document Status**: Complete
**Next Step**: Proceed to review phase (memory-footprint-review.md)
**Approval**: Pending
