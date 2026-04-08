#include "../include/auth.h"

#include <iostream>
#include <limits>

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
    ensureUserDataFileExists();

    int choice = -1;

    do {
        printHomePage();
        if (!readMenuChoice(choice)) {
            std::cout << "\n[!] Invalid input. Please enter a number from the menu.\n";
            continue;
        }

        switch (choice) {
            case 1:
                loginCitizen();
                break;
            case 2:
                signUpCitizen();
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
