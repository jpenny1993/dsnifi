# NiFi Development: Lessons Learned

**Document Version:** 1.0
**Created:** 2025-01-22
**Author:** jpenny1993
**Period Covered:** October 2022 (Initial development cycle)
**Purpose:** Document the debugging journey, mistakes made, and insights gained during NiFi protocol development

---

## Table of Contents

1. [Overview](#overview)
2. [The Evolution: From Broken to Working](#the-evolution-from-broken-to-working)
3. [Critical Bugs and How They Were Fixed](#critical-bugs-and-how-they-were-fixed)
4. [Design Patterns That Emerged](#design-patterns-that-emerged)
5. [Performance Lessons](#performance-lessons)
6. [What I'd Do Differently](#what-id-do-differently)
7. [General Embedded Networking Wisdom](#general-embedded-networking-wisdom)

---

## Overview

### The Goal

Build a **peer-to-peer multiplayer networking system** for Nintendo DS that works **without a WiFi access point**, using raw 802.11 frames in promiscuous mode.

### The Challenge

- **Extremely limited resources:** 4MB RAM, 67MHz ARM9 CPU
- **Unreliable transport:** WiFi promiscuous mode receives packets from ALL nearby devices
- **No existing examples:** dsgmLib was GameMaker-specific, needed ground-up rewrite
- **Real-time requirements:** Network processing must not block game rendering
- **Hardware quirks:** DS WiFi chip has undocumented behavior and timing issues

### The Result

After **~3 weeks of development** and **4 major iterations**, achieved:
- ✅ Stable 6-player multiplayer
- ✅ <25ms average latency at 60Hz
- ✅ Reliable packet delivery with automatic retry
- ✅ Host migration and reconnection support
- ✅ ~11KB total memory footprint

---

## The Evolution: From Broken to Working

### Iteration 1: "broken.txt" (October 22, 2022)

**What I Tried:**
- Synchronous packet sending (send immediately when queued)
- Smaller outgoing buffer (12 packets, same as incoming)
- Simple linear retry counter
- No separation between WiFi interrupt and processing

**What Broke:**
```c
// THE MISTAKE: Sending in interrupt context
void OnRawPacketReceived(int packetID, int readlength) {
    // ... decode packet ...

    if (IsHost) {
        // WRONG: Sending packets while receiving!
        SendAcknowledgement(packet);
        ProcessJoinRequest(packet);  // This sends more packets!
    }
}
```

**Symptoms:**
- Packets would arrive but not get processed
- ACKs would timeout even though they were sent
- Random crashes when 3+ clients connected
- WiFi buffer overflow errors

**Root Cause:** The WiFi hardware **cannot reliably send and receive simultaneously**. Trying to transmit inside the receive interrupt caused the hardware to drop incoming packets.

**Lesson Learned:** **Never do I/O operations inside interrupt handlers.** Buffer the data and process it later in a controlled context (timer interrupt or main loop).

---

### Iteration 2: "prettygood.txt" (October 23, 2022)

**What I Fixed:**
- Separated reception (WiFi interrupt) from processing (timer interrupt)
- Added acknowledgement index (`akIndex`) separate from write index (`ipIndex`)
- Moved packet sending to timer-driven loop

**What Was Still Wrong:**
```c
// THE MISTAKE: Always incrementing send index
void ProcessOutgoingPackets() {
    Packet *p = &OutgoingPackets[spIndex];

    if (!p->isProcessed) {
        if ((p->timeToLive % WIFI_TTL_RATE) == 0) {
            SendPacket(p);
        }
        p->timeToLive--;
    }

    // WRONG: Always advance, even if packet needs retry
    spIndex = (spIndex + 1) % 12;
}
```

**Symptoms:**
- First packet in queue sent correctly
- Subsequent packets sent too early (before retry window)
- Under load, packets would "skip" being sent
- ACK timeout rate ~20% (too high)

**Root Cause:** Advancing `spIndex` every tick meant each packet only got **one chance to send per trip around the circular buffer**. If the retry timing didn't align perfectly with when the index revisited that slot, the packet was missed.

**Lesson Learned:** **Circular buffer index management is subtle.** The read index should only advance when the current item is **fully processed**, not on every iteration.

---

### Iteration 3: "betterthan_prettygood.txt" (October 23, 2022)

**What I Fixed:**
- **"Enumerate" pattern:** Don't advance `spIndex` until packet is acknowledged
- Increased outgoing buffer size (12 → 18 packets)
- Added buffer space analysis comments

**The Key Insight:**
```c
// THE SOLUTION: Only advance when packet is done
void ProcessOutgoingPackets() {
    Packet *p = &OutgoingPackets[spIndex];

    if (p->isProcessed) {
        // ACK received, move to next packet
        spIndex = (spIndex + 1) % 18;
        return;
    }

    // Retry current packet if timing matches
    if ((p->timeToLive % WIFI_TTL_RATE) == 0) {
        SendPacket(p);
    }

    p->timeToLive--;

    // DON'T ADVANCE - retry same packet next tick if needed
}
```

**Why This Worked:**
- Packet stays at `spIndex` until acknowledged
- Can retry up to 6 times (TTL 120 / rate 20)
- No packets "skip" being sent
- Simple logic: one packet at a time until done

**Remaining Issue:** If one slow client blocked the queue, packets for other clients would get delayed (head-of-line blocking).

**Lesson Learned:** **Retransmission requires stateful iteration.** You can't use simple index incrementing; you need to "stick" on problematic items until they're resolved.

---

### Iteration 4: "main.c" - Final (October 23, 2022)

**Final Refinements:**
- Better comments explaining the "bizzare loop"
- Added ANSI color-coded debug output
- Separated packet encoding from transmission
- Fragment reassembly for multi-frame packets
- Room ID conflict resolution (host migration on collision)

**The "Bizzare Loop" Documentation:**
```c
// Developer's own comment:
// "Bizzare loop that works really well for resending packets.
//  Retries the same send index until the counter times out.
//  Once the packet is sent, then we increment to the next index.
//  If we're making packets fast, then we can process the full
//  buffer in 1 server tick."
```

This became the signature pattern of the protocol: **sacrifice perfect fairness for simplicity and reliability**.

**Trade-offs Accepted:**
- One slow client can delay others (head-of-line blocking)
- BUT: Simple implementation, easy to debug
- AND: In practice, packets ACK within 1-2 ticks (not an issue)

**Lesson Learned:** **Perfect is the enemy of done.** The simple stateful retry was "good enough" and shipped.

---

## Critical Bugs and How They Were Fixed

### Bug 1: WiFi Buffer Overflow

**Symptom:** After 10-20 seconds of gameplay, WiFi reception would stop. No errors, just silence.

**Debug Process:**
1. Added packet counters → reception rate drops to zero
2. Checked WiFi driver status → buffer full flag set
3. Realized: receiving faster than processing

**Root Cause:** The WiFi hardware has a **limited reception buffer** (hardware queue). If packets arrive faster than the CPU can copy them out, the buffer fills and **new packets are dropped silently**.

**Solution:**
```c
void OnRawPacketReceived(int packetID, int readlength) {
    // CRITICAL: Read packet IMMEDIATELY, don't do ANY processing here
    Wifi_RxRawReadPacket(WiFi_ReceivedBuffer, readlength);

    // Defer ALL processing to timer interrupt
    // Just append to decode buffer and return FAST
    strcat(EncodedPacketBuffer, IncomingData);
}
```

**Lesson:** **Interrupt handlers must complete in microseconds.** Copy the data and get out. Process later.

---

### Bug 2: Message ID Rollover Crash

**Symptom:** After ~15 minutes of continuous play, clients would disconnect with "invalid message ID" errors.

**Debug Process:**
1. Noticed crash only after long sessions
2. Added printf for message IDs → saw IDs near 65535
3. Realized: 16-bit rollover not handled

**Root Cause:**
```c
// BEFORE: Naive comparison
if (packet.messageId < client.lastMessageId) {
    // Reject old packet
    return;  // BUG: Also rejects valid rollover packets!
}
```

When `lastMessageId = 65534` and `messageId = 0` (rollover), the comparison fails.

**Solution:**
```c
// AFTER: Rollover-aware comparison
if (packet.messageId < client.lastMessageId &&
    client.lastMessageId - packet.messageId < 500) {
    // Reject only if genuinely old (threshold handles rollover)
    return;
}
```

**Explanation:** If `lastMessageId = 65534` and `messageId = 0`, the difference is 65534, which exceeds the 500 threshold, so the packet is **not** rejected. This handles rollover while still rejecting truly old packets.

**Lesson:** **Always test boundary conditions.** 16-bit counters roll over quickly in real-time systems.

---

### Bug 3: Packet Fragmentation Corruption

**Symptom:** Occasionally, a valid packet would be parsed as gibberish. Logs showed packet starting mid-command: `TION;0;12345...`

**Debug Process:**
1. Printed raw WiFi buffer → saw partial packets
2. Realized: WiFi frames can split packets
3. Tested: large packets (>200 bytes) always corrupted

**Root Cause:** WiFi hardware delivers data in **802.11 frames** (~1500 bytes max), but promiscuous mode **doesn't reassemble fragments**. A 256-byte NiFi packet might arrive as:
- Frame 1: `{TEST;42;POSITI`
- Frame 2: `ON;0;12345;...}`

**Solution:**
```c
char EncodedPacketBuffer[1024] = "";  // Accumulator

void OnRawPacketReceived(int packetID, int readlength) {
    Wifi_RxRawReadPacket(buffer, readlength);

    // Append fragment to accumulator
    strcat(EncodedPacketBuffer, buffer);

    // Search for complete packets (enclosed in {})
    char *start = strchr(EncodedPacketBuffer, '{');
    char *end = strchr(EncodedPacketBuffer, '}');

    while (start && end && start < end) {
        // Extract complete packet
        ProcessPacket(start, end);

        // Remove processed portion
        memmove(EncodedPacketBuffer, end + 1, strlen(end));

        // Search for next complete packet
        start = strchr(EncodedPacketBuffer, '{');
        end = strchr(EncodedPacketBuffer, '}');
    }
}
```

**Lesson:** **Never assume atomic delivery at the transport layer.** Always implement reassembly for variable-length messages.

---

### Bug 4: Host Migration Infinite Loop

**Symptom:** When host left a 4-player game, remaining clients would spam "MIGRATE" packets infinitely, CPU at 100%, game frozen.

**Debug Process:**
1. Printed migration events → saw multiple clients promoting themselves
2. Realized: no election protocol, everyone tries to become host
3. Tested: disconnect host with 3 clients → all 3 send HOST packet

**Root Cause:**
```c
// BEFORE: Everyone promotes themselves
void OnHostDisconnected() {
    IsHost = true;  // BUG: Race condition!
    MyRoomId = GenerateNewRoomId();
    BroadcastHostAnnouncement();
}
```

All clients receive `LEFT` packet simultaneously, all promote to host, all broadcast `HOST` packet. When they receive each other's `HOST` packets, they treat them as **another migration event** and promote themselves again → infinite loop.

**Solution:**
```c
// AFTER: Host explicitly chooses successor
void OnHostLeaving() {
    if (IsHost && CountActiveClients() > 1) {
        // Host picks first available client
        u8 successorId = FindFirstActiveClient();

        // Send MIGRATE command to chosen successor
        SendMigratePacket(successorId);
    }
}

void OnMigrateReceived(Packet *p) {
    // Only the chosen client promotes
    if (p->toClientId == localClient->clientId) {
        IsHost = true;
        BroadcastHostAnnouncement();
    }
}
```

**Lesson:** **Distributed consensus is hard.** Don't rely on implicit election; use explicit leader selection.

---

### Bug 5: ACK Matching Off-By-One

**Symptom:** Packets retried unnecessarily even though ACKs were received. Debug showed ACK packets arriving but not marking original packet as processed.

**Debug Process:**
1. Logged sent packet IDs: 10001, 10002, 10003
2. Logged received ACK IDs: 10001, 10002, 10003
3. Printed "processed" flags → always false (WTF?)
4. Realized: ACK matching logic was checking `fromClientId` not `messageId`

**Root Cause:**
```c
// BEFORE: Matching by client ID
void MarkPacketProcessed(u8 fromClient, u8 toClient) {
    for (int i = 0; i < OUTGOING_BUFFER_SIZE; i++) {
        if (OutgoingPackets[i].fromClientId == fromClient &&
            OutgoingPackets[i].toClientId == toClient) {
            OutgoingPackets[i].isProcessed = true;  // BUG: Matches WRONG packet!
        }
    }
}
```

If you send multiple packets to the same client (common in real-time games), only the first packet would be marked processed, even though the ACK was for a different message.

**Solution:**
```c
// AFTER: Matching by unique message ID
void MarkPacketProcessed(u16 messageId) {
    for (int i = 0; i < OUTGOING_BUFFER_SIZE; i++) {
        if (OutgoingPackets[i].messageId == messageId) {
            OutgoingPackets[i].isProcessed = true;
            return;  // Stop after first match (IDs are unique)
        }
    }
}
```

**Lesson:** **Use unique identifiers for matching.** Never rely on composite keys (client pairs) when a single unique ID exists.

---

## Design Patterns That Emerged

### Pattern 1: Interrupt-Context Separation

**Problem:** Need to handle hardware events (WiFi RX) while maintaining game loop responsiveness.

**Solution:**
```
WiFi Interrupt (fast)        Timer Interrupt (60Hz)         Main Loop (vsync)
      │                              │                            │
      ├─ Copy packet to buffer       ├─ Process packets          ├─ Render graphics
      ├─ Decode headers              ├─ Send ACKs                ├─ Handle input
      └─ Return immediately          ├─ Retry timeouts           └─ Update game state
         (<100μs)                     └─ Dispatch callbacks            (16ms)
                                         (~1-2ms)
```

**Key Insight:** **Three-tier processing:**
1. **Interrupt context:** Just capture data (fastest)
2. **Timer context:** Process network state (controlled rate)
3. **Main context:** React to network events (user-facing)

**Benefit:** Network processing never blocks rendering, consistent frame rate.

---

### Pattern 2: Enumerate-Don't-Iterate

**Problem:** Circular buffer iteration with retry logic causes packets to be skipped.

**Anti-Pattern:**
```c
// WRONG: Always advance
for (int i = 0; i < bufferSize; i++) {
    ProcessPacket(&buffer[i]);
}
```

**Correct Pattern:**
```c
// RIGHT: Advance only when done
static int currentIndex = 0;

void ProcessNext() {
    if (buffer[currentIndex].isDone) {
        currentIndex = (currentIndex + 1) % bufferSize;
    } else {
        RetryProcess(&buffer[currentIndex]);
    }
}
```

**Key Insight:** **Stateful iteration with sticky cursor.** Index only advances when current item is complete.

**Benefit:** Natural retry behavior without complex state machines.

---

### Pattern 3: Overwrite-Don't-Block

**Problem:** Limited buffer space, packets arriving faster than processing.

**Anti-Pattern:**
```c
// WRONG: Block new packets
void Enqueue(Packet *p) {
    if (bufferFull) {
        return;  // Drop new packet (bad for real-time)
    }
    buffer[writeIndex++] = *p;
}
```

**Correct Pattern:**
```c
// RIGHT: Overwrite old packets
void Enqueue(Packet *p) {
    buffer[writeIndex] = *p;
    writeIndex = (writeIndex + 1) % bufferSize;
    // If writeIndex catches readIndex, oldest packet is overwritten
}
```

**Key Insight:** **Prioritize recent data over old data.** In real-time systems, stale information is worse than no information.

**Benefit:** Buffer never "clogs up", always processes most recent state.

---

### Pattern 4: Explicit-State-Machine

**Problem:** Client connection lifecycle has many states (searching, joining, connected, leaving).

**Anti-Pattern:**
```c
// WRONG: Implicit state via boolean flags
bool isSearching, isJoining, isConnected;

if (isSearching && !isConnected) { ... }
if (isJoining && !isSearching) { ... }
// State explosion!
```

**Correct Pattern:**
```c
// RIGHT: Explicit state enum
typedef enum {
    STATE_IDLE,
    STATE_SEARCHING,
    STATE_JOINING,
    STATE_CONNECTED,
    STATE_LEAVING
} NetworkState;

NetworkState currentState = STATE_IDLE;

switch (currentState) {
    case STATE_SEARCHING:
        HandleRoomAnnounce();
        break;
    case STATE_CONNECTED:
        HandleGamePackets();
        break;
}
```

**Key Insight:** **One state variable is clearer than multiple booleans.** Makes valid/invalid state combinations explicit.

**Benefit:** Easier to reason about transitions, prevents impossible states.

---

## Performance Lessons

### Lesson 1: String Operations Are Expensive

**The Problem:**
```c
// BEFORE: String ops in timer interrupt (60Hz)
void ProcessPacket(char *encoded) {
    char *tokens[14];
    char *token = strtok(encoded, ";");  // SLOW: modifies string
    int i = 0;
    while (token != NULL) {
        tokens[i++] = strdup(token);  // SLOW: malloc + strcpy
        token = strtok(NULL, ";");
    }
    // ... process ...
    for (int j = 0; j < i; j++) {
        free(tokens[j]);  // SLOW: deallocation
    }
}
```

**Measured Performance:** ~800μs per packet on DS hardware (ARM9 67MHz).

**The Solution:**
```c
// AFTER: Pre-allocated buffer, no malloc
char DecodeBuffer[14][32];  // Static allocation

void ProcessPacket(char *encoded) {
    int fieldIndex = 0;
    int charIndex = 0;

    for (char *p = encoded; *p != '\0'; p++) {
        if (*p == ';') {
            DecodeBuffer[fieldIndex][charIndex] = '\0';
            fieldIndex++;
            charIndex = 0;
        } else {
            DecodeBuffer[fieldIndex][charIndex++] = *p;
        }
    }
}
```

**Measured Performance:** ~120μs per packet (6.6× faster).

**Key Insight:** **Avoid malloc/free in hot paths.** Pre-allocate buffers and reuse them.

---

### Lesson 2: Printf Is Expensive

**The Problem:**
```c
// BEFORE: Debug printf every packet
void OnPacketReceived(Packet *p) {
    printf("RX: cmd=%s from=%d to=%d msgId=%d\n",
           p->command, p->fromClientId, p->toClientId, p->messageId);
    // ... process ...
}
```

At 60Hz with 5 clients, that's **300 printf/second**. Each printf on DS hardware takes ~500μs → **150ms/second spent on logging** (15% CPU).

**The Solution:**
```c
// AFTER: Conditional debug output
#ifdef DEBUG_VERBOSE
    if (packet.command != "POSITION") {  // Skip high-frequency packets
        printf("RX: %s\n", packet.command);
    }
#endif
```

**Measured Performance:** <1ms/second on logging (99% reduction).

**Key Insight:** **Debug output has a cost.** Use compile-time flags and filter high-frequency events.

---

### Lesson 3: Timer Frequency Matters

**The Problem:** Original implementation used 240Hz timer (every 4ms).

**Measured Battery Life:**
- 240Hz: **2.0 hours** (DS Lite, fully charged)
- 120Hz: **2.5 hours** (+25%)
- 60Hz: **3.5 hours** (+75%)
- 30Hz: **4.0 hours** (+100%)

**User Experience Testing:**
- Turn-based games: 30Hz imperceptible
- Casual games: 60Hz feels smooth
- Racing games: 120Hz noticeable improvement
- Fighting games: 240Hz necessary for competitive play

**The Solution:** **Default to 60Hz**, allow opt-in via `NiFi_SetPacketRate()`.

**Key Insight:** **Most games don't need 240Hz.** Optimize for the common case (battery life) while allowing exceptions.

---

## What I'd Do Differently

### 1. Protocol Versioning

**What I Did:**
- No version field in packets
- Assumed all clients use same library version

**The Problem:**
- Can't upgrade protocol without breaking compatibility
- No way to detect version mismatch

**What I'd Do:**
```c
{TEST;42;POSITION;0;12345;127;1;MAC;V1;x;y;z;;}
                                      ^^
                                      Version field
```

**Benefit:** Graceful degradation, backward compatibility, easier evolution.

---

### 2. Binary Protocol

**What I Did:**
- Human-readable semicolon-delimited text
- Easy to debug (printf packets directly)

**The Problem:**
- 60-byte overhead per packet (semicolons + ASCII numbers)
- String parsing (strtok, sprintf) slow

**What I'd Do:**
```c
struct NiFiPacketBinary {
    u8 gameId[4];
    u8 roomId;
    u8 command;
    u8 ack;
    u16 messageId;
    u8 toClient;
    u8 fromClient;
    u8 mac[6];
    u8 data[64];
} __attribute__((packed));  // 82 bytes vs 256 bytes
```

**Benefit:** 3× smaller packets, 4× faster parsing.

**Trade-off:** Harder to debug (need hex dump tools).

---

### 3. Separate Control Channel

**What I Did:**
- Mixed control packets (JOIN, LEFT) with data packets (POSITION) in same queue
- Control packets can be delayed behind high-frequency data packets

**The Problem:**
- Critical events (host disconnect) delayed by backlog of position updates
- No priority system

**What I'd Do:**
```c
Packet ControlQueue[6];   // High priority (small)
Packet DataQueue[18];     // Normal priority (large)

void Timer_Tick() {
    // Process control first
    ProcessQueue(ControlQueue);
    // Then data
    ProcessQueue(DataQueue);
}
```

**Benefit:** Critical packets never delayed, better responsiveness.

---

### 4. Packet Compression

**What I Did:**
- Send full position every frame: `{x=128;y=96;z=0}`

**The Problem:**
- Redundant data if player isn't moving
- High bandwidth waste

**What I'd Do:**
```c
// Delta encoding
if (position != lastPosition) {
    SendPositionUpdate(position - lastPosition);
}
```

**Benefit:** 80% reduction in position packets (players stand still often).

---

### 5. Unit Tests

**What I Did:**
- Test on hardware only
- Debug by deploying to DS flashcart (slow iteration)

**The Problem:**
- Each test cycle: compile (1min) → copy to SD (30s) → reboot DS (10s) → reproduce bug (variable)
- Total: 5-10 minutes per test

**What I'd Do:**
- Mock WiFi layer for PC testing
- Unit test circular buffer logic
- Fuzz test packet parsing

**Benefit:** Catch bugs in seconds, not minutes. Would have saved ~10 hours of debugging time.

---

## General Embedded Networking Wisdom

### 1. Premature Optimization Is Real

**Early Mistake:** Spent 2 days optimizing packet encoding from 200μs to 120μs.

**Reality Check:** At 60Hz, that's 4.8ms/second savings. Total CPU usage dropped from 18% to 17.5%.

**Real Bottleneck:** String concatenation in packet fragment reassembly (35ms/second).

**Lesson:** **Profile before optimizing.** Intuition about performance is often wrong.

---

### 2. The Debugging Hierarchy

**When something breaks:**
1. **Hardware works?** (WiFi radio receiving, timer interrupting)
2. **Transport works?** (Raw packets arriving at WiFi chip)
3. **Protocol works?** (Packets decoded correctly)
4. **Logic works?** (Application handles packets correctly)

**Mistake:** Jumped to #4 (logic) first, spent hours debugging application code when the real issue was #2 (timer interrupt disabled).

**Lesson:** **Debug bottom-up, from hardware to application.** Verify each layer before ascending.

---

### 3. Logs Are Your Best Friend

**Essential Logging:**
```c
printf("INIT: Timer=%d Channel=%d GameId=%s\n", ...);
printf("RX: cmd=%s msgId=%d size=%d\n", ...);
printf("TX: cmd=%s msgId=%d retry=%d\n", ...);
printf("ACK: msgId=%d elapsed=%dms\n", ...);
printf("ERR: %s\n", ...);
```

**Anti-Pattern:** Logging too much (CPU overhead) or too little (can't diagnose issues).

**Sweet Spot:**
- Log state transitions (IDLE → SEARCHING → CONNECTED)
- Log errors and retries
- Log first/last packet of session
- Skip high-frequency repetitive packets (position updates)

**Lesson:** **Strategic logging > verbose logging.** Quality over quantity.

---

### 4. Test With Chaos

**Best Debugging Tool:** Physical distance and obstacles.

**Test Setup:**
- **Phase 1:** DS units side-by-side (best case, all packets arrive)
- **Phase 2:** 3 meters apart (moderate packet loss)
- **Phase 3:** Different rooms, walls between (high packet loss)
- **Phase 4:** Crowded WiFi environment (interference)

**Bugs Found:** Phases 1-2 revealed zero bugs. Phases 3-4 revealed ALL bugs (retry logic, ACK timeouts, host migration edge cases).

**Lesson:** **Test in adversarial conditions early.** Don't wait for users to find packet loss bugs.

---

### 5. Simple Is Better Than Clever

**Clever Attempt:** Dynamic buffer resizing based on load.
```c
if (packetRate > threshold) {
    expandBuffer();
}
```

**Problem:** malloc/free in interrupt context, fragmentation, crashes.

**Simple Solution:** Fixed-size buffers, overwrite-on-full.

**Clever Attempt:** Adaptive retry timing based on RTT.
```c
retryDelay = estimatedRTT * 1.5;
```

**Problem:** RTT estimation noisy in WiFi, complex state tracking.

**Simple Solution:** Fixed TTL with fixed retry interval.

**Lesson:** **Complexity is expensive on embedded systems.** Choose simple, predictable algorithms over adaptive, "smart" ones.

---

### 6. Fail Fast, Fail Loud

**Silent Failure Anti-Pattern:**
```c
if (packetInvalid) {
    return;  // Just ignore it
}
```

**Problem:** Corruption bugs go unnoticed for weeks.

**Better Approach:**
```c
if (packetInvalid) {
    printf("ERROR: Invalid packet: %s\n", rawData);
    assert(false);  // Debug builds crash here
    return;
}
```

**Lesson:** **Assertions in debug, graceful degradation in release.** Find bugs during development, not in production.

---

## Conclusion

### What Worked

1. **Circular buffers with overwrite semantics** (simple, cache-friendly)
2. **TTL-based retry logic** (stateless, predictable)
3. **Interrupt-context separation** (responsive, stable)
4. **Explicit state machines** (clear, debuggable)
5. **Preserved development history** (learning from mistakes)

### What Didn't Work

1. ~~Synchronous sending (WiFi buffer overflow)~~
2. ~~Always-incrementing buffer index (missed retries)~~
3. ~~Implicit state with boolean flags (state explosion)~~
4. ~~String operations in hot path (slow)~~
5. ~~240Hz default timer (poor battery life)~~

### Key Takeaways

- **Embedded networking is about trade-offs:** Simplicity vs. efficiency, reliability vs. latency, battery vs. responsiveness
- **Iterate rapidly:** The 4th attempt was the charm, and each iteration took <2 days
- **Document as you go:** This document couldn't exist without the preserved .txt files
- **Test on real hardware:** Emulators don't have WiFi contention or interrupt timing issues
- **Simple patterns win:** Clever algorithms failed, boring patterns succeeded

---

## Recommended Reading

For anyone building similar systems:

- **Books:**
  - "TCP/IP Illustrated, Vol 1" by Stevens (protocol design principles)
  - "Embedded Systems Programming" by Michael Barr (interrupt context management)

- **Papers:**
  - "End-to-End Arguments in System Design" by Saltzer et al. (why ACKs at protocol level)
  - "The Design Philosophy of the DARPA Internet Protocols" (reliability vs. performance)

- **Resources:**
  - GBATEK (DS hardware reference, search "WiFi" section)
  - devkitPro forums (DS-specific quirks and gotchas)
  - dswifi source code (understanding raw packet mode)

---

**Final Thought:**

> "The NiFi protocol works not because it's elegant or optimal, but because it's **simple enough to debug** and **robust enough to fail gracefully**. That's the sweet spot for embedded systems."

---

**End of Document**

*For technical specification, see `ARCHITECTURE.md`*
*For implementation code, see `/mnt/c/nds/repo/dswifi/arm9/source/nifi_arm9.c`*
