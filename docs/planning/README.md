# Feature Planning Documentation

This folder contains planning documents for all NiFi features.

## Documentation Structure

Each feature should have its own folder with the following documents:

- **`feature-name-design.md`** - Design specification (WHAT and WHY)
- **`feature-name-implementation-guide.md`** - Step-by-step implementation instructions (HOW)
- **`feature-name-review.md`** - Critical analysis and risk assessment (REVIEW)
- **`feature-name-step-by-step-guide.md`** - Alternative name for implementation guide (optional alias)

See [docs/HOW-TO-DOCUMENT-FEATURES.md](../HOW-TO-DOCUMENT-FEATURES.md) for complete documentation guidelines.

---

## Feature Index

### Implemented Features

#### Spectator Mode
**Status:** Documented (Implementation Pending)
**Complexity:** High
**Folder:** `spectator-mode/`

A passive observation mode that allows a Nintendo DS to observe an active NiFi game without joining or transmitting any packets.

**Documents:**
- ✅ [Design Specification](spectator-mode-design.md)
- ✅ [Implementation Guide](spectator-mode-implementation-guide.md)

**Key Features:**
- Zero network footprint (truly passive)
- Room discovery and targeting
- Dynamic client discovery
- Event handler support

---

#### Room Status System
**Status:** Documented (Implementation Pending)
**Complexity:** Medium-High
**Folder:** `room-status/`

A room state management system that controls player joining based on game state (lobby open/closed, in-game with drop-in support).

**Documents:**
- ✅ [Design Specification](room-status/room-status-design.md)
- ✅ [Design & Implementation Review](room-status/room-status-review.md)
- ✅ [Implementation Guide](room-status/room-status-implementation-guide.md)

**Key Features:**
- Four room states (LOBBY_OPEN, LOBBY_CLOSED, INGAME_OPEN, INGAME_CLOSED)
- MAC-based returning player support
- Performance optimization via packet filtering
- Spectator mode compatibility

---

### Planned Features

[Add planned features here as they are proposed]

---

### Deprecated/Archived Features

[Add deprecated features here if any are removed]

---

## Adding a New Feature

1. **Create a feature folder:**
   ```bash
   mkdir -p docs/planning/my-feature-name
   ```

2. **Copy templates from `docs/templates/`:**
   ```bash
   cp docs/templates/design-spec-template.md docs/planning/my-feature-name/my-feature-name-design.md
   cp docs/templates/review-template.md docs/planning/my-feature-name/my-feature-name-review.md
   cp docs/templates/implementation-guide-template.md docs/planning/my-feature-name/my-feature-name-implementation-guide.md
   ```

3. **Fill out the templates** following the guidelines in [HOW-TO-DOCUMENT-FEATURES.md](../HOW-TO-DOCUMENT-FEATURES.md)

4. **Update this README** with your feature in the appropriate section

---

## Feature Documentation Standards

### Required Documents

**All features MUST have:**
- ✅ Design Specification
- ✅ Implementation Guide

**Complex features SHOULD have:**
- ✅ Design & Implementation Review

**Completed features MAY have:**
- ✅ Post-Implementation Review/Postmortem

### Naming Conventions

- Folder: `kebab-case-feature-name/`
- Design: `feature-name-design.md`
- Implementation: `feature-name-implementation-guide.md`
- Review: `feature-name-review.md`

### Document Quality Standards

Each document should include:
- Version number and date
- Table of contents
- Clear section headings
- Code examples where relevant
- Cross-references to related docs
- Testing procedures
- Troubleshooting guidance

---

## Feature Complexity Levels

### Low Complexity
- **Time:** 1-4 hours implementation
- **Documents:** Design + Implementation Guide
- **Example:** Simple API additions, minor optimizations

### Medium Complexity
- **Time:** 4-12 hours implementation
- **Documents:** Design + Review + Implementation Guide
- **Example:** Feature enhancements, protocol additions

### High Complexity
- **Time:** 12+ hours implementation
- **Documents:** Design + Review + Implementation Guide + potential Postmortem
- **Example:** Major features, architectural changes, cross-feature integrations

---

## Feature Dependencies

### Spectator Mode ⟷ Room Status
- **Dependency:** Room Status MUST check `!IsSpectatorMode` before MAC filtering
- **Reason:** Spectators need to observe all packets from target room
- **Impact:** Breaking this breaks spectator mode entirely
- **Reference:** `room-status-review.md` Section "Spectator Mode Compatibility"

### [Add other feature dependencies here as they are discovered]

---

## Quick Reference

### Feature Development Workflow

1. **Planning Phase**
   - Create design specification
   - Conduct design review
   - Update design based on review

2. **Implementation Phase**
   - Create implementation guide
   - Follow guide step-by-step
   - Update guide with discoveries

3. **Testing Phase**
   - Follow test plans in documents
   - Document test results

4. **Completion Phase**
   - Optional: Create post-mortem
   - Update this README with feature status

### Common Pitfalls

1. **Not documenting before coding** → Leads to rework
2. **Ignoring cross-feature impacts** → Breaks existing features
3. **Skipping review for "simple" features** → Misses critical issues
4. **Not updating docs during implementation** → Documentation becomes stale

### Best Practices

- ✅ Write design before code
- ✅ Review design before implementation
- ✅ Update docs as you go
- ✅ Test instructions on someone else
- ✅ Link related documents together
- ✅ Document WHY, not just WHAT

---

## Need Help?

- **Complete Guide:** [docs/HOW-TO-DOCUMENT-FEATURES.md](../HOW-TO-DOCUMENT-FEATURES.md)
- **Templates:** [docs/templates/](../templates/)
- **Examples:** See `spectator-mode/` or `room-status/` folders

---

**Last Updated:** 2025-01-22
**Maintainer:** NiFi Development Team
