# [Feature Name] - Step-by-Step Implementation Guide

**Document Version:** 1.0
**Created:** YYYY-MM-DD
**Updated:** YYYY-MM-DD
**Purpose:** Detailed implementation instructions for [feature name]
**Prerequisites:**
- Read `[feature-name]-design.md` first for architecture overview
- [Any other prerequisites]
- [Dependencies or required knowledge]

---

## Table of Contents

1. [Overview](#overview)
2. [Prerequisites and Compatibility](#prerequisites-and-compatibility)
3. [Phase 1: [Phase Name]](#phase-1-phase-name)
4. [Phase 2: [Phase Name]](#phase-2-phase-name)
5. [Phase 3: [Phase Name]](#phase-3-phase-name)
6. [Phase N: Testing and Validation](#phase-n-testing-and-validation)
7. [Troubleshooting Guide](#troubleshooting-guide)
8. [Quick Reference](#quick-reference)

---

## Overview

This guide provides **exact code changes** for implementing [feature name]. Each phase includes:
- ✅ Specific file locations and line numbers
- ✅ Complete before/after code comparisons
- ✅ Integration points with existing code
- ✅ Testing checkpoints to verify correctness
- ✅ Expected behavior after each phase

**Estimated Total Time:** X-Y hours (including testing)

**Implementation Order:** Follow phases sequentially. Each phase builds on the previous one.

---

## Prerequisites and Compatibility

### ⚠️ IMPORTANT: [Important Prerequisite or Compatibility Note]

[Detailed explanation of any critical compatibility issues or dependencies]

**Key Points:**
- [Point 1]
- [Point 2]
- [Point 3]

---

## Phase 1: [Phase Name]

**Goal:** [What this phase accomplishes]

**Estimated Time:** X hours

**Files Modified:**
1. `/path/to/file1.h` (description)
2. `/path/to/file2.c` (description)
3. `/path/to/file3.c` (description)

---

### Step 1.1: [Step Description] (file.h)

**File:** `/path/to/file.h`

**Location:** After line XXX (after [landmark])

**Add the following:**

```c
// Code to add with detailed comments
// explaining what each part does
```

**Testing Checkpoint:** Compile the project. Should succeed with no errors.

```bash
cd /path/to/project
make clean
make
```

---

### Step 1.2: [Step Description] (file.c)

**File:** `/path/to/file.c`

**Location:** After line XXX

**Find this section (around line XXX):**
```c
// Existing code to locate
```

**Add immediately after:**
```c
// New code to add
```

**Key Points:**
- [Important point about this change]
- [Another important point]

**Testing Checkpoint:** Compile again. Should succeed.

---

### Step 1.3: [Step Description]

**File:** `/path/to/file.c`

**Location:** [Location description]

**Find this code block:**
```c
// Original code
```

**Replace with:**
```c
// New code
```

**Key Changes:**
- [What changed]
- [Why it changed]
- [Expected behavior]

---

### Phase 1 Completion Checklist

Before proceeding to Phase 2, verify:

- [ ] All files compile without errors
- [ ] [Specific check]
- [ ] [Specific check]
- [ ] [Specific check]

**Expected State:** [Description of what should be working at this point]

---

## Phase 2: [Phase Name]

**Goal:** [What this phase accomplishes]

**Estimated Time:** X hours

**Files Modified:**
1. `/path/to/file.c` (description)

---

### Step 2.1: [Step Description]

[Follow same format as Phase 1 steps]

---

### Step 2.2: [Step Description]

[Continue with all steps]

---

### Phase 2 Completion Checklist

Before proceeding to Phase 3, verify:

- [ ] [Check 1]
- [ ] [Check 2]
- [ ] [Check 3]

**Expected State:** [Description]

---

## Phase 3: [Phase Name]

[Continue with additional phases...]

---

## Phase N: Testing and Validation

**Goal:** Comprehensive testing of all features.

**Estimated Time:** X hours

**Test Categories:**
1. Functional tests (feature correctness)
2. Integration tests (interaction with other features)
3. Edge case tests (boundary conditions)
4. Performance tests (speed, memory, battery)

---

### Test N.1: [Test Name]

**Test:** [Brief description]

**Prerequisites:** [What's needed to run this test]

**Steps:**
1. [Step 1]
2. [Step 2]
3. [Step 3]
4. [Step 4]

**Expected Result:**
- [Expected outcome 1]
- [Expected outcome 2]
- [Expected outcome 3]

**Pass Criteria:** ✅ [What must be true for test to pass]

---

### Test N.2: [Test Name]

**Test:** [Description]

**Method 1: [Testing Approach]**
1. [Step]
2. [Step]

**Method 2: [Alternative Approach]**
1. [Step]
2. [Step]

**Expected Result:**
- [Outcome]

**Pass Criteria:** ✅ [Criteria]

---

### Test N.3: [Test Name]

[Continue with all necessary tests]

---

### Phase N Completion Checklist

After completing all tests, verify:

- [ ] All tests passed
- [ ] No crashes, hangs, or errors
- [ ] [Specific verification]
- [ ] [Specific verification]

**Final Validation:**
- [ ] Library compiles without warnings
- [ ] Example application runs successfully
- [ ] Documentation complete and accurate
- [ ] Code reviewed for edge cases

**Implementation Complete!** 🎉

---

## Troubleshooting Guide

### Problem: [Common Problem Description]

**Possible Causes:**
1. [Cause 1]
2. [Cause 2]
3. [Cause 3]

**Solutions:**
- [Solution 1]
- [Solution 2]
- [Solution 3]

---

### Problem: [Another Common Problem]

**Possible Causes:**
1. [Cause]

**Solutions:**
- [Solution]

---

### Problem: [Compilation Error]

**Common Issues:**
1. [Issue 1]
2. [Issue 2]

**Solutions:**
- [Solution 1]
- [Solution 2]

---

## Quick Reference

### Key Files Modified

| File | Purpose | Major Changes |
|------|---------|---------------|
| `file1.h` | [Purpose] | [Changes] |
| `file2.c` | [Purpose] | [Changes] |

### Key Functions Added

| Function | Purpose |
|----------|---------|
| `NewFunction1()` | [Purpose] |
| `NewFunction2()` | [Purpose] |

### Key Modifications

| Function | Modification |
|----------|--------------|
| `ExistingFunction()` | [What was modified] |
| `AnotherFunction()` | [What was modified] |

### Estimated Line Count

- **New code:** ~XXX lines
- **Modified code:** ~XX lines
- **Total changes:** ~XXX lines

### Build Commands

```bash
# Build library
cd /path/to/library
make clean
make

# Build test application
cd /path/to/testapp
make clean
make

# Deploy to device
cp testapp.nds /path/to/device/
```

---

**END OF IMPLEMENTATION GUIDE**

This guide provides complete step-by-step instructions for implementing [feature name]. Follow phases sequentially and use testing checkpoints to verify correctness at each stage.
