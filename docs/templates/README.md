# Documentation Templates

This folder contains templates for documenting features in the NiFi project.

## Available Templates

### 1. design-spec-template.md
Template for creating feature design specifications. Use this to define WHAT to build and WHY.

**When to use:** Before starting implementation
**Copy to:** `docs/planning/[feature-name]/[feature-name]-design.md`

### 2. implementation-guide-template.md
Template for creating step-by-step implementation guides. Use this to provide EXACT instructions for HOW to implement.

**When to use:** During or just before implementation
**Copy to:** `docs/planning/[feature-name]/[feature-name]-implementation-guide.md`

### 3. review-template.md
Template for creating design and implementation reviews. Use this to identify issues and validate approach.

**When to use:** After design spec, before implementation
**Copy to:** `docs/planning/[feature-name]/[feature-name]-review.md`

## How to Use

See the complete guide: [docs/HOW-TO-DOCUMENT-FEATURES.md](../HOW-TO-DOCUMENT-FEATURES.md)

## Quick Start

1. Create a feature folder:
   ```bash
   mkdir -p docs/planning/my-feature
   ```

2. Copy the design spec template:
   ```bash
   cp docs/templates/design-spec-template.md docs/planning/my-feature/my-feature-design.md
   ```

3. Fill it out following the template structure

4. Repeat for review and implementation guide templates

## Examples

See existing feature documentation for examples:
- `docs/planning/spectator-mode/` - Complex feature with all documents
- `docs/planning/room-status/` - Medium complexity feature

## Need Help?

Read the complete documentation guide at [docs/HOW-TO-DOCUMENT-FEATURES.md](../HOW-TO-DOCUMENT-FEATURES.md)
