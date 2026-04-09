# ProcureChain Roles and SOP Guide

## Purpose

This document defines who can do what in ProcureChain and the standard operating procedure (SOP) for each role.

It reflects the current implementation as of 2026-04-09.

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
- Search published document by ID
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
4. Use Verify Published Document Hash to validate stored and recomputed hash values.
5. Open Budget Summary to review published budget allocations.
6. Open Audit Trail for accountability review and optional CSV export.
7. Logout.

## Super Admin SOP

### Scope

Super Admin is the governance and platform-control role.

### Allowed Actions

- Admin Overview Dashboard and Analytics Hub
- Documents Workspace (including upload and manual status override)
- Approvals Workspace (view pending and analytics)
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

### Not Allowed / Special Rule

- Approve/Reject actions are reserved for approver roles (Budget Officer and Municipal Administrator).

### Standard Procedure

1. Login as Super Admin.
2. Start from Overview Dashboard to inspect KPIs and alerts.
3. Use Analytics Hub for deeper governance reporting.
4. Use Audit and Integrity tools for trust checks.
5. Use Account Administration for account lifecycle tasks.
6. Logout after completion.

## Procurement Officer SOP

### Scope

Procurement Officer is the document preparation and submission role.

### Allowed Actions

- Upload documents
- View all documents
- Search documents (with recent previews and suggestions)
- Use advanced document filters (with available-value suggestions)
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
2. Upload document with title, category, description, and source file path.
3. Confirm successful import and SHA-256 hash output after upload.
4. Confirm approval requests were generated automatically.
5. Use search/filter tools to monitor document progress.
6. Review audit trail if needed.
7. Logout.

## Budget Officer SOP

### Scope

Budget Officer is an approver and budget-domain operator.

### Allowed Actions

- View pending approvals
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
5. Check approval analytics for governance performance.
6. Logout.

## Municipal Administrator SOP

### Scope

Municipal Administrator is a second required approver role.

### Allowed Actions

- View pending approvals
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
2. System creates pending approval rows for required approver roles:
   - Budget Officer
   - Municipal Administrator
3. Each approver can only decide their own pending entries.
4. If any required approver rejects, document becomes rejected.
5. If all required decisions are non-rejected, document becomes published.

## Budget Consensus SOP (Cross-Role)

1. Budget Officer or Super Admin submits a budget entry (fiscal year/category/allocated amount/description).
2. System creates pending budget approvals for Budget Officer and Municipal Administrator.
3. Each approver can only decide their own pending budget entries.
4. Any rejection marks the budget entry rejected.
5. Budget entry is published to public budgets only after unanimous non-rejected decisions.

## Account Lifecycle SOP (Super Admin)

1. Open Account Administration Workspace.
2. List accounts to inspect current statuses.
3. Choose account type (Citizen/Admin), then target username.
4. Select action:
   - Deactivate
   - Reactivate
   - Reset password
5. Share temporary password securely when reset is used.

## Audit and Integrity SOP

- Audit trail is append-based and includes chain index for blockchain-backed actions.
- Audit trail rows are hash-linked using previousHash and currentHash.
- Filtered CSV export should be used for reporting windows.
- Blockchain validation should be run before final governance reporting.
- Document integrity verification should be run for suspicious records.
- Blockchain explorer can be used for node and per-block tamper diagnostics.

## Operational Notes

- Inactive accounts are blocked at login.
- Menus are dynamic by role; inaccessible actions are hidden in normal flows.
- Search-heavy screens now show recent or available values before input to reduce invalid entries.
- Hashing now uses SHA-256, but ProcureChain remains a classroom file-based simulation.
