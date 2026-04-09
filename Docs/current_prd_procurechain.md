# ProcureChain: Municipal Procurement Document Tracking System
## Product Requirements Document (PRD)

## 1. Project Overview

Project Type: CC 203 Finals Project  
Platform: C++ CLI (Command-Line Interface)  
Project Level: Beginner-friendly, procedural C++  
Course Scope: File handling, arrays, vectors, functions, and structs  
Excluded Scope: OOP redesign, databases, web development, networking, external blockchain platforms

### Current Build Status (2026-04-09)

Implemented in code right now:

- citizen signup/login and admin signup/login
- menu routing with citizen and admin dashboards
- admin document management (upload/view/search/update status)
- document import upload flow with file metadata capture (pdf/docx/csv/txt)
- admin command center with grouped workspaces and overview dashboard
- advanced document filters (status, date/date range, category, department, uploader)
- guided search/filter suggestions with recent available records shown before input
- approvals module (request creation + pending view + approve/reject with document status transitions)
- citizen published-document search and verification flow
- analytics hub with approval, budget, audit, integrity, and executive views
- compact/full layout mode toggle for analytics rendering
- optional paged detail table flow in analytics reports
- budget consensus module (submit/view pending/approve/reject/publish) and variance reporting
- blockchain module (node file setup + chain validation and node consistency checking)
- blockchain explorer with per-node mismatch and per-block consensus diagnostics
- blockchain append hooks on key actions (upload/approve/reject/manual status/budget updates)
- published document viewing (published-only)
- document integrity verification with SHA-256 hash recomputation and blockchain checks
- audit logging for core auth/citizen/admin actions
- audit CSV export (all rows and filtered rows)
- audit export input hardening (date range and filename validation)
- audit-to-blockchain linking through chain index values
- audit hash-linking through previousHash/currentHash fields
- user account lifecycle administration for Super Admin
  - list accounts
  - deactivate/reactivate account
  - reset password with generated temporary password
- modular folder-aligned source/header split is in place
- admin sub-role permission enforcement is active in dashboard actions
- admin menus are role-visible (users only see actions/workspaces available to their role)

Still pending against optional desired scope:

- optional forced password change-on-next-login flow
- optional delegated approvals
- optional richer reporting presets

## 2. Product Name

Product Name: ProcureChain

Full Title: ProcureChain: A Municipal Procurement Document Tracking System

Tagline: Secure procurement document tracking, approval, and verification for accountable local governance.

## 3. Background and Context

Municipal procurement involves sensitive and important records such as purchase requests, purchase orders, contracts, notices of award, and acceptance documents. These records must be managed carefully to reduce tampering, improve transparency, and ensure that no single official can unilaterally approve and publish procurement records.

For this finals project, the system is adapted into a CLI-based procurement tracking system that uses:

- layered admin roles
- citizen account access
- text-file storage
- audit logging
- unanimous approval workflow
- simulated blockchain validation using multiple ledger files
- governance-focused reporting extensions

## 4. Problem Statement

Without a structured process:

- document authenticity may be difficult to verify
- approval history can become unclear
- budget allocations may not be visible to users
- records may be altered without easy detection
- accountability may weaken when controls are fragmented

There is a need for a beginner-friendly procurement document tracking system that can simulate secure handling, multi-official approval, and transparent record monitoring using only C++ CLI and file handling.

## 5. Proposed Solution

ProcureChain is a C++ CLI-based municipal procurement document tracking system that allows authorized officials to upload and manage procurement documents while requiring unanimous approval from designated approvers before publication.

The system allows registered citizens to:

- sign up and log in
- view published procurement records
- view procurement budget summaries
- inspect and export audit records

The system allows governance and compliance actions for admins to:

- run advanced document filtering
- review approval analytics
- review budget variance reports
- manage account lifecycle (Super Admin)
- validate blockchain integrity and cross-node consistency

## 6. Target Users

### 6.1 System Administrators

#### Super Admin

Can:

- access all records and dashboards
- validate blockchain consistency
- verify document integrity
- run full account lifecycle controls

#### Procurement Officer

Can:

- upload procurement documents
- view/search/filter document records

#### Approving Officers

Examples:

- Budget Officer
- Municipal Administrator

Can:

- review pending procurement documents
- approve or reject submitted records
- view approval analytics dashboard

### 6.2 Registered Citizens

Can:

- sign up
- log in
- view published procurement documents
- view procurement budget summaries
- view and export the audit trail

## 7. Core Features and Acceptance Criteria

### Feature 1: Account-Based Access

Priority: Critical

Description:

The system requires authentication before access and displays role-specific dashboards.

Acceptance Criteria:

- Main menu includes Login, Sign Up, Exit
- Citizens can create accounts in users.txt
- Admins can create and use admin accounts in admins.txt
- System routes users to role-correct menu
- Role-visible menus hide inaccessible admin actions in normal navigation flows

### Feature 2: User Account Lifecycle Administration

Priority: High

Description:

Super Admin can manage account operational state and credentials.

Acceptance Criteria:

- Super Admin can list users/admins with status and last update timestamp
- Super Admin can deactivate/reactivate accounts
- Inactive accounts cannot log in
- Super Admin can reset password with generated temporary password
- Lifecycle actions are logged in audit trail

### Feature 3: Procurement Document Management and Verification

Priority: Critical

Description:

The system stores procurement records and supports integrity verification.

Acceptance Criteria:

- Document upload requires title, category, description, and source file path
- Source file import accepts pdf/docx/csv/txt and copies files into local storage
- Document records store ID/title/category/description/department/date/uploader/status/hash/file metadata/budget fields
- Verification compares recomputed SHA-256 hash with stored hash
- Citizen verification checks whether hash evidence is present in blockchain records
- System displays VALID/POTENTIALLY TAMPERED/CHAIN-MISSING outcomes
- Published records are visible to citizen accounts

### Feature 4: Advanced Document Filters

Priority: High

Description:

Admins can query document records using combined filters.

Acceptance Criteria:

- Filter supports status, exact date, from/to range, category, department, uploader
- Multiple filters can be combined in one query
- Range validation rejects fromDate greater than toDate
- Empty result set displays clear no-match message
- Filter actions are logged in audit trail
- Filter/search screens show recent available values/suggestions before input

### Feature 5: Unanimous Consensus Approval Protocol

Priority: Critical

Description:

Documents become published only after all required approvers decide and none reject.

Acceptance Criteria:

- Approval records stored per approver in approvals.txt
- Required approver roles are Budget Officer and Municipal Administrator
- Any rejection marks the document rejected
- Complete non-rejected decisions publish document
- Approve/reject actions update both approvals and document status
- Citizens can only verify and inspect published documents

### Feature 6: Approval Analytics Dashboard

Priority: High

Description:

Dashboard summarizes approval performance and bottlenecks.

Acceptance Criteria:

- approvals.txt stores createdAt and decidedAt
- Dashboard shows decided/pending/approved/rejected counts
- Rejection rate is computed from decided rows
- Average decision time is computed from rows with complete timestamps
- Throughput by role displayed as chart
- Dashboard is reachable via overview-driven analytics hub navigation
- Layout mode toggle updates chart and table density
- Optional paged detail table can be opened for deeper row inspection

### Feature 7: Procurement Budget Consensus and Variance

Priority: High

Description:

Budget module supports consensus-governed budget publication and variance analysis.

Acceptance Criteria:

- budget_entries.txt stores submitted budget entries
- budget_approvals.txt stores unanimous approval workflow per entry
- Budget Officer and Municipal Administrator can approve/reject pending entries
- Any rejection blocks publication
- Full non-rejected decisions publish the budget entry into budgets.txt
- Variance report compares allocated vs actual totals
- Actual totals derive from approved/published documents by budget category
- Report shows variance amount and utilization percentage

### Feature 8: Audit Trail and CSV Export

Priority: Critical

Description:

System keeps a readable audit history and supports CSV output for demos/compliance.

Acceptance Criteria:

- Major actions append to audit_log.txt with timestamp/action/target/actor/chainIndex/previousHash/currentHash
- Audit view renders table and action-frequency chart
- CSV export supports all rows
- CSV export supports filtered rows (date/action/actor/target)
- Export validates date range bounds before file write
- Export validates filename safety before file write
- Export action is itself logged
- Audit rows are hash-linked with previousHash and currentHash values

### Feature 9: Simulated Blockchain Ledger

Priority: High

Description:

System simulates blockchain behavior using 5 synchronized node files.

Acceptance Criteria:

- Block format includes index/timestamp/action/documentID/actor/previousHash/currentHash
- Key actions append to all node files
- Validation checks chain linkage in each node
- Validation checks full-node content consistency
- Validation result logged in audit trail
- Explorer screen shows node-level integrity, first mismatch, per-block agreement counts, and tamper alerts

### Feature 10: Audit-to-Blockchain Linking

Priority: High

Description:

Blockchain-backed actions expose chain index to audit rows for stronger traceability.

Acceptance Criteria:

- appendBlockchainAction returns block index
- audit writer accepts optional chain index argument
- blockchain-backed actions pass chain index to logAuditAction
- audit table displays chain index when present

## 8. System Flow

### Main Menu

- Login
- Sign Up
- Exit

### Citizen Menu

- View Published Documents
- View Budget Summary
- View Audit Trail
- Search Published Document by ID
- Verify Published Document Hash
- Logout

### Admin Command Center

- Overview Dashboard
  - open analytics hub
  - quick integrity snapshot
  - refresh overview
  - toggle compact/full layout
- Documents Workspace
  - upload, view, search, filter, manual status update
- Approvals Workspace
  - pending approvals, approve/reject, detailed approval analytics
- Budget Workspace
  - view published allocations, submit budget entries, approve/reject budget entries, variance report
- Audit and Integrity Workspace
  - audit trail, blockchain validation, document integrity verification, integrity snapshot, blockchain explorer
- Account Administration Workspace
  - lifecycle controls
- Logout

Note:

- workspace and action menus are role-visible; unauthorized choices are hidden.

## 9. Data Storage Design

### admins.txt

adminID|fullName|username|password|role|status|updatedAt

### users.txt

userID|fullName|username|password|status|updatedAt

### documents.txt

docID|title|category|description|department|dateUploaded|uploader|status|hashValue|fileName|fileType|filePath|fileSizeBytes|budgetCategory|amount

### approvals.txt

docID|approverUsername|role|status|createdAt|decidedAt

### budgets.txt

category|amount

### budget_entries.txt

entryID|entryType|fiscalYear|category|allocatedAmount|description|createdAt|createdBy|status|publishedAt

### budget_approvals.txt

entryID|approverUsername|role|status|createdAt|decidedAt

### audit_log.txt

timestamp|action|targetID|actor|chainIndex|previousHash|currentHash

### blockchain node files

index|timestamp|action|documentID|actor|previousHash|currentHash

## 10. Technical Constraints

Because this is a beginner-level finals project, implementation remains within:

- functions
- arrays
- vectors
- structs
- file handling
- procedural logic
- menu-based navigation

The system does not rely on:

- OOP class architecture
- external databases
- frameworks
- web technologies
- real blockchain infrastructure

## 11. Non-Functional Requirements

Simplicity:
- code and outputs must be explainable during defense

Readability:
- menus, logs, and reports should remain text-clean and understandable

Maintainability:
- modules are separated by concern in src and include

Reliability:
- system should safely load legacy rows and persist expanded schema rows

## 12. Success Criteria

The project is successful if it can demonstrate:

- login and sign-up flow
- layered role-based access
- procurement document storage and advanced filtering
- unanimous approval workflow
- approval analytics
- budget summary and variance reporting
- audit trail logging and CSV export
- simulated blockchain validation with linked audit evidence

Integrity note:

- Hash computation now uses SHA-256 in the project verification module.

## 13. Scope Summary

ProcureChain is a beginner-friendly C++ CLI municipal procurement document tracking system. It uses file handling, vectors, arrays, functions, and structs to implement:

- role-based access
- citizen sign-up/login
- procurement document management
- unanimous approval before publication
- governance reporting
- budget visibility and variance
- audit logs and exports
- a simulated blockchain using 5 text-based ledger files
