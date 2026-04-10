#include "../include/help.h"
#include "../include/ui.h"
#include "../include/auth.h"

#include <iostream>

namespace {

void printSection(const std::string& title) {
    std::cout << "\n  " << ui::bold(title) << "\n";
    std::cout << "  " << ui::muted("--------------------------------------------------------------") << "\n";
}

void printHelpLine(const std::string& text) {
    std::cout << "    " << text << "\n";
}

void printKeyConceptsSection() {
    printSection("KEY CONCEPTS");

    std::cout << "\n  " << ui::info("Approval Flow") << "\n";
    printHelpLine("Documents require unanimous approval from all designated approver roles.");
    printHelpLine("If any approver rejects, the document is marked as rejected.");
    printHelpLine("Only when all required approvers approve does the document get published.");

    std::cout << "\n  " << ui::info("Budget Consensus") << "\n";
    printHelpLine("Budget entries follow the same unanimous approval model.");
    printHelpLine("Once all approvers agree, the budget entry is published and");
    printHelpLine("reflected in the public budget allocations.");

    std::cout << "\n  " << ui::info("Blockchain Integrity") << "\n";
    printHelpLine("Key actions are recorded across 5 simulated blockchain ledger nodes.");
    printHelpLine("Each block is hash-linked to the previous one for tamper detection.");
    printHelpLine("The blockchain explorer can verify consistency across all nodes.");

    std::cout << "\n  " << ui::info("Audit Trail") << "\n";
    printHelpLine("All significant actions are logged with hash-linked audit records.");
    printHelpLine("The audit chain provides a cryptographic history of system activity.");
    printHelpLine("Audit logs can be exported as CSV for external review.");

    std::cout << "\n  " << ui::info("Password Security") << "\n";
    printHelpLine("All passwords are hashed using SHA-256 before being stored.");
    printHelpLine("After a password reset, you will be required to set a new password");
    printHelpLine("on your next login.");

    std::cout << "\n  " << ui::info("Delegated Approvals") << "\n";
    printHelpLine("Approvers (Budget Officer, Municipal Administrator) can delegate");
    printHelpLine("their approval authority to another admin for a specified date range.");
    printHelpLine("Delegated actions are fully audited.");
}

} // namespace

void showHelpMenu(const std::string& role) {
    clearScreen();
    ui::printSectionTitle("IN-APP HELP");
    std::cout << "  Your Role: " << ui::bold(role) << "\n";

    printSection("AVAILABLE ACTIONS FOR YOUR ROLE");

    if (role == "Citizen") {
        printHelpLine("[1] View Published Documents - Browse all published procurement documents");
        printHelpLine("[2] View Procurement Budgets - See published budget allocations");
        printHelpLine("[3] View Audit Trail        - Review system activity history");
        printHelpLine("[4] Search Published Document - Find a specific document by ID");
        printHelpLine("[5] Verify Document Hash    - Verify document integrity via blockchain");
        printHelpLine("[6] Notification Inbox      - Check for recently published documents");
    } else if (role == "Procurement Officer") {
        printHelpLine("Documents Workspace:");
        printHelpLine("  - Upload Document: Create new procurement document records");
        printHelpLine("  - View/Search/Filter documents and export to TXT");
        printHelpLine("");
        printHelpLine("Budget Workspace:");
        printHelpLine("  - View budget allocations and variance reports");
        printHelpLine("");
        printHelpLine("Audit and Integrity:");
        printHelpLine("  - View audit trail and blockchain explorer");
    } else if (role == "Budget Officer") {
        printHelpLine("Approvals Workspace:");
        printHelpLine("  - View pending approvals and approve/reject documents");
        printHelpLine("  - View approval analytics and manage delegations");
        printHelpLine("");
        printHelpLine("Budget Workspace:");
        printHelpLine("  - Submit and approve/reject budget entries");
        printHelpLine("  - View variance reports");
        printHelpLine("");
        printHelpLine("Audit and Integrity:");
        printHelpLine("  - View audit trail and blockchain explorer");
    } else if (role == "Municipal Administrator") {
        printHelpLine("Approvals Workspace:");
        printHelpLine("  - View pending approvals and approve/reject documents");
        printHelpLine("  - View approval analytics and manage delegations");
        printHelpLine("");
        printHelpLine("Budget Workspace:");
        printHelpLine("  - Approve/reject budget entries and view variance reports");
        printHelpLine("");
        printHelpLine("Audit and Integrity:");
        printHelpLine("  - View audit trail and blockchain explorer");
    } else if (role == "Super Admin") {
        printHelpLine("Full Access - All workspaces and administrative controls:");
        printHelpLine("  - Overview Dashboard with analytics hub");
        printHelpLine("  - Documents: upload, search, filter, export, manual status override");
        printHelpLine("  - Approvals: escalation queue, manage approval rules");
        printHelpLine("  - Budget: submit entries, view variance reports");
        printHelpLine("  - Audit: full trail, CSV export, blockchain validation");
        printHelpLine("  - Account Admin: manage accounts, reset passwords, backup/restore");
    }

    printKeyConceptsSection();

    printSection("NEED MORE HELP?");
    printHelpLine("Refer to Docs/roles.md for detailed role SOPs.");
    printHelpLine("Refer to README.md for system overview and data file formats.");

    std::cout << "\n";
    waitForEnter();
}
