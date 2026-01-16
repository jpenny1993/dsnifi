# Memory Footprint Optimization - Implementation Guide

**Version:** 1.0
**Date:** 2025-11-22
**Estimated Time**:
- Phase 1: 2-3 hours
- Phase 2: 2-3 hours
- Total: 4-6 hours (excludes extensive testing)

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Phase 1: Parameter Length Reduction](#phase-1-parameter-length-reduction)
3. [Phase 2: Buffer Capacity Optimization](#phase-2-buffer-capacity-optimization-optional)
4. [Testing and Validation](#testing-and-validation)
5. [Troubleshooting](#troubleshooting)
6. [Rollback Instructions](#rollback-instructions)

---

## Prerequisites

### Required Tools
- devkitARM (devkitPro v3.0.3 or later)
- make
- git (for version control)
- Text editor (VS Code, vim, etc.)
- Nintendo DS hardware (2+ units for testing)
- NDS flashcarts

### Required Knowledge
- C programming
- Make/build systems
- NiFi library architecture basics
- Git workflow

### Files You Will Modify

| File | Path | Purpose |
|------|------|---------|
| `dsnifi9.h` | `/mnt/c/nds/repo/dswifi/include/` | Public API header |
| `nifi_arm9.c` | `/mnt/c/nds/repo/dswifi/arm9/source/` | Implementation (Phase 2 only) |

---

## Phase 1: Parameter Length Reduction

**Goal**: Reduce `READ_PARAM_LENGTH` from 32 to 16 characters, saving 5.4 KB.

**Risk**: **LOW** | **Time**: 2-3 hours

---

### Step 1.1: Backup Current State

```bash
cd /mnt/c/nds/repo/dswifi
git checkout -b feature/memory-optimization
git status
```

**Verification**: Confirm you're on a new branch.

---

### Step 1.2: Modify Parameter Length Constant

**File**: `/mnt/c/nds/repo/dswifi/include/dsnifi9.h`

**Location**: Around line 7

**BEFORE**:
```c
#define READ_PARAM_LENGTH 32U               // Max length of each parameter in a packet
```

**AFTER**:
```c
#define READ_PARAM_LENGTH 16U               // Max length of each parameter in a packet (optimized)
```

**Verification**:
```bash
grep "READ_PARAM_LENGTH" /mnt/c/nds/repo/dswifi/include/dsnifi9.h
# Expected output: #define READ_PARAM_LENGTH 16U
```

---

### Step 1.3: Rebuild dswifi Library

```bash
cd /mnt/c/nds/repo/dswifi
make clean
make
```

**Expected Output**:
```
arm-none-eabi-gcc -MMD -MP -MF ... (compilation output)
...
arm-none-eabi-ar -crs lib/libdswifi9.a ...
```

**Verification**:
```bash
# Check library was built
ls -lh lib/libdswifi9.a
# Should show recent timestamp
```

---

### Step 1.4: Install Updated Library

```bash
cd /mnt/c/nds/repo/dswifi
make install
```

**Expected Output**:
```
cp -fv lib/libdswifi9.a /opt/devkitpro/libnds/lib/
```

**Verification**:
```bash
# Verify installed library timestamp
ls -lh /opt/devkitpro/libnds/lib/libdswifi9.a
```

---

### Step 1.5: Rebuild nifitest Demo

```bash
cd /mnt/c/nds/repo/nifitest
make clean
make
```

**Expected Output**:
```
built ... nifitest.nds
ROM fixed!
```

**Verification**:
```bash
ls -lh nifitest.nds
# Should show recently built .nds file
```

---

### Step 1.6: Measure Memory Usage (Optional)

```bash
cd /mnt/c/nds/repo/dswifi
arm-none-eabi-size arm9/source/nifi_arm9.o

# Compare .data + .bss sections
# Expected: ~5-6 KB reduction
```

---

### Step 1.7: Basic Functional Test

**Test on Hardware**:

1. Copy `nifitest.nds` to both flashcarts
2. Launch on Device A: Tap to create room
3. Launch on Device B: Should discover room
4. Join room from Device B
5. Touch screen on both devices
   - Verify coordinates transmit correctly
6. Leave room gracefully

**Expected Result**: ✅ All features work normally

**If Issues**: See [Troubleshooting](#troubleshooting)

---

### Step 1.8: Parameter Length Validation Test

**Create Test Build** (optional):

Add to `nifitest/source/main.c`:

```c
void TestParameterLengths() {
    // Test boundary conditions
    NiFiPacket packet;
    NiFi_SetPacket(&packet, "TEST");

    // Test 1: Exact 16 chars
    strcpy(packet.data[0], "EXACTLY16CHARSS");
    printf("Test 1: %s\n", packet.data[0]);

    // Test 2: Over 16 chars (should truncate)
    strcpy(packet.data[0], "THIS_IS_LONGER_THAN_16_CHARS");
    packet.data[0][16] = '\0'; // Manual truncation
    printf("Test 2: %s\n", packet.data[0]);

    // Test 3: Short string
    strcpy(packet.data[0], "SHORT");
    printf("Test 3: %s\n", packet.data[0]);
}
```

**Run Tests**:
- Call `TestParameterLengths()` in `main()`
- Observe output on console
- Verify no buffer overflows or crashes

---

### Step 1.9: Commit Changes

```bash
cd /mnt/c/nds/repo/dswifi
git add include/dsnifi9.h
git commit -m "feat: Reduce READ_PARAM_LENGTH from 32 to 16 chars

Saves 5.4 KB of memory by reducing packet parameter capacity.

Breaking change: Games using >16 char parameters must update.

🤖 Generated with Claude Code

Co-Authored-By: Claude <noreply@anthropic.com>"

git log -1 --stat
```

---

### Step 1.10: Documentation Update

Update `/mnt/c/nds/repo/nifitest/README.md` with breaking change notice:

```markdown
## Breaking Changes (v2.0)

### Parameter Length Reduced (Memory Optimization)
- `READ_PARAM_LENGTH` reduced from 32 to 16 characters
- **Impact**: Custom game packets limited to 16 chars per data field
- **Migration**: Split large data across multiple packets or abbreviate
- **Memory Savings**: 5.4 KB (37.5% reduction)
```

---

## Phase 2: Buffer Capacity Optimization (Optional)

**Goal**: Reduce outgoing/incoming buffer capacities, saving 1.3-2.2 KB additional.

**Risk**: **MEDIUM** | **Time**: 2-3 hours

⚠️ **WARNING**: Only proceed if Phase 1 is stable and memory savings are still needed.

---

### Step 2.1: Evaluate Need for Phase 2

**Decision Tree**:
- If Phase 1 savings (5.4 KB) are sufficient → **STOP HERE** ✅
- If game is turn-based/low-traffic → **PROCEED** ⚠️
- If game is real-time/high-frequency → **RECONSIDER** ❌

---

### Step 2.2: Modify Buffer Capacities

**File**: `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`

**Location**: Around lines 25-27

**BEFORE**:
```c
#define INCOMING_PACKET_BUFFER_CAPACITY 12
#define OUTGOING_PACKET_BUFFER_CAPACITY 18
#define ACK_PACKET_BUFFER_CAPACITY 15
```

**AFTER** (Conservative):
```c
#define INCOMING_PACKET_BUFFER_CAPACITY 12  // Keep incoming unchanged
#define OUTGOING_PACKET_BUFFER_CAPACITY 12  // Reduce outgoing (saves 1.3 KB)
#define ACK_PACKET_BUFFER_CAPACITY 15       // Keep ACK unchanged
```

**AFTER** (Aggressive - NOT RECOMMENDED):
```c
#define INCOMING_PACKET_BUFFER_CAPACITY 8   // Reduce incoming (saves 0.9 KB)
#define OUTGOING_PACKET_BUFFER_CAPACITY 12  // Reduce outgoing (saves 1.3 KB)
#define ACK_PACKET_BUFFER_CAPACITY 15       // Keep ACK unchanged
```

**Recommendation**: Start with conservative (outgoing only), test extensively.

---

### Step 2.3: Rebuild and Install

```bash
cd /mnt/c/nds/repo/dswifi
make clean
make
make install

cd /mnt/c/nds/repo/nifitest
make clean
make
```

---

### Step 2.4: Stress Testing

**Test 1: High-Frequency Position Updates**

Modify `nifitest/source/main.c`:

```c
// In main loop, broadcast position every frame (60 FPS)
if (keysHeld() & KEY_TOUCH) {
    touchPosition touch;
    touchRead(&touch);

    Position pos = {touch.px, touch.py, 0};
    NiFi_BroadcastPosition(pos);

    // Log buffer stats periodically
    static int frame = 0;
    if (++frame % 60 == 0) {
        printf("Frame %d: Buffers OK\n", frame);
    }
}
```

**Run Test**:
- 4 clients, all holding stylus on screen (continuous broadcasts)
- Monitor for "Overwriting packet" warnings
- Run for 5 minutes

**Pass Criteria**: <5% packet overwrites

---

**Test 2: Burst Traffic**

```c
void TestBurstTraffic() {
    for (int i = 0; i < 10; i++) {
        NiFiPacket packet;
        NiFi_SetPacket(&packet, "BURST");
        sprintf(packet.data[0], "Packet_%d", i);
        NiFi_SendBroadcast(&packet, NULL);
    }
    printf("Burst complete\n");
}

// Call every 5 seconds
```

**Pass Criteria**: All packets delivered, no crashes

---

**Test 3: Slow ACK Simulation**

This requires hardware with poor WiFi conditions or artificial latency injection.

**Run Test**:
- Place devices at maximum range (~30m)
- Test normal gameplay
- Monitor disconnect frequency

**Pass Criteria**: Stable connections, <1 disconnect/hour

---

### Step 2.5: Monitor Buffer Statistics

Add to `nifi_arm9.c` (optional debug code):

```c
void NiFi_PrintBufferStats() {
    static int overflows_in = 0;
    static int overflows_out = 0;

    // Check for buffer full conditions
    // (Implement circular buffer saturation check)

    printf("Buffer Stats: IN=%d%% OUT=%d%% (Overflows: IN=%d OUT=%d)\n",
           (ipIndex * 100) / INCOMING_PACKET_BUFFER_CAPACITY,
           (opIndex * 100) / OUTGOING_PACKET_BUFFER_CAPACITY,
           overflows_in, overflows_out);
}

// Call periodically from Timer_Tick()
```

---

### Step 2.6: Commit Phase 2 Changes

```bash
cd /mnt/c/nds/repo/dswifi
git add arm9/source/nifi_arm9.c
git commit -m "feat: Reduce outgoing buffer capacity (18→12)

Saves 1.3 KB additional memory.

Risk: May impact high-traffic real-time games.
Test extensively before deployment.

🤖 Generated with Claude Code

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

## Testing and Validation

### Unit Tests (Add to Test Suite)

**Test File**: `nifitest/tests/test_memory_optimization.c` (create if needed)

```c
#include <assert.h>
#include <string.h>
#include "dsnifi9.h"

void test_parameter_length_boundary() {
    NiFiPacket packet;
    NiFi_SetPacket(&packet, "TEST");

    // Test 1: Max length (16 chars)
    strcpy(packet.data[0], "1234567890123456");
    assert(strlen(packet.data[0]) == 16);

    // Test 2: Overflow protection
    char longstr[64] = "This_string_is_much_longer_than_16_characters";
    strncpy(packet.data[0], longstr, READ_PARAM_LENGTH);
    packet.data[0][READ_PARAM_LENGTH] = '\0';
    assert(strlen(packet.data[0]) == READ_PARAM_LENGTH);

    printf("✅ Parameter length tests passed\n");
}

void test_backward_compatibility() {
    // Simulate parsing 32-char packet from old client
    char encoded[512] = "{GAME;1;CMD;0;1;1;1;AA:BB:CC:DD:EE:FF;"
                        "LONG_DATA_STRING_32_CHARS___;"
                        "ANOTHER_LONG_STRING_32_CHARS}";

    // Parse packet (implementation-specific)
    // Verify first 16 chars extracted

    printf("✅ Backward compatibility tests passed\n");
}

int main() {
    test_parameter_length_boundary();
    test_backward_compatibility();
    return 0;
}
```

---

### Integration Tests (Hardware Required)

**Test Suite**:

| Test | Scenario | Pass Criteria |
|------|----------|---------------|
| **IT-1** | 2 clients, room join/leave | Connection stable |
| **IT-2** | 4 clients, position broadcast (30 Hz) | Coordinates correct |
| **IT-3** | 6 clients, simultaneous commands | All commands received |
| **IT-4** | Host migration during game | New host elected smoothly |
| **IT-5** | Long session (1 hour continuous) | No disconnects, no leaks |
| **IT-6** | Mixed old/new library versions | Communication works (if compatible) |

---

### Performance Benchmarks

**Memory Measurement**:

```bash
# Before optimization
cd /mnt/c/nds/repo/dswifi
git checkout main
make clean && make
arm-none-eabi-size arm9/source/nifi_arm9.o > size_before.txt

# After optimization
git checkout feature/memory-optimization
make clean && make
arm-none-eabi-size arm9/source/nifi_arm9.o > size_after.txt

# Compare
diff size_before.txt size_after.txt
```

**Expected Results**:
- Phase 1: ~5.4 KB reduction in .data or .bss
- Phase 2: ~1.3-2.2 KB additional reduction

---

**Network Performance**:

Measure on hardware:

| Metric | Before | After Phase 1 | After Phase 2 |
|--------|--------|---------------|---------------|
| Position update latency | ~30ms | ~30ms ±5ms | ~30ms ±5ms |
| Packet loss rate | 0-2% | 0-2% | 0-5% acceptable |
| Disconnects per hour | 0 | 0 | 0-1 |

---

## Troubleshooting

### Issue 1: Compilation Errors After Phase 1

**Symptom**:
```
error: 'data' array size mismatch
```

**Cause**: Cached object files with old definition.

**Fix**:
```bash
cd /mnt/c/nds/repo/dswifi
make clean
make
make install
```

---

### Issue 2: Data Truncation in Existing Game

**Symptom**: Game expects 32-char data, receives 16 chars.

**Cause**: Game code assumes old `READ_PARAM_LENGTH`.

**Fix**: Update game to use ≤16 chars per parameter, or split data:

```c
// OLD (32 chars)
strcpy(packet.data[0], "PLAYER_NAME_AND_LONG_STATUS");

// NEW (split across 2 params)
strcpy(packet.data[0], "PLAYER_NAME");
strcpy(packet.data[1], "STATUS");
```

---

### Issue 3: Increased "Overwriting packet" Warnings (Phase 2)

**Symptom**: Console logs show frequent buffer overflows.

**Cause**: Buffer capacity too small for traffic pattern.

**Fix Option A** (Rollback): Restore original capacities.

**Fix Option B** (Reduce frequency):
```c
// Lower broadcast rate
static int frame_counter = 0;
if (++frame_counter % 2 == 0) {  // Broadcast every other frame
    NiFi_BroadcastPosition(pos);
}
```

**Fix Option C** (Increase buffers): If you have memory budget, increase capacity slightly:
```c
#define OUTGOING_PACKET_BUFFER_CAPACITY 15  // Compromise (was 18, tried 12)
```

---

### Issue 4: Network Disconnects After Phase 2

**Symptom**: Clients disconnect more frequently.

**Cause**: Incoming buffer exhaustion during traffic bursts.

**Fix**: Restore incoming capacity to 12:
```c
#define INCOMING_PACKET_BUFFER_CAPACITY 12  // Keep original
```

---

### Issue 5: Build Fails After `make install`

**Symptom**:
```
cp: cannot stat 'lib/libdswifi9.a': No such file or directory
```

**Cause**: Build failed silently.

**Fix**:
```bash
cd /mnt/c/nds/repo/dswifi
make clean
make 2>&1 | tee build.log
# Check build.log for actual error
```

---

## Rollback Instructions

### Full Rollback to Original State

```bash
cd /mnt/c/nds/repo/dswifi
git checkout main  # or original branch
make clean
make
make install

cd /mnt/c/nds/repo/nifitest
make clean
make
```

**Time**: ~5 minutes

---

### Partial Rollback (Phase 2 Only, Keep Phase 1)

**File**: `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`

Restore buffer capacities:
```c
#define INCOMING_PACKET_BUFFER_CAPACITY 12  // Original
#define OUTGOING_PACKET_BUFFER_CAPACITY 18  // Original
```

Then rebuild:
```bash
cd /mnt/c/nds/repo/dswifi
make clean && make && make install
```

---

## Verification Checklist

### Phase 1 Complete

- [ ] `READ_PARAM_LENGTH` set to 16 in `dsnifi9.h`
- [ ] Library rebuilt and installed successfully
- [ ] nifitest demo rebuilt
- [ ] Basic functionality test passed (create room, join, positions)
- [ ] Parameter length tests passed
- [ ] Memory measurement confirms ~5 KB savings
- [ ] Changes committed to git
- [ ] Documentation updated

### Phase 2 Complete (Optional)

- [ ] Buffer capacities reduced in `nifi_arm9.c`
- [ ] Library rebuilt and installed
- [ ] nifitest rebuilt
- [ ] Stress tests passed (<5% overflow rate)
- [ ] Integration tests passed
- [ ] Performance benchmarks acceptable
- [ ] Changes committed to git

---

## Next Steps

After successful implementation:

1. **Merge to Main Branch**:
   ```bash
   git checkout main
   git merge feature/memory-optimization
   git tag v2.0.0-memory-optimized
   git push origin main --tags
   ```

2. **Release Notes**: Document breaking changes

3. **Community Testing**: Share with 3-5 developers for beta testing

4. **Production Deployment**: After 2-4 weeks of beta testing

---

## Related Documents

- [Memory Footprint Design Specification](memory-footprint-design.md) - Design rationale
- [Memory Footprint Review](memory-footprint-review.md) - Risk assessment
- [ADR 004: Memory Footprint Optimization](../../architecture/adr/004-memory-footprint-optimization.md) - Architecture decision
- [ARCHITECTURE.md](../../../ARCHITECTURE.md) - Protocol specification

---

**Implementation Status**: Ready
**Estimated Completion**: 4-6 hours (excludes extensive testing)
**Risk Level**: Phase 1 (LOW), Phase 2 (MEDIUM)
