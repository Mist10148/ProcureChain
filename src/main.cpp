#include "../include/auth.h"
#include "auth.cpp"

#include <iostream>
#include <limits>

const int HOME_MIN_CHOICE = 0;
const int HOME_MAX_CHOICE = 2;

// Renders the landing menu shown to all users.
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

// Reads numeric menu input safely and resets stream on invalid input.
bool readMenuChoice(int& choice) {
    std::cin >> choice;

    if (std::cin.fail()) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        return false;
    }

    return true;
}

// Reads and validates bounded menu choices to reduce repeated range checks.
bool readBoundedMenuChoice(int& choice, int minChoice, int maxChoice) {
    if (!readMenuChoice(choice)) {
        return false;
    }

    if (choice < minChoice || choice > maxChoice) {
        return false;
    }

    return true;
}

// Renders the citizen-only menu shown after successful login.
void printCitizenMenu(const User& citizen) {
    std::cout << "\n==============================================================\n";
    std::cout << "  CITIZEN DASHBOARD\n";
    std::cout << "==============================================================\n";
    std::cout << "  Logged in as: " << citizen.fullName << " (" << citizen.userId << ")\n\n";
    std::cout << "  [1] View Published Documents\n";
    std::cout << "  [2] Verify Document Integrity\n";
    std::cout << "  [3] View Procurement Budgets\n";
    std::cout << "  [0] Logout\n";
    std::cout << "--------------------------------------------------------------\n";
    std::cout << "  Enter your choice: ";
}

// Renders the admin stub menu until full admin features are implemented.
void printAdminMenu(const Admin& admin) {
    std::cout << "\n==============================================================\n";
    std::cout << "  ADMIN DASHBOARD (STUB)\n";
    std::cout << "==============================================================\n";
    std::cout << "  Logged in as: " << admin.fullName << " (" << admin.adminId << ")\n";
    std::cout << "  Role        : " << admin.role << "\n\n";
    std::cout << "  [1] Upload Document (Coming Soon)\n";
    std::cout << "  [2] Approval Workflow (Coming Soon)\n";
    std::cout << "  [3] Manage Budgets (Coming Soon)\n";
    std::cout << "  [0] Logout\n";
    std::cout << "--------------------------------------------------------------\n";
    std::cout << "  Enter your choice: ";
}

// Handles the citizen session loop until the user chooses to logout.
void runCitizenDashboard(const User& citizen) {
    int citizenChoice = -1;

    do {
        clearScreen();
        printCitizenMenu(citizen);

        if (!readBoundedMenuChoice(citizenChoice, 0, 3)) {
            std::cout << "\n[!] Invalid input. Please enter a number from the menu.\n";
            continue;
        }

        switch (citizenChoice) {
            case 1:
                showPublishedDocuments();
                break;
            case 2:
                verifyDocumentIntegrity();
                break;
            case 3:
                viewBudgetAllocations();
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

// Handles the admin stub session loop until logout.
void runAdminDashboard(const Admin& admin) {
    int adminChoice = -1;

    do {
        clearScreen();
        printAdminMenu(admin);

        if (!readBoundedMenuChoice(adminChoice, 0, 3)) {
            std::cout << "\n[!] Invalid input. Please enter a number from the menu.\n";
            continue;
        }

        switch (adminChoice) {
            case 1:
            case 2:
            case 3:
                std::cout << "\n[!] This admin feature is a planned module and is not implemented yet.\n";
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

// Prompts for login account type and routes to the correct auth flow.
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
    // Prepare data storage before any auth action is used.
    ensureUserDataFileExists();

    int choice = -1;

    do {
        // Refreshes the screen before showing the home page.
        clearScreen();
        printHomePage();

        // Prevents non-numeric input from breaking the menu loop.
        if (!readBoundedMenuChoice(choice, HOME_MIN_CHOICE, HOME_MAX_CHOICE)) {
            std::cout << "\n[!] Invalid input. Please enter a number from the menu.\n";
            continue;
        }

        // Routes the user to the selected auth action.
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
