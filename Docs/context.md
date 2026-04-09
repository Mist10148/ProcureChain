# Context for ProcureChain Collaborators

## Live Status (2026-04-09)

Current implemented baseline:

- citizen and admin auth flows exist (signup/login)
- citizen dashboard is active
- admin dashboard supports document upload/view/search, pending approvals, approve/reject decisions, manual status update, audit trail viewing, and blockchain validation
- admin budget management is implemented (view/add/update)
- published documents, verification, budget view, and audit logging are integrated
- codebase is modularized by feature using src and include modules
- admin dashboard actions enforce role permissions (Procurement/Budget/Municipal/Super Admin)
- approval request generation is restricted to designated approver roles
- key actions append blockchain transactions (upload/approve/reject/status/budget updates)
- startup seeding initializes demo admin accounts, approvals, and blockchain starter rows when needed
- role-based regression scenarios have been executed and validated through data and audit outputs

Newly implemented governance/reporting slice:

- advanced document filters (status, exact date, date range, category, department, uploader)
- audit CSV export (all rows and filtered rows)
- approval analytics dashboard (rejection rate, average decision time, throughput by role)
- budget variance report (allocated vs actual, variance, utilization)
- account lifecycle admin controls (deactivate/reactivate/reset password)
- audit-to-blockchain linking via chain index

## Development Constraints

The implementation must stay within topics already covered in class:

- basic C++ syntax
- functions
- arrays
- vectors
- structs
- file handling
- simple control flow and menus

Do not redesign the project around:

- OOP / classes
- inheritance / polymorphism
- advanced frameworks
- networking
- real databases
- web development
- external blockchain platforms

The code should remain easy to explain in a finals defense.

## Project Type

- Language: C++
- Interface: CLI / terminal-based
- Storage: TXT files using delimiters
- Data handling: vectors, arrays, structs, and functions
- Hash and blockchain approach: simulated blockchain using text files

## Final Project Goal

Build a CLI system that simulates a municipal procurement document tracking system.

The system allows:

- layered admin access
- citizen account login/signup
- procurement document storage and tracking
- unanimous approval workflow before publishing
- document integrity verification
- budget viewing and variance reporting
- audit trail viewing and CSV export
- simulated blockchain ledger validation

## Core Functional Requirements

### 1. Account-Based Access

Supported roles:

- Super Admin
- Procurement Officer
- Budget Officer
- Municipal Administrator
- Citizen/User

Citizens sign up or log in before accessing published procurement records and transparency tools.

### 2. Account Lifecycle Administration (Super Admin)

Super Admin can:

- list all citizen and admin accounts
- deactivate or reactivate accounts
- reset account password using generated temporary password

Inactive accounts are denied at login.

### 3. Procurement Document Management

The system handles:

- purchase requests
- canvass forms
- purchase orders
- notices of award
- contracts
- inspection or acceptance records

Document fields currently stored:

- document ID
- title
- category
- department
- upload date
- uploader
- status
- hash value
- budget category
- amount

### 4. Advanced Document Filtering

Admin filters support:

- status
- exact date
- date range (from/to)
- category (contains)
- department (contains)
- uploader (contains)

Combined filtering is supported in one action.

### 5. Unanimous Consensus Approval Protocol

Publication requires decisions from required approver roles.

Behavior:

- document upload generates pending approval rows
- one rejection marks document rejected
- all required decisions without rejection publishes document
- pending decisions keep document pending_approval

### 6. Approval Analytics

Analytics dashboard computes:

- total rows
- decided rows
- pending rows
- approved count
- rejected count
- rejection rate
- average decision time (rows with complete timestamps)
- throughput chart by role

### 7. Budget Viewing and Variance

Budget module supports:

- viewing category allocations
- adding category
- updating category amount
- variance report comparing allocated vs actual totals from approved/published documents

### 8. Audit Trail and Export

Audit captures key actions and supports:

- table rendering
- action frequency chart
- CSV export (all rows)
- CSV export (filtered rows by date/action/actor/target)

### 9. Simulated Blockchain

Three ledger files represent node copies.

Each block stores:

- index
- timestamp
- action
- document ID
- actor
- previous hash
- current hash

Validation checks chain linkage and cross-node consistency.

### 10. Audit-to-Blockchain Linking

For blockchain-backed actions:

- append function returns block index
- audit row stores chainIndex
- audit viewer displays chain index when present

## Folder Structure

Use this structure:

    ProcureChain/
    |
    |-- src/
    |-- include/
    |-- data/
    |   |-- blockchain/
    |-- Docs/
    |-- README.md

## Meaning of Each Data File

### admins.txt

Stores admin accounts and roles.

Format:

    adminID|fullName|username|password|role|status|updatedAt

### users.txt

Stores citizen accounts.

Format:

    userID|fullName|username|password|status|updatedAt

### documents.txt

Stores procurement document metadata and budget mapping.

Format:

    docID|title|category|department|dateUploaded|uploader|status|hashValue|budgetCategory|amount

### approvals.txt

Stores approval state per document and approver.

Format:

    docID|approverUsername|role|status|createdAt|decidedAt

### budgets.txt

Stores budget allocation per category.

Format:

    category|amount

### audit_log.txt

Stores activity logs and optional blockchain references.

Format:

    timestamp|action|targetID|actor|chainIndex

### blockchain node files

Simulate 3 ledger nodes:

- node1_chain.txt
- node2_chain.txt
- node3_chain.txt

Format:

    index|timestamp|action|documentID|actor|previousHash|currentHash

## Role Structure

### Super Admin

Can:

- manage accounts and lifecycle status
- view all records and reports
- run blockchain validation
- run document verification

### Procurement Officer

Can:

- upload procurement documents
- search/view/filter document records

### Approving Officers

Examples:

- Budget Officer
- Municipal Administrator

Can:

- review pending documents
- approve/reject documents
- view approval analytics

### Citizen/User

Can:

- sign up
- log in
- view published documents
- view budgets
- view audit trail and export CSV

## Recommended CLI Flow

### Main Menu

- Login
- Sign Up
- Exit

### Citizen Menu

- View Published Documents
- View Budget Summary
- View Audit Trail
- Logout

### Admin Menu

- Upload Document
- View All Documents
- Search Document by ID
- View Pending Approvals
- Approve/Reject Document
- Update Document Status
- Manage Budgets
- View Audit Trail
- Validate Blockchain
- Verify Document Integrity
- Advanced Document Filters
- Approval Analytics Dashboard
- Account Lifecycle Management
- Logout

## Important Boundaries

Do not turn this into:

- a web app
- a DB-backed system
- an OOP-heavy capstone
- a real blockchain network
- an enterprise-grade architecture

This remains a school finals project and should stay scoped accordingly.
