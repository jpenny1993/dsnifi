# ADR 006: WEP WiFi Access Point Investigation

**Status:** Rejected

**Date:** 2025-11-22

**Decision**: Creating a cross-platform WEP WiFi access point application is not feasible due to operating system-level restrictions on Windows and macOS. While technically possible on Linux, this approach cannot provide a universal solution for NDS connectivity.

---

## Context

### Problem Statement

Nintendo DS devices require WEP encryption or open networks for WiFi connectivity. Modern operating systems and WiFi networks predominantly use WPA2/WPA3, making it difficult for NDS users to connect without:

1. Configuring routers with legacy WEP encryption (insecure and increasingly unsupported)
2. Creating separate open guest networks (no encryption, security risk)
3. Using dedicated WEP-compatible hardware (rare and expensive)

### Motivation

**User Request**: Create a cross-platform (Linux, Windows, macOS) application that can host a WEP WiFi access point, allowing NDS devices to connect and access the internet through the host computer.

**Benefits if Feasible**:
- NDS connectivity without router reconfiguration
- Portable solution (works anywhere with a computer)
- Controlled access (software can implement authentication)
- Bridge to modern networks via computer's internet connection

### Investigation Scope

This ADR documents a technical investigation into the feasibility of creating cross-platform software that acts as a WEP WiFi access point for Nintendo DS connectivity.

---

## Investigation Findings

### Platform-Specific WiFi AP Capabilities

#### **Linux: POSSIBLE (with significant caveats)**

**Software Options:**
- **hostapd**: Primary tool for creating software access points on Linux
- **NetworkManager**: GUI-based hotspot creation
- **create_ap/linux-wifi-hotspot**: Community wrapper scripts

**WEP Support Status:**
- WEP disabled by default in hostapd since version 2.10 (2021-2022)
- Can be re-enabled by compiling hostapd from source with `CONFIG_WEP` flag
- Linux kernel's mac80211 subsystem still contains WEP code
- NetworkManager can still create WEP hotspots through GUI on some distributions

**Working Solutions:**
- Multiple confirmed reports of successful NDS connections via Linux hostapd
- Raspberry Pi projects successfully hosting WEP access points for NDS
- Configuration requires:
  ```
  auth_algs=3              # Shared key authentication
  wep_default_key=0
  wep_key0="your_key_here"
  hw_mode=b                # 802.11b mode required for NDS
  ```

**Challenges:**
- Most Linux distributions ship hostapd without CONFIG_WEP enabled
- Requires root privileges for hostapd execution
- Need to configure DHCP server (dnsmasq)
- Need to configure NAT/routing (iptables/nftables)
- WiFi hardware must support AP mode

**Verdict:** ✅ **FEASIBLE** on Linux but requires:
- Custom-compiled hostapd with WEP support OR NetworkManager
- Root access
- Compatible WiFi hardware
- Network configuration expertise

#### **Windows: NOT FEASIBLE**

**Legacy Hosted Network (Windows 7-8):**
- Never supported WEP encryption
- Only supported WPA2-PSK with AES cipher suite
- This was a mandatory hardware requirement by Microsoft

**Modern Mobile Hotspot (Windows 10-11):**
- Legacy hosted network feature deprecated/removed in Windows 11
- Modern Mobile Hotspot only supports WPA2/WPA3
- Windows 11 explicitly blocks WEP as "obsolete and insecure"
- No netsh workarounds available

**API Status:**
- `WlanHostedNetwork` API: Deprecated, no WEP support
- `NetworkOperatorTetheringManager` (WinRT): No WEP support
- WiFi Direct: No WEP support

**Tested Approaches:**
- `netsh wlan set hostednetwork`: No WEP encryption option
- Third-party tools: All rely on same underlying APIs that block WEP
- Virtual WiFi technology: Hardcoded to require WPA2

**Verdict:** ❌ **NOT FEASIBLE** - Windows cannot create WEP access points through any official or documented API. OS-level policy prevents WEP for security reasons.

#### **macOS: NOT FEASIBLE (on modern versions)**

**Historical Support:**
- macOS 10.7 (Lion) and earlier: Internet Sharing supported WEP only
- macOS 10.8 (Mountain Lion): Added WPA2, WEP still accessible via hidden menu
- macOS 10.11.4 (El Capitan): WEP workaround stopped working
- macOS 10.13 (High Sierra): WEP completely removed

**CoreWLAN Framework:**
- Can create ad-hoc networks (IBSS mode) with WEP using `startIBSSMode()`
- Example code shows `CWIBSSModeSecurity.WEP104` support
- However: Creates peer-to-peer networks, not true infrastructure mode APs
- Only works with internal Airport WiFi hardware
- Does not support USB WiFi adapters

**Verdict:** ❌ **NOT FEASIBLE** - Modern macOS versions (10.11+) cannot create WEP infrastructure mode access points. IBSS mode is not suitable for NDS connectivity (requires infrastructure mode).

---

### WEP Support Availability Summary

| Platform | WEP in AP Mode | Status | Notes |
|----------|----------------|--------|-------|
| **Linux** | ✅ YES | Available but disabled by default | Requires recompiling hostapd or using NetworkManager |
| **Windows** | ❌ NO | Completely removed | Never supported in hosted network API |
| **macOS** | ❌ NO | Removed since 10.11.4 | Deprecated as insecure protocol |

---

### Technical Challenges

#### **Hardware Requirements**
- WiFi chipset must support AP mode (not all do)
- Driver must support AP mode in the operating system
- Sufficient power for simultaneous AP operation and network bridging

**Recommended Chipsets for Linux:**
- Atheros: AR9271, AR9280 (excellent AP mode support)
- Realtek: RTL8812AU, RTL8192CU (mixed results)
- Intel: Variable support depending on model

#### **Network Configuration Complexity**
- DHCP server setup and management
- NAT configuration for internet sharing
- DNS forwarding setup
- Firewall rules for packet forwarding
- Potential conflicts with NetworkManager/systemd-networkd

#### **Permission Requirements**
Root/administrator access required for:
- hostapd execution (Linux)
- Network interface configuration
- Firewall/routing rules
- DHCP server operation

#### **Nintendo DS Specific Requirements**
- **Mode**: 802.11b only (not mixed b/g)
- **Authentication**: Shared Key Authentication
- **Basic rates**: Must advertise 1 and 2 Mbps
- **Channel**: Manual selection (1-11 for US, 1-13 for Europe)
- **WEP key**: 5 characters (64-bit) or 13 characters (128-bit)

**Known Compatibility Issues:**
- DS won't connect if AP is in mixed b/g mode
- Requires proper shared key authentication setup
- DHCP must be properly configured with appropriate lease times

---

## Decision

### Cross-Platform WEP Access Point: REJECTED ❌

**Reason**: A truly cross-platform WEP WiFi access point application is **not feasible** because:

1. **Windows Absolute Blocker**: Windows operating system prevents WEP access point creation at the OS policy level. No API or workaround exists.

2. **macOS Absolute Blocker**: Modern macOS versions (10.11+, released 2015) have completely removed WEP support for access points.

3. **Linux Limited Viability**: While technically possible on Linux, it requires:
   - Custom compilation of hostapd with deprecated WEP support
   - Or using NetworkManager (if WEP still enabled in distribution)
   - Root privileges and network configuration expertise
   - Compatible WiFi hardware

**Impact**: Only 1 out of 3 target platforms (Linux) can support this approach, and even then with significant setup complexity. This fails the "cross-platform" requirement.

### What About Linux-Only Solution?

**Considered**: Creating a Linux-only WEP access point application

**Decision**: Also rejected in favor of alternative approach (see ADR 007)

**Rationale**:
- Limits user base to Linux users only
- Requires hardware with AP mode support
- Must bundle custom-compiled hostapd
- Network configuration complexity
- WEP is fundamentally insecure (crackable in minutes)
- Alternative approaches may be superior (see below)

---

## Alternatives Considered

### Alternative 1: Platform-Specific Solutions (Rejected)

**Description**: Create different implementations per platform:
- Linux: hostapd with WEP
- Windows: Provide error message + guide to Linux VM
- macOS: Provide error message + guide to Linux VM

**Pros:**
- Leverages platform capabilities where available
- Clear messaging about limitations

**Cons:**
- Poor user experience for Windows/macOS users
- Requires VM setup (complex for average user)
- Still requires custom hostapd compilation for Linux
- Doesn't solve the core problem for 2/3 of users

**Rejection Reason**: Fails to meet cross-platform requirement, poor UX.

---

### Alternative 2: Raspberry Pi Dedicated Device (Deferred)

**Description**: Pre-configured Raspberry Pi that acts as WEP access point

**Pros:**
- Works with all computer platforms (device is independent)
- Portable and low power
- Can run 24/7 if needed
- Pre-configured, no user setup

**Cons:**
- Requires additional hardware purchase (~$50-75)
- Not "software-only" solution
- Device to carry and maintain
- Overkill for occasional use

**Decision**: Viable but outside scope of software-only solution. Could be future product.

---

### Alternative 3: OpenWRT/DD-WRT Router Configuration Tool (Deferred)

**Description**: Cross-platform tool that generates configuration files for OpenWRT/DD-WRT routers

**Pros:**
- Works on all platforms (just generates config files)
- Leverages existing hardware (old router)
- Reliable and battle-tested firmware
- Can be left running permanently

**Cons:**
- Requires compatible router hardware
- Requires router flashing (technical, risky)
- Not a direct solution, more of a guide/helper
- Doesn't provide connectivity itself

**Decision**: Useful complementary tool but doesn't solve core problem.

---

### Alternative 4: NiFi Protocol Bridge (RECOMMENDED - See ADR 007)

**Description**: Instead of creating a WEP access point, create a bridge application that communicates directly with NDS devices using the existing NiFi protocol over raw 802.11 frames.

**Key Differences:**
- No need for WEP or any encryption layer
- Uses WiFi monitor mode instead of AP mode
- PC acts as virtual NDS client in NiFi network
- Can bridge NiFi packets to internet via TCP/IP tunnel

**Advantages Over WEP AP:**
- No encryption constraints (WEP not needed)
- Monitor mode more widely supported than AP mode
- Direct access to NiFi protocol for manipulation
- Maintains game compatibility (games work unmodified)
- Can support existing NiFi features (spectator mode, room status, etc.)

**Challenges:**
- Must implement full NiFi protocol stack
- Requires dual WiFi adapters (one for monitor mode, one for internet)
- Platform limitations still exist but less severe
- Custom implementation needed (no existing tools)

**Decision**: INVESTIGATING - See ADR 007 for full technical feasibility assessment.

---

## Consequences

### For Users Seeking NDS WiFi Connectivity

**Impact**: Cross-platform WEP access point solution will not be developed.

**Recommended Alternatives:**

1. **NiFi Protocol Bridge** (See ADR 007)
   - Primary recommended approach
   - Direct NiFi protocol support
   - Better cross-platform viability
   - Under investigation

2. **Linux-Only Solutions** (For technical users)
   - Use linux-wifi-hotspot tool with NetworkManager
   - Or compile custom hostapd with WEP support
   - Raspberry Pi dedicated device

3. **Router-Based Solutions**
   - Flash old router with OpenWRT/DD-WRT
   - Configure WEP access point on dedicated hardware
   - Most reliable but requires additional device

4. **Network Configuration** (Simplest but least secure)
   - Create guest network on existing router
   - Use open network (no encryption) with MAC filtering
   - Isolate on separate VLAN if possible

### For Development Team

**Lessons Learned:**
- OS-level security policies can block entire approaches
- Cross-platform compatibility requires checking each platform thoroughly
- Deprecated protocols have limited support across platforms
- Alternative approaches may be superior to obvious solutions

**Next Steps:**
- Focus investigation on NiFi protocol bridge (ADR 007)
- If NiFi bridge feasible, prioritize that development path
- Document WEP AP limitations in user-facing documentation
- Maintain hardware solution recommendations (Raspberry Pi, router flashing)

---

## Related Documents

- [ADR 005: WPA2/WPA3 WiFi Support Investigation](005-wpa2-wpa3-wifi-support.md) - Previous WiFi investigation
- [ADR 007: NiFi Protocol Bridge](007-nifi-protocol-bridge.md) - Alternative approach (in progress)
- [Protocol Specification](../protocol-specification.md) - NiFi protocol details

---

## References

### Cross-Platform WEP Support Research

**Linux:**
- [Debian Bug Report: hostapd no WEP support](https://groups.google.com/g/linux.debian.bugs.dist/c/Nd677H87dWE)
- [hostapd ChangeLog](https://w1.fi/cgit/hostap/plain/hostapd/ChangeLog) - WEP disabled by default in 2.10
- [Create NDS-Compatible Hotspot on Linux](https://gbatemp.net/threads/create-an-nds-compatible-hotspot-on-linux.543283/)
- [linux-wifi-hotspot GitHub](https://github.com/lakinduakash/linux-wifi-hotspot)

**Windows:**
- [Windows Hosted Network WEP Discussion](https://social.technet.microsoft.com/Forums/windows/en-US/023fd3a2-7c2c-408b-9fe1-db90477b50ae/windows-7-virtual-hosted-network-wep-encryption)
- [Windows 11 WEP Not Supported](https://answers.microsoft.com/en-us/windows/forum/all/why-cant-i-make-my-win11-hotspot-pick-wep/98c5fa14-33ef-4370-818b-5e7cb67221fa)
- [Windows 11 Hosted Network Deprecated](https://learn.microsoft.com/en-us/answers/questions/5552619/hosted-network-supported-says-no)

**macOS:**
- [macOS Internet Sharing WEP Removal](https://superuser.com/questions/504915/how-to-share-internet-on-mac-with-wep)
- [CoreWLAN Apple Documentation](https://developer.apple.com/documentation/corewlan)

### Nintendo DS WiFi Specifications
- [Compatible Wireless Modes and Security Types - Nintendo Support](https://en-americas-support.nintendo.com/app/answers/detail/a_id/498/)
- [GBAtemp: NDS WiFi Notes](https://gbatemp.net/threads/notes-on-nds-wifi-connectivity-that-fix-issues-you-may-encounter.571110/)
- [DS-Homebrew WiFi Wiki](https://wiki.ds-homebrew.com/ds-index/wifi)

---

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-11-22 | Initial ADR - Cross-platform WEP AP rejected |

---

## Next Steps

1. ✅ Complete WEP AP investigation (COMPLETE - REJECTED)
2. 🔄 Investigate NiFi protocol bridge approach (IN PROGRESS - See ADR 007)
3. ⏳ Based on ADR 007 outcome, determine final implementation path
4. ⏳ Document recommended alternatives in user-facing documentation

---

**Current Status**: ADR closed. Cross-platform WEP WiFi access point is not feasible. Investigation continues with alternative approach documented in ADR 007.

**END OF ADR 006**
