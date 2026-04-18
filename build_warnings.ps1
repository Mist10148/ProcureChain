g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude `
  src/main.cpp `
  src/analytics.cpp `
  src/approvals.cpp `
  src/audit.cpp `
  src/auth.cpp `
  src/backup.cpp `
  src/blockchain.cpp `
  src/budget.cpp `
  src/delegation.cpp `
  src/documents.cpp `
  src/help.cpp `
  src/notifications.cpp `
  src/summarizer.cpp `
  src/ui.cpp `
  src/verification.cpp `
  -o build/ProcureChain-warn.exe
