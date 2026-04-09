#include "../include/auth.h"
#include "../include/audit.h"
#include "../include/budget.h"
#include "../include/blockchain.h"
#include "../include/documents.h"
#include "../include/approvals.h"
#include "../include/analytics.h"
#include "../include/ui.h"
#include "../include/verification.h"

#include <iostream>
#include <limits>
#include <vector>

const int HOME_MIN_CHOICE = 0;
const int HOME_MAX_CHOICE = 2;

namespace {
// Role checks are centralized here so dashboard routing stays readable.
bool isRole(const Admin& admin, const std::string& roleName) {
    return admin.role == roleName;
}

bool canUploadDocuments(const Admin& admin) {
    // Procurement and Super Admin roles can create new document rows.
    return isRole(admin, "Procurement Officer") || isRole(admin, "Super Admin");
}

bool canViewPendingApprovals(const Admin& admin) {
    // Roles participating in consensus can inspect pending approval tasks.
    return isRole(admin, "Budget Officer") ||
           isRole(admin, "Municipal Administrator") ||
           isRole(admin, "Super Admin");
}

bool canViewApprovalAnalytics(const Admin& admin) {
    // Analytics is available to governance roles and Super Admin.
    return isRole(admin, "Budget Officer") ||
           isRole(admin, "Municipal Administrator") ||
           isRole(admin, "Super Admin");
}

bool canApproveOrReject(const Admin& admin) {
    // Final decision actions are limited to designated approver roles.
    return isRole(admin, "Budget Officer") || isRole(admin, "Municipal Administrator");
}

bool canManualOverrideStatus(const Admin& admin) {
    // Manual overrides are intentionally strict to preserve auditability.
    return isRole(admin, "Super Admin");
}

bool canManageBudgets(const Admin& admin) {
    // Budget officer domain action with Super Admin fallback authority.
    return isRole(admin, "Budget Officer") || isRole(admin, "Super Admin");
}

bool canValidateBlockchain(const Admin& admin) {
    // Integrity validation endpoint restricted to Super Admin.
    return isRole(admin, "Super Admin");
}

bool canVerifyDocumentIntegrity(const Admin& admin) {
    // Document hash verification is a privileged operation.
    return isRole(admin, "Super Admin");
}

bool canManageAccounts(const Admin& admin) {
    // Account lifecycle controls are restricted to Super Admin.
    return isRole(admin, "Super Admin");
}

void denyAdminAction(const Admin& admin, const std::string& actionCode) {
    std::cout << "\n" << ui::error("[!] Access denied for your role") << " (" << admin.role << ").\n";
    logAuditAction("ACCESS_DENIED_" + actionCode, "N/A", admin.username);
    waitForEnter();
}
} // namespace

bool readMenuChoice(int& choice) {
    // Keep input recovery in one place to prevent duplicated fail-state handling.
    std::cin >> choice;

    if (std::cin.fail()) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        return false;
    }

    return true;
}

bool readBoundedMenuChoice(int& choice, int minChoice, int maxChoice) {
    // Shared guard for numeric menus with bounded ranges.
    if (!readMenuChoice(choice)) {
        return false;
    }

    if (choice < minChoice || choice > maxChoice) {
        return false;
    }

    return true;
}

void printMenuActions(const std::vector<std::string>& options, const std::string& zeroLabel) {
    for (std::size_t i = 0; i < options.size(); ++i) {
        std::cout << "  " << ui::info("[" + std::to_string(i + 1) + "]") << " " << options[i] << "\n";
    }
    std::cout << "  " << ui::info("[0]") << " " << zeroLabel << "\n";
    std::cout << ui::muted("--------------------------------------------------------------") << "\n";
    std::cout << "  Enter your choice: ";
}

bool readMenuSelection(const std::vector<std::string>& options, int& selectedIndex) {
    int rawChoice = -1;
    if (!readBoundedMenuChoice(rawChoice, 0, static_cast<int>(options.size()))) {
        return false;
    }

    if (rawChoice == 0) {
        selectedIndex = -1;
        return true;
    }

    selectedIndex = rawChoice - 1;
    return true;
}

void printHomePage() {
    // Landing menu used before authentication.
    ui::printSectionTitle("MUNICIPAL PROCUREMENT DOCUMENT TRACKING SYSTEM");
    std::cout << "  " << ui::muted("Secure Access Portal") << "\n";
    std::cout << "  Please choose an option below:\n\n";
    std::cout << "  " << ui::info("[1]") << " Login\n";
    std::cout << "  " << ui::info("[2]") << " Sign Up\n";
    std::cout << "  " << ui::info("[0]") << " Exit\n";
    std::cout << ui::muted("--------------------------------------------------------------") << "\n";
    std::cout << "  Enter your choice: ";
}

void printCitizenMenu(const User& citizen) {
    // Citizen scope is read-only: published docs, budgets, and audit views.
    ui::printSectionTitle("CITIZEN DASHBOARD");
    std::cout << "  Logged in as: " << citizen.fullName << " (" << citizen.userId << ")\n\n";
    std::cout << "  " << ui::info("[1]") << " View Published Documents\n";
    std::cout << "  " << ui::info("[2]") << " View Procurement Budgets\n";
    std::cout << "  " << ui::info("[3]") << " View Audit Trail\n";
    std::cout << "  " << ui::muted("[Info]") << " Integrity verification is Admin-only (Super Admin).\n";
    std::cout << "  " << ui::info("[0]") << " Logout\n";
    std::cout << ui::muted("--------------------------------------------------------------") << "\n";
    std::cout << "  Enter your choice: ";
}

void printAdminMenu(const Admin& admin, const std::vector<std::string>& workspaceLabels) {
    ui::printSectionTitle("ADMIN COMMAND CENTER");
    ui::printBreadcrumb({"ADMIN", "COMMAND CENTER"});
    std::cout << "  Logged in as: " << admin.fullName << " (" << admin.adminId << ")\n";
    std::cout << "  Role        : " << ui::roleLabel(admin.role) << "\n\n";
    printMenuActions(workspaceLabels, "Logout");
}

void runDocumentsWorkspace(const Admin& admin) {
    std::vector<int> actionCodes;
    std::vector<std::string> actionLabels;

    if (canUploadDocuments(admin)) {
        actionCodes.push_back(1);
        actionLabels.push_back("Upload Document");
    }
    actionCodes.push_back(2);
    actionLabels.push_back("View All Documents");
    actionCodes.push_back(3);
    actionLabels.push_back("Search Document by ID");
    actionCodes.push_back(4);
    actionLabels.push_back("Advanced Document Filters");
    actionCodes.push_back(6);
    actionLabels.push_back("Export Documents to TXT");
    if (canManualOverrideStatus(admin)) {
        actionCodes.push_back(5);
        actionLabels.push_back("Update Document Status (Manual Override)");
    }

    int choice = -1;

    do {
        clearScreen();
        ui::printSectionTitle("DOCUMENTS WORKSPACE");
        ui::printBreadcrumb({"ADMIN", "WORKSPACES", "DOCUMENTS"});
        printMenuActions(actionLabels, "Back");

        if (!readMenuSelection(actionLabels, choice)) {
            std::cout << "\n" << ui::error("[!] Invalid input. Please enter a number from the menu.") << "\n";
            waitForEnter();
            continue;
        }

        if (choice == -1) {
            break;
        }

        const int selectedAction = actionCodes[static_cast<std::size_t>(choice)];

        switch (selectedAction) {
            case 1:
                uploadDocumentAsAdmin(admin);
                break;
            case 2:
                viewAllDocumentsForAdmin(admin);
                break;
            case 3:
                searchDocumentByIdForAdmin(admin);
                break;
            case 4:
                filterDocumentsForAdmin(admin);
                break;
            case 6:
                exportDocumentsToTxt(admin);
                break;
            case 5:
                updateDocumentStatusForAdmin(admin);
                break;
            default:
                break;
        }
    } while (choice != -1);
}

void runApprovalsWorkspace(const Admin& admin) {
    std::vector<int> actionCodes;
    std::vector<std::string> actionLabels;

    if (canViewPendingApprovals(admin)) {
        actionCodes.push_back(1);
        actionLabels.push_back("View Pending Approvals");
    }
    if (canApproveOrReject(admin)) {
        actionCodes.push_back(2);
        actionLabels.push_back("Approve Document");
        actionCodes.push_back(3);
        actionLabels.push_back("Reject Document");
    }
    if (canViewApprovalAnalytics(admin)) {
        actionCodes.push_back(4);
        actionLabels.push_back("Approval Analytics Dashboard (Detailed)");
    }

    if (actionCodes.empty()) {
        clearScreen();
        ui::printSectionTitle("APPROVALS WORKSPACE");
        std::cout << "\n" << ui::warning("[!] No approvals actions are available for your role.") << "\n";
        waitForEnter();
        return;
    }

    int choice = -1;

    do {
        clearScreen();
        ui::printSectionTitle("APPROVALS WORKSPACE");
        ui::printBreadcrumb({"ADMIN", "WORKSPACES", "APPROVALS"});
        printMenuActions(actionLabels, "Back");

        if (!readMenuSelection(actionLabels, choice)) {
            std::cout << "\n" << ui::error("[!] Invalid input. Please enter a number from the menu.") << "\n";
            waitForEnter();
            continue;
        }

        if (choice == -1) {
            break;
        }

        const int selectedAction = actionCodes[static_cast<std::size_t>(choice)];

        switch (selectedAction) {
            case 1:
                viewPendingApprovalsForAdmin(admin);
                break;
            case 2:
                approveDocumentAsAdmin(admin);
                break;
            case 3:
                rejectDocumentAsAdmin(admin);
                break;
            case 4:
                viewApprovalAnalyticsDashboard(admin);
                break;
            default:
                break;
        }
    } while (choice != -1);
}

void runBudgetWorkspace(const Admin& admin) {
    std::vector<int> actionCodes;
    std::vector<std::string> actionLabels;

    actionCodes.push_back(1);
    actionLabels.push_back("View Budget Allocations");
    actionCodes.push_back(2);
    actionLabels.push_back("View Budget Variance");
    if (canManageBudgets(admin)) {
        actionCodes.push_back(3);
        actionLabels.push_back("Manage Budgets");
    }

    int choice = -1;

    do {
        clearScreen();
        ui::printSectionTitle("BUDGET WORKSPACE");
        ui::printBreadcrumb({"ADMIN", "WORKSPACES", "BUDGETS"});
        printMenuActions(actionLabels, "Back");

        if (!readMenuSelection(actionLabels, choice)) {
            std::cout << "\n" << ui::error("[!] Invalid input. Please enter a number from the menu.") << "\n";
            waitForEnter();
            continue;
        }

        if (choice == -1) {
            break;
        }

        const int selectedAction = actionCodes[static_cast<std::size_t>(choice)];

        switch (selectedAction) {
            case 1:
                viewBudgetAllocations(admin.username);
                break;
            case 2:
                viewBudgetVarianceReport(admin.username);
                break;
            case 3:
                manageBudgetsForAdmin(admin);
                break;
            default:
                break;
        }
    } while (choice != -1);
}

void runAuditIntegrityWorkspace(const Admin& admin) {
    std::vector<int> actionCodes;
    std::vector<std::string> actionLabels;

    actionCodes.push_back(1);
    actionLabels.push_back("View Audit Trail");
    if (canValidateBlockchain(admin)) {
        actionCodes.push_back(2);
        actionLabels.push_back("Validate Blockchain");
    }
    if (canVerifyDocumentIntegrity(admin)) {
        actionCodes.push_back(3);
        actionLabels.push_back("Verify Document Integrity");
    }
    actionCodes.push_back(4);
    actionLabels.push_back("Integrity Snapshot (Visual)");

    int choice = -1;

    do {
        clearScreen();
        ui::printSectionTitle("AUDIT AND INTEGRITY WORKSPACE");
        ui::printBreadcrumb({"ADMIN", "WORKSPACES", "AUDIT + INTEGRITY"});
        printMenuActions(actionLabels, "Back");

        if (!readMenuSelection(actionLabels, choice)) {
            std::cout << "\n" << ui::error("[!] Invalid input. Please enter a number from the menu.") << "\n";
            waitForEnter();
            continue;
        }

        if (choice == -1) {
            break;
        }

        const int selectedAction = actionCodes[static_cast<std::size_t>(choice)];

        switch (selectedAction) {
            case 1:
                viewAuditTrail(admin.username);
                break;
            case 2:
                validateBlockchainNodes(admin.username);
                break;
            case 3:
                verifyDocumentIntegrity(admin.username);
                break;
            case 4:
                viewIntegritySnapshot(admin);
                break;
            default:
                break;
        }
    } while (choice != -1);
}

void runAccountAdministrationWorkspace(const Admin& admin) {
    if (!canManageAccounts(admin)) {
        clearScreen();
        ui::printSectionTitle("ACCOUNT ADMINISTRATION");
        std::cout << "\n" << ui::warning("[!] No account administration actions are available for your role.") << "\n";
        waitForEnter();
        return;
    }

    const std::vector<std::string> actionLabels = {"Account Lifecycle Management"};
    int choice = -1;

    do {
        clearScreen();
        ui::printSectionTitle("ACCOUNT ADMINISTRATION");
        ui::printBreadcrumb({"ADMIN", "WORKSPACES", "ACCOUNT ADMIN"});
        printMenuActions(actionLabels, "Back");

        if (!readMenuSelection(actionLabels, choice)) {
            std::cout << "\n" << ui::error("[!] Invalid input. Please enter a number from the menu.") << "\n";
            waitForEnter();
            continue;
        }

        if (choice == -1) {
            break;
        }

        switch (choice) {
            case 0:
                manageAccountLifecycleForAdmin(admin);
                break;
            default:
                break;
        }
    } while (choice != -1);
}

void runCitizenDashboard(const User& citizen) {
    // Event loop for citizen actions until explicit logout.
    int citizenChoice = -1;

    do {
        // Re-render the dashboard every loop so each action returns to a clean menu state.
        clearScreen();
        printCitizenMenu(citizen);

        if (!readBoundedMenuChoice(citizenChoice, 0, 3)) {
            std::cout << "\n" << ui::error("[!] Invalid input. Please enter a number from the menu.") << "\n";
            continue;
        }

        switch (citizenChoice) {
            case 1:
                showPublishedDocuments(citizen.username);
                break;
            case 2:
                viewBudgetAllocations(citizen.username);
                break;
            case 3:
                viewAuditTrail(citizen.username);
                break;
            case 0:
                logAuditAction("CITIZEN_LOGOUT", citizen.userId, citizen.username);
                std::cout << "\n" << ui::success("[+] You have been logged out successfully.") << "\n";
                break;
            default:
                break;
        }

    } while (citizenChoice != 0);
}

void runAdminDashboard(const Admin& admin) {
    // Multi-level admin command center with grouped workspaces and analytics hub.
    std::vector<int> workspaceCodes;
    std::vector<std::string> workspaceLabels;

    workspaceCodes.push_back(1);
    workspaceLabels.push_back("Overview Dashboard");
    workspaceCodes.push_back(2);
    workspaceLabels.push_back("Documents Workspace");

    if (canViewPendingApprovals(admin) || canApproveOrReject(admin) || canViewApprovalAnalytics(admin)) {
        workspaceCodes.push_back(3);
        workspaceLabels.push_back("Approvals Workspace");
    }

    workspaceCodes.push_back(4);
    workspaceLabels.push_back("Budget Workspace");

    workspaceCodes.push_back(5);
    workspaceLabels.push_back("Audit and Integrity");

    if (canManageAccounts(admin)) {
        workspaceCodes.push_back(6);
        workspaceLabels.push_back("Account Administration");
    }

    int adminChoice = -1;

    do {
        clearScreen();
        printAdminMenu(admin, workspaceLabels);

        if (!readMenuSelection(workspaceLabels, adminChoice)) {
            std::cout << "\n" << ui::error("[!] Invalid input. Please enter a number from the menu.") << "\n";
            continue;
        }

        if (adminChoice == -1) {
            logAuditAction("ADMIN_LOGOUT", admin.adminId, admin.username);
            std::cout << "\n" << ui::success("[+] You have been logged out successfully.") << "\n";
            break;
        }

        const int selectedWorkspace = workspaceCodes[static_cast<std::size_t>(adminChoice)];

        switch (selectedWorkspace) {
            case 1:
                viewAdminOverviewDashboard(admin);
                break;
            case 2:
                runDocumentsWorkspace(admin);
                break;
            case 3:
                runApprovalsWorkspace(admin);
                break;
            case 4:
                runBudgetWorkspace(admin);
                break;
            case 5:
                runAuditIntegrityWorkspace(admin);
                break;
            case 6:
                runAccountAdministrationWorkspace(admin);
                break;
            default:
                break;
        }

    } while (adminChoice != -1);
}

void handleLoginFlow() {
    // Two-step login flow: choose account type, then delegate to account login.
    clearScreen();
    ui::printSectionTitle("LOGIN TYPE");
    std::cout << "  " << ui::info("[1]") << " Citizen Login\n";
    std::cout << "  " << ui::info("[2]") << " Admin Login\n";
    std::cout << "  " << ui::info("[0]") << " Back to Home\n";
    std::cout << ui::muted("--------------------------------------------------------------") << "\n";
    std::cout << "  Enter your choice: ";

    int loginType = -1;
    if (!readBoundedMenuChoice(loginType, 0, 2)) {
        std::cout << "\n" << ui::error("[!] Invalid input. Please enter a number from the menu.") << "\n";
        return;
    }

    if (loginType == 1) {
        User loggedInCitizen;
        if (loginCitizen(loggedInCitizen)) {
            runCitizenDashboard(loggedInCitizen);
        }
        return;
    }

    if (loginType == 2) {
        Admin loggedInAdmin;
        if (loginAdmin(loggedInAdmin)) {
            runAdminDashboard(loggedInAdmin);
        }
    }
}

int main() {
    // Program bootstrap initializes UI capabilities and required data files,
    // then runs the home menu loop until the user exits.
    ui::initializeUi();

    // Startup ensures all required files and seed data exist before any user interaction.
    ensureUserDataFileExists();
    ensureApprovalsDataFileExists();
    ensureBlockchainNodeFilesExist();

    int choice = -1;

    do {
        clearScreen();
        printHomePage();

        if (!readBoundedMenuChoice(choice, HOME_MIN_CHOICE, HOME_MAX_CHOICE)) {
            std::cout << "\n" << ui::error("[!] Invalid input. Please enter a number from the menu.") << "\n";
            continue;
        }

        switch (choice) {
            case 1:
                handleLoginFlow();
                break;
            case 2:
                signUpAccount();
                break;
            case 0:
                std::cout << "\n" << ui::success("Thank you for using ProcureChain.") << "\n";
                break;
            default:
                break;
        }

    } while (choice != 0);

    return 0;
}
