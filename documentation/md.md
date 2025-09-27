# CryptoClaude Documentation Index

## Overview
This document provides a comprehensive index of all markdown documentation files, their purpose, ownership, and modification authority.

## Strategic Documents (Project Owner Approval Required)

### target.md
**Purpose:** Approved target state for the software functionality
**Contents:** Detailed description of user console interface and expected outcomes
**Owner:** SDM (draft) → Project Owner (approval)
**Modification Authority:** Project Owner only
**Dependencies:** Business requirements, user stories

### algo.md
**Purpose:** Approved trading algorithm specification
**Contents:** Target portfolio definition, trade determination logic, risk parameters
**Owner:** SDM (draft) → Project Owner (approval)
**Modification Authority:** Project Owner only
**Dependencies:** Market analysis, risk management requirements

### schema.md
**Purpose:** SQL database structure specification
**Contents:** Table definitions, relationships, constraints, indexes
**Owner:** SDM (draft) → Project Owner (approval)
**Modification Authority:** Developer (with SDM + Project Owner approval)
**Dependencies:** Data model requirements, performance considerations
**Evolution:** Approved changes allowed as system develops

## Tactical Documents (SDM Approval Required)

### arch.md
**Purpose:** Target technical architecture specification
**Contents:** Function definitions, source file structure, IDE/technology choices
**Owner:** SDM (draft) → Developer (refinement)
**Modification Authority:** Developer (daily refinements), SDM (structural changes)
**Dependencies:** Technical requirements, platform constraints

### busreq.md
**Purpose:** Detailed business requirements specification
**Contents:** Granular component requirements, completion tracking
**Owner:** SDM (creation and completion tracking)
**Modification Authority:** Developer (completion updates), SDM (requirement changes)
**Dependencies:** target.md, stakeholder requirements

### weeklyplanning.md
**Purpose:** High-level week-by-week development plan
**Contents:** Weekly goals, success criteria, milestone tracking
**Owner:** SDM (creation) → Developer (progress updates)
**Modification Authority:** Developer (progress), SDM (future plan changes)
**Dependencies:** Business requirements, resource availability

## Operational Documents (Developer Authority)

### dayplan.md
**Purpose:** Detailed daily development tasks for current week
**Contents:** Daily action steps, success criteria, completion status
**Owner:** SDM (weekly creation) → Developer (daily updates)
**Modification Authority:** Developer (completion updates), SDM (task modifications)
**Refresh Cycle:** Created fresh each Monday for the work week

## Process Documents (Project Owner Authority)

### setupprocess.md
**Purpose:** Initial project setup and organization process
**Contents:** File organization, documentation creation, team initialization
**Owner:** SDM
**Modification Authority:** Project Owner only
**Execution:** One-time during project initialization

### dailyprocess.md
**Purpose:** Standard daily development workflow
**Contents:** Daily task orchestration, review cycles, approval gates
**Owner:** SDM
**Modification Authority:** Project Owner only
**Execution:** Repeated daily during development

### weeklyprocess.md
**Purpose:** Weekly comprehensive review and planning process
**Contents:** Deep validation, comprehensive testing, planning updates
**Owner:** SDM
**Modification Authority:** Project Owner only
**Execution:** Repeated weekly during development

## Team Documents (SDM Authority)

### agents.md
**Purpose:** Team role definitions and responsibilities
**Contents:** Agent personas, authority levels, interaction patterns
**Owner:** SDM
**Modification Authority:** Project Owner only
**Dependencies:** Team structure, process requirements

### filing.md
**Purpose:** Directory and subdirectory structure specification
**Contents:** Complete project directory structure, file organization rules, Librarian guidelines
**Owner:** SDM (proposed and maintained)
**Modification Authority:** SDM (structure changes), Librarian (maintenance updates)
**Dependencies:** Project organization needs, file management requirements

### md.md (This Document)
**Purpose:** Documentation index and management guide
**Contents:** File purposes, ownership, modification rules
**Owner:** SDM
**Modification Authority:** SDM (updates), Project Owner (structural changes)
**Maintenance:** Updated when new documents are created or roles change

## Daily Validation Documents (Agent Reports)

### day#validation.md
**Purpose:** Daily validator findings and recommendations
**Contents:** Daily validation results, issues found, test results
**Owner:** Validator
**Modification Authority:** Validator only
**Lifecycle:** Created daily during development, numbered sequentially (day1validation.md, day2validation.md, etc.)
**Naming Convention:** day[1-9]validation.md

### day#trsvalidation.md
**Purpose:** Daily Trading and Risk Specialist assessment reports
**Contents:** Algorithm analysis, risk assessments, optimization recommendations
**Owner:** TRS
**Modification Authority:** TRS only
**Lifecycle:** Created daily when algorithm changes occur, numbered sequentially
**Naming Convention:** day[1-9]trsvalidation.md

### day#plan.md
**Purpose:** Daily development plan for specific day
**Contents:** Detailed daily tasks, success criteria, progress tracking
**Owner:** SDM (creation) → Developer (updates)
**Modification Authority:** Developer (completion updates), SDM (task modifications)
**Lifecycle:** Created for each development day, numbered sequentially
**Naming Convention:** day[1-9]plan.md

## Weekly Validation Documents

### week#trsvalidation.md
**Purpose:** Weekly comprehensive Trading and Risk Specialist assessment
**Contents:** Weekly algorithm review, performance analysis, strategic recommendations
**Owner:** TRS
**Modification Authority:** TRS only
**Lifecycle:** Created weekly during development, numbered sequentially
**Naming Convention:** week[1-9]trsvalidation.md

### week#validation.md
**Purpose:** Weekly comprehensive Validator assessment
**Contents:** Weekly system validation, comprehensive testing results, technical analysis
**Owner:** Validator
**Modification Authority:** Validator only
**Lifecycle:** Created weekly during development, numbered sequentially
**Naming Convention:** week[1-9]validation.md

## Archive Directory

### Purpose
Storage for legacy documentation that has been superseded by the new structured approach.

### Contents
- Previous planning documents
- Historical development reports
- Superseded specifications

### Access
Read-only reference, no active development use

## Tests Directory

### Purpose
Centralized storage for all test scripts, validation programs, and testing utilities.

### Organization
- Component-specific test subdirectories
- Integration test suites
- Performance and validation scripts

### Maintenance
Updated by Developer and Validator as needed for quality assurance.

---

## Approved .md File List (Librarian Reference)

### Core Strategic Documents (Required)
- **agents.md** - Team role definitions
- **algo.md** - Trading algorithm specification
- **arch.md** - Technical architecture specification
- **busreq.md** - Business requirements specification
- **schema.md** - Database structure specification
- **target.md** - Target state specification

### Process Documents (Required)
- **dailyprocess.md** - Daily development workflow
- **setupprocess.md** - Initial project setup process
- **weeklyprocess.md** - Weekly review and planning process
- **weeklyplanning.md** - High-level weekly development plan

### Documentation Management (Required)
- **filing.md** - Directory and subdirectory structure specification
- **md.md** - Documentation index and standards (this file)

### Daily Documents (Created as needed, numbered sequentially)
- **day#validation.md** - Daily validation reports
- **day#plan.md** - Daily development plans
- **day#trsvalidation.md** - Daily TRS assessments

### Weekly Documents (Created as needed, numbered sequentially)
- **week#trsvalidation.md** - Weekly TRS comprehensive assessments
- **week#validation.md** - Weekly comprehensive validation reports

### Current Working Documents (Single instances)
- **dayplan.md** - Current week's daily plan

**⚠️ LIBRARIAN RULE**: No .md file should be created that is not listed above. Any new file types require SDM approval and addition to this list.

---

## Document Lifecycle Management

### Creation Process:
1. SDM identifies documentation need and verifies against approved list
2. SDM drafts document according to ownership rules
3. Required approvals obtained before activation
4. Document added to this index if new type

### Update Process:
1. Authorized agent makes modifications according to authority rules
2. Librarian maintains file organization and naming consistency
3. SDM updates index if structural changes occur
4. Dependencies reviewed for cascade effects

### Quality Gates:
- All strategic documents require Project Owner approval
- All tactical documents require SDM approval for structural changes
- All process documents are locked to Project Owner authority
- All new .md file types require SDM approval

### Librarian Maintenance Tasks:
1. **File Organization**: Ensure all .md files are in proper directories
2. **Naming Compliance**: Verify files follow naming conventions
3. **Content Structure**: Remove bloat, maintain consistent formatting
4. **Archive Management**: Move outdated files to archive directory
5. **Index Compliance**: Ensure no unauthorized .md files exist

---

*Document Owner: SDM*
*Last Updated: Setup Phase*
*Next Review: After first weekly cycle*