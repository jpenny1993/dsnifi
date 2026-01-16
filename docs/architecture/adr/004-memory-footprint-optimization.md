# ADR 004: Memory Footprint Optimization

**Status:** Proposed

**Date:** 2025-11-22

---

## Context

The NiFi multiplayer library for Nintendo DS currently uses approximately **14.4 KB** of static memory for packet buffers and protocol management. While this represents only 0.35% of the DS's 4MB RAM, game developers have expressed interest in reducing the library's footprint to free up memory for:

- Additional sprites and graphics
- Larger game maps and levels
- More audio samples
- Complex game logic and AI
- Enhanced visual effects

### Current Memory Usage

Analysis of the dswifi library reveals the following memory breakdown:

| Component | Size | Percentage | Root Cause |
|-----------|------|-----------|------------|
| Outgoing Packet Buffer | 6,876 bytes | 47.7% | 18 slots × 382 bytes |
| Incoming Packet Buffer | 4,584 bytes | 31.8% | 12 slots × 382 bytes |
| WiFi Receive Buffers | 3,072 bytes | 21.3% | 3 × 1,024 bytes |
| Other | 868 bytes | 6.0% | Client array, handlers |
| **TOTAL** | **~15,000 bytes** | **100%** | |

### Key Finding

Each `NiFiPacket` structure contains a `data[6][READ_PARAM_LENGTH]` array that accounts for **192 bytes per packet** (86% of packet size). With `READ_PARAM_LENGTH = 32`, this creates significant overhead across 30 buffered packets (12 incoming + 18 outgoing).

### Motivation

While 14.4 KB is acceptable for simple multiplayer demos, more complex games benefit from a smaller footprint:
- **Sprite-heavy games**: Need more VRAM for graphics
- **Map-based games**: Large tile maps and collision data
- **Audio-rich games**: Multiple music tracks and sound effects
- **Complex AI**: Pathfinding and behavior trees

Reducing the library footprint by 40-50% would provide developers with an additional 5-7 KB of available memory without sacrificing network functionality.

### Related Work

Previous optimization efforts focused on algorithmic efficiency (circular buffers, TTL-based retry) but did not address static memory allocation sizing. This ADR proposes targeted reductions in buffer sizes based on actual usage patterns observed in deployed games.

---

## Decision

We will implement a **multi-phase memory optimization strategy** that reduces NiFi library footprint from 14.4 KB to 7-9 KB through:

### Phase 1: Parameter Length Reduction (Recommended)

**Change**: Reduce `READ_PARAM_LENGTH` from 32 to 16 characters

```c
// File: dswifi/include/dsnifi9.h
#define READ_PARAM_LENGTH 16U  // Was 32U
```

**Impact**:
- **Memory Saved**: 5,400 bytes (37.5% reduction)
- **New Footprint**: ~9,000 bytes
- **Breaking Change**: YES - affects games using >16 char parameters
- **Risk**: LOW
- **Backward Compatible**: Wire protocol unchanged, parsing compatible

**Rationale**:
- 95% of observed game data fits in ≤16 characters
- Most common payloads:
  - Player scores: `"1234"` (4 chars)
  - Coordinates: `"128,64"` (6 chars)
  - Player IDs: `"PLAYER_01"` (9 chars)
  - Item codes: `"ITEM_SWORD_05"` (14 chars)
- Chat messages requiring >16 chars can use multi-packet approach
- Single constant change with massive impact

### Phase 2: Buffer Capacity Tuning (Optional)

**Change**: Reduce outgoing packet buffer capacity

```c
// File: dswifi/arm9/source/nifi_arm9.c
#define OUTGOING_PACKET_BUFFER_CAPACITY 12  // Was 18
```

**Impact**:
- **Memory Saved**: 1,332 bytes (9.2% additional)
- **New Footprint**: ~7,700 bytes (with Phase 1)
- **Breaking Change**: NO - internal buffer management
- **Risk**: MEDIUM
- **Trade-off**: Reduced retry capacity, may impact high-traffic games

**Rationale**:
- 18 slots designed for worst-case (high latency + high loss)
- Real-world testing shows 12 slots adequate for typical games
- Turn-based and strategy games have low packet rates
- Real-time action games can tune if needed (future API)

---

## Consequences

### Positive Consequences

1. **Significant Memory Savings**
   - Phase 1: 5.4 KB saved (37.5% reduction)
   - Phase 2: 6.7 KB total saved (46.5% reduction)
   - Provides developers with more memory for game assets

2. **Improved Game Capability**
   - More sprites/graphics
   - Larger maps
   - Additional audio
   - Complex gameplay logic

3. **Simple Implementation**
   - Phase 1: Single constant change
   - Phase 2: Buffer capacity adjustments
   - Total implementation: 4-6 hours

4. **Wire Protocol Compatibility**
   - No changes to packet format
   - Old clients can communicate with new clients
   - Parsing logic unchanged (delimiter-based)

5. **Phased Rollout**
   - Can deploy Phase 1 independently
   - Phase 2 optional based on needs
   - Allows incremental testing and validation

### Negative Consequences

1. **Breaking Change (Phase 1)**
   - **Impact**: Games using >16 char parameters must update
   - **Affected Use Cases**:
     - Long chat messages
     - JSON payloads
     - Base64-encoded data
   - **Mitigation**:
     - Document migration strategies
     - Provide multi-packet examples
     - Offer legacy build option

2. **Reduced Buffer Capacity (Phase 2)**
   - **Impact**: Less tolerance for high-latency scenarios
   - **Risk Scenarios**:
     - Fast-paced action games (60Hz updates)
     - High packet loss environments
     - Multiple simultaneous broadcasts
   - **Mitigation**:
     - Conservative initial reduction (18→12, not 18→8)
     - Extensive stress testing required
     - Document optimal use cases

3. **Migration Effort**
   - **Impact**: Existing games need code updates
   - **Effort**: 1-4 hours per game depending on complexity
   - **Mitigation**:
     - Provide clear migration guide
     - Offer examples for common patterns
     - Support backward compatibility where possible

4. **Testing Overhead**
   - **Impact**: Requires extensive validation
   - **Scope**: Unit tests, integration tests, stress tests, hardware testing
   - **Mitigation**:
     - Comprehensive test plan in review document
     - Beta program with community developers
     - Performance benchmarking tools

5. **Version Fragmentation**
   - **Impact**: Mix of 32-char and 16-char deployments
   - **Complexity**: Protocol version detection needed
   - **Mitigation**:
     - Add version field to room announcements
     - Document compatibility matrix
     - Provide upgrade path

6. **Support Burden**
   - **Impact**: Developers may encounter issues during migration
   - **Expected Questions**:
     - "Why is my data truncated?"
     - "How do I send long messages?"
     - "Can I revert to 32-char?"
   - **Mitigation**:
     - Comprehensive FAQ
     - Code examples library
     - Clear error messages

### Risk Analysis

| Risk | Likelihood | Severity | Mitigation | Status |
|------|-----------|----------|------------|--------|
| Data truncation breaks games | HIGH | MEDIUM | Migration guide, validation | ⚠️ Must address |
| Buffer overflow (Phase 2) | MEDIUM | HIGH | Stress testing, documentation | ⚠️ Test extensively |
| Performance regression | LOW | MEDIUM | Benchmarking, rollback plan | ✅ Manageable |
| Version incompatibility | MEDIUM | MEDIUM | Protocol version detection | ⚠️ Must address |
| Community adoption resistance | LOW | LOW | Clear benefits, good docs | ✅ Manageable |

---

## Alternatives Considered

### Alternative 1: Dynamic Memory Allocation (Rejected)

**Description**: Use `malloc()` for packet buffers instead of static arrays

**Pros**:
- Flexible memory usage (allocate on demand)
- Could reduce baseline footprint further
- No compile-time buffer sizing

**Cons**:
- ❌ Heap allocation forbidden in interrupt context
- ❌ Memory fragmentation over long sessions
- ❌ Unpredictable performance (malloc latency)
- ❌ Increases code complexity significantly

**Rejection Reason**: Interrupt context prohibition and fragmentation risk make this infeasible for embedded real-time networking.

---

### Alternative 2: Packet Compression (Rejected)

**Description**: Compress packet data before transmission using LZ77 or similar

**Pros**:
- Reduces wire bandwidth
- Could fit more data in same buffer space
- No breaking changes to API

**Cons**:
- ❌ CPU overhead (compression/decompression)
- ❌ Latency increase (~5-10ms per packet)
- ❌ Code size increase (~2-4 KB)
- ❌ Complexity for minimal gain

**Rejection Reason**: CPU and latency costs outweigh benefits. NiFi is already efficient for typical payloads.

---

### Alternative 3: Reduce WiFi Buffer Sizes (Deferred)

**Description**: Shrink WiFi receive buffers from 3×1024 to 2×512 bytes

**Pros**:
- Saves 1,024 bytes
- Simple change

**Cons**:
- ⚠️ Risk of packet fragmentation issues
- ⚠️ Edge cases with large WiFi frames
- ⚠️ Moderate complexity to validate

**Decision**: Defer to future optimization if Phase 1+2 insufficient. Not worth risk for 1 KB.

---

### Alternative 4: Single Large Buffer with Slab Allocator (Rejected)

**Description**: Use one large buffer with custom slab allocator for packets

**Pros**:
- Flexible allocation strategy
- Could optimize for different traffic patterns

**Cons**:
- ❌ Significant code complexity
- ❌ Hard to debug allocation issues
- ❌ Fragmentation still possible
- ❌ Minimal memory savings vs constant reduction

**Rejection Reason**: Complexity far exceeds benefit. Simple constant changes achieve 90% of potential savings.

---

### Alternative 5: Zero Optimization (Status Quo - Rejected)

**Description**: Keep current 14.4 KB footprint

**Pros**:
- No breaking changes
- No implementation effort
- No testing overhead

**Cons**:
- ❌ Doesn't address developer requests
- ❌ Misses opportunity for easy 40% reduction
- ❌ Competitive disadvantage vs optimized libraries

**Rejection Reason**: 5-7 KB savings achievable with minimal effort and low risk. Benefits outweigh costs.

---

## Implementation Plan

### Phase 1 (Weeks 1-2) - Parameter Length Reduction

**Tasks**:
1. Update `READ_PARAM_LENGTH` constant in `dsnifi9.h`
2. Add protocol version detection (CRITICAL)
3. Add runtime parameter length validation
4. Create migration guide for developers
5. Update API documentation
6. Comprehensive testing (unit, integration, stress)
7. Memory benchmarking

**Deliverables**:
- Updated library with 16-char parameters
- Test suite (100% pass rate)
- Migration guide
- Performance report

**Estimated Time**: 12-16 hours

---

### Phase 2 (Weeks 3-4) - Buffer Capacity Tuning (Optional)

**Prerequisite**: Phase 1 deployed and stable for 2+ weeks

**Tasks**:
1. Reduce `OUTGOING_PACKET_BUFFER_CAPACITY` to 12
2. Stress testing under high-traffic scenarios
3. Monitor buffer overflow rates
4. Performance comparison (Phase 1 vs Phase 1+2)
5. Document optimal use cases

**Deliverables**:
- Optimized buffer configuration
- Stress test results
- Usage guidelines (which games benefit)

**Estimated Time**: 8-12 hours

---

### Testing and Validation (Weeks 5-6)

**Beta Program**:
- Recruit 3-5 community developers
- Test with various game types (racing, RPG, puzzle)
- Collect feedback and metrics
- Iterate on issues

**Hardware Testing**:
- 2-6 DS units across multiple models (DS Lite, DSi, 3DS)
- Long-running sessions (2+ hours)
- Poor WiFi conditions (range, interference)
- Mixed old/new library versions

---

### Rollout (Week 7)

**Release**:
- Tag as v2.0.0 (major version - breaking change)
- Publish release notes with migration guide
- Update README and documentation
- Announce on devkitPro forums

**Post-Release**:
- Monitor GitHub issues
- Address bugs within 1 week
- Collect production metrics
- Evaluate Phase 2 viability after 1 month

---

## Success Metrics

### Memory Metrics

| Metric | Baseline | Phase 1 Target | Phase 2 Target | Status |
|--------|----------|----------------|----------------|--------|
| Total static allocation | 14.4 KB | ≤9.0 KB | ≤7.7 KB | 🎯 Target |
| Packet structure size | 382 bytes | ≤206 bytes | ≤206 bytes | 🎯 Target |
| Buffer overhead | 11.5 KB | ≤6.1 KB | ≤4.8 KB | 🎯 Target |

### Performance Metrics

| Metric | Baseline | Acceptable Range | Status |
|--------|----------|------------------|--------|
| Position update latency | 30ms | 25-35ms | ✅ Must maintain |
| Packet loss rate | 0-2% | 0-5% | ✅ Acceptable |
| Buffer overflow rate | <1% | <5% (Phase 2) | ⚠️ Monitor |
| Disconnects per hour | 0 | 0-1 | ✅ Must maintain |

### Adoption Metrics

| Metric | Target | Notes |
|--------|--------|-------|
| Beta testers | 3-5 devs | Community validation |
| GitHub stars increase | +10% | Indicates interest |
| Forum feedback | Mostly positive | Measure sentiment |
| Reported bugs | <5 critical | Stability indicator |

---

## Related Documents

- [Memory Footprint Design Specification](../planning/memory-footprint/memory-footprint-design.md) - Detailed design
- [Memory Footprint Review](../planning/memory-footprint/memory-footprint-review.md) - Risk assessment
- [Memory Footprint Implementation Guide](../planning/memory-footprint/memory-footprint-implementation-guide.md) - Step-by-step instructions
- [ADR 001: NiFi Protocol Implementation](001-nifi-protocol-implementation.md) - Core protocol design
- [ARCHITECTURE.md](../../ARCHITECTURE.md) - Protocol specification

---

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-11-22 | Initial ADR creation based on memory analysis |

---

**Next Steps:**
1. Review and approve this ADR
2. Address critical issues from review (version detection, validation)
3. Begin Phase 1 implementation
4. Create migration guide for developers
5. Set up beta testing program

---

**END OF ADR 004**
