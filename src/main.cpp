#include "../include/auth.h"
#include "auth.cpp"

#include <iostream>
#include <limits>

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

// Temporary placeholders so citizen navigation works while modules are built next.
void printCitizenFeaturePlaceholder(const std::string& featureName) {
    clearScreen();
    std::cout << "\n==============================================================\n";
    std::cout << "  " << featureName << "\n";
    std::cout << "==============================================================\n";
    std::cout << "  This feature is queued for the next implementation step.\n";
    std::cout << "  Press Enter to return to the citizen dashboard...";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get();
}

// Handles the citizen session loop until the user chooses to logout.
void runCitizenDashboard(const User& citizen) {
    int citizenChoice = -1;

    do {
        clearScreen();
        printCitizenMenu(citizen);

        if (!readMenuChoice(citizenChoice)) {
            std::cout << "\n[!] Invalid input. Please enter a number from the menu.\n";
            continue;
        }

        switch (citizenChoice) {
            case 1:
                printCitizenFeaturePlaceholder("VIEW PUBLISHED DOCUMENTS");
                break;
            case 2:
                printCitizenFeaturePlaceholder("VERIFY DOCUMENT INTEGRITY");
                break;
            case 3:
                printCitizenFeaturePlaceholder("VIEW PROCUREMENT BUDGETS");
                break;
            case 0:
                std::cout << "\n[+] You have been logged out successfully.\n";
                break;
            default:
                std::cout << "\n[!] Invalid choice. Please select a valid menu option.\n";
                break;
        }

    } while (citizenChoice != 0);
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
        if (!readMenuChoice(choice)) {
            std::cout << "\n[!] Invalid input. Please enter a number from the menu.\n";
            continue;
        }

        // Routes the user to the selected auth action.
        switch (choice) {
            case 1: {
                User loggedInCitizen;
                if (loginCitizen(loggedInCitizen)) {
                    runCitizenDashboard(loggedInCitizen);
                }
                break;
            }
            case 2:
                signUpAccount();
                break;
            case 0:
                std::cout << "\nThank you for using ProcureChain.\n";
                break;
            default:
                std::cout << "\n[!] Invalid choice. Please select a valid menu option.\n";
                break;
        }

    } while (choice != 0);

    return 0;
}
