# Context for ProcureChain Collaborators

## Live Status (2026-04-09)

Current implemented baseline:

- citizen and admin auth flows exist (signup/login)
- citizen dashboard is active
- admin command center supports grouped workspaces for documents, approvals, budgets, audit/integrity, and account administration
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

Latest integrity and consensus implementation updates:

- document upload now requires title, category, and description
- source file import path is optional (metadata-only upload is supported)
- allowed import extensions are pdf, docx, csv, txt
- document category uses guided choices with Other for custom input
- uploaded files are copied into data/documents and hashed with SHA-256
- citizen can search a published document and verify stored/recomputed hash with blockchain presence checks
- budget workflow moved to consensus entries with unanimous approval before publication
- budget input is separated from document upload and handled in Budget Workspace
- budget entries use dedicated files (budget_entries.txt and budget_approvals.txt)
- audit log now has hash-linked fields previousHash and currentHash
- blockchain explorer view provides node-level integrity and per-block consensus/tamper diagnostics
- empty seed files are auto-populated with realistic mock records for analytics/demo flows

Latest analytics and UX updates:

- overview dashboard as top-level admin entry
- analytics hub with approval, budget, audit, integrity, and executive screens
- compact/full layout mode toggle for analytics rendering
- optional paged detail table flow inside analytics reports
- adaptive chart and bar sizing based on selected layout mode
- hardened audit export input checks for date range and filename safety
- role-visible admin menus now hide unauthorized actions/workspaces
- search/filter screens now show recent/available suggestions before input
- document search supports exact ID, ID prefix, and keyword-guided matching

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
- description
- department
- upload date
- uploader
- status
- hash value
- file name
- file type
- file path
- file size bytes
- budget category
- amount

Input behavior note:

- budget category and amount remain in schema for compatibility and analytics, but they are no longer prompted in document upload.

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

- submitting budget entries with fiscal year, category, allocated amount, and description
- unanimous budget approval workflow (Budget Officer + Municipal Administrator)
- publishing approved budgets to public budget summary
- variance report comparing allocated vs actual totals from approved/published documents

### 8. Audit Trail and Export

Audit captures key actions and supports:

- table rendering
- action frequency chart
- CSV export (all rows)
- CSV export (filtered rows by date/action/actor/target)
- hash-linked audit chain using previousHash and currentHash fields

### 9. Simulated Blockchain

Five ledger files represent node copies.

Each block stores:

- index
- timestamp
- action
- document ID
- actor
- previous hash
- current hash

Validation checks chain linkage and cross-node consistency.

Explorer checks include:

- per-node chain integrity
- first mismatch index per node against reference chain
- per-block consensus counts across all 5 nodes
- tamper/divergence alerts

### 10. Audit-to-Blockchain Linking

For blockchain-backed actions:

- append function returns block index
- audit row stores chainIndex
- audit viewer displays chain index when present

Hashing note:

- document and chain integrity now use SHA-256 hash computation in the verification module

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

    docID|title|category|description|department|dateUploaded|uploader|status|hashValue|fileName|fileType|filePath|fileSizeBytes|budgetCategory|amount

### approvals.txt

Stores approval state per document and approver.

Format:

    docID|approverUsername|role|status|createdAt|decidedAt

### budgets.txt

Stores budget allocation per category.

Format:

    category|amount

### budget_entries.txt

Stores budget entry submissions awaiting consensus.

Format:

    entryID|entryType|fiscalYear|category|allocatedAmount|description|createdAt|createdBy|status|publishedAt

### budget_approvals.txt

Stores per-approver budget consensus decisions.

Format:

    entryID|approverUsername|role|status|createdAt|decidedAt

### audit_log.txt

Stores activity logs and optional blockchain references.

Format:

    timestamp|action|targetID|actor|chainIndex|previousHash|currentHash

### blockchain node files

Simulate 5 ledger nodes:

- node1_chain.txt
- node2_chain.txt
- node3_chain.txt
- node4_chain.txt
- node5_chain.txt

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
- search published document by ID
- verify published document hash and blockchain presence
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

### Admin Command Center

- Overview Dashboard
    - open analytics hub
    - quick integrity snapshot
    - toggle compact/full layout mode
- Documents Workspace
    - upload with file import, view, search, advanced filters, manual status update
- Approvals Workspace
    - pending queue, approve, reject, detailed approval analytics
- Budget Workspace
    - published budget summary, budget entry submission, budget approvals, variance report
- Audit and Integrity Workspace
    - audit trail, blockchain validation, integrity verification, integrity snapshot, blockchain explorer
- Account Administration Workspace
    - account lifecycle management
- Logout

Note: menus are role-visible. Users only see actions available to their role.

## Roles SOP Reference

Detailed role-by-role SOP is documented in:

- Docs/roles.md

## Important Boundaries

Do not turn this into:

- a web app
- a DB-backed system
- an OOP-heavy capstone
- a real blockchain network
- an enterprise-grade architecture

This remains a school finals project and should stay scoped accordingly.
