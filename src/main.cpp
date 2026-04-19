#include "../include/auth.h"
#include "../include/audit.h"
#include "../include/backup.h"
#include "../include/budget.h"
#include "../include/blockchain.h"
#include "../include/delegation.h"
#include "../include/documents.h"
#include "../include/approvals.h"
#include "../include/analytics.h"
#include "../include/help.h"
#include "../include/notifications.h"
#include "../include/ui.h"
#include "../include/verification.h"

#include <iostream>
#include <fstream>
#include <limits>
#include <vector>

const int HOME_MIN_CHOICE = 0;
const int HOME_MAX_CHOICE = 2;

namespace {
struct StorageHealthCheck {
    std::string label;
    std::string primaryPath;
    std::string fallbackPath;
};

bool g_writeOperationsBlocked = false;
std::vector<std::string> g_storageHealthIssues;

StorageHealthCheck makeHealthCheck(const std::string& label,
                                   const std::string& primaryPath,
                                   const std::string& fallbackPath) {
    StorageHealthCheck check;
    check.label = label;
    check.primaryPath = primaryPath;
    check.fallbackPath = fallbackPath;
    return check;
}

bool canReadStorageFile(const StorageHealthCheck& check, std::string& resolvedPath) {
    resolvedPath.clear();

    std::ifstream primary(check.primaryPath);
    if (primary.is_open()) {
        resolvedPath = check.primaryPath;
        return true;
    }

    std::ifstream fallback(check.fallbackPath);
    if (fallback.is_open()) {
        resolvedPath = check.fallbackPath;
        return true;
    }

    return false;
}

void refreshStartupStorageHealth() {
    g_storageHealthIssues.clear();

    const std::vector<StorageHealthCheck> checks = {
        makeHealthCheck("users", "data/users.txt", "../data/users.txt"),
        makeHealthCheck("admins", "data/admins.txt", "../data/admins.txt"),
        makeHealthCheck("documents", "data/documents.txt", "../data/documents.txt"),
        makeHealthCheck("approvals", "data/approvals.txt", "../data/approvals.txt"),
        makeHealthCheck("approval rules", "data/approval_rules.txt", "../data/approval_rules.txt"),
        makeHealthCheck("budgets", "data/budgets.txt", "../data/budgets.txt"),
        makeHealthCheck("budget entries", "data/budget_entries.txt", "../data/budget_entries.txt"),
        makeHealthCheck("audit log", "data/audit_log.txt", "../data/audit_log.txt"),
        makeHealthCheck("password flags", "data/password_flags.txt", "../data/password_flags.txt"),
        makeHealthCheck("delegations", "data/delegations.txt", "../data/delegations.txt")
    };

    for (std::size_t i = 0; i < checks.size(); ++i) {
        std::string resolvedPath;
        if (!canReadStorageFile(checks[i], resolvedPath)) {
            g_storageHealthIssues.push_back("Unable to open " + checks[i].label + " data.");
            continue;
        }

        std::ifstream in(resolvedPath);
        std::string firstLine;
        if (!std::getline(in, firstLine)) {
            g_storageHealthIssues.push_back("Unable to read " + checks[i].label + " data header.");
        }
    }

    g_writeOperationsBlocked = !g_storageHealthIssues.empty();
}

void showStartupStorageHealthSummaryIfNeeded() {
    if (!g_writeOperationsBlocked) {
        return;
    }

    clearScreen();
    ui::printSectionTitle("STORAGE HEALTH WARNING");
    std::cout << ui::warning("[!] Write operations are temporarily blocked until storage issues are repaired.") << "\n\n";
    for (std::size_t i = 0; i < g_storageHealthIssues.size() && i < 5; ++i) {
        std::cout << "  - " << g_storageHealthIssues[i] << "\n";
    }
    if (g_storageHealthIssues.size() > 5) {
        std::cout << "  - ...and " << (g_storageHealthIssues.size() - 5) << " more issue(s)\n";
    }
    std::cout << "\n  Read-only actions remain available.\n";
    waitForEnter();
}

bool guardWriteOperation(const std::string& actionLabel) {
    if (!g_writeOperationsBlocked) {
        return true;
    }

    std::cout << "\n" << ui::error("[!] " + actionLabel + " is blocked because storage health checks failed.") << "\n";
    for (std::size_t i = 0; i < g_storageHealthIssues.size() && i < 3; ++i) {
        std::cout << "  - " << g_storageHealthIssues[i] << "\n";
    }
    if (g_storageHealthIssues.size() > 3) {
        std::cout << "  - ...and " << (g_storageHealthIssues.size() - 3) << " more issue(s)\n";
    }
    waitForEnter();
    return false;
}

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
    return isRole(admin, "Budget Officer") || isRole(admin, "Municipal Administrator") || isRole(admin, "Super Admin");
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
    std::cout << "  " << ui::info("[3]") << " View Public Audit Trail\n";
    std::cout << "  " << ui::info("[4]") << " Search Published Document (ID or Keyword)\n";
    std::cout << "  " << ui::info("[5]") << " Verify Published Document Hash\n";
    std::cout << "  " << ui::info("[6]") << " Notification Inbox\n";
    std::cout << "  " << ui::info("[7]") << " Help\n";
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
    actionLabels.push_back("Search Document (ID or Keyword)");
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
                if (guardWriteOperation("Document upload")) {
                    uploadDocumentAsAdmin(admin);
                }
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
                if (guardWriteOperation("Manual document status update")) {
                    updateDocumentStatusForAdmin(admin);
                }
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
        actionCodes.push_back(8);
        actionLabels.push_back("Request-for-Comment Thread");
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
    if (canManageAccounts(admin)) {
        actionCodes.push_back(5);
        actionLabels.push_back("Escalation Queue (Overdue SLA)");
        actionCodes.push_back(6);
        actionLabels.push_back("Manage Approval Rules");
    }
    if (canApproveOrReject(admin)) {
        actionCodes.push_back(7);
        actionLabels.push_back("Delegation Management");
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
                if (guardWriteOperation("Document approval")) {
                    approveDocumentAsAdmin(admin);
                }
                break;
            case 3:
                if (guardWriteOperation("Document rejection")) {
                    rejectDocumentAsAdmin(admin);
                }
                break;
            case 4:
                viewApprovalAnalyticsDashboard(admin);
                break;
            case 5:
                viewEscalationQueueForAdmin(admin);
                break;
            case 6:
                if (guardWriteOperation("Approval rule management")) {
                    manageApprovalRulesForAdmin(admin);
                }
                break;
            case 7:
                if (guardWriteOperation("Delegation management")) {
                    runDelegationManagement(admin);
                }
                break;
            case 8:
                if (guardWriteOperation("Approval comments")) {
                    addApprovalCommentAsAdmin(admin);
                }
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
                if (guardWriteOperation("Budget management")) {
                    manageBudgetsForAdmin(admin);
                }
                break;
            default:
                break;
        }
    } while (choice != -1);
}

void viewAuditTrailMenuForAdmin(const Admin& admin) {
    if (!canManageAccounts(admin)) {
        viewPublicAuditTrail(admin.username);
        return;
    }

    int choice = -1;
    do {
        clearScreen();
        ui::printSectionTitle("AUDIT TRAIL ACCESS");
        ui::printBreadcrumb({"ADMIN", "WORKSPACES", "AUDIT + INTEGRITY", "AUDIT TRAILS"});
        std::cout << "  " << ui::info("[1]") << " View Public Audit Trail\n";
        std::cout << "  " << ui::info("[2]") << " View Internal Audit Log\n";
        std::cout << "  " << ui::info("[0]") << " Back\n";
        std::cout << ui::muted("--------------------------------------------------------------") << "\n";
        std::cout << "  Enter your choice: ";

        if (!readBoundedMenuChoice(choice, 0, 2)) {
            std::cout << "\n" << ui::error("[!] Invalid input. Please enter a number from the menu.") << "\n";
            waitForEnter();
            continue;
        }

        if (choice == 1) {
            viewPublicAuditTrail(admin.username);
        } else if (choice == 2) {
            viewInternalAuditLog(admin);
        }
    } while (choice != 0);
}
void runAuditIntegrityWorkspace(const Admin& admin) {
    std::vector<int> actionCodes;
    std::vector<std::string> actionLabels;

    actionCodes.push_back(1);
    actionLabels.push_back("Audit Trail Access");
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
    actionCodes.push_back(5);
    actionLabels.push_back("Blockchain Explorer (Dynamic Nodes)");
    actionCodes.push_back(7);
    actionLabels.push_back("Blockchain Incident Reports");
    if (canValidateBlockchain(admin)) {
        actionCodes.push_back(6);
        actionLabels.push_back("Repair Blockchain Replicas");
    }

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
                viewAuditTrailMenuForAdmin(admin);
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
            case 5:
                viewBlockchainExplorer(admin.username);
                break;
            case 6:
                repairBlockchainNodes(admin.username);
                break;
            case 7:
                viewBlockchainIncidentReports(admin.username);
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

    const std::vector<std::string> actionLabels = {"Account Lifecycle Management", "Data Backup & Restore"};
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
                if (guardWriteOperation("Account lifecycle management")) {
                    manageAccountLifecycleForAdmin(admin);
                    refreshStartupStorageHealth();
                }
                break;
            case 1:
                runBackupWorkspace(admin);
                refreshStartupStorageHealth();
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

        if (!readBoundedMenuChoice(citizenChoice, 0, 7)) {
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
                viewPublicAuditTrail(citizen.username);
                break;
            case 4:
                searchPublishedDocumentForCitizen(citizen.username);
                break;
            case 5:
                verifyPublishedDocumentAsCitizen(citizen.username);
                break;
            case 6:
                showCitizenNotificationInbox(citizen);
                break;
            case 7:
                showHelpMenu("Citizen");
                break;
            case 0:
                if (!ui::confirmAction("Are you sure you want to logout?",
                                       "Confirm Logout",
                                       "Stay Logged In")) {
                    citizenChoice = -1;
                    std::cout << "\n" << ui::warning("[!] Logout cancelled.") << "\n";
                    waitForEnter();
                    break;
                }
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

    workspaceCodes.push_back(7);
    workspaceLabels.push_back("Notification Inbox");
    workspaceCodes.push_back(8);
    workspaceLabels.push_back("Help");

    int adminChoice = -1;

    do {
        clearScreen();
        printAdminMenu(admin, workspaceLabels);

        if (!readMenuSelection(workspaceLabels, adminChoice)) {
            std::cout << "\n" << ui::error("[!] Invalid input. Please enter a number from the menu.") << "\n";
            continue;
        }

        if (adminChoice == -1) {
            if (!ui::confirmAction("Are you sure you want to logout?",
                                   "Confirm Logout",
                                   "Stay Logged In")) {
                adminChoice = 0;
                continue;
            }
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
            case 7:
                showAdminNotificationInbox(admin);
                break;
            case 8:
                showHelpMenu(admin.role);
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
            if (!checkAndForcePasswordChange(loggedInCitizen.username, false)) {
                std::cout << "\n" << ui::error("[!] You must change your password to continue.") << "\n";
                waitForEnter();
                return;
            }
            showCitizenNotificationInbox(loggedInCitizen);
            runCitizenDashboard(loggedInCitizen);
        }
        return;
    }

    if (loginType == 2) {
        Admin loggedInAdmin;
        if (loginAdmin(loggedInAdmin)) {
            if (!checkAndForcePasswordChange(loggedInAdmin.username, true)) {
                std::cout << "\n" << ui::error("[!] You must change your password to continue.") << "\n";
                waitForEnter();
                return;
            }
            showAdminNotificationInbox(loggedInAdmin);
            runAdminDashboard(loggedInAdmin);
        }
    }
}

int main() {
    // Program bootstrap initializes UI capabilities and required data files,
    // then runs the home menu loop until the user exits.
    ui::initializeUi();
    clearScreen();
    ui::showStartupSplash();

    // Startup ensures all required files and seed data exist before any user interaction.
    ensureUserDataFileExists();
    ensureApprovalsDataFileExists();
    ensureBlockchainNodeFilesExist();
    ensureDelegationFileExists();
    refreshStartupStorageHealth();
    showStartupStorageHealthSummaryIfNeeded();

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
                if (guardWriteOperation("Account sign-up")) {
                    signUpAccount();
                }
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



