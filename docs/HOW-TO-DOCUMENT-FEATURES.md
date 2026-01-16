# How to Document Features - Complete Guide

**Version:** 1.0
**Created:** 2025-01-22
**Purpose:** Standard process for documenting new features in the NiFi project

---

## Table of Contents

1. [Overview](#overview)
2. [Document Types](#document-types)
3. [Folder Structure](#folder-structure)
4. [Document Workflow](#document-workflow)
5. [Writing Guidelines](#writing-guidelines)
6. [Template Usage Guide](#template-usage-guide)
7. [Examples](#examples)
8. [Best Practices](#best-practices)
9. [Quick Reference](#quick-reference)

---

## Overview

### Why Consistent Documentation Matters

Every feature in the NiFi project requires **four types of documents**:
1. **Design Specification** - Architecture and requirements
2. **Implementation Guide** - Step-by-step coding instructions
3. **Design & Implementation Review** - Critical analysis before/during implementation
4. **Step-by-Step Guide** - Detailed implementation instructions with exact code

This ensures:
- ✅ Clear requirements before coding
- ✅ Structured implementation approach
- ✅ Critical issues identified early
- ✅ Future maintainers can understand decisions

### When to Create Documentation

**Before Writing Any Code:**
1. Create Design Specification
2. Create Implementation Review (analyze design)
3. Update design based on review findings

**During Implementation:**
4. Create Step-by-Step Implementation Guide
5. Update documents as you discover edge cases

**After Implementation:**
6. Create post-implementation review (if needed)
7. Document lessons learned

---

## Document Types

### 1. Design Specification (`*-design.md`)

**Purpose:** Defines WHAT to build and WHY

**Key Sections:**
- Executive Summary (what and why)
- Feature Requirements (functional and non-functional)
- Technical Architecture (how it works conceptually)
- Implementation Plan (phases and tasks)
- Testing Strategy
- Performance Considerations
- Security/Privacy implications
- Future Enhancements

**When to Write:** Before starting implementation

**Audience:** Developers, architects, stakeholders

**Example:** `spectator-mode-design.md`

---

### 2. Step-by-Step Implementation Guide (`*-implementation-guide.md`)

**Purpose:** Provides EXACT instructions for HOW to implement

**Key Sections:**
- Prerequisites and compatibility notes
- Phase-by-phase implementation
- Exact file locations and line numbers
- Before/after code examples
- Testing checkpoints after each phase
- Troubleshooting guide
- Quick reference

**When to Write:** During or just before implementation

**Audience:** Developers doing the implementation

**Example:** `spectator-mode-implementation-guide.md`

---

### 3. Design & Implementation Review (`*-review.md`)

**Purpose:** Identifies issues and validates approach BEFORE implementation

**Key Sections:**
- Critical issues identified
- Compatibility analysis (with other features)
- Code compatibility analysis
- Implementation risks (high/medium/low)
- Recommended approach (corrected phases)
- Testing considerations
- Decision points with recommendations

**When to Write:** After design spec, before implementation

**Audience:** Technical reviewers, implementers

**Example:** `room-status-review.md`

---

### 4. Post-Implementation Review (optional, `*-postmortem.md`)

**Purpose:** Documents what happened during implementation

**Key Sections:**
- What went well
- What went wrong
- Lessons learned
- Deviations from plan
- Performance results
- Future improvements

**When to Write:** After implementation completes

**Audience:** Future developers, team retrospectives

---

## Folder Structure

### Standard Layout

```
docs/
├── HOW-TO-DOCUMENT-FEATURES.md          (This guide)
├── templates/                            (Template files)
│   ├── design-spec-template.md
│   ├── implementation-guide-template.md
│   ├── review-template.md
│   └── README.md
├── planning/                             (All feature planning docs)
│   ├── feature-name/                     (One folder per feature)
│   │   ├── feature-name-design.md
│   │   ├── feature-name-implementation-guide.md
│   │   ├── feature-name-review.md
│   │   └── feature-name-step-by-step-guide.md  (alias or copy)
│   ├── another-feature/
│   │   ├── another-feature-design.md
│   │   └── ...
│   └── README.md                         (Index of all features)
├── architecture/                         (System-wide architecture docs)
│   ├── adr/                             (Architecture Decision Records)
│   │   ├── 001-decision-title.md
│   │   └── 002-another-decision.md
│   └── protocol-specification.md
└── development/                          (Development guides)
    ├── lessons-learned.md
    └── coding-standards.md
```

### Naming Conventions

**Feature Folder:** `kebab-case-feature-name/`
- Example: `spectator-mode/`, `room-status/`, `packet-encryption/`

**Design Spec:** `feature-name-design.md`
- Example: `spectator-mode-design.md`

**Implementation Guide:** `feature-name-implementation-guide.md`
- Example: `room-status-implementation-guide.md`

**Review:** `feature-name-review.md`
- Example: `spectator-mode-review.md`

---

## Document Workflow

### Standard Feature Development Process

```
1. PLANNING PHASE
   ↓
   Create Design Spec
   ↓
   Conduct Design Review
   ↓
   Update Design Spec (if needed)
   ↓

2. IMPLEMENTATION PHASE
   ↓
   Create Implementation Guide
   ↓
   Follow guide step-by-step
   ↓
   Update guide with discoveries
   ↓

3. TESTING PHASE
   ↓
   Follow test plans in documents
   ↓
   Document test results
   ↓

4. COMPLETION PHASE
   ↓
   Optional: Create post-mortem
   ↓
   Update docs/planning/README.md
```

### Typical Timeline

| Phase | Documents | Time |
|-------|-----------|------|
| Planning | Design Spec | 2-4 hours |
| Review | Review Document | 1-2 hours |
| Implementation | Implementation Guide | During coding |
| Testing | Test results in docs | With testing |
| **Total** | **All docs** | **~5-10% of implementation time** |

---

## Writing Guidelines

### General Principles

1. **Be Specific:** Use exact file paths, line numbers, and code examples
2. **Be Visual:** Include ASCII diagrams, state machines, and flowcharts
3. **Be Practical:** Focus on actionable information, not theory
4. **Be Complete:** Cover all edge cases and error scenarios
5. **Be Honest:** Document risks, limitations, and unknowns

### Code Examples

**Good:**
```c
// nifi_arm9.c - Line 150
// Add this after the existing SetupNiFiClient function
bool NiFi_CanPlayerJoin(char macAddress[13]) {
    u8 activeCount = CountActiveClients();
    return (activeCount < CLIENT_MAX);
}
```

**Bad:**
```c
// Add a function somewhere to check if players can join
```

### Technical Writing Style

- **Use Active Voice:** "Add the function" not "The function should be added"
- **Use Imperative:** "Create the struct" not "You should create the struct"
- **Be Concise:** Short sentences, clear meaning
- **Use Examples:** Show don't just tell
- **Avoid Jargon:** Or define it when necessary

### Testing Instructions

**Good:**
```
Test 1: Basic Join Flow (5 min)
1. Device A creates room (press A on menu)
2. Device B scans for rooms (press B on menu)
3. Verify B sees A's room in list
4. Device B joins room (press A on room name)
5. Verify B appears in A's client list

Pass Criteria: ✅ B successfully joins and appears in A's list
```

**Bad:**
```
Test joining rooms and make sure it works.
```

---

## Template Usage Guide

### Using design-spec-template.md

1. **Copy the template:**
   ```bash
   cp docs/templates/design-spec-template.md docs/planning/my-feature/my-feature-design.md
   ```

2. **Fill in the header:**
   - Set document version (start at 1.0)
   - Set creation date
   - Set status (Design Phase initially)
   - Estimate implementation time
   - Set complexity level

3. **Write Executive Summary:**
   - Explain what you're building (2-3 paragraphs)
   - List 3-5 use cases
   - Identify current limitations
   - Describe benefits after implementation

4. **Define Requirements:**
   - Break into Functional (FR1, FR2, etc.)
   - Add Non-Functional (NFR1, NFR2, etc.)
   - Be specific and testable

5. **Design Architecture:**
   - Create ASCII diagrams
   - Show state machines if applicable
   - Explain key architectural changes with code examples

6. **Plan Implementation:**
   - Break into logical phases (3-6 phases typical)
   - Estimate time per phase
   - List files to modify
   - Show example code changes

7. **Consider Performance:**
   - Estimate CPU impact
   - Estimate memory impact
   - Estimate battery impact
   - Suggest optimizations

8. **Plan Testing:**
   - Unit tests with code examples
   - Integration test scenarios
   - Performance test procedures

9. **Address Security/Privacy:**
   - Identify concerns
   - Propose mitigations

10. **Document Future Work:**
    - List enhancements out of scope
    - Explain benefits and approaches

---

### Using implementation-guide-template.md

1. **Copy the template:**
   ```bash
   cp docs/templates/implementation-guide-template.md docs/planning/my-feature/my-feature-implementation-guide.md
   ```

2. **Reference the design:**
   - Link to design spec in prerequisites
   - Note any changes from design during implementation

3. **Break implementation into phases:**
   - Mirror design doc phases OR
   - Reorganize based on implementation reality

4. **For each step, provide:**
   - Exact file path
   - Exact line number or landmark
   - "Find this code" block showing context
   - "Add this" or "Replace with" showing exact code
   - Explanation of what changed and why

5. **Add testing checkpoints:**
   - After each major step
   - After each phase
   - Include compile commands
   - Include expected behavior

6. **Write comprehensive tests:**
   - Functional correctness tests
   - Integration tests with other features
   - Edge case tests
   - Performance tests

7. **Create troubleshooting section:**
   - Common problems you encountered
   - Symptoms, causes, solutions
   - Debugging tips

8. **Add quick reference:**
   - Summary tables
   - Build commands
   - File change summary

---

### Using review-template.md

1. **Copy the template:**
   ```bash
   cp docs/templates/review-template.md docs/planning/my-feature/my-feature-review.md
   ```

2. **Define review scope:**
   - What are you reviewing? (design doc, code, plan)
   - When in the process? (pre/during/post implementation)

3. **Assess risk level:**
   - 🔴 HIGH RISK - blocking issues, will break things
   - 🟡 MEDIUM RISK - significant concerns, needs mitigation
   - 🟢 LOW RISK - minor issues, easy to fix

4. **Identify critical issues:**
   - Show problematic code/design
   - Explain what's wrong and why it matters
   - Provide corrected approach
   - Prioritize issues (High/Medium/Low)

5. **Analyze compatibility:**
   - With other features
   - With existing code
   - With hardware limitations
   - Show conflicts and solutions

6. **Check code compatibility:**
   - Verify functions exist
   - Verify data structures match
   - Verify global variables are safe
   - Table format for easy scanning

7. **Document risks:**
   - Group by severity
   - Explain impact
   - Propose mitigation

8. **Recommend approach:**
   - Correct the phases from design doc
   - Add preparation phase if needed
   - Show minimal viable implementation first

9. **Update test plans:**
   - Add missing test scenarios
   - Flag high-risk areas for extra testing

10. **Make decisions:**
    - Document key decision points
    - Present options
    - Recommend best option with rationale

---

## Examples

### Example 1: Simple Feature (Low Complexity)

**Feature:** Add packet rate control

**Documents Needed:**
- ✅ `packet-rate-control-design.md` (2 hours to write)
- ✅ `packet-rate-control-implementation-guide.md` (1 hour to write)
- ⚠️ `packet-rate-control-review.md` (optional, 30 min)

**Folder Structure:**
```
docs/planning/packet-rate-control/
├── packet-rate-control-design.md
└── packet-rate-control-implementation-guide.md
```

**Why Review is Optional:**
- Low complexity
- No cross-feature conflicts
- Straightforward implementation

---

### Example 2: Complex Feature (High Complexity)

**Feature:** Spectator mode

**Documents Needed:**
- ✅ `spectator-mode-design.md` (4 hours to write)
- ✅ `spectator-mode-review.md` (2 hours to write)
- ✅ `spectator-mode-implementation-guide.md` (3 hours to write)

**Folder Structure:**
```
docs/planning/spectator-mode/
├── spectator-mode-design.md
├── spectator-mode-review.md
└── spectator-mode-implementation-guide.md
```

**Why All Docs Needed:**
- High complexity
- Multiple cross-feature interactions
- Significant architectural changes
- High risk of compatibility issues

---

### Example 3: Feature with Cross-Dependencies

**Feature:** Room status system

**Documents Needed:**
- ✅ `room-status-design.md`
- ✅ `room-status-review.md` (CRITICAL - checks spectator compatibility)
- ✅ `room-status-implementation-guide.md`

**Special Notes in Review:**
- Analyze impact on spectator mode
- Verify MAC filtering doesn't break spectators
- Add integration tests

---

## Best Practices

### Documentation Best Practices

1. **Write Design Before Code**
   - Prevents rework
   - Identifies issues early
   - Gets team alignment

2. **Update Docs During Implementation**
   - Don't wait until the end
   - Document discoveries immediately
   - Update troubleshooting as you debug

3. **Use Real Code Examples**
   - Copy from actual implementation
   - Test code examples compile
   - Include full context (file, line number)

4. **Version Your Documents**
   - Increment version on significant changes
   - Note update date and what changed
   - Keep old versions in git history

5. **Link Documents Together**
   - Design refs → Implementation guide
   - Implementation guide refs → Design
   - Review refs → Both design and implementation

6. **Test Your Instructions**
   - Have someone else follow your guide
   - Note where they get stuck
   - Update based on feedback

### Code Documentation Best Practices

1. **Document Why, Not What**
   ```c
   // Good:
   // Bypass MAC filtering for spectators - they discover clients dynamically
   if (!IsSpectatorMode) { ... }

   // Bad:
   // Check if not spectator mode
   if (!IsSpectatorMode) { ... }
   ```

2. **Reference Design Docs in Code**
   ```c
   // Implements spectator client discovery as described in
   // spectator-mode-design.md Section 4.1 (Client Discovery)
   void UpdateSpectatorClientList(NiFiPacket *packet) { ... }
   ```

3. **Mark TODOs Clearly**
   ```c
   // TODO(spectator-mode): Add state synchronization packets
   // See spectator-mode-design.md "Future Enhancements"
   ```

4. **Document Cross-Feature Interactions**
   ```c
   // IMPORTANT: Room status MAC filtering checks !IsSpectatorMode
   // See room-status-review.md "Spectator Mode Compatibility"
   if (!IsSpectatorMode && currentRoomStatus == INGAME_CLOSED) { ... }
   ```

---

## Quick Reference

### Document Checklist

**Design Spec:**
- [ ] Header filled (version, date, status, time, complexity)
- [ ] Executive summary (what, why, use cases)
- [ ] Functional requirements (FR1, FR2, ...)
- [ ] Non-functional requirements (NFR1, NFR2, ...)
- [ ] Architecture diagrams
- [ ] Implementation phases with time estimates
- [ ] Test strategy
- [ ] Performance analysis
- [ ] Security/privacy considerations
- [ ] Future enhancements

**Implementation Guide:**
- [ ] Prerequisites and compatibility notes
- [ ] Phase-by-phase breakdown
- [ ] Step-by-step instructions with exact locations
- [ ] Before/after code examples
- [ ] Testing checkpoints
- [ ] Troubleshooting guide
- [ ] Quick reference section

**Review:**
- [ ] Risk assessment (High/Medium/Low)
- [ ] Critical issues identified with priorities
- [ ] Compatibility analysis
- [ ] Code compatibility check
- [ ] Implementation risks
- [ ] Recommended approach (corrected phases)
- [ ] Testing considerations
- [ ] Decision points with recommendations
- [ ] Go/No-Go recommendation

### Time Estimates

| Document Type | Simple Feature | Medium Feature | Complex Feature |
|---------------|----------------|----------------|-----------------|
| Design Spec | 1-2 hours | 2-3 hours | 3-5 hours |
| Review | 30 min | 1 hour | 2-3 hours |
| Implementation Guide | 1 hour | 2 hours | 3-4 hours |
| **Total** | **~3 hours** | **~5 hours** | **~8-12 hours** |

### Common Sections

**All Documents Should Have:**
- Document version and date
- Table of contents
- Clear section headings
- Code examples where relevant
- Cross-references to other docs

**Design Specs Need:**
- Architecture diagrams
- State machines (if applicable)
- Packet formats (if applicable)
- API function signatures

**Implementation Guides Need:**
- Exact file paths and line numbers
- Testing checkpoints
- Troubleshooting section
- Build commands

**Reviews Need:**
- Risk indicators (🔴🟡🟢)
- Issue priorities (HIGH/MEDIUM/LOW)
- Compatibility analysis
- Decision points

---

## Getting Started

### Quick Start Guide

1. **Choose a feature to implement**

2. **Create feature folder:**
   ```bash
   mkdir -p docs/planning/my-feature-name
   ```

3. **Copy design template:**
   ```bash
   cp docs/templates/design-spec-template.md \\
      docs/planning/my-feature-name/my-feature-name-design.md
   ```

4. **Fill out design spec** (1-4 hours)

5. **Copy review template:**
   ```bash
   cp docs/templates/review-template.md \\
      docs/planning/my-feature-name/my-feature-name-review.md
   ```

6. **Conduct review** (30 min - 2 hours)

7. **Update design based on review**

8. **Copy implementation template:**
   ```bash
   cp docs/templates/implementation-guide-template.md \\
      docs/planning/my-feature-name/my-feature-name-implementation-guide.md
   ```

9. **Fill out implementation guide as you code**

10. **Update docs/planning/README.md with your feature**

---

## Questions?

If you have questions about this documentation process:

1. Look at existing examples:
   - `docs/planning/spectator-mode/` (complex feature)
   - `docs/planning/room-status/` (medium feature)

2. Check the templates:
   - `docs/templates/design-spec-template.md`
   - `docs/templates/implementation-guide-template.md`
   - `docs/templates/review-template.md`

3. Read this guide again (it's comprehensive!)

4. Ask the team or leave a TODO in the document

---

**Remember:** Good documentation saves time in the long run. It prevents bugs, reduces rework, and helps future maintainers understand your decisions. Invest the time upfront!
