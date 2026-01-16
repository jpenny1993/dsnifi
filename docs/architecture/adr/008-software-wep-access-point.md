# ADR 008: Software WEP Access Point with Monitor Mode

**Status:** Investigating - Not Recommended

**Date:** 2025-11-22

**Decision**: After comprehensive investigation, implementing a software WEP access point using WiFi monitor mode and packet injection is **technically feasible but not recommended**. While the approach could theoretically work, it offers higher complexity with no significant advantages over the NiFi protocol bridge (ADR 007) for the target use case. **Recommendation: Defer to NiFi bridge approach.**

---

## Context

### Problem Statement

Following the rejection of traditional WEP access point approaches (ADR 006 - blocked on Windows/macOS) and the approval of the NiFi protocol bridge (ADR 007 - feasible on Linux), a question arose:

**If we can use monitor mode + packet injection for NiFi protocol, why not use the same approach to implement WEP in software?**

### Proposed Approach

Create a "software WEP access point" that:

1. **Uses WiFi monitor mode** to capture all 802.11 frames (including WEP-encrypted)
2. **Implements WEP encryption/decryption in software** (RC4 cipher)
3. **Implements 802.11 management frames in userspace** (beacons, probe, auth, assoc)
4. **Uses packet injection** to send WEP-encrypted frames and management frames to DS
5. **Handles full AP state machine** in application code

**Key Insight**: This would bypass OS-level WEP restrictions by implementing WEP at the application level rather than relying on driver/OS support.

### Motivation

**Potential Advantages Over NiFi Bridge:**
- Works with **any DS software**, not just NiFi-enabled games
- Provides standard WiFi infrastructure mode
- Could support commercial DS games (if servers existed)
- Familiar WiFi paradigm for users

**Comparison to Traditional WEP AP (ADR 006):**
- Bypasses OS WEP blocks on Windows/macOS
- Doesn't rely on hostapd or OS AP mode
- Complete control over implementation

### Investigation Scope

This ADR documents a comprehensive technical investigation into:
1. Feasibility of software WEP implementation
2. Feasibility of userspace 802.11 access point
3. Beacon timing and management frame injection
4. Comparison to NiFi bridge approach
5. Development complexity and timeline
6. Recommendation for path forward

---

## Investigation Findings

### 1. Software WEP Implementation

#### WEP Algorithm Feasibility

**Algorithm**: WEP uses RC4 stream cipher with the following characteristics:

- **Cipher**: RC4 (Ron's Code 4)
- **Key Lengths**: 40-bit (WEP-40/64-bit) or 104-bit (WEP-104/128-bit)
- **IV**: 24-bit initialization vector
- **Integrity**: CRC-32 checksum

**Computational Complexity:**

From research findings:
- RC4 is designed for software efficiency
- Uses only byte-level operations
- Hardware implementations: 640 Mbps to 32 Gbps
- Software implementations: More than sufficient for 802.11b (1-11 Mbps)

**Performance Assessment:**
- 802.11b maximum: 11 Mbps
- NDS typical: 1-2 Mbps
- RC4 software overhead: **Negligible** for these rates

**Verdict:** ✅ **HIGHLY FEASIBLE** - WEP encryption/decryption is computationally trivial for modern PCs.

#### Available Libraries

**OpenSSL EVP Interface:**
```c
#include <openssl/evp.h>

EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
EVP_EncryptInit_ex(ctx, EVP_rc4(), NULL, wep_key, NULL);
EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len);
EVP_EncryptFinal_ex(ctx, ciphertext + len, &len2);
EVP_CIPHER_CTX_free(ctx);
```

**Alternative Implementations:**
- Numerous open-source RC4 implementations on GitHub
- aircrack-ng suite contains WEP implementation
- Can be implemented from scratch in ~200 lines of C

**Verdict:** ✅ **WELL-SUPPORTED** - Multiple library options available.

#### Real-Time Decryption Evidence

**aircrack-ng Suite:**

From research:
- **airdecap-ng**: Decrypts WEP from capture files (post-capture only)
- **airtun-ng**: Decrypts WEP traffic in **real-time**, creates virtual interface
- Performance: Handles 802.11b traffic speeds without issues

**Benchmark Data:**
- WEP cracking with PTW attack: 35,006 IVs processed in ~9 minutes
- Real-time decryption proven by airtun-ng in production use

**Verdict:** ✅ **PROVEN** - Real-time WEP encryption/decryption is demonstrably feasible.

---

### 2. Software Access Point Implementation

#### 802.11 Management Frame Requirements

A software access point must handle the following management frames:

**Required Management Frames:**

| Frame Type | Direction | Purpose | Frequency |
|------------|-----------|---------|-----------|
| **Beacon** | AP → All | Announce AP presence | ~100ms intervals |
| **Probe Request** | Client → All | Scan for APs | On-demand |
| **Probe Response** | AP → Client | Respond to scan | On-demand |
| **Authentication Request** | Client → AP | Begin authentication | On-demand |
| **Authentication Response** | AP → Client | Accept/reject auth | On-demand |
| **Association Request** | Client → AP | Join network | On-demand |
| **Association Response** | AP → Client | Assign AID | On-demand |
| **Deauthentication** | Either | Disconnect | On-demand |

**WEP Shared Key Authentication:**
- 4-way handshake
- AP sends challenge text
- Client encrypts challenge with WEP key
- AP verifies encrypted response

**802.11 State Machine:**

Based on [IEEE 802.11 State Machine](https://dot11ap.wordpress.com/the-ieee-802-11-state-machine/):

```
State 1: Unauthenticated, Unassociated
    ↓ (Authentication)
State 2: Authenticated, Unassociated
    ↓ (Association)
State 3: Authenticated, Associated (can send data)
```

**Complexity Assessment:** Moderate to High - requires correct state transitions and frame sequencing.

#### Management Frame Injection Feasibility

**Question**: Can we generate and inject 802.11 management frames in monitor mode?

**Answer**: ✅ **YES - PROVEN**

**Evidence:**

1. **Linux Kernel Documentation**:
   - [mac80211 packet injection](https://docs.kernel.org/networking/mac80211-injection.html)
   - Explicitly supports management frame injection
   - Format: `[radiotap header][IEEE 802.11 header][payload]`

2. **Existing Tools**:
   - **Scapy**: Can forge beacon frames ([4ARMED tutorial](https://www.4armed.com/blog/forging-wifi-beacon-frames-using-scapy/))
   - **hostapd-mana**: Rogue AP tool with beacon generation
   - **wperf**: 802.11 frame injection tool for mac80211
   - Numerous "evil twin" and fake AP tools

3. **Code Examples**:
   ```python
   # Scapy beacon frame injection
   from scapy.all import *

   beacon = RadioTap() / \
            Dot11(type=0, subtype=8, addr1="ff:ff:ff:ff:ff:ff",
                  addr2=ap_mac, addr3=ap_mac) / \
            Dot11Beacon(cap="ESS+privacy") / \
            Dot11Elt(ID="SSID", info="TestAP") / \
            Dot11Elt(ID="Rates", info=b"\x02\x04\x0b\x16")

   sendp(beacon, iface="wlan0", loop=1, inter=0.1)
   ```

**Verdict:** ✅ **WELL-ESTABLISHED** - Management frame injection is proven and widely used.

---

### 3. Beacon Timing Challenge

#### The Critical Problem

**Requirement**: Access points must transmit beacon frames at regular intervals, typically every 100 TU (Time Units) = 102.4 milliseconds.

**Challenge**: Maintaining precise timing in userspace

**Userspace Timing Precision:**
- Linux userspace: ~1-10ms jitter typical
- High-resolution timers (timerfd): Better but not guaranteed
- CSMA/CA adds unpredictable delays (must wait for clear channel)
- Cannot guarantee sub-millisecond precision

**From Linux Kernel Documentation:**

> "mac80211 cannot guarantee proper sequencing for beacons"

This is a critical limitation - the kernel explicitly states beacon sequencing cannot be guaranteed from userspace.

#### hostapd's Approach

**How does hostapd handle this?**

From investigation of hostapd source code:
1. hostapd generates beacon frames in userspace ([beacon.c](https://github.com/sensepost/hostapd-mana/blob/master/src/ap/beacon.c))
2. Passes beacon to kernel via `wpa_driver_nl80211_set_ap()` function
3. **Kernel/driver handles actual transmission timing**
4. Uses mac80211 subsystem beacon transmission support

**Key Finding**: hostapd does **NOT** do pure userspace beacon timing - it relies on kernel support.

#### DS Hardware Tolerance

**Critical Unknown**: Does Nintendo DS require precise beacon intervals?

**802.11 Standard:**
- Beacons can be delayed by CSMA/CA (channel contention)
- Some jitter is expected and tolerated
- Clients typically tolerate missed beacons (within reason)

**Nintendo DS Specifics:**
- No documented beacon timing requirements found
- Likely implements standard 802.11b tolerance
- **Untested with userspace beacon jitter**

**Risk Assessment**: MEDIUM-HIGH - Beacon timing may or may not work without kernel support.

**Verdict:** ⚠️ **UNCERTAIN** - Major unknown that requires hardware testing to validate.

---

### 4. Existing Implementations

#### Complete Userspace WEP AP

**Question**: Has anyone implemented a complete userspace WEP access point without kernel AP support?

**Answer**: ❌ **NO complete implementation found**

**What Exists:**

1. **hostapd** (The Standard):
   - Most complete AP implementation
   - **Uses kernel mac80211 for beacon timing**
   - Not pure userspace
   - Could extract components (beacon generation, state machine)

2. **Component Libraries**:
   - **lorcon**: Packet injection library
   - **libmoep**: 802.11 frame manipulation
   - **FWAP**: "Minimal access point" but incomplete
   - **wperf**: Frame injection/reception for mac80211

3. **Rogue AP Tools**:
   - Focus on evil twin attacks
   - Don't implement full functional AP
   - Rely on kernel for actual connectivity

**Conclusion**: Would be building a novel implementation from scratch with no proven precedent.

#### Real-Time WEP Tools

**airtun-ng** (from aircrack-ng suite):
- Decrypts WEP traffic in real-time
- Creates virtual network interface (at0)
- **Does NOT act as full access point**
- Only handles decryption bridge

**Verdict:** Components exist but no complete solution found.

---

### 5. Comparison to NiFi Bridge (ADR 007)

#### Complexity Comparison

| Component | Software WEP AP | NiFi Bridge | Winner |
|-----------|----------------|-------------|---------|
| **Protocol Layer** | 802.11 + WEP | NiFi protocol | NiFi (simpler) |
| **Management Frames** | Must implement beacons, probe, auth, assoc | Not needed | NiFi (none needed) |
| **State Machine** | Full 802.11 (3 states + transitions) | NiFi protocol (simpler) | NiFi (simpler) |
| **Encryption** | WEP encrypt/decrypt | None needed | NiFi (no encryption) |
| **Timing Requirements** | Precise beacon timing (~100ms) | Flexible packet timing (16ms @ 60Hz) | NiFi (flexible) |
| **Kernel Cooperation** | May need kernel for beacons | Pure userspace with libpcap | NiFi (userspace) |
| **DHCP Server** | Required for IP assignment | Not needed for basic operation | NiFi (optional) |
| **NAT/Routing** | Required for internet | Only for internet bridge feature | NiFi (optional) |
| **Network Stack** | Full TCP/IP stack | Can operate without | NiFi (simpler) |
| **Development Time** | 20-28 weeks | 17-24 weeks | NiFi (faster) |
| **Precedent** | None found | WMB project proves it works | NiFi (proven) |
| **Risk Level** | HIGH (untested beacon timing) | MEDIUM (proven concept) | NiFi (lower risk) |

#### Advantages of Software WEP AP

1. **Universal DS Compatibility**: Works with ANY DS software
   - All DS games (commercial + homebrew)
   - Standard WiFi infrastructure mode
   - DS expects normal WEP AP behavior

2. **Standard Protocol**: Uses standard 802.11 + WEP
   - Well-documented protocol
   - Familiar to developers
   - Standard networking tools work

3. **Full Internet Access**: Could provide standard internet connectivity
   - DS gets IP address via DHCP
   - Full TCP/IP networking
   - Would work like traditional router

#### Disadvantages of Software WEP AP

1. **Higher Complexity**: Significantly more complex than NiFi bridge
   - 802.11 management layer
   - WEP encryption layer
   - Full state machine
   - DHCP/NAT configuration

2. **Beacon Timing Uncertainty**: Critical unknown
   - No precedent of userspace beacon timing
   - Kernel documentation says it can't guarantee sequencing
   - DS tolerance unknown
   - May require kernel module development

3. **No Proven Precedent**: Building from scratch
   - hostapd uses kernel support
   - No complete userspace implementation exists
   - Higher risk of failure

4. **Same Hardware Limitations**: No advantage
   - Still requires monitor mode adapter
   - Same platform constraints as NiFi bridge
   - No improvement in compatibility

5. **Longer Development Time**: 3-4 weeks additional
   - More components to implement
   - More testing required
   - More debugging complexity

6. **DHCP/NAT Required**: Cannot function without
   - NiFi bridge doesn't need DHCP for basic operation
   - Adds system configuration complexity
   - More user setup steps

7. **No Advanced Features**: Limited extensibility
   - Cannot easily add spectator mode
   - Cannot add custom room status features
   - Cannot extend protocol

#### Advantages of NiFi Bridge (ADR 007)

1. **95% Confidence on Linux**: Proven by WMB project
2. **17-24 Weeks Development**: Faster to implement
3. **Simpler Implementation**: No 802.11 management, no encryption
4. **Pure Userspace**: No kernel dependencies
5. **Perfect for Target Use Case**: NiFi homebrew games
6. **Advanced Features**: Spectator mode, room status, custom extensions
7. **Development Can Start Immediately**: Phase 1-3 roadmap defined

#### Disadvantages of NiFi Bridge

1. **NiFi Games Only**: Doesn't work with non-NiFi DS software
   - Note: Commercial DS games require Nintendo WFC servers (defunct)
   - So this limitation mainly affects non-NiFi homebrew

---

### 6. Technical Challenges

#### Challenge 1: Beacon Timing and Precision

**Problem**: Beacons must be sent at regular ~100ms intervals

**Userspace Constraints:**
- Linux scheduling: not real-time
- Timer precision: ~1-10ms jitter
- CSMA/CA delays: unpredictable (must wait for clear channel)
- Context switching: adds latency

**Possible Solutions:**

1. **High-Resolution Timers** (timerfd):
   - Better than standard timers
   - Still has jitter
   - Not hard real-time

2. **Real-Time Scheduling** (SCHED_FIFO):
   - Requires root privileges
   - Can improve but not eliminate jitter
   - May impact system performance

3. **Kernel Module**:
   - Could provide precise timing
   - Defeats purpose of userspace solution
   - Adds complexity and platform issues

**DS Tolerance Testing Needed:**
- Unknown if DS tolerates 1-10ms jitter
- Standard 802.11 tolerates some jitter (CSMA/CA)
- Would require empirical testing with real DS hardware

**Risk Assessment**: MEDIUM-HIGH - May work, may not; requires testing to know.

---

#### Challenge 2: Sequence Number Management

**Problem**: From [mac80211 documentation](https://docs.kernel.org/networking/mac80211-injection.html):

> "mac80211 cannot guarantee proper sequencing for beacons"

**IEEE 802.11 Requirement:**
- Every frame must have sequence number
- Sequence numbers must increment
- Receivers may reject out-of-order frames

**Implementation Requirements:**
- Track sequence counter in userspace
- Manually insert into all management frames
- Coordinate with data frame sequence numbers
- Handle rollover (12-bit counter, wraps at 4096)

**Mitigation**:
- Implement sequence counter in application
- Insert manually into radiotap header
- Test with DS to ensure acceptance

**Risk Assessment**: MEDIUM - Solvable but adds implementation complexity.

---

#### Challenge 3: 802.11 State Machine Implementation

**Required State Transitions:**

Based on [802.11 Association Process](https://documentation.meraki.com/MR/Wi-Fi_Basics_and_Best_Practices/802.11_Association_Process_Explained):

```
1. Beacon transmission (continuous background)
   ↓
2. Client sends Probe Request → AP sends Probe Response
   ↓
3. Client sends Authentication Request → AP sends Authentication Response
   ↓ (WEP Shared Key: additional challenge/response here)
4. Client sends Association Request → AP sends Association Response
   ↓
5. Client in State 3 (can send data frames)
   ↓
6. WEP-encrypted data frames exchanged
   ↓
7. Client or AP sends Deauthentication (disconnect)
```

**WEP Shared Key Authentication (4-way handshake):**
```
Client                                  AP
  |                                     |
  |-- Authentication Request ------->  |
  |    (Algorithm: Shared Key)         |
  |                                     |
  |<-- Authentication Response ------  |
  |    (Challenge Text)                |
  |                                     |
  |-- Authentication Request ------->  |
  |    (Encrypted Challenge)           |
  |                                     |
  |<-- Authentication Response ------  |
  |    (Success/Failure)               |
```

**Implementation Complexity:**
- More complex than NiFi protocol state machine
- NiFi only has: SCAN → ROOM, JOIN → ACCEPT/DENY, CLIENT announcements
- 802.11 has more states and transitions

**Comparison to NiFi Protocol:**

| Protocol | States | Commands | Complexity |
|----------|--------|----------|------------|
| **802.11 + WEP** | 3 states | 7+ frame types | High |
| **NiFi** | Simple peer model | 9 commands | Medium |

**Risk Assessment**: MEDIUM - Well-documented but more complex than NiFi.

---

#### Challenge 4: DHCP and Network Configuration

**Requirements for Functional AP:**

1. **DHCP Server** (required):
   - IP address assignment
   - Subnet configuration
   - Gateway configuration
   - DNS server configuration
   - Lease management

2. **NAT/Routing** (for internet access):
   - iptables/nftables rules
   - IP forwarding
   - Masquerading

3. **DNS Forwarding**:
   - Forward DNS queries to internet
   - Or run local DNS resolver

**Comparison to NiFi Bridge:**
- NiFi bridge doesn't need DHCP for basic operation
- Only needs NAT/routing if providing internet bridge feature
- Software WEP AP cannot function without DHCP

**User Experience Impact:**
- More complex setup
- Requires root privileges
- More potential for configuration errors

**Risk Assessment**: LOW-MEDIUM - Well-understood but adds complexity.

---

#### Challenge 5: Platform Compatibility

**Same Limitations as NiFi Bridge:**

Since both approaches use monitor mode + packet injection:

| Platform | Monitor Mode | Injection | Beacons | Overall |
|----------|--------------|-----------|---------|---------|
| **Linux** | ✅ Excellent | ✅ Full support | ⚠️ Userspace timing | 85-90% |
| **Windows** | ⚠️ Limited adapters | ⚠️ Limited adapters | ⚠️ Untested | 60-65% |
| **macOS** | ⚠️ Native limited | ❌ Need external | ⚠️ Untested | 50-55% |

**New Concern: Beacon Transmission**
- Monitor mode adapters support data frame injection
- Beacon injection support varies by chipset
- Some drivers may have restrictions

**Recommended Adapters** (same as ADR 007):
- Atheros AR9271 (TP-Link TL-WN722N v1)
- Ralink RT3070, RT3071, RT5370, RT5572
- Realtek RTL8812AU (Alfa AWUS036ACH)

**Verdict:** Platform compatibility similar to NiFi bridge, possibly slightly worse due to beacon requirements.

---

### 7. Development Estimate

#### Phase Breakdown

**Phase 1: WEP Implementation** (2 weeks)
- Implement RC4 encryption using OpenSSL
- WEP key derivation (IV + key)
- CRC-32 checksum
- Encryption/decryption functions
- Performance testing
- **Deliverable**: Working WEP encrypt/decrypt library

**Phase 2: Management Frame Generation** (3 weeks)
- Radiotap header construction
- Beacon frame with proper Information Elements
- Probe request/response
- Authentication frames (open + shared key)
- Association request/response
- Sequence number management
- **Deliverable**: Management frame generator library

**Phase 3: Beacon Transmission** (2-3 weeks)
- High-resolution timer implementation
- Beacon injection via libpcap
- Timing precision measurement
- Test with DS hardware (can DS see beacons?)
- Optimize timing
- **Deliverable**: Beacon transmission system (with DS validation)

**Phase 4: 802.11 State Machine** (4-5 weeks)
- Full state machine implementation
- Probe request/response handler
- Authentication handler (open + WEP shared key)
- Association handler
- Client tracking (AID assignment)
- Deauthentication handling
- **Deliverable**: Complete AP state machine

**Phase 5: WEP Data Frame Handling** (2-3 weeks)
- Encrypt outgoing data frames
- Decrypt incoming data frames
- IV management and tracking
- Frame routing between clients
- Broadcast/multicast handling
- **Deliverable**: WEP data frame handling

**Phase 6: Network Integration** (2-3 weeks)
- DHCP server setup (dnsmasq)
- IP address assignment
- NAT/routing configuration
- DNS forwarding
- Internet connectivity testing
- **Deliverable**: Functional internet access

**Phase 7: Cross-Platform Support** (3-4 weeks)
- Windows port using Npcap
- macOS port with external adapter
- Platform-specific driver setup
- Testing on all platforms
- **Deliverable**: Multi-platform support

**Phase 8: Testing and Polish** (2-3 weeks)
- DS compatibility testing (multiple firmware versions)
- Multiple DS devices
- Range testing
- Stability testing (long-running sessions)
- Performance optimization
- Documentation
- **Deliverable**: Production-ready application

**Total Estimated Timeline: 20-28 weeks**

**Comparison:**
- **Software WEP AP**: 20-28 weeks
- **NiFi Bridge (ADR 007)**: 17-24 weeks
- **Difference**: 3-4 weeks longer

---

## Decision

### Software WEP Access Point: INVESTIGATING - NOT RECOMMENDED

**Technical Feasibility**: CONDITIONAL YES (60-65% confidence)

The software WEP AP approach is **technically possible** but faces significant challenges:

**Feasible Components:**
- ✅ WEP encryption/decryption (95% confidence)
- ✅ Management frame generation (90% confidence)
- ✅ Management frame injection (85% confidence)
- ⚠️ Beacon timing precision (70% confidence)
- ✅ 802.11 state machine (80% confidence)
- ⚠️ DS compatibility (65% confidence)

**Blocker**: Beacon timing uncertainty without kernel support is a significant risk.

### Why Not Recommended

#### 1. No Proven Precedent

- **hostapd** (the gold standard) uses kernel mac80211 support
- No complete userspace WEP AP implementation found
- Would be building novel implementation from scratch
- Higher risk of unforeseen issues

**Contrast**: NiFi bridge has **WMB project** as proof of concept

#### 2. Higher Complexity, Same Limitations

- Both approaches require monitor mode + injection hardware
- Both have same platform limitations (Linux best, Windows/macOS conditional)
- Software WEP AP adds:
  - 802.11 management layer
  - WEP encryption layer
  - Full state machine
  - DHCP/NAT requirements
- **No advantage in compatibility**

**Result**: More work for same hardware constraints

#### 3. Beacon Timing Uncertainty

- Critical component with no userspace precedent
- Linux kernel explicitly says "cannot guarantee beacon sequencing"
- DS tolerance unknown
- May require kernel module (defeats purpose)

**Risk**: May implement everything and find beacons don't work reliably

#### 4. Wrong Problem for Target Use Case

**Target**: NiFi homebrew games (from nifitest repository)

- Commercial DS games need Nintendo WFC servers (defunct)
- Software WEP AP doesn't solve that
- NiFi bridge is **perfect fit** for NiFi homebrew
- Software WEP AP solves broader problem but not the immediate need

**Result**: Solving wrong problem

#### 5. NiFi Bridge Already Validated

From ADR 007:
- 95% confidence on Linux
- WMB project proves feasibility
- 17-24 weeks development (3-4 weeks faster)
- Phase 1-3 roadmap defined
- Can start development immediately

**Result**: Better option already available

#### 6. Longer Development Timeline

- Software WEP AP: 20-28 weeks
- NiFi Bridge: 17-24 weeks
- **Difference**: 3-4 weeks additional development

**Result**: More time for less certainty

---

## Recommendation: Defer to NiFi Bridge (ADR 007)

### Recommended Path

**Phase 1: Implement NiFi Bridge (Current - ADR 007)**
- 95% confidence, proven precedent
- 17-24 weeks development
- Perfect for target use case (NiFi homebrew)
- Lower risk, faster time to value

**Phase 2: Re-evaluate After NiFi Bridge Complete**
- Gain experience with DS WiFi behavior
- Learn DS timing tolerances and quirks
- Understand real-world hardware limitations
- Make informed decision with actual data

**Phase 3: Consider Software WEP AP (If Needed)**
- With NiFi bridge working, have foundation of knowledge
- Can test beacon timing with real DS
- Can validate all assumptions
- Lower risk with empirical data

**Alternative: Hardware Solution**
- Raspberry Pi dedicated WEP AP device
- More reliable than software-only solution
- Works with all computers (platform-independent)
- Could be future product offering

---

## Consequences

### Positive Outcomes (If Pursued)

1. **Universal DS Compatibility**: Would work with any DS software
2. **Standard Protocol**: Uses familiar WiFi infrastructure
3. **Full Feature Set**: Complete internet access capability
4. **Learning Experience**: Deep understanding of 802.11 protocol

### Negative Outcomes (If Pursued)

1. **High Risk**: No precedent, beacon timing uncertain
2. **Longer Development**: 3-4 weeks additional time
3. **Higher Complexity**: More components to implement and debug
4. **Same Hardware Limitations**: No improvement over NiFi bridge
5. **Opportunity Cost**: Delays NiFi bridge development
6. **May Not Work**: Beacon timing may fail with DS hardware

### By Deferring to NiFi Bridge

1. **Lower Risk**: 95% confidence, proven by WMB
2. **Faster Delivery**: 3-4 weeks sooner
3. **Perfect Fit**: Ideal for NiFi homebrew target use case
4. **Foundation for Future**: Learn DS behavior for later decisions
5. **Advanced Features**: Can add spectator mode, room status, etc.
6. **Development Can Start Now**: Phase 1-3 roadmap ready

---

## Alternatives Considered

### Alternative 1: Hybrid Approach (Considered but Complex)

**Description**: Implement NiFi bridge first, then add WEP AP mode as secondary feature

**Pros**:
- Get both solutions eventually
- Learn from NiFi bridge experience
- Lower risk incremental approach

**Cons**:
- Significant additional development (20+ weeks after NiFi bridge)
- Maintenance burden of two protocols
- May never need WEP AP if NiFi bridge sufficient

**Decision**: Defer decision until after NiFi bridge complete

---

### Alternative 2: Kernel Module for Beacon Timing (Rejected)

**Description**: Implement kernel module to handle precise beacon timing

**Pros**:
- Solve beacon timing problem definitively
- Could provide better performance

**Cons**:
- Significantly more complex (kernel development)
- Platform-specific (Linux only)
- Requires kernel compilation/installation
- Security concerns (kernel code)
- Poor user experience

**Rejection Reason**: Defeats purpose of userspace solution, too complex

---

### Alternative 3: Hardware Solution - Raspberry Pi (Deferred)

**Description**: Pre-configured Raspberry Pi that runs hostapd with WEP

**Pros**:
- Uses proven hostapd with kernel support
- Works with all computers (platform-independent)
- Can run 24/7 if needed
- Reliable and battle-tested

**Cons**:
- Requires hardware purchase (~$50-75)
- Not pure software solution
- Additional device to carry

**Decision**: Viable future option, but pursue software-first approach

---

## Related Documents

- [ADR 006: WEP WiFi Access Point Investigation](006-wifi-connectivity-solutions.md) - Traditional WEP AP rejected
- [ADR 007: NiFi Protocol Bridge](007-nifi-protocol-bridge.md) - Recommended approach (investigating)
- [Protocol Specification](../protocol-specification.md) - NiFi protocol details
- [ADR 001: NiFi Protocol Implementation](001-nifi-protocol-implementation.md) - Core protocol design

---

## References

### Monitor Mode and Packet Injection

- [Monitor mode - Wikipedia](https://en.wikipedia.org/wiki/Monitor_mode)
- [How to use packet injection with mac80211 — Linux Kernel documentation](https://docs.kernel.org/networking/mac80211-injection.html)
- [802.11 Packet Injection for Windows - CodeProject](https://www.codeproject.com/Articles/28713/802-11-Packet-Injection-for-Windows)

### hostapd Implementation

- [hostapd: IEEE 802.11 AP Authenticator](https://w1.fi/hostapd/)
- [hostapd Linux documentation](https://wireless.docs.kernel.org/en/latest/en/users/documentation/hostapd.html)
- [hostapd-mana/beacon.c](https://github.com/sensepost/hostapd-mana/blob/master/src/ap/beacon.c) - Beacon generation
- [Linux and BIP - beacon creation](https://wifidiving.substack.com/p/linux-and-bip-configuration-and-beacon)

### WEP and RC4 Encryption

- [Wired Equivalent Privacy - Wikipedia](https://en.wikipedia.org/wiki/Wired_Equivalent_Privacy)
- [WEP - UCSD Crypto Projects](https://mathweb.ucsd.edu/~crypto/Projects/DavidChang/WEP.htm)
- [RC4 - Wikipedia](https://en.wikipedia.org/wiki/RC4)
- [OpenSSL EVP Symmetric Encryption](https://wiki.openssl.org/index.php/EVP_Symmetric_Encryption_and_Decryption)
- [GitHub - jonjon98/WEP-RC4-Cracking](https://github.com/jonjon98/WEP-RC4-Cracking)
- [GitHub - ng256/ARC4](https://github.com/ng256/ARC4)

### Aircrack-ng Suite

- [Aircrack-ng](https://www.aircrack-ng.org/)
- [airdecap-ng documentation](https://www.aircrack-ng.org/doku.php?id=airdecap-ng)
- [airtun-ng documentation](https://www.aircrack-ng.org/doku.php?id=airtun-ng)
- [How to crack a WEP key and decrypt live traffic](https://www.netexpertise.eu/en/systems/linux/crack-wep-key-and-decrypt-live-traffic.html)

### 802.11 Protocol

- [The IEEE 802.11 State Machine](https://dot11ap.wordpress.com/the-ieee-802-11-state-machine/)
- [802.11 Association Process - Cisco Meraki](https://documentation.meraki.com/MR/Wi-Fi_Basics_and_Best_Practices/802.11_Association_Process_Explained)
- [Understanding 802.11 State Machine - Aruba](https://blogs.arubanetworks.com/industries/understanding-802-11-state-machine/)
- [Beacon frame - Wikipedia](https://en.wikipedia.org/wiki/Beacon_frame)
- [IEEE 802.11b - Wikipedia](https://en.wikipedia.org/wiki/IEEE_802.11b-1999)

### Rogue AP and Frame Injection

- [4ARMED - Forging WiFi Beacon Frames Using Scapy](https://www.4armed.com/blog/forging-wifi-beacon-frames-using-scapy/)
- [Rogue AP - KaliTut](https://kalitut.com/rogue-ap-fake-access-points/)
- [WIFI-ARSENAL GitHub](https://github.com/merlinepedra/WIFI-ARSENAL)
- [GitHub - anyfi/wperf](https://github.com/anyfi/wperf)

### Nintendo DS Requirements

- [Compatible Wireless Modes - Nintendo Support](https://en-americas-support.nintendo.com/app/answers/detail/a_id/498/)
- [Nintendo DS notes](https://helpful.knobs-dials.com/index.php/Nintendo_DS_notes)

---

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-11-22 | Initial ADR - Software WEP AP investigated, not recommended |

---

## Next Steps

1. ✅ Complete investigation of software WEP AP approach (COMPLETE)
2. ✅ Document findings in ADR 008 (COMPLETE)
3. ✅ Update ADR 007 with cross-reference (PENDING)
4. 🔄 Continue with NiFi bridge approach (ADR 007) as recommended path
5. ⏳ After NiFi bridge complete: Re-evaluate with empirical DS WiFi knowledge
6. ⏳ Future consideration: Raspberry Pi hardware WEP AP solution

---

**Current Status**: Investigation complete. Software WEP AP is technically feasible (60-65% confidence) but NOT RECOMMENDED due to higher complexity, no proven precedent, beacon timing uncertainty, and longer development time compared to NiFi bridge approach. **Recommendation: Proceed with NiFi bridge (ADR 007).**

**END OF ADR 008**
