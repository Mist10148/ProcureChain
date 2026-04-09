# ProcureChain

ProcureChain is a C++ command-line municipal procurement tracking system for classroom use.

It focuses on role-based governance, transparent approvals, and tamper-evident record tracking while staying procedural and beginner-friendly.

## Current Feature Set

- Citizen and admin signup/login with role-aware dashboards
- Account lifecycle controls (Super Admin): list, deactivate, reactivate, reset password
- Document upload, search, full listing, and manual status override
- Advanced document filters by status, date/date range, category, department, and uploader
- Unanimous approval workflow for required approver roles
- Approval analytics dashboard:
  - average decision time
  - rejection rate
  - throughput by role
- Budget summary, budget add/update, and budget variance reporting
- Audit trail display with action frequency chart
- Audit export to CSV:
  - export all rows
  - export filtered rows
- Simulated blockchain with 3 node files and validation checks
- Audit-to-blockchain linking via chain index on blockchain-backed actions
- Document integrity verification using a classroom hash approach

## Build and Run

From project root in PowerShell:

```powershell
g++ -std=c++17 src/main.cpp src/auth.cpp src/documents.cpp src/verification.cpp src/budget.cpp src/audit.cpp src/approvals.cpp src/blockchain.cpp src/ui.cpp -o procurechain.exe
.\procurechain.exe
```

## Seeded Admin Accounts

- Super Admin: admin_test / admin1234
- Procurement Officer: proc_admin / proc1234
- Budget Officer: budget_admin / budget1234
- Municipal Administrator: mun_admin / mun1234

## Main Data Files

- data/users.txt
- data/admins.txt
- data/documents.txt
- data/approvals.txt
- data/budgets.txt
- data/audit_log.txt
- data/blockchain/node1_chain.txt
- data/blockchain/node2_chain.txt
- data/blockchain/node3_chain.txt

## Data Schema Notes (Current)

- users.txt: userID|fullName|username|password|status|updatedAt
- admins.txt: adminID|fullName|username|password|role|status|updatedAt
- documents.txt: docID|title|category|department|dateUploaded|uploader|status|hashValue|budgetCategory|amount
- approvals.txt: docID|approverUsername|role|status|createdAt|decidedAt
- budgets.txt: category|amount
- audit_log.txt: timestamp|action|targetID|actor|chainIndex

Existing legacy rows are still supported and migrated safely during startup.

## Role Access Snapshot

- Citizen:
  - view published documents
  - view budgets
  - view audit trail and export CSV
- Procurement Officer:
  - upload documents
  - view/search/filter documents
- Budget Officer, Municipal Administrator:
  - review pending approvals
  - approve/reject documents
  - view approval analytics
- Super Admin:
  - all high-privilege controls
  - blockchain validation
  - document integrity verification
  - account lifecycle management

## Docs Folder

Planning and collaboration documents are maintained under:

- Docs/current_prd_procurechain.md
- Docs/context.md
- Docs/phase_tasks.md
- Docs/checklist.md

## Limitations

- Hashing is intentionally non-cryptographic (classroom simulation).
- Storage is flat-file based and not designed for concurrent multi-process writes.
- This project prioritizes explainable procedural C++ over enterprise architecture.
