# ProcureChain: Municipal Procurement Document Tracking System
## Product Requirements Document (Implemented Baseline)

## 1. Project Overview

**Project Type:** CC 203 Finals Project  
**Platform:** C++ CLI (Command-Line Interface)  
**Architecture Style:** Procedural, file-based  
**Date Updated:** 2026-04-09

ProcureChain is a classroom-focused municipal procurement tracking system with role-based access, multi-role approvals, reporting, and simulated blockchain validation.

## 2. Goals

1. Track procurement document lifecycle from upload to publication.
2. Enforce multi-role approvals before publication.
3. Provide transparency through audit logs and budget reports.
4. Add tamper-evidence using blockchain-style linked records.
5. Keep implementation explainable for finals defense (procedural C++ only).

## 3. User Roles

- Super Admin
- Procurement Officer
- Budget Officer
- Municipal Administrator
- Citizen/User

## 4. Implemented Core Features

### 4.1 Account-Based Access

- Citizen signup/login
- Admin signup/login with role selection
- Role-based dashboard routing

### 4.2 User Account Lifecycle Administration

**Super Admin only:**
- List all citizen and admin accounts
- Deactivate account
- Reactivate account
- Reset account password with auto-generated temporary password

### 4.3 Document Management

- Upload document with metadata and amount
- View all documents (admin)
- Search by document ID (admin)
- Manual status override (Super Admin)
- View published documents (citizen)

### 4.4 Advanced Document Filters

Admin filter criteria:
- status
- exact date
- date range
- category
- department
- uploader

### 4.5 Approval Workflow

- Auto-create approval rows at upload
- Required approver roles: Budget Officer and Municipal Administrator
- Per-approver decision recording
- Automatic document status transitions:
  - rejected if any rejection exists
  - published if all required approvals are decided and none rejected
  - pending_approval otherwise

### 4.6 Approval Analytics Dashboard

Metrics shown:
- total/decided/pending approval rows
- approved and rejected counts
- rejection rate
- average decision time
- role-based throughput chart

### 4.7 Budget Module

- View budget allocations
- Add budget category
- Update budget amount
- Budget variance report:
  - allocated vs actual totals by category
  - variance amount
  - utilization percentage

### 4.8 Audit Trail and CSV Export

- Append-only audit logging
- Audit table viewer
- Action frequency chart
- CSV export modes:
  - export all rows
  - export filtered rows (date/action/actor/target)

### 4.9 Simulated Blockchain

- 3 node files
- linked hash chain per node
- cross-node consistency validation
- append on key actions (upload/approve/reject/status/budget updates)

### 4.10 Audit-to-Blockchain Linking

- blockchain-backed actions store chain index in audit rows
- audit table displays optional chain index column

### 4.11 Document Integrity Verification

- role restricted verification action
- stored hash vs recomputed hash check
- valid/tampered output (classroom simulation)

## 5. Current Data Schema

- admins.txt: adminID|fullName|username|password|role|status|updatedAt
- users.txt: userID|fullName|username|password|status|updatedAt
- documents.txt: docID|title|category|department|dateUploaded|uploader|status|hashValue|budgetCategory|amount
- approvals.txt: docID|approverUsername|role|status|createdAt|decidedAt
- budgets.txt: category|amount
- audit_log.txt: timestamp|action|targetID|actor|chainIndex
- blockchain nodes: index|timestamp|action|documentID|actor|previousHash|currentHash

Backward compatibility is maintained for legacy rows with fewer columns.

## 6. Constraints

- Procedural C++ only
- Text-file persistence only
- No OOP redesign
- No external DB/web stack
- No production cryptography claims

## 7. Success Criteria

The implementation is considered successful when it demonstrates:

1. Correct role-based behavior
2. Working document lifecycle and approvals
3. Usable governance reporting (filters, analytics, variance)
4. Exportable audit evidence
5. Detectable blockchain inconsistency/tamper evidence
6. Stable compile/run flow in terminal environment
