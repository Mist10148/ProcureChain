#include "../include/auth.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <cstdlib>

const std::string USERS_FILE_PATH = "data/users.txt";
const std::string ADMINS_FILE_PATH = "data/admins.txt";

// Clears terminal output to keep each menu/page visually focused.
void clearScreen() {
    std::system("cls");
}

// Draws a consistent section header for auth pages.
void printAuthPageHeader(const std::string& title) {
    clearScreen();
    std::cout << "\n==============================================================\n";
    std::cout << "  " << title << "\n";
    std::cout << "==============================================================\n";
}

// Clears leftover newline/input before getline-based prompts.
void clearInputBuffer() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

// Ensures the user storage file exists and starts with a known header.
void ensureUserDataFileExists() {
    std::ifstream checkFile(USERS_FILE_PATH);
    if (!checkFile.good()) {
        std::ofstream createFile(USERS_FILE_PATH);
        createFile << "userID|fullName|username|password\n";
    }

    // Initializes admin storage used by admin signup and role assignment.
    std::ifstream adminCheckFile(ADMINS_FILE_PATH);
    if (!adminCheckFile.good()) {
        std::ofstream createAdminFile(ADMINS_FILE_PATH);
        createAdminFile << "adminID|fullName|username|password|role\n";
    }
}

// Counts current user records (excluding the header row).
int countExistingUsers() {
    std::ifstream file(USERS_FILE_PATH);
    if (!file.is_open()) {
        return 0;
    }

    std::string line;
    int count = 0;

    std::getline(file, line); // Skip header.

    while (std::getline(file, line)) {
        if (!line.empty()) {
            count++;
        }
    }

    return count;
}

// Generates sequential IDs like U001, U002, ...
std::string generateNextUserId() {
    int nextNumber = countExistingUsers() + 1;

    std::ostringstream idBuilder;
    idBuilder << 'U' << std::setw(3) << std::setfill('0') << nextNumber;
    return idBuilder.str();
}

// Counts current admin records (excluding the header row).
int countExistingAdmins() {
    std::ifstream file(ADMINS_FILE_PATH);
    if (!file.is_open()) {
        return 0;
    }

    std::string line;
    int count = 0;

    std::getline(file, line); // Skip header.

    while (std::getline(file, line)) {
        if (!line.empty()) {
            count++;
        }
    }

    return count;
}

// Generates sequential IDs like A001, A002, ...
std::string generateNextAdminId() {
    int nextNumber = countExistingAdmins() + 1;

    std::ostringstream idBuilder;
    idBuilder << 'A' << std::setw(3) << std::setfill('0') << nextNumber;
    return idBuilder.str();
}

// Checks if a username already exists in users.txt.
bool isUsernameTaken(const std::string& username) {
    std::ifstream file(USERS_FILE_PATH);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    std::getline(file, line); // Skip header.

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        // Parse: userID|fullName|username|password
        std::stringstream parser(line);
        std::string userId;
        std::string fullName;
        std::string currentUsername;
        std::string password;

        std::getline(parser, userId, '|');
        std::getline(parser, fullName, '|');
        std::getline(parser, currentUsername, '|');
        std::getline(parser, password, '|');

        if (currentUsername == username) {
            return true;
        }
    }

    std::ifstream adminFile(ADMINS_FILE_PATH);
    if (!adminFile.is_open()) {
        return false;
    }

    std::getline(adminFile, line); // Skip header.

    while (std::getline(adminFile, line)) {
        if (line.empty()) {
            continue;
        }

        // Parse: adminID|fullName|username|password|role
        std::stringstream parser(line);
        std::string adminId;
        std::string fullName;
        std::string currentUsername;
        std::string password;
        std::string role;

        std::getline(parser, adminId, '|');
        std::getline(parser, fullName, '|');
        std::getline(parser, currentUsername, '|');
        std::getline(parser, password, '|');
        std::getline(parser, role, '|');

        if (currentUsername == username) {
            return true;
        }
    }

    return false;
}

bool signUpCitizen() {
    clearInputBuffer();

    User newUser;
    printAuthPageHeader("CITIZEN SIGN UP");
    std::cout << "Full Name: ";
    std::getline(std::cin, newUser.fullName);

    std::cout << "Username : ";
    std::getline(std::cin, newUser.username);

    if (newUser.fullName.empty() || newUser.username.empty()) {
        std::cout << "[!] Full name and username are required.\n";
        return false;
    }

    if (isUsernameTaken(newUser.username)) {
        std::cout << "[!] Username is already taken.\n";
        return false;
    }

    std::cout << "Password : ";
    std::getline(std::cin, newUser.password);

    if (newUser.password.empty()) {
        std::cout << "[!] Password is required.\n";
        return false;
    }

    // Assign ID only after validation passes.
    newUser.userId = generateNextUserId();

    std::ofstream file(USERS_FILE_PATH, std::ios::app);
    if (!file.is_open()) {
        std::cout << "[!] Failed to save user account.\n";
        return false;
    }

    file << newUser.userId << '|'
         << newUser.fullName << '|'
         << newUser.username << '|'
         << newUser.password << '\n';

    // Flush user data immediately so signup is persisted before returning.
    file.flush();

    std::cout << "[+] Account created successfully. Your User ID is " << newUser.userId << ".\n";
    return true;
}

// Maps numeric role selection to a stored admin role label.
std::string getAdminRoleByChoice(int choice) {
    switch (choice) {
        case 1:
            return "Procurement Officer";
        case 2:
            return "Budget Officer";
        case 3:
            return "Municipal Administrator";
        case 4:
            return "Super Admin";
        default:
            return "";
    }
}

bool signUpAdmin() {
    clearInputBuffer();

    Admin newAdmin;
    printAuthPageHeader("ADMIN SIGN UP");
    std::cout << "Full Name: ";
    std::getline(std::cin, newAdmin.fullName);

    std::cout << "Username : ";
    std::getline(std::cin, newAdmin.username);

    if (newAdmin.fullName.empty() || newAdmin.username.empty()) {
        std::cout << "[!] Full name and username are required.\n";
        return false;
    }

    if (isUsernameTaken(newAdmin.username)) {
        std::cout << "[!] Username is already taken.\n";
        return false;
    }

    std::cout << "Password : ";
    std::getline(std::cin, newAdmin.password);

    if (newAdmin.password.empty()) {
        std::cout << "[!] Password is required.\n";
        return false;
    }

    std::cout << "\nSelect Admin Role:\n";
    std::cout << "  [1] Procurement Officer\n";
    std::cout << "  [2] Budget Officer\n";
    std::cout << "  [3] Municipal Administrator\n";
    std::cout << "  [4] Super Admin\n";
    std::cout << "Enter role choice: ";

    int roleChoice = 0;
    std::cin >> roleChoice;
    if (std::cin.fail()) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "[!] Invalid role input.\n";
        return false;
    }

    newAdmin.role = getAdminRoleByChoice(roleChoice);
    if (newAdmin.role.empty()) {
        std::cout << "[!] Invalid role option selected.\n";
        return false;
    }

    // Assign ID only after all validations pass.
    newAdmin.adminId = generateNextAdminId();

    std::ofstream file(ADMINS_FILE_PATH, std::ios::app);
    if (!file.is_open()) {
        std::cout << "[!] Failed to save admin account.\n";
        return false;
    }

    file << newAdmin.adminId << '|'
         << newAdmin.fullName << '|'
         << newAdmin.username << '|'
         << newAdmin.password << '|'
         << newAdmin.role << '\n';

    // Flush admin data immediately so signup is persisted before returning.
    file.flush();

    std::cout << "[+] Admin account created successfully. Your Admin ID is " << newAdmin.adminId << ".\n";
    std::cout << "[+] Assigned Role: " << newAdmin.role << "\n";
    return true;
}

void signUpAccount() {
    clearScreen();
    std::cout << "\n==============================================================\n";
    std::cout << "  ACCOUNT SIGN UP TYPE\n";
    std::cout << "==============================================================\n";
    std::cout << "  [1] Citizen Account\n";
    std::cout << "  [2] Admin Account\n";
    std::cout << "  [0] Back to Home\n";
    std::cout << "--------------------------------------------------------------\n";
    std::cout << "  Enter your choice: ";

    int accountTypeChoice = -1;
    std::cin >> accountTypeChoice;

    if (std::cin.fail()) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "\n[!] Invalid input. Please enter a number from the menu.\n";
        return;
    }

    // Routes signup to the selected account type page.
    switch (accountTypeChoice) {
        case 1:
            signUpCitizen();
            break;
        case 2:
            signUpAdmin();
            break;
        case 0:
            break;
        default:
            std::cout << "\n[!] Invalid choice. Please select a valid menu option.\n";
            break;
    }
}

bool loginCitizen() {
    clearInputBuffer();

    std::string username;
    std::string password;

    printAuthPageHeader("CITIZEN LOGIN");
    std::cout << "Username: ";
    std::getline(std::cin, username);
    std::cout << "Password: ";
    std::getline(std::cin, password);

    std::ifstream file(USERS_FILE_PATH);
    if (!file.is_open()) {
        std::cout << "[!] Unable to open user account records.\n";
        return false;
    }

    std::string line;
    std::getline(file, line); // Skip header.

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        // Parse and compare credentials line-by-line.
        std::stringstream parser(line);
        std::string userId;
        std::string fullName;
        std::string currentUsername;
        std::string currentPassword;

        std::getline(parser, userId, '|');
        std::getline(parser, fullName, '|');
        std::getline(parser, currentUsername, '|');
        std::getline(parser, currentPassword, '|');

        if (currentUsername == username && currentPassword == password) {
            std::cout << "[+] Welcome, " << fullName << " (" << userId << ").\n";
            return true;
        }
    }

    std::cout << "[!] Invalid username or password.\n";
    return false;
}
