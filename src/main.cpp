#include "../include/auth.h"
#include "../include/audit.h"
#include "../include/budget.h"
#include "../include/blockchain.h"
#include "../include/documents.h"
#include "../include/approvals.h"
#include "../include/ui.h"
#include "../include/verification.h"

#include <iostream>
#include <limits>

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

void printAdminMenu(const Admin& admin) {
    // Admin dashboard exposes all actions; authorization is enforced at runtime
    // in the dispatcher, not by hiding menu options.
    ui::printSectionTitle("ADMIN DASHBOARD");
    std::cout << "  Logged in as: " << admin.fullName << " (" << admin.adminId << ")\n";
    std::cout << "  Role        : " << ui::roleLabel(admin.role) << "\n\n";
    std::cout << "  " << ui::info("[1]") << " Upload Document\n";
    std::cout << "  " << ui::info("[2]") << " View All Documents\n";
    std::cout << "  " << ui::info("[3]") << " Search Document by ID\n";
    std::cout << "  " << ui::info("[4]") << " View Pending Approvals\n";
    std::cout << "  " << ui::info("[5]") << " Approve Document\n";
    std::cout << "  " << ui::info("[6]") << " Reject Document\n";
    std::cout << "  " << ui::info("[7]") << " Update Document Status (Manual Override)\n";
    std::cout << "  " << ui::info("[8]") << " Manage Budgets\n";
    std::cout << "  " << ui::info("[9]") << " View Audit Trail\n";
    std::cout << "  " << ui::info("[10]") << " Validate Blockchain\n";
    std::cout << "  " << ui::info("[11]") << " Verify Document Integrity\n";
    std::cout << "  " << ui::info("[12]") << " Advanced Document Filters\n";
    std::cout << "  " << ui::info("[13]") << " Approval Analytics Dashboard\n";
    std::cout << "  " << ui::info("[14]") << " Account Lifecycle Management\n";
    std::cout << "  " << ui::info("[0]") << " Logout\n";
    std::cout << ui::muted("--------------------------------------------------------------") << "\n";
    std::cout << "  Enter your choice: ";
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
    // Event loop for admin actions. Each case applies an access gate before
    // executing side effects so authorization remains centralized and explicit.
    int adminChoice = -1;

    do {
        // Access gates are enforced at action time to protect every entry path consistently.
        clearScreen();
        printAdminMenu(admin);

        if (!readBoundedMenuChoice(adminChoice, 0, 14)) {
            std::cout << "\n" << ui::error("[!] Invalid input. Please enter a number from the menu.") << "\n";
            continue;
        }

        switch (adminChoice) {
            case 1:
                if (!canUploadDocuments(admin)) {
                    denyAdminAction(admin, "UPLOAD");
                    break;
                }
                uploadDocumentAsAdmin(admin);
                break;
            case 2:
                viewAllDocumentsForAdmin(admin);
                break;
            case 3:
                searchDocumentByIdForAdmin(admin);
                break;
            case 4:
                if (!canViewPendingApprovals(admin)) {
                    denyAdminAction(admin, "VIEW_PENDING_APPROVALS");
                    break;
                }
                viewPendingApprovalsForAdmin(admin);
                break;
            case 5:
                if (!canApproveOrReject(admin)) {
                    denyAdminAction(admin, "APPROVE");
                    break;
                }
                approveDocumentAsAdmin(admin);
                break;
            case 6:
                if (!canApproveOrReject(admin)) {
                    denyAdminAction(admin, "REJECT");
                    break;
                }
                rejectDocumentAsAdmin(admin);
                break;
            case 7:
                if (!canManualOverrideStatus(admin)) {
                    denyAdminAction(admin, "STATUS_OVERRIDE");
                    break;
                }
                updateDocumentStatusForAdmin(admin);
                break;
            case 8:
                if (!canManageBudgets(admin)) {
                    denyAdminAction(admin, "BUDGET_MANAGE");
                    break;
                }
                manageBudgetsForAdmin(admin);
                break;
            case 9:
                viewAuditTrail(admin.username);
                break;
            case 10:
                if (!canValidateBlockchain(admin)) {
                    denyAdminAction(admin, "BLOCKCHAIN_VALIDATE");
                    break;
                }
                validateBlockchainNodes(admin.username);
                break;
            case 11:
                if (!canVerifyDocumentIntegrity(admin)) {
                    denyAdminAction(admin, "VERIFY_DOCUMENT");
                    break;
                }
                verifyDocumentIntegrity(admin.username);
                break;
            case 12:
                filterDocumentsForAdmin(admin);
                break;
            case 13:
                if (!canViewApprovalAnalytics(admin)) {
                    denyAdminAction(admin, "VIEW_APPROVAL_ANALYTICS");
                    break;
                }
                viewApprovalAnalyticsDashboard(admin);
                break;
            case 14:
                if (!canManageAccounts(admin)) {
                    denyAdminAction(admin, "ACCOUNT_LIFECYCLE");
                    break;
                }
                manageAccountLifecycleForAdmin(admin);
                break;
            case 0:
                logAuditAction("ADMIN_LOGOUT", admin.adminId, admin.username);
                std::cout << "\n" << ui::success("[+] You have been logged out successfully.") << "\n";
                break;
            default:
                break;
        }

    } while (adminChoice != 0);
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
