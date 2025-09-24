# CryptoClaude Daily Development Process

## Overview
This document defines the standard daily development workflow for the CryptoClaude project, ensuring consistent quality gates and progress tracking.

## Process Authority
**Owner:** SDM (orchestration and enforcement)
**Modification Rights:** Project Owner only
**Execution Frequency:** Every development day

## Daily Workflow Stages

### Stage 1: Daily Planning (SDM Orchestrated)

#### Planning Preparation
**Responsibility:** SDM
**Activities:**
1. Review previous day's completion status from dayplan.md
2. Analyze current week's progress against weeklyplanning.md targets
3. Identify any blockers or dependency issues from previous day
4. Assess need for plan modifications based on current state

#### Clarification Requests
**Trigger:** Uncertain requirements or major plan deviations needed
**Process:**
1. SDM identifies specific decision points or clarifications needed
2. SDM requests input from Project Owner on:
   - Algorithm modifications
   - Target state adjustments
   - Major architectural decisions
   - Resource allocation changes

#### Daily Plan Creation
**Responsibility:** SDM
**Process:**
1. SDM creates/updates dayplan.md based on weeklyplanning.md
2. SDM defines specific daily tasks with clear action steps
3. SDM establishes success criteria focusing on:
   - Component existence and functionality
   - Compilation success
   - Test passing status
   - Validator approval
   - Project Owner approval (if required)

### Stage 2: Plan Review (Conditional)

#### TRS Review Trigger
**Condition:** Daily plan includes algorithm changes or risk modifications
**Process:**
1. TRS reviews dayplan.md for algorithm impact
2. TRS assesses risk implications of proposed changes
3. TRS generates tradval.md report with concerns/recommendations
4. Project Owner reviews TRS report and approves/modifies plan

#### Direct Approval
**Condition:** No algorithm changes in daily plan
**Process:**
1. SDM submits dayplan.md directly to Project Owner
2. Project Owner reviews and approves daily plan
3. Development stage authorized to begin

### Stage 3: Development Execution (Developer Autonomous)

#### Implementation Phase
**Responsibility:** Developer
**Authority:** Autonomous execution within approved plan boundaries
**Activities:**
1. Execute daily tasks as defined in dayplan.md
2. Implement creative and lateral solutions within requirements
3. Maintain alignment with target state vision
4. Document implementation decisions in arch.md as needed

#### On-the-Fly Testing
**Responsibility:** Developer
**Requirements:**
1. Code compilation verification after each major change
2. Unit testing for new components
3. Integration testing for modified interfaces
4. Functional testing for user-facing features
5. Smoke testing for end-to-end workflows

#### Progress Tracking
**Process:**
1. Developer updates dayplan.md completion status in real-time
2. Developer marks success criteria as achieved
3. Developer notes any deviations or issues encountered
4. Developer escalates to SDM if plan modifications needed

### Stage 4: Light Daily Validation (Validator)

#### Validation Scope - "Light Review"
**Time Limit:** 30-45 minutes maximum
**Focus Areas:**
1. **Compilation Check:** All code compiles without errors
2. **Smoke Test:** Basic functionality verification
3. **Integration Check:** New code interfaces properly with existing system
4. **Dependency Verification:** Required libraries and components available

#### Validation Process
**Responsibility:** Validator
**Activities:**
1. Pull latest code and attempt compilation
2. Run existing test suites to ensure no regressions
3. Execute basic functional tests for new features
4. Verify integration points and API compatibility
5. Check for missing dependencies or configuration issues

#### Validation Reporting
**Output:** validation.md (daily light review)
**Contents:**
1. Compilation status (Pass/Fail with details)
2. Test execution results (Pass/Fail with failure details)
3. Integration assessment (Compatible/Issues with specifics)
4. Dependency status (Complete/Missing with requirements)
5. Overall recommendation (Approve/Request Fixes/Escalate)

### Stage 5: Conditional TRS Review

#### Review Trigger
**Condition:** Algorithm changes implemented during the day
**Process:**
1. TRS reviews implemented algorithm modifications
2. TRS assesses performance and risk implications
3. TRS generates light tradval.md report focusing on:
   - Algorithm logic correctness
   - Risk calculation accuracy
   - Performance impact assessment
   - Optimization opportunities

### Stage 6: Review and Resolution (Project Owner)

#### Report Review
**Responsibility:** Project Owner
**Process:**
1. Review validation.md report for technical issues
2. Review tradval.md report (if algorithm changes made)
3. Assess overall daily progress against success criteria
4. Determine if iteration with Developer is required

#### Issue Resolution
**Iteration Trigger:** Validation failures or concerns identified
**Process:**
1. Project Owner identifies specific issues requiring Developer attention
2. Developer addresses identified issues through additional development cycles
3. Validator re-runs applicable portions of validation
4. Process repeats until validation approvals obtained

### Stage 6.5: Git Commit and Version Control (Developer)

#### Commit Authorization
**Trigger:** All validations approved and Project Owner approval obtained
**Responsibility:** Developer
**Process:**
1. Stage all daily deliverables and changes using `git add`
2. Review staged changes with `git status` and `git diff --staged`
3. Create comprehensive commit message following project standards
4. Execute git commit with detailed description of daily achievements

#### Commit Message Standards
**Required Format:**
```
🚀 [Week X, Day Y] COMPLETE: [Brief Achievement Summary]

## [Primary Achievement Category]
### ✅ [Specific Deliverable 1] - [Status]
- [Key implementation detail]
- [Key implementation detail]

### ✅ [Specific Deliverable 2] - [Status]
- [Key implementation detail]
- [Key implementation detail]

## [Secondary Achievement Category]
- [Achievement summary]
- [Achievement summary]

## Validation Results
- ✅ [Validation type]: [Status/Result]
- ✅ [Success criteria]: [Status/Result]

## Implementation Statistics
- [File counts, lines of code, etc.]

🔧 Generated with [Claude Code](https://claude.ai/code)

Co-Authored-By: Claude <noreply@anthropic.com>
```

#### Commit Best Practices
**Requirements:**
1. **Comprehensive Scope:** Include all daily deliverables in single logical commit
2. **Clear Attribution:** Document what was achieved and why it matters
3. **Validation Evidence:** Reference validation approvals obtained
4. **Progress Tracking:** Quantify implementation progress made
5. **Professional Standards:** Maintain consistent commit message quality

**Quality Gates:**
- All staged changes reviewed and intentional
- Commit message accurately reflects daily achievements
- No sensitive information or temporary files included
- Repository remains in clean, deployable state

#### Post-Commit Verification
**Process:**
1. Verify commit completed successfully with `git status`
2. Confirm working directory is clean
3. Validate commit message and content with `git show --name-status`
4. Update commit hash in daily documentation if required

### Stage 7: Progress Documentation (Developer + SDM)

#### Developer Updates
**Responsibility:** Developer
**Activities:**
1. Mark completed tasks in dayplan.md
2. Update weeklyplanning.md with progress made
3. Request SDM approval for any future plan modifications
4. Update arch.md with any architectural refinements made

#### SDM Documentation Updates
**Responsibility:** SDM
**Activities:**
1. Update busreq.md to reflect completed requirements
2. Mark off completed components and functionality
3. Update target architecture if modifications approved
4. Assess impact on future planning and make adjustments

### Stage 8: Day Transition

#### End-of-Day Assessment
**Responsibility:** SDM
**Process:**
1. Verify all daily success criteria have been met
2. Confirm all validation approvals obtained
3. Update setupprocess.md completion status if setup tasks completed
4. Assess readiness for next day or week transition

#### Next Day Preparation
**Condition:** Regular day-to-day transition
**Action:** Return to Stage 1 for next day's planning cycle

#### Week Transition
**Condition:** End of work week reached
**Action:** Execute weeklyprocess.md comprehensive review cycle

## Success Criteria Templates

### Component Development:
- [ ] Component exists and is properly integrated
- [ ] Code compiles without errors or warnings
- [ ] Unit tests pass for component functionality
- [ ] Integration tests pass for component interfaces
- [ ] Validator approves technical implementation
- [ ] Project Owner approves component completion
- [ ] Git commit completed with comprehensive documentation

### Algorithm Changes:
- [ ] Algorithm modification properly implemented
- [ ] Risk calculations updated and verified
- [ ] Performance impact assessed and acceptable
- [ ] Code compiles and existing tests still pass
- [ ] TRS approves algorithm correctness
- [ ] Validator approves technical integration
- [ ] Project Owner approves algorithm change
- [ ] Git commit completed with comprehensive documentation

### Documentation Updates:
- [ ] Required documentation updated and accurate
- [ ] Architecture documentation reflects current state
- [ ] Planning documents updated with progress
- [ ] Success criteria properly marked
- [ ] SDM approves documentation completeness
- [ ] Git commit completed with comprehensive documentation

## Quality Gates

### Mandatory Gates (Cannot Proceed Without):
1. Code compilation success
2. Basic smoke tests passing
3. Validator technical approval
4. Project Owner approval (when applicable)
5. Git commit completed with comprehensive documentation
6. SDM progress documentation completion

### Conditional Gates (When Applicable):
1. TRS algorithm approval (for algorithm changes)
2. Project Owner approval (for major changes)
3. Integration test success (for interface modifications)

## Escalation Triggers

### Immediate SDM Escalation:
- Compilation failures that cannot be resolved
- Test failures indicating system regression
- Integration issues affecting existing functionality
- Resource or dependency blocking issues

### Project Owner Escalation:
- Algorithm modification requirements
- Target state clarification needs
- Major architectural change requirements
- Timeline or scope adjustment needs

---

**Process Status:** Active for daily execution
**Review Cycle:** Weekly assessment for process improvements
**Modification Authority:** Project Owner only

*Document Owner: SDM*
*Created: Setup Phase*
*Last Updated: Initial creation*