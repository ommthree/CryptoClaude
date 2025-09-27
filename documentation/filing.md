# CryptoClaude Project Filing System
## Directory and Subdirectory Structure Specification

**Proposed by:** SDM
**Authority:** SDM (creation and structure modifications)
**Maintained by:** Librarian (file organization and placement)
**Escalation Rule:** Librarian must check with SDM before moving files that might break functionality

---

## Root Directory Structure

### `/CryptoClaude/` (Project Root)
**Purpose:** Main project directory containing all project assets
**Contents:** Core directories, build files, executable files, and root-level documentation

### Required Root-Level Files
- **Makefile** - Build system configuration
- **README.md** (if needed for GitHub/external documentation)
- **Build artifacts** - Compiled executables and test programs
- **install_dependencies.sh** - Dependency installation script

---

## Core Directories

### `/documentation/` ✅ **PRIMARY DOCUMENTATION LOCATION**
**Purpose:** All .md files and project documentation
**Authority:** Librarian maintains organization, SDM approves structure changes
**Contents:**
```
documentation/
├── agents.md                    # Team role definitions
├── algo.md                     # Trading algorithm specification
├── arch.md                     # Technical architecture
├── busreq.md                   # Business requirements
├── dailyprocess.md             # Daily workflow process
├── day1validation.md           # Daily validation reports
├── day2validation.md           # (numbered sequentially)
├── day1trsvalidation.md        # Daily TRS assessments
├── day2trsvalidation.md        # (numbered sequentially)
├── day1plan.md                 # Daily development plans
├── day2plan.md                 # (numbered sequentially)
├── dayplan.md                  # Current week's daily plan
├── filing.md                   # Directory structure (this file)
├── md.md                       # Documentation index and standards
├── schema.md                   # Database structure specification
├── setupprocess.md             # Initial setup process
├── target.md                   # Target state specification
├── week1trsvalidation.md       # Weekly TRS assessments
├── week1validation.md          # Weekly validation reports
├── weeklyplanning.md           # Weekly development planning
└── weeklyprocess.md            # Weekly workflow process
```

**Librarian Rules:**
- All .md files MUST be in `/documentation/` directory
- No .md files should exist outside this directory (except README.md if needed)
- File names must match approved list in md.md
- Archive old/superseded documentation per lifecycle rules

### `/src/` ✅ **SOURCE CODE LOCATION**
**Purpose:** All source code files (.cpp, .h)
**Authority:** Developer creates/modifies, Librarian organizes structure
**Librarian Rule:** Never move source files without SDM approval (will break compilation)

#### Source Code Structure:
```
src/
├── Core/                       # Core business logic
│   ├── Analytics/              # Technical analysis and indicators
│   ├── Api/                    # API connection management
│   ├── Config/                 # Configuration management
│   ├── Console/                # Console interface components
│   ├── Data/                   # Data processing and management
│   │   └── Providers/          # Data source providers
│   ├── Database/               # Database integration
│   │   ├── Models/             # Data models
│   │   └── Repositories/       # Data access layer
│   ├── Http/                   # HTTP client implementations
│   ├── Json/                   # JSON processing utilities
│   ├── Monitoring/             # System monitoring
│   ├── Portfolio/              # Portfolio management
│   ├── Quality/                # Data quality management
│   ├── Risk/                   # Risk management
│   ├── Sentiment/              # Sentiment analysis
│   ├── Strategy/               # Trading strategies
│   ├── Trading/                # Trading engine
│   └── Utils/                  # Utility functions
├── Configuration/              # Legacy configuration (to be cleaned up)
└── UI/                         # User interface components
    ├── Controls/               # UI controls
    └── ViewModels/             # UI view models
```

### `/config/` ✅ **CONFIGURATION FILES**
**Purpose:** JSON configuration files for different environments
**Authority:** Developer modifies, Librarian maintains organization
**Security:** File permissions must be 600 for sensitive configs

**Structure:**
```
config/
├── production.json             # Production environment settings
└── development.json            # Development environment settings
```

**Librarian Rules:**
- Verify file permissions (600 for production configs)
- No unauthorized config files
- Alert SDM to any config security issues

### `/data/` ✅ **DATA STORAGE**
**Purpose:** Database files and data storage
**Authority:** System creates, Librarian monitors organization
**Contents:**
- SQLite database files (.db)
- Data backups
- Temporary data files

**Librarian Rules:**
- Monitor for proper database file organization
- Archive old backup files per retention policy
- Never move active database files without SDM approval

### `/logs/` ✅ **LOG FILES**
**Purpose:** System logs and monitoring data
**Authority:** System creates, Librarian maintains organization
**Contents:**
- Application log files
- Performance monitoring logs
- Error logs

**Librarian Rules:**
- Rotate large log files as needed
- Maintain log file organization
- Archive old logs per retention policy

---

## Archive and Temporary Directories

### `/archive/` ✅ **ARCHIVED DOCUMENTATION**
**Purpose:** Historical and superseded documentation
**Authority:** Librarian manages, SDM approves archival policies
**Contents:** Legacy .md files, old development reports, superseded specifications

**Librarian Responsibilities:**
- Move outdated documentation from `/documentation/` to `/archive/`
- Maintain archive organization by date/version
- Regular cleanup of very old archived files (>6 months)

### `/tests/` ✅ **TEST FILES AND EXECUTABLES**
**Purpose:** Test programs, validation scripts, and test utilities
**Authority:** Developer creates, Validator uses, Librarian organizes
**Contents:**
- Compiled test executables
- Test utilities and scripts
- Test data files

**Librarian Rules:**
- Organize test files by type/purpose
- Remove obsolete test executables
- Never move active test files without developer approval

### `/include/` (External Dependencies)
**Purpose:** External library headers and dependencies
**Authority:** Build system creates, Librarian monitors
**Contents:** Third-party library files (nlohmann-json, etc.)

**Librarian Rules:**
- Do not modify or move dependency files
- Alert SDM to any dependency organization issues

---

## Temporary and Build Directories

### Build Artifacts (Root Level)
**Purpose:** Compiled executables and build outputs
**Authority:** Build system creates, Librarian can clean up old files
**Contents:**
- Main executable files
- Test executables (various *Test files)
- Object files (.o files)
- Debug symbols (.dSYM directories)

**Librarian Rules:**
- Regular cleanup of old build artifacts (>1 week old)
- Preserve current executables needed for testing
- Remove orphaned .dSYM directories from deleted executables

### Temporary Directories
**Purpose:** Temporary files and test artifacts
**Authority:** System creates, Librarian maintains cleanup
**Examples:**
- `/x64/Debug/` (Visual Studio build artifacts)
- `test path with spaces/` (Testing directory - can be removed)
- Various cache and temporary directories

**Librarian Rules:**
- Regular cleanup of temporary files
- Remove test directories after testing completion
- Alert SDM before removing directories that might be needed

---

## File Movement Guidelines for Librarian

### ✅ **SAFE TO MOVE** (No SDM approval needed):
- .md files within `/documentation/` directory
- Old log files to archive
- Obsolete build artifacts and executables
- Temporary test files and directories
- Archive files within `/archive/` directory

### ⚠️ **CHECK WITH SDM FIRST** (Potential functionality impact):
- Any .cpp or .h files in `/src/` directory
- Active database files in `/data/`
- Configuration files in `/config/`
- Current build executables
- Include files or dependencies
- Any files referenced by Makefile or build system

### ❌ **NEVER MOVE** (Will break system):
- Files currently being used by running processes
- Core dependency files
- Active configuration files during system operation

---

## Maintenance Schedule

### Daily Maintenance (Librarian):
1. Verify all .md files are in `/documentation/`
2. Check for unauthorized .md files in other locations
3. Verify file naming compliance with md.md standards
4. Clean up obvious temporary files

### Weekly Maintenance (Librarian):
1. Archive completed daily/weekly validation reports
2. Clean up old build artifacts (>1 week)
3. Organize test files and remove obsolete executables
4. Review directory structure for organization issues

### Monthly Maintenance (Librarian with SDM approval):
1. Archive old log files (>30 days)
2. Review and cleanup archive directory
3. Assess directory structure efficiency
4. Propose structure improvements to SDM

---

## Directory Structure Validation Checklist

### Core Structure Present ✅
- [ ] `/documentation/` exists with all required .md files
- [ ] `/src/` exists with organized source code structure
- [ ] `/config/` exists with proper configuration files
- [ ] `/data/` exists for database storage
- [ ] `/logs/` exists for log file storage
- [ ] `/archive/` exists for historical documentation

### Organization Standards ✅
- [ ] No .md files outside `/documentation/` (except README.md if needed)
- [ ] All config files have proper permissions (600)
- [ ] Source code follows modular directory structure
- [ ] Test files are organized and current
- [ ] Build artifacts are current and not cluttering root

### Security and Access ✅
- [ ] Sensitive files have appropriate permissions
- [ ] No unauthorized files in secure directories
- [ ] Archive contains only non-sensitive historical data
- [ ] Log files follow retention policies

---

**Authority:** SDM (structure definition and modifications)
**Implementation:** Librarian (daily organization and maintenance)
**Escalation:** Librarian → SDM for any file moves that might impact functionality

*Document Owner: SDM*
*Maintained by: Librarian*
*Last Updated: Week 1 completion*