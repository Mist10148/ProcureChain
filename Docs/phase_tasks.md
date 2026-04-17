# ProcureChain Phase Tasks and Implementation Timeline

## Project
ProcureChain: Municipal Procurement Document Tracking System  
Platform: C++ CLI (Procedural)  
Storage: TXT files in data folder  
Last Updated: 2026-04-17

## Purpose of this Document

This file tracks the implementation phases of the ProcureChain finals project.

It documents:

- what was planned per phase
- what has already been implemented in code
- what validation activities are completed
- what optional improvements remain

This is intended to help collaborators quickly understand the status and next actions without reading the entire source code first.

## Phase 0: Repository and Baseline Setup

### Objectives

- establish consistent folder structure
- split source and headers by feature area
- initialize core data files and blockchain node file paths

### Output

- modular source/header organization under src and include
- startup bootstrap checks for required data files
- seed-friendly baseline for users, admins, approvals, and blockchain rows

### Status

Completed

## Phase 13: Verification Reliability and Hash Stability Hotfixes

### Objectives

- ensure citizen verify-file detection works from practical folder patterns
- prevent startup logic from rewriting canonical document hashes
- align docs and user guidance for upload vs verify folder usage

### Implemented

- verify candidate discovery now supports:
	- `data/verify/<DocumentID>/...`
	- `data/verify/<DocumentID>_...`
	- extensionless candidate files for compatibility
- startup document normalization now only backfills missing hashes (no blanket rewrite)
- markdown documentation refreshed to describe upload staging vs citizen verification folders

### Status

Completed

## Phase 11: Governance Safeguards and Transparency Exports

### Objectives

- prevent duplicate-content ambiguity during document upload
- preserve explicit status-transition timeline per document
- enforce conflict-of-interest for approval decisions
- prevent overrun publication decisions using threshold guardrails
- generate monthly transparency report artifacts for governance review

### Implemented

- duplicate hash warning on upload with action options: cancel, continue, or link as amendment
- duplicate-link amendment path allows linking against matched documents regardless of status
- document status history ledger added (`data/document_status_history.txt`) with `docID|timestamp|actorUsername|fromStatus|toStatus|note`
- document detail panel now renders status timeline table
- strict conflict-of-interest guard: uploader cannot approve/reject own document (including delegated owner path)
- budget overrun guardrails enforced for document publish and budget publish paths
	- warn at 90% utilization
	- block above 100% utilization
- monthly transparency report generator added in Budget Workspace
	- exports both TXT and CSV by default
	- summarizes published documents, approval/rejection counts, and variance by category

### Status

Completed

## Phase 12: AI Summarization and Dynamic Node Evolution

### Objectives

- add AI summarizer actions in citizen/admin document detail screens
- support audit-trail document drill-down for citizen transparency flow
- migrate blockchain from fixed node count to dynamic `5 + total admins`
- add Super Admin hard-delete for admin accounts with node cleanup
- surface tamper alerts in notification inboxes

### Implemented (In Progress)

- added Python Gemini summarizer runner under ai_summarizer/scripts
- added AI runtime workspace folders for input/output manipulation
- added summary cache file schema at data/summarizer.txt
- integrated document detail summary actions (view cached, generate/refresh)
- added citizen public-audit drill-down to open published document detail
- migrated blockchain, verification, analytics, and backup to dynamic node paths
- added deterministic per-admin node naming and orphan cleanup
- added hard-delete admin workflow and linked node removal
- added tamper alert persistence and inbox rendering
- added reproducible Python dependency file at ai_summarizer/requirements.txt
- added root .gitignore and .env.example for safe GEMINI_API_KEY handling

### Remaining Validation (Demo Rehearsal)

- full compile and role smoke test for citizen/admin/super-admin flows
- Gemini dependency and API-key failure fallback verification
- tamper scenario simulation and inbox visibility verification

### Status

Completed (implementation) / Demo validation pending

## Phase 1: Authentication and Role Routing

### Objectives

- implement login flow
- implement signup flow for users and admins
- enforce role-based menu routing

### Implemented

- citizen signup and login
- admin signup and login
- role-aware dashboard routing
- duplicate username protection
- role display and enforcement in dashboards

### Status

Completed

## Phase 2: Document Registry Core

### Objectives

- support document upload and listing
- support document lookup by ID
- support visibility rules for published records

### Implemented

- upload of procurement document records
- upload requires title/category/description and supports optional source file import path
- category selection includes guided choices with Other for custom category input
- source file import accepts pdf/docx/csv/txt and copies into local document storage
- SHA-256 hash is computed from imported file content and stored with metadata
- upload supports optional comma-separated tags with normalized storage
- full admin listing and ID search
- ranked admin search supports ID/title/description/category/tag keywords
- citizen published-only viewing
- citizen published-document search by ID or keyword with ranked result suggestions
- metadata persistence using pipe-delimited format

### Status

Completed

## Phase 3: Approval Workflow

### Objectives

- generate approval requests for required approver roles
- implement approve/reject actions
- automate document status transitions

### Implemented

- request generation now uses category-based approval rules with DEFAULT fallback
- new document uploads create approval rows for all active admins in each required role (uploader excluded)
- approval row creation is duplicate-safe per (docID, approverUsername, role)
- per-approver status persistence in approvals.txt
- request-for-comment thread persistence in approval_comments.txt
- comment thread action for pending approvals (direct/delegated access)
- reject-any rule and unanimous publish rule
- pending state handling while decisions are incomplete
- publication is withheld until unanimous non-rejected decisions are completed
- super-admin approval rule management (add/update/delete)
- super-admin escalation queue for overdue pending approvals

### Status

Completed

## Phase 4: Blockchain and Integrity Verification

### Objectives

- append key actions to simulated blockchain
- validate chain linkage and node consistency
- support document integrity checks
- provide an explorer view for node and block-level tamper diagnostics

### Implemented

- append to five node ledger files
- per-node validation and cross-node consistency checks
- integrity verification flow for document hash comparison and blockchain hash evidence
- blockchain explorer with first mismatch index and per-block agreement counts

### Status

Completed

## Phase 5: Budget and Audit Baseline

### Objectives

- implement budget consensus submission/approval/publication flows
- centralize audit logging for core actions

### Implemented

- budget entry submission using fiscal year, category, allocated amount, and description
- budget category input now uses guided choices with custom Other fallback
- budget approvals by Budget Officer and Municipal Administrator
- new budget submissions create approval rows for all active Budget Officer and Municipal Administrator accounts (submitter excluded)
- budget approval row creation is duplicate-safe per (entryID, approverUsername, role)
- budget publication gate (published only after unanimous approvals)
- published budget summary viewing
- audit append and audit trail rendering
- action frequency display
- hash-linked audit chain with previousHash/currentHash

### Status

Completed

## Phase 6: Governance Reporting and Admin Controls (Current Expansion)

### Objectives

- add governance-grade querying and reporting
- improve account administration controls
- increase traceability across modules

### Implemented Work Items

#### 6.1 Advanced Document Filters

Implemented criteria:

- status
- exact upload date
- date range
- category contains
- tags contains
- department contains
- uploader contains

Notes:

- filters can be combined in one query
- date range checks enforce valid bounds
- filter screen now shows available-value suggestions before prompt input
- recent/available document previews are shown before search/filter actions

#### 6.2 Approval Analytics Dashboard

Implemented metrics:

- total approval rows
- decided and pending rows
- approved and rejected totals
- rejection rate
- average decision time (hours)
- throughput by role
- overdue pending approvals based on per-category SLA days
- pending SLA compliance rate
- per-role bottleneck table (pending, overdue, average age, worst overdue)

Latest expansion under analytics hub:

- approval funnel and throughput trends
- budget utilization comparisons
- audit activity timeline and action frequency
- integrity status cards and risk indicators
- executive snapshot
- compact/full layout mode toggle
- optional paged detail table flow

Data dependency:

- approvals rows now include createdAt and decidedAt timestamps

#### 6.3 Budget Variance Report

Implemented computation:

- allocated values from budgets.txt
- actual totals aggregated from approved/published documents
- variance and utilization per category
- published budget totals become the public baseline only after budget consensus publication

#### 6.4 Account Lifecycle Administration

Implemented controls (Super Admin):

- list users and admins with status
- deactivate account
- reactivate account
- reset password via generated temporary password
- deactivate/hard-delete admin operations are blocked when target accounts have pending document or budget approvals

Enforcement:

- inactive accounts are denied during login
- blocked lifecycle attempts leave approval datasets unchanged and preserve consensus thresholds on pending items

#### 6.5 Audit CSV Export

Implemented modes:

- export all rows
- export filtered rows (date/action/actor/target)
- export input validation for date range bounds and filename safety

#### 6.7 Admin Navigation Restructure

Implemented architecture:

- top-level admin command center
- grouped workspaces (documents, approvals, budget, audit/integrity, account admin)
- overview dashboard as entry to analytics hub and integrity snapshot
- role-visible workspace/action menus (unauthorized options are hidden)

#### 6.8 Search Guidance and Input Ergonomics

Implemented behavior:

- document search supports exact ID, ID prefix, and title/category keyword matching
- search screens show recent or available records before input
- approval decision screen lists pending document IDs for the current approver
- audit filtered export shows available action/actor/target/date suggestions before input

#### 6.6 Audit-to-Blockchain Linkage

Implemented behavior:

- blockchain append returns chain index
- audit rows store optional chain index
- audit view displays chain index when present

### Status

Completed

## Phase 10: Search, Discussion, and Compliance Extensions

### Objectives

- improve retrieval quality using ranked keyword search
- add pre-decision discussion channel for approvers
- expand analytics with department and compliance governance lenses

### Implemented

- ranked full-text search scoring across ID/title/description/category/tags
- citizen and admin search prompts updated to "ID or Keyword"
- request-for-comment thread action in Approvals Workspace
- persistent approval comments in data/approval_comments.txt
- analytics hub additions: Department Workload Report and Compliance Audit Report

### Status

Completed

## Phase 6A: Amendment and Version Lineage

### Objectives

- support rejected-document amendment flow (v1 to v2)
- preserve lineage between document versions

### Implemented

- upload supports optional rejected base document reference
- amendment uploads increment versionNumber and store previousDocId
- detail/search screens display version lineage history
- backward-compatible parsing defaults old rows to version 1

### Status

Completed

## Phase 7: Backward Compatibility and Data Migration Safety

### Objectives

- avoid breaking legacy rows after schema expansion
- support optional fields during parsing

### Implemented

- tolerant parsers for expanded file formats
- writes persist the current schema while older rows still load safely

### Status

Completed

## Phase 8: Regression Validation and Demo Readiness

### Validation Objectives

- verify role gates across all dashboards
- verify lifecycle login enforcement
- verify analytics and variance math with sample data
- verify audit export behavior and filter correctness
- verify blockchain validation and integrity workflow

### Validation State

Partially completed

Completed now:

- compile success after feature integration
- smoke launch/exit run
- role-based command paths validated through audit/data updates
- scripted admin walkthrough validated overview dashboard, analytics hub, layout toggles, and executive snapshot flow
- post-startup data migration checks confirm updated document/audit/blockchain schemas are active
- realistic seed datasets now auto-populate empty files for documents, approvals, budgets, budget consensus, and audit chain

Still to execute before final defense:

- full scripted end-to-end walkthrough per role
- one presentation-ready filtered CSV export sample
- one intentional tamper simulation followed by blockchain validation
- one citizen hash verification demonstration showing on-chain and mismatch outcomes

## Phase 9: Security, UX, and Governance Extensions

### Objectives

- harden credential storage with SHA-256 password hashing
- add approval notes for governance traceability
- improve login experience with notification inbox
- add in-app help for defense demo clarity
- add data backup/restore for data management
- enforce forced password change after admin reset
- enable delegated approval authority

### Implemented

- SHA-256 password hashing at rest with automatic plaintext migration on startup
- Optional approval/rejection notes on document and budget approval decisions
- Notes displayed in document detail approval chain panel
- Notification inbox shown on login for admins (pending approvals, overdue items) and citizens (recently published documents)
- Notification inbox accessible as a workspace menu option
- In-app help system with role-specific action guidance and key concept explanations
- Data backup and restore workspace (Super Admin) with timestamped backup folders
- Forced password change after Super Admin password reset using separate password_flags.txt
- Delegated approvals: Budget Officer and Municipal Administrator can delegate authority to another admin for a date range
- Delegated pending items shown in approval decision hints
- Delegation management workspace (create, view, revoke)
- All delegation and backup/restore actions are audited

### New Files

- include/notifications.h, src/notifications.cpp
- include/help.h, src/help.cpp
- include/backup.h, src/backup.cpp
- include/delegation.h, src/delegation.cpp
- data/password_flags.txt
- data/delegations.txt

### Status

Completed

## Current Priority Queue

1. run full role-by-role final regression walkthrough
2. produce demo artifacts (CSV export and validation screenshots/log snippets)
3. polish menu prompts/messages for defense presentation
4. prepare concise commit history for final submission branch

## Summary of Implementation Position

The project has moved beyond baseline CLI CRUD and now includes a governance-focused feature set:

- advanced query/filter capability
- approval performance analytics
- budget variance reporting
- budget consensus publication workflow
- account lifecycle administration
- audit export and chain-linked traceability
- blockchain explorer and tamper diagnostics across five nodes
- SHA-256 password hashing at rest
- approval/rejection notes
- notification inbox
- in-app help system
- data backup and restore
- forced password change after reset
- delegated approval authority
- duplicate hash safeguards with amendment-link fallback
- document status timeline ledger
- strict uploader conflict-of-interest protection
- publish-time budget overrun guardrails
- monthly transparency report export (TXT + CSV)

This places ProcureChain in a strong final-defense state while remaining within the required procedural C++ course scope.
