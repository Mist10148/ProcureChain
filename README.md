# ProcureChain

ProcureChain is a C++ command-line municipal procurement tracking system.

It is designed for classroom-level procedural programming and focuses on:

- Role-based access for citizens and administrators
- Role-visible menus that only show actions available to the logged-in role
- Procurement document lifecycle tracking
- File-import-backed procurement document registry (pdf, docx, csv, txt)
- Multi-role approval workflow
- Category-based approval rule engine with default fallback
- Budget consensus workflow and published budget projection
- Audit trail logging and CSV export
- Simulated blockchain consistency checks
- Blockchain explorer across 5 nodes with divergence/tamper detection
- Governance reporting and account lifecycle administration
- Admin Command Center with grouped workspaces and analytics hub
- Compact and full analytics layout modes with optional paged detail tables
- Guided search/filter prompts with available-value suggestions and recent record previews
- SHA-256 password hashing at rest for all stored credentials
- Optional approval/rejection notes with display in document detail panels
- Notification inbox for pending actions on login
- In-app role-specific help system
- Data backup and restore (Super Admin)
- Forced password change after Super Admin reset
- Delegated approval authority with date-range control

The system stores records in text files under the data folder and keeps the implementation modular through separate source files per feature.

## System Description

ProcureChain helps a municipality track procurement documents from upload to publication.

High-level process:

1. A procurement admin uploads a document with title, category, description, and an optional source file path.
2. Category is selected from guided choices, with Other for custom input.
3. Approval requests are created from category-based approval rules.
4. Approvers approve or reject.
5. Document is published only when unanimous required approvals are completed.
6. Citizens can view published documents.
7. Citizens can search a published document and verify its SHA-256 hash against blockchain records.
8. Every important action is written to a hash-linked audit log.
9. Key events are appended to five blockchain node files and can be validated for consistency.
10. Governance dashboards provide approval, budget, audit, integrity, and executive analytics.
11. Admin navigation is grouped into overview plus role-gated workspaces.

## Main Features

- Citizen signup and login
- Admin signup and login with role assignment
- User account lifecycle admin tools (Super Admin)
  - list accounts
  - deactivate/reactivate accounts
  - reset password with generated temporary password (forces password change on next login)
- SHA-256 password hashing at rest (plaintext passwords auto-migrated on startup)
- Forced password change after Super Admin password reset
- Published document viewing for citizens
- Citizen published document search by document ID
- Citizen published document hash verification (stored hash, recomputed hash, blockchain presence)
- Admin document upload, search, full listing, and status update
- Document upload requires title, category, and description
- Source file upload/import is optional (pdf/docx/csv/txt); metadata-only upload is supported
- Amendment upload supports rejected-document revision flow (v1 to v2 lineage)
- Category input uses guided choices with an Other option for custom categories
- Document input no longer collects budget allocation fields
- Allowed import extensions: pdf, docx, csv, txt
- Uploaded document file is copied to data/documents and SHA-256 hash is displayed after success
- Search assistance with recent document previews, ID/prefix/title matching, and guided selection
- Advanced document filters by status/date/category/department/uploader
- Filter suggestion hints (status/category/department/uploader/date) shown before input
- Approval routing uses configurable per-category rules with a DEFAULT fallback
- Approval decision screen shows the current approver's pending document list before input
- Publication gate for documents: rejected if any reject, published only with unanimous non-rejected decisions
- Super Admin escalation queue lists overdue pending approvals based on rule SLA
- Approval analytics dashboard
  - rejection rate
  - average decision time
  - throughput by role
  - overdue pending count and pending SLA compliance
  - per-role SLA bottlenecks (pending load, overdue count, average age, worst overdue)
- Admin overview dashboard and analytics hub
  - approval funnel and throughput trends
  - budget utilization comparisons
  - audit activity timeline and action frequency
  - integrity status cards and risk indicators
  - executive snapshot
  - compact/full layout toggle
  - optional paged detail tables
- Budget submission and consensus approvals
- Budget and documents are handled in separate workspaces, but both use unanimous consensus publication logic
- Budget entry fields: fiscal year, category, allocated amount, description
- Budget publish gate: budget becomes publicly visible only after unanimous approvals
- Published budget summary and budget variance reporting
- Budget variance report (allocated vs actual)
- Timestamped audit trail with chain index and hash chain references
- CSV export for audit logs
  - export all
  - export filtered
  - validated date range and safe filename checks
  - filter-value suggestions shown before filtered export input
- Simulated blockchain append and validation across 5 node files
- Blockchain explorer: per-node integrity, consensus matrix, per-block agreement, tamper alerts
- Audit-to-blockchain linking for blockchain-backed actions
- Optional approval/rejection notes attached to decisions (shown in document detail panel)
- Notification inbox shown on login and available as menu option
- In-app help system with role-specific guidance
- Data backup and restore (Super Admin, timestamped backup folders)
- Delegated approval authority (Budget Officer, Municipal Administrator can delegate to another admin for a date range)

## Requirements

- Windows with PowerShell
- C++ compiler (g++ recommended, such as MSYS2 MinGW)

Optional check:

    g++ --version

## How To Build and Run

### Option A: Run from project root (recommended)

Open PowerShell in the project folder and run:

    g++ -std=c++17 src/main.cpp src/auth.cpp src/documents.cpp src/verification.cpp src/budget.cpp src/audit.cpp src/approvals.cpp src/blockchain.cpp src/ui.cpp src/analytics.cpp src/notifications.cpp src/help.cpp src/backup.cpp src/delegation.cpp -o procurechain.exe
    .\procurechain.exe

### Option B: Run from src folder

    cd src
    g++ -std=c++17 main.cpp auth.cpp documents.cpp verification.cpp budget.cpp audit.cpp approvals.cpp blockchain.cpp ui.cpp analytics.cpp notifications.cpp help.cpp backup.cpp delegation.cpp -o ..\procurechain.exe
    cd ..
    .\procurechain.exe

### Notes

- On startup, the system ensures required files and headers exist.
- Existing legacy rows are loaded with backward-compatible parsing.
- Writes persist the current expanded schema.
- When seed files are empty, realistic mock records are generated for documents, approvals, budgets, budget consensus, and hash-linked audit rows.

## Quick Login Accounts

Seeded admin accounts include:

- Super Admin: admin_test / admin1234
- Procurement Officer: proc_admin / proc1234
- Budget Officer: budget_admin / budget1234
- Municipal Administrator: mun_admin / mun1234

You can also create new citizen and admin accounts from the Sign Up menu.

## Data File Formats (Current)

### data/users.txt

    userID|fullName|username|password|status|updatedAt

### data/admins.txt

    adminID|fullName|username|password|role|status|updatedAt

### data/documents.txt

  docID|title|category|description|department|dateUploaded|uploader|status|hashValue|fileName|fileType|filePath|fileSizeBytes|budgetCategory|amount|versionNumber|previousDocId

Note: budgetCategory and amount are retained for compatibility/analytics, but budget inputs are managed in Budget Workspace.

### data/approval_rules.txt

    category|requiredRoles|maxDecisionDays

### data/approvals.txt

    docID|approverUsername|role|status|createdAt|decidedAt|note

### data/budgets.txt

    category|amount

### data/budget_entries.txt

  entryID|entryType|fiscalYear|category|allocatedAmount|description|createdAt|createdBy|status|publishedAt

### data/budget_approvals.txt

  entryID|approverUsername|role|status|createdAt|decidedAt|note

### data/password_flags.txt

    username|mustChangePassword

### data/delegations.txt

    delegatorUsername|delegateeUsername|startDate|endDate|status

### data/audit_log.txt

  timestamp|action|targetID|actor|chainIndex|previousHash|currentHash

### data/blockchain/node1_chain.txt
### data/blockchain/node2_chain.txt
### data/blockchain/node3_chain.txt
### data/blockchain/node4_chain.txt
### data/blockchain/node5_chain.txt

    index|timestamp|action|documentID|actor|previousHash|currentHash

## Menu Overview

### Home

- Login
- Sign Up
- Exit

### Citizen Dashboard

- View Published Documents
- View Procurement Budgets
- View Audit Trail (with export)
- Search Published Document by ID
- Verify Published Document Hash
- Notification Inbox
- Help
- Logout

### Admin Command Center

- Overview Dashboard
  - open Analytics Hub
  - quick Integrity Snapshot
  - refresh overview
  - toggle layout mode
- Documents Workspace
  - upload, view, search, filter, manual status update
- Approvals Workspace
  - view pending, approve, reject, detailed approval analytics
  - escalation queue for overdue pending approvals (Super Admin)
  - approval rule management (Super Admin)
  - delegation management (Budget Officer, Municipal Administrator)
- Budget Workspace
  - view published allocations, submit budget entries, approve/reject budget entries, variance report
- Audit and Integrity Workspace
  - view audit trail, validate blockchain, verify document integrity, integrity snapshot, blockchain explorer
- Account Administration Workspace
  - account lifecycle management
  - data backup and restore (Super Admin)
- Notification Inbox
- Help
- Logout

Role gates are enforced per action, and menu options are displayed dynamically so users only see what they are allowed to run.

## Governance Reporting Highlights

### Advanced Document Filters

Admins can filter with combined criteria:

- status
- exact upload date
- upload date range
- category contains
- department contains
- uploader contains

### Approval Analytics Dashboard

Dashboard computes and displays:

- total approval rows
- decided rows
- pending rows
- approved and rejected rows
- rejection rate
- average decision hours (rows with complete timing)
- throughput chart by approver role
- overdue pending approvals based on category rule SLA days
- pending SLA compliance percentage
- per-role bottleneck table (pending, overdue, average age, worst overdue)

### Analytics Hub Views

The analytics hub provides dashboard screens for:

- approval funnel and throughput trends
- budget utilization comparisons
- audit activity timeline and action frequency
- integrity status cards and risk indicators
- executive snapshot

It also supports:

- compact/full layout mode toggle
- optional paged detail tables from analytics views

### Budget Variance Report

Report compares:

- allocated budget per category
- actual total from approved/published documents per budget category
- variance amount
- utilization percentage

## Audit and Blockchain Linking

Audit trail remains append-based and now supports optional chain linkage.

- Blockchain-backed actions write block entries and return chain index.
- The corresponding audit row stores that chain index.
- Audit viewer displays chain index per row when available.

## Documentation Location

Project planning and implementation docs are in:

- Docs/current_prd_procurechain.md
- Docs/context.md
- Docs/phase_tasks.md
- Docs/checklist.md
- Docs/roles.md

## Important Limitation

ProcureChain now uses SHA-256 for deterministic integrity checks, but this remains a classroom file-based simulation and not a production security system.

## Troubleshooting

- Build fails with compiler not found:
  install g++ and ensure it is in PATH.

- Data file open errors:
  run the executable from project root so relative paths resolve correctly.

- Menu loops after invalid input:
  enter numeric menu choices only.
