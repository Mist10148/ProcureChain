# ProcureChain Roles and SOP Guide

## Purpose

This document defines who can do what in ProcureChain and the standard operating procedure (SOP) for each role.

It reflects the current implementation as of 2026-04-17.

## Runtime Setup Note

- AI summarizer dependencies are managed from `ai_summarizer/requirements.txt`.
- `GEMINI_API_KEY` must be provided via shell environment or project-root `.env`.
- Root `.gitignore` excludes `.env` and `.env.*` (except `.env.example`).

## Access Model Summary

ProcureChain has two account groups:

- Citizen accounts
- Admin accounts

Admin accounts are split into four roles:

- Super Admin
- Procurement Officer
- Budget Officer
- Municipal Administrator

The UI now uses role-visible menus. Users only see actions they are allowed to execute.

## Citizen Role SOP

### Scope

Citizen accounts are transparency users (read-only for governance data).

### Allowed Actions

- View published procurement documents
- Search published document by ID or keyword
- Open published document detail from public audit trail drill-down
- View cached AI summary for selected published documents
- Generate/refresh AI summary for selected published documents
- Verify published document hash and blockchain registration
- View procurement budgets
- View audit trail
- Export audit trail CSV (from audit screen)

### Not Allowed

- Upload documents
- Approve/reject documents
- Manage budgets
- Validate blockchain
- Verify document integrity
- Manage accounts

### Standard Procedure

1. Login as Citizen.
2. Open Published Documents to inspect publicly available records.
3. Use Search Published Document when a specific record trace is needed.
4. Open document detail and run AI summary actions when plain-language context is needed.
5. Use Verify Published Document Hash to validate stored and recomputed hash values.
  - Put your own document copy in `data/verify` before running verify.
  - Supported placement patterns:
    - `data/verify/<DocumentID>/candidate.ext`
    - `data/verify/<DocumentID>_candidate.ext`
    - `data/verify/<DocumentID>.ext`
6. Open Budget Summary to review published budget allocations.
7. Open Audit Trail for accountability review, CSV export, and document drill-down.
8. Logout.

## Super Admin SOP

### Scope

Super Admin is the governance and platform-control role.

### Allowed Actions

- Admin Overview Dashboard and Analytics Hub
- Documents Workspace (including upload and manual status override)
- Approvals Workspace (view pending and analytics)
- Approvals Workspace request-for-comment thread
- Approvals Workspace escalation queue (overdue SLA items)
- Approvals Workspace rule management (category roles and SLA days)
- Budget Workspace (submit entries and monitor publication after consensus)
- Audit and Integrity Workspace
  - View audit trail
  - Validate blockchain
  - Verify document integrity
  - View integrity snapshot
  - View blockchain explorer
- Account Administration Workspace
  - List all accounts
  - Deactivate/reactivate accounts
  - Reset account passwords
  - Hard-delete admin accounts with linked node cleanup

### Not Allowed / Special Rule

- Approve/Reject actions are reserved for approver roles (Budget Officer and Municipal Administrator).

### Standard Procedure

1. Login as Super Admin.
2. Start from Overview Dashboard to inspect KPIs and alerts.
3. Use Analytics Hub for deeper governance reporting.
4. Use Audit and Integrity tools for trust checks.
5. Use Approvals Workspace to monitor escalation queue and configure approval rules.
6. Use Account Administration for account lifecycle tasks.
7. Review Notification Inbox for tamper alerts and pending governance actions.
8. Logout after completion.

## Procurement Officer SOP

### Scope

Procurement Officer is the document preparation and submission role.

### Allowed Actions

- Upload documents
- View all documents
- Search documents by ID or keyword (with ranked suggestions)
- Use advanced document filters including tags (with available-value suggestions)
- Resolve duplicate-hash warning prompts during upload (cancel/continue/link-as-amendment)
- View budget allocations and variance report
- View audit trail, integrity snapshot, and blockchain explorer

### Not Allowed

- Approve/reject documents
- Manual status override
- Manage budgets
- Validate blockchain
- Verify document integrity
- Manage accounts

### Standard Procedure

1. Login as Procurement Officer.
2. Upload document with title, category, and description.
3. Pick category from guided options (or use Other for custom value).
4. Optionally add comma-separated tags for discoverability.
5. Provide file path only when available (optional upload) and confirm SHA-256 output.
6. If revising a rejected record, enter rejected base Document ID to create amendment version (v2+).
7. Confirm approval requests were generated automatically.
8. Use search/filter tools to monitor document progress.
9. Review audit trail if needed.
10. Logout.

## Budget Officer SOP

### Scope

Budget Officer is an approver and budget-domain operator.

### Allowed Actions

- View pending approvals
- Use request-for-comment thread on pending approvals
- Approve or reject assigned documents
- View approval analytics dashboard
- View budget allocations and variance report
- Submit budget entries for consensus publication
- View pending budget entries
- Approve/reject budget entries assigned to this account
- View documents (list/search/filter)
- View audit trail, integrity snapshot, and blockchain explorer

### Not Allowed

- Upload documents
- Manual status override
- Verify document integrity
- Validate blockchain
- Manage accounts

### Standard Procedure

1. Login as Budget Officer.
2. Open Approvals Workspace and review pending decisions shown on screen.
3. Approve/reject documents based on review outcome.
4. Use Budget Workspace to submit and process budget entries.
5. Pick budget category from guided options (or use Other for custom value).
6. Check approval analytics for governance performance.
7. Logout.

## Municipal Administrator SOP

### Scope

Municipal Administrator is a second required approver role.

### Allowed Actions

- View pending approvals
- Use request-for-comment thread on pending approvals
- Approve or reject assigned documents
- View approval analytics dashboard
- View documents (list/search/filter)
- View budget allocations and variance report
- View pending budget entries
- Approve/reject budget entries assigned to this account
- View audit trail, integrity snapshot, and blockchain explorer

### Not Allowed

- Upload documents
- Manual status override
- Manage budgets
- Submit budget entries
- Verify document integrity
- Validate blockchain
- Manage accounts

### Standard Procedure

1. Login as Municipal Administrator.
2. Review pending items in Approvals Workspace.
3. Approve/reject assigned documents.
4. Monitor approval analytics and audit trail as needed.
5. Logout.

## Approval Protocol SOP (Cross-Role)

1. Procurement Officer uploads a document.
2. System creates pending approval rows using category-based rules in approval_rules.txt.
3. If no category rule is found, DEFAULT rule is applied.
4. Approvers can add request-for-comment entries before decisioning.
5. Each approver can only decide their own pending entries.
6. If any required approver rejects, document becomes rejected.
7. If all required decisions are non-rejected, document becomes published.
8. Conflict-of-interest rule: uploader cannot approve or reject their own document.

## Amendment Lineage SOP

1. Locate rejected base document ID.
2. Upload revised document and provide base rejected ID when prompted.
3. System saves new row with incremented versionNumber and previousDocId reference.
4. Approvals are regenerated for the amended version under current category rules.
5. Search detail panels show full version lineage.
6. Duplicate hash warning flow can also link a new upload as amendment against any matched document status.

## Budget Consensus SOP (Cross-Role)

1. Budget Officer or Super Admin submits a budget entry (fiscal year/category/allocated amount/description).
2. System creates pending budget approvals for Budget Officer and Municipal Administrator.
3. Each approver can only decide their own pending budget entries.
4. Any rejection marks the budget entry rejected.
5. Budget entry is published to public budgets only after unanimous non-rejected decisions.
6. Overrun guardrail is applied on publication:
  - warning at 90% utilization
  - publication blocked above 100% utilization

## Monthly Transparency Report SOP

1. Open Budget Workspace.
2. Select Generate Monthly Transparency Report.
3. Enter period using YYYY-MM.
4. System exports both TXT and CSV reports by default.
5. Reports include published document count, approval/rejection counts, and variance by category.

## AI Summary SOP (Citizen and Admin)

1. Open a document detail panel from search results (or public audit drill-down for citizen users).
2. Enter AI Summary Actions.
3. Select View Cached AI Summary to read the latest available summary without re-running Python.
4. Select Generate/Refresh AI Summary to run the Gemini-backed Python summarizer.
5. If generation fails, read the returned error and use cached summary fallback when available.

## Tamper Alert SOP

1. Open Notification Inbox and review tamper alert counts.
2. Admin roles should follow up using Audit and Integrity workspace checks.
3. Citizen users can cross-check affected documents through public audit and verification tools.
4. Governance response actions must be completed through normal audit-logged workflows.

## Public Audit Drill-Down SOP (Citizen)

1. Open Public Audit Trail and apply filters as needed.
2. Choose Open Published Document Detail.
3. Select a document ID listed in the current timeline targets.
4. Review the full document detail, lineage, and approval chain.
5. Use AI Summary Actions for plain-language interpretation support.

## Account Lifecycle SOP (Super Admin)

1. Open Account Administration Workspace.
2. List accounts to inspect current statuses.
3. Choose account type (Citizen/Admin), then target username.
4. Select action:
   - Deactivate
   - Reactivate
   - Reset password (user will be forced to change password on next login)
5. Share temporary password securely when reset is used.

## Data Backup and Restore SOP (Super Admin)

1. Open Account Administration Workspace.
2. Select Data Backup & Restore.
3. Create Backup: copies all data files to data/backups/YYYY-MM-DD_HHMMSS/.
4. Restore from Backup: lists available backups, requires confirmation before overwrite.
5. Both actions are recorded in the audit trail.

## Delegation SOP (Budget Officer, Municipal Administrator)

1. Open Approvals Workspace and select Delegation Management.
2. Create Delegation: specify delegatee admin username, start date, and end date.
3. Delegatee can approve/reject on behalf of delegator within the active date range.
4. Delegated decisions are automatically annotated with delegation info in the approval note.
5. Delegated pending items appear in the delegatee's approval hints.
6. Revoke delegation at any time from Delegation Management.
7. All delegation actions are audited.

## Forced Password Change SOP

- When Super Admin resets a password, the target account is flagged.
- On next login, the user must set a new password before accessing their dashboard.
- The new password is validated (length, confirmation match) and hashed with SHA-256.

## Approval Notes SOP

- When approving or rejecting a document or budget entry, the approver can optionally add a note.
- Notes are stored with the approval record and displayed in the document detail approval chain.
- Notes from delegated decisions automatically include delegation attribution.

## Request-for-Comment SOP

- Approver opens Approvals Workspace and selects Request-for-Comment Thread.
- Approver picks a pending document ID they can act on (directly or via delegation).
- Approver reviews existing thread and adds a new comment.
- Comment is saved to approval_comments.txt and audit-logged.

## Audit and Integrity SOP

- Audit trail is append-based and includes chain index for blockchain-backed actions.
- Audit trail rows are hash-linked using previousHash and currentHash.
- Filtered CSV export should be used for reporting windows.
- Blockchain validation should be run before final governance reporting.
- Document integrity verification should be run for suspicious records.
- Blockchain explorer can be used for node and per-block tamper diagnostics.

## Operational Notes

- All passwords are stored as SHA-256 hashes; plaintext passwords are auto-migrated on startup.
- Inactive accounts are blocked at login.
- Menus are dynamic by role; inaccessible actions are hidden in normal flows.
- Search-heavy screens now show recent or available values before input to reduce invalid entries.
- Search supports ranked keyword matches using ID/title/description/category/tags.
- Notification inbox is shown on login and accessible as a menu option.
- In-app help is available from both citizen and admin dashboards.
- Hashing now uses SHA-256, but ProcureChain remains a classroom file-based simulation.
