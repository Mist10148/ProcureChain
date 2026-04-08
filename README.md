# ProcureChain
A Secure Document Access for Municipality

## Build and Run

This project is modularized, and `main.cpp` acts as the entry point that ties modules together.

Run from the project `src` folder (PowerShell):

```bash
g++ (Get-ChildItem -Name *.cpp) -Wall -Wextra -o main
.\main
```

Run from the project root folder (PowerShell):

```bash
g++ src\main.cpp src\auth.cpp src\audit.cpp src\documents.cpp src\verification.cpp src\budget.cpp src\approvals.cpp src\blockchain.cpp -Wall -Wextra -o main
.\main
```

## Current Login Flow

After selecting Login in Home, choose one of these account types:

- Citizen Login
- Admin Login

## Notes About Current Features

- Citizen flow supports signup, login, viewing published documents, verification, budget viewing, and audit trail viewing.
- Admin flow supports upload, document viewing/search, pending approval view, approve/reject actions, manual status update, audit trail view, and blockchain validation.
- Approval workflow is active: uploads create approval requests, and document status updates based on approval outcomes.
- Blockchain module supports node initialization, block append helpers, and 3-node validation/consistency checks.
- Document verification uses a simple classroom hash and is not cryptographic.

## Quick Test Credentials

Use these seeded admin accounts:

- Super Admin: `admin_test` / `admin1234`
- Procurement Officer: `proc_admin` / `proc1234`
- Budget Officer: `budget_admin` / `budget1234`
- Municipal Administrator: `mun_admin` / `mun1234`

Sample seeded citizen from test run:

- Citizen: `citizen_matrix1` / `pass1234`
