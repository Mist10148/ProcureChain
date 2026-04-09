# ProcureChain Phase Tasks and Implementation Timeline

## Project
ProcureChain: Municipal Procurement Document Tracking System  
Platform: C++ CLI (Procedural)  
Storage: TXT files in data folder  
Last Updated: 2026-04-09

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
- full admin listing and ID search
- citizen published-only viewing
- metadata persistence using pipe-delimited format

### Status

Completed

## Phase 3: Approval Workflow

### Objectives

- generate approval requests for required approver roles
- implement approve/reject actions
- automate document status transitions

### Implemented

- request generation for Budget Officer and Municipal Administrator
- per-approver status persistence in approvals.txt
- reject-any rule and unanimous publish rule
- pending state handling while decisions are incomplete

### Status

Completed

## Phase 4: Blockchain and Integrity Verification

### Objectives

- append key actions to simulated blockchain
- validate chain linkage and node consistency
- support document integrity checks

### Implemented

- append to five node ledger files
- per-node validation and cross-node consistency checks
- integrity verification flow for document hash comparison

### Status

Completed

## Phase 5: Budget and Audit Baseline

### Objectives

- implement budget summary and admin update flows
- centralize audit logging for core actions

### Implemented

- budget summary viewing
- add budget category and update amount
- audit append and audit trail rendering
- action frequency display

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
- department contains
- uploader contains

Notes:

- filters can be combined in one query
- date range checks enforce valid bounds

#### 6.2 Approval Analytics Dashboard

Implemented metrics:

- total approval rows
- decided and pending rows
- approved and rejected totals
- rejection rate
- average decision time (hours)
- throughput by role

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

#### 6.4 Account Lifecycle Administration

Implemented controls (Super Admin):

- list users and admins with status
- deactivate account
- reactivate account
- reset password via generated temporary password

Enforcement:

- inactive accounts are denied during login

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

#### 6.6 Audit-to-Blockchain Linkage

Implemented behavior:

- blockchain append returns chain index
- audit rows store optional chain index
- audit view displays chain index when present

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

Still to execute before final defense:

- full scripted end-to-end walkthrough per role
- one presentation-ready filtered CSV export sample
- one intentional tamper simulation followed by blockchain validation

## Optional Stretch Phases (If Time Allows)

### Phase 9A: Security and UX Refinements

- force password change flow after temporary reset
- clearer in-menu status/error wording for edge cases

### Phase 9B: Workflow Flexibility

- delegated approvals
- richer date presets for reports

### Phase 9C: Reporting Enhancements

- additional trend views over time windows
- optional document-to-budget drilldown report

## Current Priority Queue

1. finalize documentation consistency across README + Docs files
2. run full role-by-role final regression walkthrough
3. produce demo artifacts (CSV export and validation screenshots/log snippets)
4. polish menu prompts/messages for defense presentation

## Summary of Implementation Position

The project has moved beyond baseline CLI CRUD and now includes a governance-focused feature set:

- advanced query/filter capability
- approval performance analytics
- budget variance reporting
- account lifecycle administration
- audit export and chain-linked traceability

This places ProcureChain in a strong final-defense state while remaining within the required procedural C++ course scope.
