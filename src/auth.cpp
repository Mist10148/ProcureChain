#include "../include/auth.h"

#include "../include/approvals.h"
#include "../include/audit.h"
#include "../include/documents.h"

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

namespace {
const std::string USERS_FILE_PATH_PRIMARY = "data/users.txt";
const std::string USERS_FILE_PATH_FALLBACK = "../data/users.txt";
const std::string ADMINS_FILE_PATH_PRIMARY = "data/admins.txt";
const std::string ADMINS_FILE_PATH_FALLBACK = "../data/admins.txt";
const std::string DOCUMENTS_FILE_PATH_PRIMARY = "data/documents.txt";
const std::string DOCUMENTS_FILE_PATH_FALLBACK = "../data/documents.txt";
const std::string BUDGETS_FILE_PATH_PRIMARY = "data/budgets.txt";
const std::string BUDGETS_FILE_PATH_FALLBACK = "../data/budgets.txt";
const std::string AUDIT_FILE_PATH_PRIMARY = "data/audit_log.txt";
const std::string AUDIT_FILE_PATH_FALLBACK = "../data/audit_log.txt";

const size_t MAX_FULLNAME_LENGTH = 60;
const size_t MAX_USERNAME_LENGTH = 30;
const size_t MAX_PASSWORD_LENGTH = 50;
const size_t MIN_PASSWORD_LENGTH = 4;

bool openInputFileWithFallback(std::ifstream& file, const std::string& primaryPath, const std::string& fallbackPath) {
    file.open(primaryPath);
    if (file.is_open()) {
        return true;
    }

    file.clear();
    file.open(fallbackPath);
    return file.is_open();
}

bool openAppendFileWithFallback(std::ofstream& file, const std::string& primaryPath, const std::string& fallbackPath) {
    file.open(primaryPath, std::ios::app);
    if (file.is_open()) {
        return true;
    }

    file.clear();
    file.open(fallbackPath, std::ios::app);
    return file.is_open();
}

bool ensureFileWithHeader(const std::string& primaryPath, const std::string& fallbackPath, const std::string& headerLine) {
    std::ifstream checkFile;
    if (openInputFileWithFallback(checkFile, primaryPath, fallbackPath)) {
        return true;
    }

    std::ofstream createFile(primaryPath);
    if (createFile.is_open()) {
        createFile << headerLine << '\n';
        return true;
    }

    createFile.clear();
    createFile.open(fallbackPath);
    if (createFile.is_open()) {
        createFile << headerLine << '\n';
        return true;
    }

    return false;
}

void clearInputBuffer() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

void printAuthPageHeader(const std::string& title) {
    clearScreen();
    std::cout << "\n==============================================================\n";
    std::cout << "  " << title << "\n";
    std::cout << "==============================================================\n";
}

bool hasValidAccountInput(const std::string& fullName, const std::string& username, const std::string& password) {
    if (fullName.empty() || username.empty()) {
        std::cout << "[!] Full name and username are required.\n";
        return false;
    }

    if (fullName.size() > MAX_FULLNAME_LENGTH) {
        std::cout << "[!] Full name is too long (max " << MAX_FULLNAME_LENGTH << " characters).\n";
        return false;
    }

    if (username.size() > MAX_USERNAME_LENGTH) {
        std::cout << "[!] Username is too long (max " << MAX_USERNAME_LENGTH << " characters).\n";
        return false;
    }

    if (password.size() < MIN_PASSWORD_LENGTH) {
        std::cout << "[!] Password must be at least " << MIN_PASSWORD_LENGTH << " characters.\n";
        return false;
    }

    if (password.size() > MAX_PASSWORD_LENGTH) {
        std::cout << "[!] Password is too long (max " << MAX_PASSWORD_LENGTH << " characters).\n";
        return false;
    }

    return true;
}

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
} // namespace

void clearScreen() {
    std::system("cls");
}

void waitForEnter() {
    std::cout << "\nPress Enter to continue...";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get();
}

void ensureUserDataFileExists() {
    ensureFileWithHeader(USERS_FILE_PATH_PRIMARY, USERS_FILE_PATH_FALLBACK, "userID|fullName|username|password");
    ensureFileWithHeader(ADMINS_FILE_PATH_PRIMARY, ADMINS_FILE_PATH_FALLBACK, "adminID|fullName|username|password|role");
    ensureFileWithHeader(DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK, "docID|title|category|department|dateUploaded|uploader|status|hashValue");
    ensureFileWithHeader(BUDGETS_FILE_PATH_PRIMARY, BUDGETS_FILE_PATH_FALLBACK, "category|amount");
    ensureFileWithHeader(AUDIT_FILE_PATH_PRIMARY, AUDIT_FILE_PATH_FALLBACK, "timestamp|action|targetID|actor");

    ensureApprovalsDataFileExists();
    ensureSampleDocumentsPresent();

    std::ifstream budgetCheck;
    if (!openInputFileWithFallback(budgetCheck, BUDGETS_FILE_PATH_PRIMARY, BUDGETS_FILE_PATH_FALLBACK)) {
        std::ofstream createBudgetFile(BUDGETS_FILE_PATH_PRIMARY);
        if (!createBudgetFile.is_open()) {
            createBudgetFile.clear();
            createBudgetFile.open(BUDGETS_FILE_PATH_FALLBACK);
        }

        if (createBudgetFile.is_open()) {
            createBudgetFile << "category|amount\n";
            createBudgetFile << "Office Supplies|50000\n";
            createBudgetFile << "Infrastructure Procurement|200000\n";
            createBudgetFile << "Health Supplies|100000\n";
            createBudgetFile << "Educational Materials|75000\n";
            createBudgetFile << "Miscellaneous Procurement|30000\n";
        }
    }
}

int countExistingUsers() {
    std::ifstream file;
    if (!openInputFileWithFallback(file, USERS_FILE_PATH_PRIMARY, USERS_FILE_PATH_FALLBACK)) {
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

std::string generateNextUserId() {
    int nextNumber = countExistingUsers() + 1;
    std::ostringstream idBuilder;
    idBuilder << 'U' << std::setw(3) << std::setfill('0') << nextNumber;
    return idBuilder.str();
}

int countExistingAdmins() {
    std::ifstream file;
    if (!openInputFileWithFallback(file, ADMINS_FILE_PATH_PRIMARY, ADMINS_FILE_PATH_FALLBACK)) {
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

std::string generateNextAdminId() {
    int nextNumber = countExistingAdmins() + 1;
    std::ostringstream idBuilder;
    idBuilder << 'A' << std::setw(3) << std::setfill('0') << nextNumber;
    return idBuilder.str();
}

bool isUsernameTaken(const std::string& username) {
    std::ifstream file;
    if (!openInputFileWithFallback(file, USERS_FILE_PATH_PRIMARY, USERS_FILE_PATH_FALLBACK)) {
        return false;
    }

    std::string line;
    std::getline(file, line); // Skip header.

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

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

    std::ifstream adminFile;
    if (!openInputFileWithFallback(adminFile, ADMINS_FILE_PATH_PRIMARY, ADMINS_FILE_PATH_FALLBACK)) {
        return false;
    }

    std::getline(adminFile, line); // Skip header.

    while (std::getline(adminFile, line)) {
        if (line.empty()) {
            continue;
        }

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

    if (isUsernameTaken(newUser.username)) {
        std::cout << "[!] Username is already taken.\n";
        return false;
    }

    std::cout << "Password : ";
    std::getline(std::cin, newUser.password);

    if (!hasValidAccountInput(newUser.fullName, newUser.username, newUser.password)) {
        return false;
    }

    newUser.userId = generateNextUserId();

    std::ofstream file;
    if (!openAppendFileWithFallback(file, USERS_FILE_PATH_PRIMARY, USERS_FILE_PATH_FALLBACK)) {
        std::cout << "[!] Failed to save user account.\n";
        return false;
    }

    file << newUser.userId << '|'
         << newUser.fullName << '|'
         << newUser.username << '|'
         << newUser.password << '\n';
    file.flush();

    logAuditAction("SIGNUP_CITIZEN", newUser.userId, newUser.username);
    std::cout << "[+] Account created successfully. Your User ID is " << newUser.userId << ".\n";
    return true;
}

bool signUpAdmin() {
    clearInputBuffer();

    Admin newAdmin;
    printAuthPageHeader("ADMIN SIGN UP");
    std::cout << "Full Name: ";
    std::getline(std::cin, newAdmin.fullName);

    std::cout << "Username : ";
    std::getline(std::cin, newAdmin.username);

    if (isUsernameTaken(newAdmin.username)) {
        std::cout << "[!] Username is already taken.\n";
        return false;
    }

    std::cout << "Password : ";
    std::getline(std::cin, newAdmin.password);

    if (!hasValidAccountInput(newAdmin.fullName, newAdmin.username, newAdmin.password)) {
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

    newAdmin.adminId = generateNextAdminId();

    std::ofstream file;
    if (!openAppendFileWithFallback(file, ADMINS_FILE_PATH_PRIMARY, ADMINS_FILE_PATH_FALLBACK)) {
        std::cout << "[!] Failed to save admin account.\n";
        return false;
    }

    file << newAdmin.adminId << '|'
         << newAdmin.fullName << '|'
         << newAdmin.username << '|'
         << newAdmin.password << '|'
         << newAdmin.role << '\n';
    file.flush();

    logAuditAction("SIGNUP_ADMIN", newAdmin.adminId, newAdmin.username);
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

bool loginCitizen(User& loggedInUser) {
    clearInputBuffer();

    std::string username;
    std::string password;

    printAuthPageHeader("CITIZEN LOGIN");
    std::cout << "Username: ";
    std::getline(std::cin, username);
    std::cout << "Password: ";
    std::getline(std::cin, password);

    if (username.empty() || password.empty()) {
        std::cout << "[!] Username and password are required.\n";
        logAuditAction("LOGIN_FAILED", "N/A", username.empty() ? "anonymous" : username);
        return false;
    }

    std::ifstream file;
    if (!openInputFileWithFallback(file, USERS_FILE_PATH_PRIMARY, USERS_FILE_PATH_FALLBACK)) {
        std::cout << "[!] Unable to open user account records.\n";
        return false;
    }

    std::string line;
    std::getline(file, line); // Skip header.

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

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
            loggedInUser.userId = userId;
            loggedInUser.fullName = fullName;
            loggedInUser.username = currentUsername;
            loggedInUser.password = currentPassword;
            logAuditAction("LOGIN_SUCCESS", userId, currentUsername);
            std::cout << "[+] Welcome, " << fullName << " (" << userId << ").\n";
            return true;
        }
    }

    logAuditAction("LOGIN_FAILED", "N/A", username);
    std::cout << "[!] Invalid username or password.\n";
    return false;
}

bool loginCitizen() {
    User tempUser;
    return loginCitizen(tempUser);
}

bool loginAdmin(Admin& loggedInAdmin) {
    clearInputBuffer();

    std::string username;
    std::string password;

    printAuthPageHeader("ADMIN LOGIN");
    std::cout << "Username: ";
    std::getline(std::cin, username);
    std::cout << "Password: ";
    std::getline(std::cin, password);

    if (username.empty() || password.empty()) {
        std::cout << "[!] Username and password are required.\n";
        logAuditAction("ADMIN_LOGIN_FAILED", "N/A", username.empty() ? "anonymous" : username);
        return false;
    }

    std::ifstream file;
    if (!openInputFileWithFallback(file, ADMINS_FILE_PATH_PRIMARY, ADMINS_FILE_PATH_FALLBACK)) {
        std::cout << "[!] Unable to open admin account records.\n";
        logAuditAction("ADMIN_LOGIN_FAILED", "N/A", username);
        return false;
    }

    std::string line;
    std::getline(file, line); // Skip header.

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        std::stringstream parser(line);
        std::string adminId;
        std::string fullName;
        std::string currentUsername;
        std::string currentPassword;
        std::string role;

        std::getline(parser, adminId, '|');
        std::getline(parser, fullName, '|');
        std::getline(parser, currentUsername, '|');
        std::getline(parser, currentPassword, '|');
        std::getline(parser, role, '|');

        if (currentUsername == username && currentPassword == password) {
            loggedInAdmin.adminId = adminId;
            loggedInAdmin.fullName = fullName;
            loggedInAdmin.username = currentUsername;
            loggedInAdmin.password = currentPassword;
            loggedInAdmin.role = role;
            logAuditAction("ADMIN_LOGIN_SUCCESS", adminId, currentUsername);
            std::cout << "[+] Welcome, " << fullName << " (" << adminId << ") - " << role << ".\n";
            return true;
        }
    }

    logAuditAction("ADMIN_LOGIN_FAILED", "N/A", username);
    std::cout << "[!] Invalid admin username or password.\n";
    return false;
}

bool loginAdmin() {
    Admin tempAdmin;
    return loginAdmin(tempAdmin);
}
