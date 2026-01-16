# [Feature Name] Implementation Review - Critical Issues & Approach

**Document Version:** 1.0
**Created:** YYYY-MM-DD
**Status:** [Pre-Implementation Review | Mid-Implementation Review | Post-Implementation Review]
**Reviewers:** [Name(s) or "Technical Analysis"]

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Critical Issues Identified](#critical-issues-identified)
3. [Compatibility Analysis](#compatibility-analysis)
4. [Code Compatibility Analysis](#code-compatibility-analysis)
5. [Implementation Risks](#implementation-risks)
6. [Recommended Approach](#recommended-approach)
7. [Testing Considerations](#testing-considerations)
8. [Decision Points](#decision-points)

---

## Executive Summary

### Review Scope

[Description of what was reviewed - design document, implementation plan, actual code, etc.]

### Overall Assessment

[Risk Level: 🔴 HIGH RISK | 🟡 MEDIUM RISK | 🟢 LOW RISK] - [Brief assessment]

**Key Findings:**
- [Finding 1 with risk indicator]
- [Finding 2 with risk indicator]
- [Finding 3 with risk indicator]

### Critical Issues Summary

1. **[Issue Category]**: [Brief description]
2. **[Issue Category]**: [Brief description]
3. **[Issue Category]**: [Brief description]

---

## Critical Issues Identified

### Issue 1: [Issue Name] [⚠️ PRIORITY LEVEL]

**Current Code/Design:**
```c
// Show the problematic code or design
```

**Problem:** [Detailed explanation of what's wrong]

**CRITICAL PROBLEM:** [Why this is a problem, what will break]

**Correct Approach:** [How to fix it]

```c
// Show the corrected code
```

**Recommendation:**
- ❌ [What NOT to do]
- ✅ [What TO do]
- ✅ [Additional guidance]

---

### Issue 2: [Issue Name] [⚠️ PRIORITY LEVEL]

**Current Situation:** [Description]

**Problem:** [What's wrong]

**Also:** [Additional concerns]

**Correct Approach:** [Solution]

**Benefits:**
- ✅ [Benefit 1]
- ✅ [Benefit 2]
- ✅ [Benefit 3]

**Recommendation:**
- [Action item 1]
- [Action item 2]

---

### Issue 3: [Issue Name] [Priority Level]

[Continue with additional issues...]

---

## Compatibility Analysis

### Overview

[Description of compatibility concerns - with other features, with existing code, with hardware, etc.]

**Key Compatibility Requirements:**
1. [Requirement 1]
2. [Requirement 2]
3. [Requirement 3]

---

### Critical Conflict: [Feature A] vs [Feature B] [⚠️ HIGHEST PRIORITY]

#### The Problem

**[Feature A Description]:**
```c
// Code showing Feature A
```

**Why This Breaks [Feature B]:**
1. [Reason 1]
2. [Reason 2]
3. [Reason 3]
4. **Result: [What breaks]**

**Impact:** [Risk level and description]

---

#### The Solution: [Solution Name]

**REQUIRED MODIFICATION:**

```c
// Show the solution code with clear annotations
// Explain what the fix does
```

**Key Change:** [What changed and why]

---

### Why [Feature B] Needs This

**Design Principles:**
1. [Principle 1]
2. [Principle 2]
3. [Principle 3]

**Critical Dependency:** [Explanation]

---

### Integration Requirements

#### Required Variables

[List all shared variables and state]

```c
// Show variable declarations
```

**Cross-Feature Initialization:**
- [Initialization requirement 1]
- [Initialization requirement 2]

---

### Testing Integration

#### [Feature A] + [Feature B] Test Scenarios

**Test 1: [Test Name]** (X min)
1. [Step]
2. [Step]
3. [Step]

**Pass Criteria:** ✅ [Criteria]

**Test 2: [Test Name]** (X min)
[Test description]

**Pass Criteria:** ✅ [Criteria]

---

### Summary of Required Changes

#### [Document/Code Section Name]

1. **[Location]:** ✅ [What to add/change]
   ```c
   // Example code
   ```

2. **[Location]:** ✅ [What to add/change]

#### [Another Document/Code Section]

1. **[Location]:** ✅ [What to add/change]

---

## Code Compatibility Analysis

### Functions Referenced vs Current Implementation

| Function | Status | Location | Notes |
|----------|--------|----------|-------|
| `ExistingFunc()` | ✅ EXISTS | file.c:100 | [Notes] |
| `AnotherFunc()` | ✅ EXISTS | file.c:200 | [Notes] |
| `NewFunc()` | ❌ NEW | To be added | **Must implement** |

**Summary:** [Overview of findings]

---

### Data Structures

| Structure | Current Definition | Plan's Assumption | Compatible? |
|-----------|-------------------|-------------------|-------------|
| `StructName` | [Current fields] | [Assumed fields] | ✅ YES / ⚠️ PARTIAL / ❌ NO |

**Analysis:** [Detailed compatibility analysis]

---

### Global Variables

| Variable | Current State | Planned Change | Risk |
|----------|---------------|----------------|------|
| `variable1` | [State] | [Change] | ✅ SAFE / 🟡 CAUTION / 🔴 DANGER |
| `variable2` | [State] | [Change] | [Risk level] |

---

## Implementation Risks

### 🔴 HIGH RISK

1. **[Risk Name]** ([Issue Reference])
   - [Description of risk]
   - Risk: [What could go wrong]
   - Mitigation: [How to avoid the risk]

2. **[Risk Name]**
   - [Description]
   - Risk: [Problem]
   - Mitigation: [Solution]

### 🟡 MEDIUM RISK

3. **[Risk Name]**
   - [Description]
   - Risk: [Problem]
   - Mitigation: [Solution]

### 🟢 LOW RISK

6. **[Risk Name]**
   - [Description]
   - Risk: [Minimal problem]

---

## Recommended Approach

### Phase 0: Pre-Implementation Preparation

**Before writing any code:**

1. ✅ **[Preparation Task]**
   ```bash
   # Commands to run
   ```

2. ✅ **[Preparation Task]**
   - [Subtask]
   - [Subtask]

3. ✅ **[Preparation Task]**
   - [Subtask]

---

### Phase 1: Minimal Viable Implementation (X hours)

**Goal:** [What Phase 1 achieves]

**Step 1.1: [Step Name]** (X min)
```c
// Example code for this step
```

**Step 1.2: [Step Name]** (X min)
```c
// Example code
```

**MILESTONE:** [What should work at this point]

---

### Phase 2: [Phase Name] (X hours)

**Goal:** [What Phase 2 achieves]

[Continue with phases...]

---

## Testing Considerations

### Unit Test Scenarios (Hardware/Software Required)

**Setup:** [What's needed for testing]

#### Test 1: [Test Name] (X min)
1. [Step]
2. [Step]
3. [Step]

**Expected Result:**
- [Outcome]

**Pass Criteria:** ✅ [Criteria]

---

#### Test 2: [Test Name] (X min)

**Prerequisites:** [Prerequisites]

**Steps:**
1. [Step]
2. [Step]

**Expected Result:**
- [Outcome]

**Pass Criteria:** ✅ [Criteria]

---

### Edge Cases to Test

| Scenario | Expected Behavior | Risk |
|----------|------------------|------|
| [Scenario 1] | [Behavior] | 🟡 MEDIUM |
| [Scenario 2] | [Behavior] | 🟢 LOW |

---

## Decision Points

### Decision Point 1: [Decision Topic]

**Question:** [The decision to be made]

**Options:**
- **A:** [Option A description]
- **B:** [Option B description]
- **C:** [Option C description]

**Recommendation:** **Option X**

**Rationale:**
- ✅ [Reason 1]
- ✅ [Reason 2]
- ✅ [Reason 3]

**Action:** [What to do based on this decision]

---

### Decision Point 2: [Decision Topic]

**Question:** [Question]

**Options:**
- **A:** [Option]
- **B:** [Option]

**Recommendation:** **Option X**

**Rationale:**
- [Reasoning]

**Implementation:**
```c
// Example code if relevant
```

**Action:** [Action item]

---

## Summary

### Critical Changes to Plan

1. [🔴 CRITICAL / ⚠️ IMPORTANT / 🔧 MINOR] [Change description]
   ❌ **Remove** [What to remove]
   ✅ **Add** [What to add]

2. [Priority] [Change description]
   ✅ **[Action]** [Description]

### Implementation Timeline

| Phase | Time Estimate | Risk Level | Can Skip? |
|-------|---------------|------------|-----------|
| 0. [Phase Name] | X min | [Risk] | ❌ No |
| 1. [Phase Name] | X hours | [Risk] | ❌ No |
| 2. [Phase Name] | X hours | [Risk] | ✅ Yes ([reason]) |
| **TOTAL** | **X hours** | | |

### Go/No-Go Recommendation

[✅ GO | ⚠️ GO WITH CAUTION | ❌ NO-GO] - [Recommendation text]

**Conditions:**
1. ✅ [Condition 1]
2. ✅ [Condition 2]
3. ✅ [Condition 3]

**Confidence Level:** [🟢 HIGH | 🟡 MEDIUM | 🔴 LOW] **(XX%)**

[Explanation of confidence level and any reservations]

---

**END OF REVIEW**

*Next Steps:*
1. [Next step 1]
2. [Next step 2]
3. [Next step 3]
