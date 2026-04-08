# ProcureChain
A Secure Document Access for Municipality

## Build and Run

This project is modularized, and `main.cpp` acts as the entry point that ties modules together.

Run from the project `src` folder (PowerShell):

```bash
g++ main.cpp -o main.exe
.\main.exe
```

Run from the project root folder (PowerShell):

```bash
g++ src/main.cpp -o main.exe
.\main.exe
```

## Current Login Flow

After selecting Login in Home, choose one of these account types:

- Citizen Login
- Admin Login (stub dashboard)

## Notes About Current Features

- Citizen flow supports signup, login, viewing published documents, verification, and budget viewing.
- Admin flow currently opens a stub dashboard for menu/navigation testing while admin modules are still pending.
- Document verification uses a simple classroom hash and is not cryptographic.

## Quick Test Credentials

Use the seeded admin account:

- Username: `admin_test`
- Password: `admin1234`
