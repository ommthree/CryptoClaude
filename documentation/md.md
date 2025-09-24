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

### md.md (This Document)
**Purpose:** Documentation index and management guide
**Contents:** File purposes, ownership, modification rules
**Owner:** SDM
**Modification Authority:** SDM (updates), Project Owner (structural changes)
**Maintenance:** Updated when new documents are created or roles change

## Validation Documents (Agent Reports)

### validation.md
**Purpose:** Validator findings and recommendations
**Contents:** Daily/weekly validation results, issues found, test results
**Owner:** Validator
**Modification Authority:** Validator only
**Lifecycle:** Created per validation cycle, archived after resolution

### tradval.md
**Purpose:** Trading and Risk Specialist assessment reports
**Contents:** Algorithm analysis, risk assessments, optimization recommendations
**Owner:** TRS
**Modification Authority:** TRS only
**Lifecycle:** Created when algorithm changes occur, archived after implementation

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

## Document Lifecycle Management

### Creation Process:
1. SDM identifies documentation need
2. SDM drafts document according to ownership rules
3. Required approvals obtained before activation
4. Document added to this index

### Update Process:
1. Authorized agent makes modifications according to authority rules
2. SDM updates index if structural changes occur
3. Dependencies reviewed for cascade effects

### Quality Gates:
- All strategic documents require Project Owner approval
- All tactical documents require SDM approval for structural changes
- All process documents are locked to Project Owner authority

---

*Document Owner: SDM*
*Last Updated: Setup Phase*
*Next Review: After first weekly cycle*