# Context for ProcureChain Collaborators

## Current Implementation Snapshot (2026-04-09)

Implemented and tested baseline includes:

- role-based auth and dashboards
- account lifecycle controls (Super Admin)
- document upload/view/search/status update
- advanced multi-criteria document filters
- unanimous approval workflow
- approval analytics dashboard
- budget management and variance reporting
- audit viewing plus CSV export (all and filtered)
- blockchain append and validation
- audit-to-blockchain linkage via chain index
- document integrity verification

## Project Boundaries

This is a beginner-level procedural C++ finals project.

Keep changes within:
- functions
- structs
- vectors
- file handling
- simple CLI menus

Avoid introducing:
- full OOP redesign
- DBMS/web stack
- external distributed systems

## Folder Layout

- src/: implementation modules
- include/: module headers
- data/: runtime storage
- data/blockchain/: simulated node ledgers
- Docs/: planning and project-tracking documents

## Data Files and Formats

### admins.txt
adminID|fullName|username|password|role|status|updatedAt

### users.txt
userID|fullName|username|password|status|updatedAt

### documents.txt
docID|title|category|department|dateUploaded|uploader|status|hashValue|budgetCategory|amount

### approvals.txt
docID|approverUsername|role|status|createdAt|decidedAt

### budgets.txt
category|amount

### audit_log.txt
timestamp|action|targetID|actor|chainIndex

### blockchain node files
index|timestamp|action|documentID|actor|previousHash|currentHash

## Role Intent

### Super Admin
- full governance controls
- account lifecycle management
- status override, blockchain validation, verification

### Procurement Officer
- upload documents
- view/search/filter documents

### Budget Officer and Municipal Administrator
- approve/reject pending documents
- view approval analytics

### Citizen
- transparency views: published docs, budgets, audit trail

## Module Responsibilities

- auth.cpp: account storage, login, lifecycle admin actions
- documents.cpp: document CRUD-like actions and advanced filters
- approvals.cpp: approval decisions + analytics calculations
- budget.cpp: budget management + variance report
- audit.cpp: audit append/view/export
- blockchain.cpp: append + validation
- verification.cpp: hash recomputation and integrity check
- main.cpp: menu routing and role gates
- ui.cpp: consistent terminal rendering

## Integration Notes

- Legacy rows are migrated forward lazily: loaders accept old field counts and writers persist current schema.
- Blockchain append now returns block index so audit records can reference chain location.
- Document status changes recompute hash using original hash-source fields for compatibility.

## Quick Build Command

```powershell
g++ -std=c++17 src/main.cpp src/auth.cpp src/documents.cpp src/verification.cpp src/budget.cpp src/audit.cpp src/approvals.cpp src/blockchain.cpp src/ui.cpp -o procurechain.exe
```
