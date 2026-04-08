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
            case 1:
                loginCitizen();
                break;
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
