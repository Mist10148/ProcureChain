# ProcureChain

ProcureChain is a C++ command-line municipal procurement tracking system.

It is designed for classroom-level procedural programming and focuses on:

- Role-based access for citizens and administrators
- Procurement document lifecycle tracking
- Multi-role approval workflow
- Budget viewing and management
- Audit trail logging and CSV export
- Simulated blockchain consistency checks
- Governance reporting and account lifecycle administration
- Admin Command Center with grouped workspaces and analytics hub
- Compact and full analytics layout modes with optional paged detail tables

The system stores records in text files under the data folder and keeps the implementation modular through separate source files per feature.

## System Description

ProcureChain helps a municipality track procurement documents from upload to publication.

High-level process:

1. A procurement admin uploads a document with budget metadata and amount.
2. Approval requests are created for required approver roles.
3. Approvers approve or reject.
4. Document status updates automatically based on approval outcomes.
5. Citizens can view published documents.
6. Every important action is written to an audit log.
7. Key events are appended to five blockchain node files and can be validated for consistency.
8. Governance dashboards provide approval, budget, audit, integrity, and executive analytics.
9. Admin navigation is grouped into overview plus role-gated workspaces.

## Main Features

- Citizen signup and login
- Admin signup and login with role assignment
- User account lifecycle admin tools (Super Admin)
  - list accounts
  - deactivate/reactivate accounts
  - reset password with generated temporary password
- Published document viewing for citizens
- Admin document upload, search, full listing, and status update
- Advanced document filters by status/date/category/department/uploader
- Approval routing for Budget Officer and Municipal Administrator
- Approval analytics dashboard
  - rejection rate
  - average decision time
  - throughput by role
- Admin overview dashboard and analytics hub
  - approval funnel and throughput trends
  - budget utilization comparisons
  - audit activity timeline and action frequency
  - integrity status cards and risk indicators
  - executive snapshot
  - compact/full layout toggle
  - optional paged detail tables
- Budget summary and admin budget management
- Budget variance report (allocated vs actual)
- Timestamped audit trail with chain index references
- CSV export for audit logs
  - export all
  - export filtered
  - validated date range and safe filename checks
- Simulated blockchain append and validation across 5 node files
- Audit-to-blockchain linking for blockchain-backed actions

## Requirements

- Windows with PowerShell
- C++ compiler (g++ recommended, such as MSYS2 MinGW)

Optional check:

    g++ --version

## How To Build and Run

### Option A: Run from project root (recommended)

Open PowerShell in the project folder and run:

    g++ -std=c++17 src/main.cpp src/auth.cpp src/documents.cpp src/verification.cpp src/budget.cpp src/audit.cpp src/approvals.cpp src/blockchain.cpp src/ui.cpp src/analytics.cpp -o procurechain.exe
    .\procurechain.exe

### Option B: Run from src folder

    cd src
    g++ -std=c++17 main.cpp auth.cpp documents.cpp verification.cpp budget.cpp audit.cpp approvals.cpp blockchain.cpp ui.cpp analytics.cpp -o ..\procurechain.exe
    cd ..
    .\procurechain.exe

### Notes

- On startup, the system ensures required files and headers exist.
- Existing legacy rows are loaded with backward-compatible parsing.
- Writes persist the current expanded schema.

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

    docID|title|category|department|dateUploaded|uploader|status|hashValue|budgetCategory|amount

### data/approvals.txt

    docID|approverUsername|role|status|createdAt|decidedAt

### data/budgets.txt

    category|amount

### data/audit_log.txt

    timestamp|action|targetID|actor|chainIndex

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
- Budget Workspace
  - view allocations, variance report, manage budgets
- Audit and Integrity Workspace
  - view audit trail, validate blockchain, verify document integrity, integrity snapshot
- Account Administration Workspace
  - account lifecycle management
- Logout

Role gates are enforced per action.

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

## Important Limitation

The verification hash is intentionally simple for classroom simulation and is not cryptographic security.

## Troubleshooting

- Build fails with compiler not found:
  install g++ and ensure it is in PATH.

- Data file open errors:
  run the executable from project root so relative paths resolve correctly.

- Menu loops after invalid input:
  enter numeric menu choices only.
