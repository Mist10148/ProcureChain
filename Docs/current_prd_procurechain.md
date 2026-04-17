# ProcureChain: Municipal Procurement Document Tracking System
## Product Requirements Document (PRD)

## 1. Overview

Project Type: CC 203 Finals Project  
Platform: C++ CLI  
Architecture Style: Procedural (functions, structs, vectors, file handling)  
Out of Scope: OOP redesign, web stack, DBMS, networking, external blockchain services

ProcureChain is a CLI system that tracks municipal procurement records with role-based controls, consensus approvals, audit traceability, and simulated blockchain integrity checks.

## 2. Current Implementation Status (2026-04-17)

Implemented in code:

- Citizen and admin signup/login
- Role-visible command center and workspaces
- Document upload/view/search/filter/export flows
- Optional source-file import (`pdf`, `docx`, `csv`, `txt`) with SHA-256 hash capture
- Optional normalized document tags (comma-separated)
- Duplicate SHA-256 hash warning on upload with cancel/continue/link-as-amendment actions
- Ranked full-text search (`ID`, title, description, category, tags)
- Advanced document filters (status, exact date, date range, category, tags, department, uploader)
- Amendment lineage (`versionNumber`, `previousDocId`)
- Document status timeline ledger (`who`, `when`, `from`, `to`, `note`)
- Category-rule approvals with `DEFAULT` fallback and SLA thresholds
- Conflict-of-interest guard blocking uploader from approving/rejecting own document
- Pending approval queue with direct + delegated access
- Request-for-comment thread for approvals (persistent per-document discussion)
- Approval analytics dashboard
- Escalation queue for overdue pending approvals (Super Admin)
- Budget entry consensus workflow and publication gate
- Budget variance reporting
- Budget overrun guardrails on publish paths (warn at 90%, block above 100%)
- Monthly transparency report export (`TXT` + `CSV`) for published docs, approvals/rejections, and variance
- Audit trail table, action frequency, CSV export (all/filtered)
- Audit hash-linking (`previousHash`, `currentHash`) and optional `chainIndex`
- Blockchain append/validate/explorer across 5 node files
- Analytics hub with: approval funnel, budget utilization, audit activity, integrity status, executive snapshot, department workload, compliance audit
- Account lifecycle admin tools (Super Admin)
- SHA-256 password-at-rest + plaintext migration on startup
- Forced password change after admin reset
- Notification inbox and in-app help
- Backup/restore workspace (Super Admin)
- Delegation management for Budget Officer and Municipal Administrator
- AI summarizer integration (Python + Gemini) for document detail views with cache support
- Citizen public-audit drill-down to open document detail directly from timeline entries
- Dynamic blockchain node topology: base five nodes plus one node per admin record
- Super Admin hard-delete for admin accounts with linked node cleanup
- Tamper alert notifications surfaced in admin and citizen inbox views
- Citizen verify-folder detection supports both `data/verify/<DocumentID>/...` and `data/verify/<DocumentID>_...` patterns
- Startup hash normalization only backfills missing hashes (prevents accidental canonical hash drift)

## 3. Target Users

### 3.1 Citizen

Can:

- View published documents
- Search published documents by ID or keyword
- Verify document hash and blockchain presence
- View procurement budgets
- View/export audit trail
- Open notification inbox and help

### 3.2 Super Admin

Can:

- Access all dashboards/workspaces
- Manage account lifecycle and password resets
- Manage approval rules and monitor escalation queue
- Validate blockchain and verify document integrity
- Run backup and restore

### 3.3 Procurement Officer

Can:

- Upload documents (with optional tags)
- View/search/filter/export document records
- Track status and lineage

### 3.4 Budget Officer and Municipal Administrator

Can:

- View pending approvals
- Approve/reject documents and budget entries
- Use request-for-comment thread
- View approval analytics
- Use delegation management

## 4. Core Functional Requirements

1. Account-based access with role routing and role-visible menus.
2. Super Admin lifecycle controls: list, deactivate, reactivate, reset password.
3. Document management with metadata, optional import file, and hash persistence.
4. Document tagging with normalized storage and display support.
5. Ranked search for admin and citizen published flows.
6. Multi-criteria document filtering including tags.
7. Rule-driven unanimous approval protocol with category + `DEFAULT` fallback.
8. Request-for-comment thread for pending approvals with persistent history.
9. Approval analytics with SLA and bottleneck insights.
10. Budget consensus publication and variance reporting.
11. Audit trail with CSV export and hash-linked rows.
12. Simulated blockchain append + validation + explorer diagnostics.
13. Audit-to-blockchain linkage through `chainIndex`.
14. Password hashing and forced password change on reset.
15. Notification inbox and role-specific help.
16. Backup/restore operations for Super Admin.
17. Delegated approval authority with date-range control.
18. AI-generated document summary with cached read and manual refresh options.
19. Public audit timeline drill-down to published document detail views.
20. Dynamic node count rule: total nodes = 5 + total admins.
21. Tamper alert persistence and inbox notification rendering.
22. Super Admin hard-delete admin action with deterministic node file cleanup.

## 4.1 Upload and Verify Folder Behavior

- `data/uploads` is an admin upload staging folder.
- Upload flow copies selected files into `data/documents` and stores canonical hash/path in `data/documents.txt`.
- `data/verify` is a citizen verification folder for authenticity checks against canonical stored hash.
- Candidate file discovery for citizen verification accepts:
  - `data/verify/<DocumentID>/file.ext`
  - `data/verify/<DocumentID>_file.ext`
  - `data/verify/<DocumentID>.ext`

## 5. System Flow

### 5.1 Main Menu

- Login
- Sign Up
- Exit

### 5.2 Citizen Dashboard

- View Published Documents
- View Procurement Budgets
- View Audit Trail
- Search Published Document (ID or Keyword)
- Verify Published Document Hash
- Open published document detail from public audit trail and run AI summary actions
- Notification Inbox
- Help
- Logout

### 5.3 Admin Command Center

- Overview Dashboard
- Documents Workspace
  - Upload Document
  - View All Documents
  - Search Document (ID or Keyword)
  - Advanced Document Filters
  - Export Documents to TXT
  - Update Document Status (manual override, role-gated)
- Approvals Workspace
  - View Pending Approvals
  - Request-for-Comment Thread
  - Approve Document
  - Reject Document
  - Approval Analytics Dashboard
  - Escalation Queue (Super Admin)
  - Manage Approval Rules (Super Admin)
  - Delegation Management
- Budget Workspace
- Audit and Integrity Workspace
- Account Administration (Super Admin)
- Notification Inbox
- Help
- Logout

## 6. Data Storage Design

### 6.1 Core Files

- data/users.txt: userID|fullName|username|password|status|updatedAt
- data/admins.txt: adminID|fullName|username|password|role|status|updatedAt
- data/documents.txt: docID|title|category|description|department|dateUploaded|uploader|status|hashValue|fileName|fileType|filePath|fileSizeBytes|budgetCategory|amount|versionNumber|previousDocId|tags
- data/approval_rules.txt: category|requiredRoles|maxDecisionDays
- data/approvals.txt: docID|approverUsername|role|status|createdAt|decidedAt|note
- data/approval_comments.txt: docID|commenterUsername|commenterRole|createdAt|commentText
- data/document_status_history.txt: docID|timestamp|actorUsername|fromStatus|toStatus|note
- data/budgets.txt: category|amount
- data/budget_entries.txt: entryID|entryType|fiscalYear|category|allocatedAmount|description|createdAt|createdBy|status|publishedAt
- data/budget_approvals.txt: entryID|approverUsername|role|status|createdAt|decidedAt|note
- data/password_flags.txt: username|mustChangePassword
- data/delegations.txt: delegatorUsername|delegateeUsername|startDate|endDate|status
- data/audit_log.txt: timestamp|action|targetType|targetID|actorUsername|actorRole|outcome|visibility|chainIndex|previousHash|currentHash
- data/summarizer.txt: docID|updatedAt|status|model|summaryFile|sourceFile|error
- data/tamper_alerts.txt: timestamp|severity|source|targetID|detail|actor|visibility

### 6.2 Blockchain Files

- Base nodes: data/blockchain/node1_chain.txt to data/blockchain/node5_chain.txt
- Admin-linked nodes: data/blockchain/node_admin_<username>_chain.txt
- Effective node count rule: 5 + total rows in data/admins.txt

Node row format: index|timestamp|action|documentID|actor|previousHash|currentHash

## 7. Non-Functional Requirements

- Keep logic defense-friendly and procedural.
- Preserve backward-compatible loading for older rows.
- Keep CLI prompts clear and deterministic.
- Maintain auditability for governance actions.
- Keep Python summarizer setup reproducible via `ai_summarizer/requirements.txt`.
- Keep Gemini secrets in environment variables (shell or explicitly loaded `.env`).

## 8. Success Criteria

Project is successful when demo can show:

- Role-based login and role-visible workspaces
- End-to-end document flow with approval outcomes
- Tag-aware ranked search and multi-filter queries
- Request-for-comment thread usage before decisioning
- Approval/budget/compliance analytics reporting
- Budget publication and variance checks
- Audit export and blockchain integrity diagnostics
- Account lifecycle controls, backup/restore, delegation, and forced password-change path

## 9. Detailed Implementation Scope (Expanded)

This section preserves the concise PRD above while adding the fuller implementation detail used for final defense prep.

### 9.1 Document Lifecycle and Search

- Upload requires title, category, and description.
- Category input uses guided choices with an Other option for custom category text.
- Source file path is optional; metadata-only document rows are supported.
- Supported import extensions: pdf, docx, csv, txt.
- Imported files are copied into local storage and hashed with SHA-256.
- Upload supports optional comma-separated tags, normalized before save.
- Document schema persists lineage fields: versionNumber and previousDocId.
- Amendment flow supports creating revised versions from rejected base documents.
- Admin search supports ID and ranked keyword retrieval across title, description, category, and tags.
- Citizen search runs on published documents and supports ID or keyword input.
- Advanced filter criteria supports status, exact date, date range, category, tags, department, and uploader.
- Filter and search screens show available value hints before input.
- Documents workspace supports TXT export of all/filtered records.

### 9.2 Approval Governance

- Approval routing is driven by approval_rules data with category-specific rules and DEFAULT fallback.
- Required approver rows are created with createdAt timestamps.
- Decision outcomes store decidedAt and optional notes.
- Any reject transitions document to rejected.
- Unanimous non-rejected decisions transition document to published.
- Pending approvals view includes direct and delegated pending items.
- Request-for-comment thread is available before decisioning and persists per document.
- Approval comments persist in data/approval_comments.txt with commenter role and timestamp.
- Delegated decisions preserve actor context and governance traceability.

### 9.3 Budget Consensus

- Budget entries capture fiscal year, category, allocated amount, description, and creator metadata.
- Budget approval rows are generated for required approver roles.
- Rejection blocks publication.
- Unanimous non-rejected decisions publish entries into budget allocations.
- Variance reporting compares allocated totals with approved/published document totals by category.

### 9.4 Audit, Integrity, and Blockchain

- Core actions append to audit_log with action metadata.
- Audit rows maintain hash-linked continuity using previousHash/currentHash.
- Blockchain-backed actions can store chainIndex in audit rows.
- Audit screen supports CSV export for all rows and filtered rows.
- Filtered export validates date-range bounds and filename safety.
- Blockchain writes are replicated across dynamic node files: base five plus admin-linked nodes.
- Validation checks both per-node linkage integrity and cross-node consistency.
- Explorer view surfaces first mismatch index and per-block consensus diagnostics.
- Tamper detections emit public alert rows consumed by notification inboxes.

### 9.8 AI Summary Pipeline and Drill-Down

- Document detail views (citizen and admin) expose AI summary actions:
  - view cached summary
  - generate/refresh summary via Python Gemini runner
- Selected document input is copied into ai_summarizer workspace before Python execution.
- Summary cache metadata is persisted in data/summarizer.txt and loaded by the CLI.
- Public audit trail includes document drill-down to open published detail + summary actions.

### 9.5 Security and Access Controls

- Passwords are stored as SHA-256 hashes.
- Startup migration hashes legacy plaintext credentials.
- Super Admin reset enforces forced password change before dashboard access.
- Account lifecycle controls support deactivate/reactivate/reset.
- Role-visible menus hide unauthorized actions from navigation.

### 9.6 UX and Operations

- Notification inbox is shown on login and available from dashboards.
- In-app help provides role-specific guidance.
- Data backup and restore is available in Account Administration for Super Admin.
- Delegation management (create/view/revoke) is available for Budget Officer and Municipal Administrator.
- Analytics layout supports compact/full toggle and optional paged detail tables.

## 10. Analytics and Reporting Coverage

### 10.1 Overview and Analytics Hub Views

- Approval Funnel and Throughput Trends
- Budget Utilization Comparisons
- Audit Activity Timeline and Frequency
- Integrity Status Cards
- Executive Snapshot
- Department Workload Report
- Compliance Audit Report

### 10.2 Approval Analytics Metrics

- Total approval rows
- Decided rows
- Pending rows
- Approved rows
- Rejected rows
- Rejection rate
- Average decision time
- Throughput by role
- Overdue pending count (SLA-based)
- Pending SLA compliance rate
- Per-role bottleneck indicators

### 10.3 Department Workload Report Scope

- Department-level request volume
- Pending and decided workload distribution
- Time-to-decision indicators by department
- Publication/rejection mix by department context

### 10.4 Compliance Audit Report Scope

- Rule-based compliance checks
- PASS/REVIEW REQUIRED summary
- Violation listing with sortable severity context
- Operational audit entry for compliance report generation

## 11. Expanded Data Dictionary

### 11.1 High-Change Files and Notes

- data/documents.txt
  - includes tags and lineage fields for searchability and amendment traceability
- data/approvals.txt
  - includes timing fields and optional note for governance context
- data/approval_comments.txt
  - stores request-for-comment thread history
- data/audit_log.txt
  - supports chainIndex plus hash-linked row continuity
- data/delegations.txt
  - stores delegator/delegatee with active date range and status
- data/password_flags.txt
  - stores forced password-change requirement after reset

### 11.2 Current Schema Lines

- data/users.txt: userID|fullName|username|password|status|updatedAt
- data/admins.txt: adminID|fullName|username|password|role|status|updatedAt
- data/documents.txt: docID|title|category|description|department|dateUploaded|uploader|status|hashValue|fileName|fileType|filePath|fileSizeBytes|budgetCategory|amount|versionNumber|previousDocId|tags
- data/approval_rules.txt: category|requiredRoles|maxDecisionDays
- data/approvals.txt: docID|approverUsername|role|status|createdAt|decidedAt|note
- data/approval_comments.txt: docID|commenterUsername|commenterRole|createdAt|commentText
- data/budgets.txt: category|amount
- data/budget_entries.txt: entryID|entryType|fiscalYear|category|allocatedAmount|description|createdAt|createdBy|status|publishedAt
- data/budget_approvals.txt: entryID|approverUsername|role|status|createdAt|decidedAt|note
- data/password_flags.txt: username|mustChangePassword
- data/delegations.txt: delegatorUsername|delegateeUsername|startDate|endDate|status
- data/audit_log.txt: timestamp|action|targetType|targetID|actorUsername|actorRole|outcome|visibility|chainIndex|previousHash|currentHash
- data/blockchain/nodeX_chain.txt: index|timestamp|action|documentID|actor|previousHash|currentHash

## 12. Role Capability Snapshot

### 12.1 Citizen

- published documents view/search
- published hash verification
- budget view
- audit view/export
- notifications and help

### 12.2 Super Admin

- full overview and analytics access
- account lifecycle administration
- escalation queue and approval rule management
- blockchain validation and integrity verification
- backup and restore

### 12.3 Procurement Officer

- document upload (with tags)
- document view/search/filter/export
- status monitoring and amendment preparation

### 12.4 Budget Officer and Municipal Administrator

- approve/reject documents and budget entries
- request-for-comment participation
- approval analytics access
- delegation management

## 13. Defense-Ready Boundary Statement

ProcureChain remains intentionally scoped as a classroom procedural CLI project. The system demonstrates governance and traceability concepts using text-file persistence and simulated blockchain behavior, while explicitly avoiding enterprise infrastructure concerns such as distributed networking, external ledgers, and relational database integration.
