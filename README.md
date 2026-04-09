# ProcureChain

ProcureChain is a C++ command-line municipal procurement tracking system.

It is designed for classroom-level procedural programming and focuses on:

- Role-based access for citizens and administrators
- Procurement document lifecycle tracking
- Multi-role approval workflow
- Budget viewing and management
- Audit trail logging
- Simulated blockchain consistency checks

The system stores records in text files under the data folder and keeps the implementation modular through separate source files per feature.

## System Description

ProcureChain helps a municipality track procurement documents from upload to publication.

High-level process:

1. A procurement admin uploads a document.
2. Approval requests are created for required approver roles.
3. Approvers approve or reject.
4. Document status updates automatically based on approval outcomes.
5. Citizens can view published documents, while integrity verification is restricted to Super Admin.
6. Every important action is written to an audit log.
7. Key events are appended to three blockchain node files and can be validated for consistency.

## Main Features

- Citizen signup and login
- Admin signup and login with role assignment
- Published document viewing for citizens
- Admin document upload, search, view, and status update
- Approval routing for Budget Officer and Municipal Administrator
- Budget summary and admin budget management
- Timestamped audit trail
- Simulated blockchain append and validation across 3 node files

## Requirements

- Windows with PowerShell
- C++ compiler (g++ recommended, such as MSYS2 MinGW)

Optional check:

		g++ --version

## How To Build and Run

### Option A: Run from project root (recommended)

Open PowerShell in the ProcureChain folder and run:

		g++ -std=c++17 src/main.cpp src/auth.cpp src/documents.cpp src/verification.cpp src/budget.cpp src/audit.cpp src/approvals.cpp src/blockchain.cpp src/ui.cpp -o procurechain.exe
		.\procurechain.exe

### Option B: Run from src folder

		cd src
		g++ -std=c++17 main.cpp auth.cpp documents.cpp verification.cpp budget.cpp audit.cpp approvals.cpp blockchain.cpp ui.cpp -o ..\procurechain.exe
		cd ..
		.\procurechain.exe

### Notes

- On first run, the system creates and seeds required data files if missing.
- If PowerShell wildcard expansion behaves unexpectedly, use explicit file lists as shown above.

## Quick Login Accounts

Seeded admin accounts include:

- Super Admin: admin_test / admin1234
- Procurement Officer: proc_admin / proc1234
- Budget Officer: budget_admin / budget1234
- Municipal Administrator: mun_admin / mun1234

You can also create new citizen and admin accounts from the Sign Up menu.

## What Each CPP File Does

- src/main.cpp
	Entry point. Controls home/login navigation, role-based dashboards, and routing to feature modules.

- src/auth.cpp
	Handles signup/login logic, user/admin ID generation, username checks, file initialization, and basic seeded records.

- src/documents.cpp
	Handles document upload, listing, searching, published filtering, and status update logic.

- src/approvals.cpp
	Handles approval request creation, viewing pending approvals, and approve/reject decisions with document status outcomes.

- src/budget.cpp
	Handles budget viewing, table/chart output, add/update budget operations, and admin budget submenu flow.

- src/audit.cpp
	Handles audit timestamp generation, log writing, and readable audit trail display.

- src/verification.cpp
	Handles simple classroom hash calculation and integrity verification of document records.

- src/blockchain.cpp
	Handles simulated blockchain node file setup, block append for key actions, and cross-node validation.

- src/ui.cpp
	Central UI helper module for colored output, section titles, table rendering, and mini text charts.

## How The System Works During Use

1. Start the application.
2. Choose Login or Sign Up from Home.
3. If logging in, choose Citizen or Admin login type.
4. After login, the system opens the matching dashboard:
	 - Citizen dashboard for viewing published data and transparency records
	 - Admin dashboard for document, approval, verification, budget, audit, and blockchain actions
5. Actions write to:
	 - data/documents.txt
	 - data/approvals.txt
	 - data/budgets.txt
	 - data/audit_log.txt
	 - data/blockchain/node1_chain.txt
	 - data/blockchain/node2_chain.txt
	 - data/blockchain/node3_chain.txt

## Practical Walkthrough (End-to-End)

Use this sample run-through to demo the project:

1. Start the program and login as Procurement Officer.
2. Upload a new document.
3. Logout and login as Budget Officer.
4. Open pending approvals and approve the document.
5. Logout and login as Municipal Administrator.
6. Open pending approvals and approve the same document.
7. Login as Citizen.
8. View published documents and confirm the approved document appears.
9. Logout and login as Super Admin.
10. Run document verification for that document ID.
11. View audit trail to see recorded actions.
12. Run blockchain validation.

## How To Use Each Main Menu Quickly

- Home
	- Login
	- Sign Up
	- Exit

- Citizen Dashboard
	- View Published Documents
	- View Procurement Budgets
	- View Audit Trail
	- Logout

- Admin Dashboard
	- Upload, View, Search documents
	- View pending approvals
	- Approve or reject documents (role-limited)
	- Manual status override (Super Admin)
	- Verify document integrity (Super Admin)
	- Manage budgets (Budget Officer and Super Admin)
	- View audit trail
	- Validate blockchain (Super Admin)
	- Logout

## Important Limitation

The verification hash is intentionally simple for classroom simulation and is not cryptographic security.

## Troubleshooting

- Build fails with compiler not found:
	Install g++ and ensure it is on PATH.

- Data file open errors:
	Run the executable from the project root so relative paths resolve correctly.

- Menu loops after invalid input:
	Enter numeric menu choices only.
