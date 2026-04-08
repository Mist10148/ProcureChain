#include "../include/auth.h"
#include "../include/audit.h"
#include "../include/budget.h"
#include "../include/blockchain.h"
#include "../include/documents.h"
#include "../include/approvals.h"
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
    return isRole(admin, "Procurement Officer") || isRole(admin, "Super Admin");
}

bool canViewPendingApprovals(const Admin& admin) {
    return isRole(admin, "Budget Officer") ||
           isRole(admin, "Municipal Administrator") ||
           isRole(admin, "Super Admin");
}

bool canApproveOrReject(const Admin& admin) {
    return isRole(admin, "Budget Officer") || isRole(admin, "Municipal Administrator");
}

bool canManualOverrideStatus(const Admin& admin) {
    return isRole(admin, "Super Admin");
}

bool canManageBudgets(const Admin& admin) {
    return isRole(admin, "Budget Officer") || isRole(admin, "Super Admin");
}

bool canValidateBlockchain(const Admin& admin) {
    return isRole(admin, "Super Admin");
}

void denyAdminAction(const Admin& admin, const std::string& actionCode) {
    std::cout << "\n[!] Access denied for your role (" << admin.role << ").\n";
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
    if (!readMenuChoice(choice)) {
        return false;
    }

    if (choice < minChoice || choice > maxChoice) {
        return false;
    }

    return true;
}

void printHomePage() {
    std::cout << "\n==============================================================\n";
    std::cout << "          MUNICIPAL PROCUREMENT DOCUMENT TRACKING SYSTEM      \n";
    std::cout << "==============================================================\n";
    std::cout << "  Secure Access Portal\n";
    std::cout << "  Please choose an option below:\n\n";
    std::cout << "  [1] Login\n";
    std::cout << "  [2] Sign Up\n";
    std::cout << "  [0] Exit\n";
    std::cout << "--------------------------------------------------------------\n";
    std::cout << "  Enter your choice: ";
}

void printCitizenMenu(const User& citizen) {
    std::cout << "\n==============================================================\n";
    std::cout << "  CITIZEN DASHBOARD\n";
    std::cout << "==============================================================\n";
    std::cout << "  Logged in as: " << citizen.fullName << " (" << citizen.userId << ")\n\n";
    std::cout << "  [1] View Published Documents\n";
    std::cout << "  [2] Verify Document Integrity\n";
    std::cout << "  [3] View Procurement Budgets\n";
    std::cout << "  [4] View Audit Trail\n";
    std::cout << "  [0] Logout\n";
    std::cout << "--------------------------------------------------------------\n";
    std::cout << "  Enter your choice: ";
}

void printAdminMenu(const Admin& admin) {
    std::cout << "\n==============================================================\n";
    std::cout << "  ADMIN DASHBOARD\n";
    std::cout << "==============================================================\n";
    std::cout << "  Logged in as: " << admin.fullName << " (" << admin.adminId << ")\n";
    std::cout << "  Role        : " << admin.role << "\n\n";
    std::cout << "  [1] Upload Document\n";
    std::cout << "  [2] View All Documents\n";
    std::cout << "  [3] Search Document by ID\n";
    std::cout << "  [4] View Pending Approvals\n";
    std::cout << "  [5] Approve Document\n";
    std::cout << "  [6] Reject Document\n";
    std::cout << "  [7] Update Document Status (Manual Override)\n";
    std::cout << "  [8] Manage Budgets\n";
    std::cout << "  [9] View Audit Trail\n";
    std::cout << "  [10] Validate Blockchain\n";
    std::cout << "  [0] Logout\n";
    std::cout << "--------------------------------------------------------------\n";
    std::cout << "  Enter your choice: ";
}

void runCitizenDashboard(const User& citizen) {
    int citizenChoice = -1;

    do {
        // Re-render the dashboard every loop so each action returns to a clean menu state.
        clearScreen();
        printCitizenMenu(citizen);

        if (!readBoundedMenuChoice(citizenChoice, 0, 4)) {
            std::cout << "\n[!] Invalid input. Please enter a number from the menu.\n";
            continue;
        }

        switch (citizenChoice) {
            case 1:
                showPublishedDocuments(citizen.username);
                break;
            case 2:
                verifyDocumentIntegrity(citizen.username);
                break;
            case 3:
                viewBudgetAllocations(citizen.username);
                break;
            case 4:
                viewAuditTrail(citizen.username);
                break;
            case 0:
                logAuditAction("CITIZEN_LOGOUT", citizen.userId, citizen.username);
                std::cout << "\n[+] You have been logged out successfully.\n";
                break;
            default:
                break;
        }

    } while (citizenChoice != 0);
}

void runAdminDashboard(const Admin& admin) {
    int adminChoice = -1;

    do {
        // Access gates are enforced at action time to protect every entry path consistently.
        clearScreen();
        printAdminMenu(admin);

        if (!readBoundedMenuChoice(adminChoice, 0, 10)) {
            std::cout << "\n[!] Invalid input. Please enter a number from the menu.\n";
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
            case 0:
                logAuditAction("ADMIN_LOGOUT", admin.adminId, admin.username);
                std::cout << "\n[+] You have been logged out successfully.\n";
                break;
            default:
                break;
        }

    } while (adminChoice != 0);
}

void handleLoginFlow() {
    clearScreen();
    std::cout << "\n==============================================================\n";
    std::cout << "  LOGIN TYPE\n";
    std::cout << "==============================================================\n";
    std::cout << "  [1] Citizen Login\n";
    std::cout << "  [2] Admin Login\n";
    std::cout << "  [0] Back to Home\n";
    std::cout << "--------------------------------------------------------------\n";
    std::cout << "  Enter your choice: ";

    int loginType = -1;
    if (!readBoundedMenuChoice(loginType, 0, 2)) {
        std::cout << "\n[!] Invalid input. Please enter a number from the menu.\n";
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
    // Startup ensures all required files and seed data exist before any user interaction.
    ensureUserDataFileExists();
    ensureApprovalsDataFileExists();
    ensureBlockchainNodeFilesExist();

    int choice = -1;

    do {
        clearScreen();
        printHomePage();

        if (!readBoundedMenuChoice(choice, HOME_MIN_CHOICE, HOME_MAX_CHOICE)) {
            std::cout << "\n[!] Invalid input. Please enter a number from the menu.\n";
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
                std::cout << "\nThank you for using ProcureChain.\n";
                break;
            default:
                break;
        }

    } while (choice != 0);

    return 0;
}
