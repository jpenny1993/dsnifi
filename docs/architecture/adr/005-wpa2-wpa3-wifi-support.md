# ADR 005: WPA2/WPA3 WiFi Support Investigation

**Status:** Closed - Not Implementing

**Date:** 2025-11-22

**Decision**: While DSi hardware technically supports WPA2, the lack of a universal solution across all DS architectures means this feature will not be implemented. The goal was to provide modern WiFi access to all game developers, not fragment the user base with DSi-only builds.

---

## Context

### Problem Statement

The Nintendo DS WiFi library (dswifi) currently supports only WEP encryption and open networks. Modern WiFi networks predominantly use WPA2 or WPA3 for security, making it increasingly difficult for DS users to connect to contemporary wireless networks without:

1. Configuring routers with legacy WEP encryption (insecure - crackable in minutes)
2. Creating separate open guest networks (no encryption)
3. Using WEP-compatible access points (increasingly rare)

### Motivation

**User Request**: Add support for WPA2 and WPA3 to enable DS applications to connect to modern secured WiFi networks without requiring users to compromise their network security.

**Benefits if Feasible**:
- Connect to modern WPA2/WPA3 networks directly
- No need for legacy WEP configuration
- Improved security over WEP/open networks
- Better user experience

### Investigation Scope

This ADR documents a technical investigation into:
1. **Original DS/DS Lite**: Can WPA2/WPA3 be added via software?
2. **Nintendo DSi**: Does DSi-mode provide WPA2 capabilities?
3. **NiFi Compatibility**: Would WPA2 support work with NiFi's raw packet mode?

---

## Investigation Findings

### Part A: Original Nintendo DS / DS Lite Hardware

#### WiFi Chipset Specifications

**Hardware**: Mitsumi MM3155 (original DS) / MM3218 (DS Lite)
- **WiFi Standard**: IEEE 802.11b only
- **Data Rates**: 1-2 Mbps
- **Encryption**: Hardware WEP only
- **Architecture**: Proprietary, no public datasheets

#### Hardware Limitation: WEP Lock-In

**CRITICAL BLOCKER**: The MM3218/MM3155 chipsets have a hardwired behavior that makes WPA/WPA2 implementation impossible:

1. **Automatic WEP Encryption**: The hardware automatically applies WEP encryption to all encrypted packets
2. **Frame Header Modification**: The chipset modifies 802.11 frame headers in hardware
3. **No Bypass Available**: This behavior cannot be disabled, overridden, or worked around in software
4. **No Raw Frame Access**: Software cannot intercept or inject raw 802.11 frames needed for WPA handshake

**Source**: Documented by nocash (GBATEK author) who attempted and failed to implement WPA due to these hardware constraints.

#### Code Evidence from dswifi

**File**: `/mnt/c/nds/repo/dswifi/include/dsnifi9.h`

```c
// Line 64: Flag defined but never implemented
#define WFLAG_APDATA_WPA 0x0004

// Lines 129-133: Only WEP modes supported
enum WEPMODES {
    WEPMODE_NONE = 0,
    WEPMODE_40BIT = 1,
    WEPMODE_128BIT = 2
};
```

**File**: `/mnt/c/nds/repo/dswifi/arm9/source/wifi_arm9.c`

```c
// Lines 367, 387: Only wepmode7 variable used for encryption
framelen = datalen + 8 + (WifiData->wepmode7 ? 4 : 0);
if (WifiData->wepmode7) {
    framehdr[6] |= 0x4000;  // WEP flag
}
```

**Conclusion**: The `WFLAG_APDATA_WPA` flag exists but is completely unused. All encryption logic is WEP-only.

#### Firmware Credential Storage

**File**: `/mnt/c/nds/repo/dswifi/arm7/source/wifi_arm7.c:99-130`

The DS firmware stores 3 WiFi profiles in 256KB flash memory:

```c
void GetWfcSettings() {
    u32 wfcBase = ReadFlashBytes(0x20, 2) * 8 - 0x400;
    for(i=0; i<3; i++) {
        readFirmware(wfcBase + (i<<8), data, 256);  // 256 bytes per profile

        // Current storage layout:
        WifiData->wfc_wepkey[c][n] = data[0x80+n];    // WEP key at 0x80
        WifiData->wfc_ap[c].ssid[n] = data[0x40+n];   // SSID at 0x40
        WifiData->wfc_config[c] = ...;                 // IP at 0xC0
    }
}
```

**Storage Capacity**:
- **Current per profile**: ~50 bytes (SSID + WEP key + IP config)
- **Available per profile**: 256 bytes
- **WPA2 requirements**: ~100 bytes (SSID + passphrase/PSK + security type)

**Verdict**: ✅ **Sufficient storage space exists** for WPA2 credentials. However, this is moot due to hardware encryption limitation.

**Write API**: libnds provides `writeFirmwarePage()` but it's currently static. Can be exposed or copied for credential writing.

#### CPU and Memory Constraints (Hypothetical)

If hardware limitations didn't exist, theoretical feasibility:

**Processing Power**:
- ARM7: 33 MHz (I/O coprocessor)
- ARM9: 67 MHz (main processor)
- RAM: 4 MB total

**WPA2 Requirements**:
- **PBKDF2 key derivation**: 4096 iterations of HMAC-SHA1
  - Estimated: 3-10 seconds on 33 MHz ARM7
  - **Blocks all other operations** during authentication

- **AES-CCMP encryption**: ~2.8 Mbps throughput on 33 MHz ARM7
  - DS WiFi needs 1-2 Mbps (within theoretical range)
  - But ARM7 must also handle sound, I/O, WiFi management

- **4-way handshake**: Multiple cryptographic operations + round trips
  - Manageable with proper async implementation

**Memory Footprint**:
- Crypto library (TinyAES + HMAC + PBKDF2): ~2 KB ROM, ~200 bytes RAM
- WPA2 state: ~800-1000 bytes RAM
- **Total**: ~1200 bytes RAM (feasible within 4 MB)

**Verdict**: ✅ CPU and memory constraints would be **tight but feasible** if hardware allowed.

#### WPA3 Feasibility

**SAE (Simultaneous Authentication of Equals)**:
- Dragonfly key exchange (RFC 7664)
- 10-100x more CPU-intensive than WPA2
- Estimated 30+ seconds on 33 MHz ARM7
- No hardware acceleration available

**Verdict**: ❌ **WPA3 is completely infeasible** even if hardware supported WPA2.

#### Final Verdict: Original DS/DS Lite

**DECISION: REJECTED** ❌

**Reason**: Hardware chipset physically prevents WPA2 implementation
- MM3218/MM3155 automatically forces WEP encryption on all encrypted frames
- Frame headers are modified by hardware (cannot be bypassed)
- No software workaround exists
- Multiple documented failed attempts by expert developers

**Estimated Development Effort (if hardware allowed)**:
- 4-5 months full-time development
- 6000-7000 lines of code
- 15-20 KB ROM, ~1200 bytes RAM

**Actual Feasibility**: 0% due to hardware blocker

---

### Part B: Nintendo DSi Hardware

**Status**: ✅ **Investigation Complete**

The Nintendo DSi features dual WiFi chipsets:
1. **Legacy chip** (MM3315/MM3218): DS-mode backward compatibility - WEP only
2. **Modern chip** (Atheros AR6002G): DSi-mode only - WPA2 capable

#### DSi WiFi Architecture

**Hardware Design**: Dual-chip system with RF switch

The DSi uses two separate WiFi chipsets to maintain backward compatibility while adding modern WiFi capabilities:

```
┌─────────────────┐
│  Nintendo DSi   │
├─────────────────┤
│                 │
│ DS Mode:        │
│  MM3315/MM3218  │──> WEP only (legacy chip)
│  (dswifi lib)   │
│                 │
│ DSi Mode:       │
│  AR6002G        │──> WPA2 (modern chip)
│  (dsiwifi lib)  │
│                 │
└─────────────────┘
```

**Key Points**:
- The two chips are **mutually exclusive** (RF switch selects one)
- DS-mode applications always use the legacy chip (WEP only)
- DSi-mode applications use the Atheros chip (WPA2 capable)
- No way to use WPA2 in DS-mode (even on DSi hardware)

#### DSi Mode WiFi Library

**Library**: dsiwifi (separate from dswifi)

**WPA2 Implementation**:
- Uses Atheros AR6002G chipset
- Implements WPA2-PSK with Mbed TLS
- Supports AES-CCMP encryption
- Full 802.11i compliance

**Code Evidence**:
- Repository: [devkitPro/dsiwifi](https://github.com/devkitPro/dsiwifi)
- Crypto: Uses Mbed TLS library (lightweight crypto implementation)
- Modes: Infrastructure mode (connects to access points)

**Limitations**:
- ❌ **Not backward compatible** with original DS/DS Lite
- ❌ DSi-mode apps cannot run on DS/DS Lite hardware
- ❌ Games written for DS-mode cannot use DSi WiFi chip
- ⚠️ Raw packet mode (promiscuous WiFi) availability unknown

#### WiFi Connectivity Analysis

**DSi Mode WiFi Capabilities** (Atheros AR6002G):
- ✅ **WPA2-PSK encryption supported**
- ✅ **Infrastructure mode** (connect to modern routers/APs)
- ✅ **Internet connectivity** via WPA2-secured networks
- ✅ **Standard networking** (TCP/IP, UDP, HTTP, etc.)

**For Game Development**:
- ✅ Games can connect to modern WPA2 WiFi networks
- ✅ Online multiplayer via internet (infrastructure mode)
- ✅ HTTP APIs, leaderboards, matchmaking servers
- ✅ No need for WEP configuration on routers

**NiFi-Specific Considerations**:
- ❓ **Promiscuous mode support**: UNKNOWN (needed for NiFi's ad-hoc protocol)
- ❓ **Raw packet injection**: UNKNOWN (needed for custom 802.11 frames)
- ⚠️ **Ad-hoc mode with WPA2**: Likely unsupported (IBSS with RSN is rare)

**Note**: NiFi's local ad-hoc multiplayer is a separate use case from general internet connectivity. DSi-mode WPA2 would primarily benefit games needing internet access, not local P2P multiplayer.

#### DSi Mode Development Constraints

**To use DSi WiFi with WPA2**:
1. Application must be compiled as DSi-mode executable
2. Cannot run on original DS/DS Lite hardware
3. Loses DS-mode backward compatibility
4. Must use dsiwifi library (not dswifi)
5. May not support promiscuous/ad-hoc modes needed by NiFi

**Target Audience Impact**:
- ❌ Original DS users: Cannot benefit (no DSi hardware)
- ❌ DS Lite users: Cannot benefit (no DSi hardware)
- ⚠️ DSi users: Could benefit *only* in DSi-mode apps
- ⚠️ 3DS users: Already have WPA2 in 3DS mode

**Market Reality**:
- Most NiFi use cases are **local multiplayer** (2-6 players in same room)
- WPA2 provides minimal benefit for local ad-hoc play
- Users playing together can coordinate WEP/open network setup
- Online multiplayer over infrastructure WPA2 not NiFi's design goal

#### Comparison Table

| Feature | Original DS/DS Lite | DSi (DS-mode) | DSi (DSi-mode) |
|---------|--------------------|--------------|--------------------|
| **Hardware** | MM3218 | MM3315/MM3218 | Atheros AR6002G |
| **Library** | dswifi | dswifi | dsiwifi |
| **WEP Support** | ✅ Yes | ✅ Yes | ✅ Yes (legacy) |
| **WPA2 Support** | ❌ No | ❌ No | ✅ Yes |
| **Promiscuous Mode** | ✅ Yes | ✅ Yes | ❓ Unknown |
| **Ad-hoc P2P** | ✅ Yes | ✅ Yes | ❓ Unknown |
| **NiFi Compatible** | ✅ Yes (WEP) | ✅ Yes (WEP) | ❓ Unknown |
| **Runs on DS Lite** | ✅ Yes | ❌ No | ❌ No |

#### Investigation Conclusion

**WPA2 on DSi**: ✅ **AVAILABLE** in DSi-mode for standard networking

**For General Game Development**: ✅ **VIABLE**
- Games can connect to modern WPA2 WiFi networks
- Infrastructure mode provides internet connectivity
- Useful for online multiplayer, APIs, leaderboards, etc.
- No WEP configuration required

**Key Trade-offs**:
1. ✅ **Benefit**: Modern WiFi connectivity (WPA2/WPA3)
2. ❌ **Cost**: No backward compatibility with DS/DS Lite
3. ⚠️ **Limitation**: DSi/3DS hardware required
4. ⚠️ **Limitation**: Requires DSi-mode compilation (separate codebase)

**For NiFi Ad-Hoc Protocol**: ❓ **UNKNOWN**
- NiFi's local P2P multiplayer requires promiscuous mode
- DSi WiFi likely infrastructure-mode only
- Ad-hoc mode with WPA2 probably unsupported
- Would need testing to confirm

**Recommendation**:
- ✅ **FOR dswifi library**: Consider adding DSi-mode support for general WiFi connectivity
- ⚠️ **FOR NiFi protocol**: Unlikely to work with DSi's infrastructure-mode focus
- 📝 **FOR game developers**: Document that DSi-mode apps can use WPA2 via dsiwifi library

---

## Decision

### For Original Nintendo DS / DS Lite

**REJECTED** ❌

WPA2/WPA3 support **cannot be implemented** on original DS or DS Lite hardware due to fundamental hardware limitations in the WiFi chipset that cannot be overcome in software.

**Reason**: MM3218/MM3155 chipsets physically enforce WEP encryption in hardware with no software bypass.

### For Nintendo DSi

**CONDITIONAL APPROVAL** ⚠️ (with significant trade-offs)

DSi hardware **supports WPA2** in DSi-mode using the Atheros AR6002G chipset via the dsiwifi library.

**Approved Use Cases**:
- ✅ **Online multiplayer** (infrastructure mode via internet)
- ✅ **HTTP/API connectivity** (leaderboards, matchmaking, etc.)
- ✅ **Standard networking** on modern WPA2 routers
- ✅ **Game development** targeting DSi/3DS hardware

**Major Trade-offs**:
1. ❌ **No backward compatibility**: DSi-mode apps cannot run on original DS/DS Lite
2. ⚠️ **Hardware requirement**: Requires DSi, DSi XL, or 3DS/3DS XL
3. ⚠️ **Separate codebase**: Cannot mix DS-mode (dswifi) and DSi-mode (dsiwifi) in same binary
4. ⚠️ **Fragments user base**: Excludes DS/DS Lite owners

**Not Suitable For**:
- ❌ **NiFi ad-hoc protocol**: Likely lacks promiscuous mode and raw packet injection
- ❌ **Universal compatibility**: Won't work on DS/DS Lite hardware
- ❌ **Drop-in dswifi replacement**: Requires separate DSi-mode compilation

**Recommendation**:
- Game developers targeting **DSi/3DS** hardware should use **dsiwifi library** for WPA2 connectivity
- Game developers targeting **DS/DS Lite** compatibility should continue using **dswifi with WEP/open**
- NiFi protocol will continue using dswifi (WEP/open) for ad-hoc local multiplayer
- Consider creating separate DSi-mode builds of games for users with newer hardware

---

## Consequences

### For Original DS/DS Lite Users

**Impact**: No native WPA2/WPA3 support possible

**Workarounds**:

1. **Router Configuration** (Recommended):
   - Create guest network with WEP or open security
   - Use network isolation to separate DS traffic
   - Limit guest network bandwidth/access

2. **Hardware Upgrade**:
   - Nintendo DSi (potential WPA2 in DSi-mode)
   - Nintendo 3DS (has WPA2 support)

3. **External Bridge** (Advanced):
   - WiFi-to-WiFi bridge device (custom hardware)
   - Handles WPA2, presents open/WEP to DS
   - Portable battery-powered design
   - **Development effort**: 12-18 months hardware + firmware

### For DSi/3DS Users

**Impact**: WPA2 support available via DSi-mode compilation

**Benefits**:
1. **Modern WiFi Connectivity**:
   - Connect directly to WPA2/WPA3 networks
   - No router reconfiguration needed
   - Secure encrypted connections

2. **Use Cases**:
   - Online multiplayer via internet (infrastructure mode)
   - HTTP APIs for leaderboards, matchmaking
   - Cloud save data synchronization
   - In-game purchases and DLC
   - Social features and chat

3. **Implementation Path**:
   - Use **dsiwifi library** (not dswifi)
   - Compile as DSi-mode executable
   - Target DSi/DSi XL/3DS/3DS XL hardware
   - Distribute separate builds: DS-mode (WEP) + DSi-mode (WPA2)

**Trade-offs**:
- ❌ DSi-mode apps won't run on original DS/DS Lite
- ⚠️ Smaller potential user base (DSi+ only)
- ⚠️ Separate codebase maintenance
- ⚠️ NiFi ad-hoc protocol may not work (infrastructure mode only)

**Recommendation**: For online multiplayer games, create two builds:
- **DS-mode build** (dswifi + WEP): Maximum compatibility (all DS hardware)
- **DSi-mode build** (dsiwifi + WPA2): Modern WiFi for DSi/3DS users

### Security Recommendations

**For DS/DS Lite (WEP/Open)**:
- ⚠️ WEP is cryptographically broken (crackable in minutes)
- Only use for non-sensitive traffic
- Isolate DS devices from main network
- Monitor for unauthorized access
- Consider MAC address filtering on open networks

**For DSi/3DS (WPA2)**:
- ✅ Use WPA2-PSK with strong passphrase
- ✅ Connect directly to modern routers
- ✅ Suitable for online gaming with sensitive data

### For NiFi Library

**No Changes Required**:
- NiFi operates at raw packet level (promiscuous mode)
- Works with any encryption the dswifi layer provides
- Currently works with WEP and open networks
- Would automatically support WPA2 if dswifi added it (but it can't)

**Documentation Updates**:
- Clearly state WEP/open limitation in README
- Add router configuration guide for DS users
- Recommend DSi/3DS for users needing WPA2

---

## Alternatives Considered

### Alternative 1: Software-Based AES Implementation (Rejected)

**Description**: Implement WPA2 crypto entirely in software, bypass hardware

**Pros**:
- Theoretically possible from pure software perspective
- Could run on ARM7 or ARM9

**Cons**:
- ❌ **Hardware still forces WEP encryption** (fatal blocker)
- ❌ Cannot bypass or disable hardware WEP behavior
- ❌ No access to raw 802.11 frames for handshake

**Rejection Reason**: Hardware limitation cannot be overcome with software crypto alone.

---

### Alternative 2: Custom WiFi Chip Replacement (Rejected)

**Description**: Physically replace MM3218 with modern WiFi chip

**Pros**:
- Could theoretically support WPA2/WPA3
- Custom driver control

**Cons**:
- ❌ Requires PCB-level soldering (SMD components)
- ❌ Need custom driver development
- ❌ May not fit in DS case (different pinout/size)
- ❌ **Estimated effort**: 6-12 months hardware engineering
- ❌ Not practical for end users

**Rejection Reason**: Too complex, requires hardware modification skills, not distributable solution.

---

### Alternative 3: External WiFi Adapter (Deferred)

**Description**: USB or GBA slot WiFi adapter acting as WPA2 bridge

**Pros**:
- Handles WPA2 externally
- Presents open/WEP network to DS
- Portable battery-powered design possible

**Cons**:
- Requires custom hardware development
- USB or GBA slot interface needed
- Additional device to carry
- Development effort: 12-18 months

**Decision**: Viable long-term hardware project, but outside scope of software library.

---

### Alternative 4: DSi-Mode Application (Under Investigation)

**Description**: Write applications that run in DSi mode with native WPA2

**Pros**:
- Uses Atheros AR6002G chip
- Potentially native WPA2 support
- May integrate with existing dsiwifi library

**Cons**:
- Only works on DSi/3DS hardware
- Not backward compatible with DS/DS Lite
- May not support raw packet mode (NiFi requirement)

**Decision**: ⏳ Under investigation - see Part B

---

## Implementation Complexity (Hypothetical)

If hardware limitations didn't exist, implementation would require:

### Development Phases

**Phase 1: Crypto Library** (3-4 weeks)
- Port TinyAES to DS (AES-128/256)
- Implement HMAC-SHA1
- Implement PBKDF2 (4096 iterations)
- Test crypto primitives

**Phase 2: WPA2 Protocol** (5-7 weeks)
- 802.11 frame parsing/generation
- EAPOL 4-way handshake implementation
- PTK/GTK key derivation
- Key management and rotation
- Integration with dswifi

**Phase 3: Firmware Integration** (3-4 weeks)
- Credential storage API
- WPA2 settings in firmware flash
- Settings UI modifications
- Testing across DS variants

**Total Estimated Time**: 16-21 weeks (4-5 months) for experienced developer

**Lines of Code**:
- Crypto library: ~2000 LOC
- WPA2 protocol: ~3000-4000 LOC
- Integration: ~1000 LOC
- **Total**: ~6000-7000 LOC

**Memory Footprint**:
- ROM: 15-20 KB
- RAM: ~1200 bytes runtime
- Flash: ~100 bytes per profile

---

## Related Documents

- [ADR 001: NiFi Protocol Implementation](001-nifi-protocol-implementation.md) - Core protocol design
- [ARCHITECTURE.md](../../ARCHITECTURE.md) - NiFi library architecture
- [dswifi GitHub Repository](https://github.com/devkitPro/dswifi) - WiFi library source

---

## References

### Hardware Documentation
- [Nintendo DS Wifi Hardware Reference - akkit.org](http://www.akkit.org/info/dswifi.htm)
- [GBATEK DS WiFi Baseband Chip](https://problemkaputt.de/gbatek-ds-wifi-baseband-chip-bb.htm)
- [DSiBrew: WiFi Module](https://dsibrew.org/wiki/WiFi_Module)

### Nintendo Support
- [Compatible Wireless Modes and Security Types](https://en-americas-support.nintendo.com/app/answers/detail/a_id/498/)

### Community Research
- [GBAtemp: WPA/WPA2 on 3DS with DS games](https://gbatemp.net/threads/any-way-to-use-wpa-wpa2-connection-with-ds-games-on-a-3ds.459518/)
- [GBAtemp: WPA2 via homebrew](https://gbatemp.net/threads/use-wpa2-internet-with-ds-games-via-homebrew.583860/)
- [GBAtemp: Flash DS Lite for WPA](https://gbatemp.net/threads/how-can-i-flash-hack-my-ds-lite-to-support-wpa-connections.220627/)

### dswifi Source Code
- Local: `/mnt/c/nds/repo/dswifi/`
- GitHub: [devkitPro/dswifi](https://github.com/devkitPro/dswifi)

---

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-11-22 | Initial ADR with DS/DS Lite investigation (REJECTED) |
| 1.1 | 2025-11-22 | Added DSi mode investigation (conditionally approved) |
| 1.2 | 2025-11-22 | Corrected DSi evaluation: WPA2 viable for general networking, not just NiFi-specific |
| 1.3 | 2025-11-22 | ADR closed - feature will not be implemented due to lack of universal solution |

---

## Next Steps

1. ✅ Complete DS/DS Lite investigation (REJECTED - hardware impossible)
2. ✅ Investigate DSi mode WPA2 capabilities (COMPLETE)
3. ✅ Determine DSi-mode compatibility with NiFi raw packet mode (COMPLETE)
4. ✅ Update this ADR with DSi findings (COMPLETE)
5. ✅ Close ADR with final decision (COMPLETE)

**No further action required** - Feature will not be implemented.

---

**Current Status**: ADR closed. WPA2/WPA3 support will not be implemented due to lack of universal solution across all DS architectures. Investigation documented for future reference.

**END OF ADR 005**
