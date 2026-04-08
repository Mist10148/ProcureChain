#include "../include/auth.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <ctime>

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

std::string CURRENT_CITIZEN_USERNAME = "anonymous";

const size_t MAX_FULLNAME_LENGTH = 60;
const size_t MAX_USERNAME_LENGTH = 30;
const size_t MAX_PASSWORD_LENGTH = 50;
const size_t MIN_PASSWORD_LENGTH = 4;

std::string computeSimpleHash(const std::string& text);
void ensureSampleDocumentsPresent();

// Opens a file for reading using primary path first, then fallback path.
bool openInputFileWithFallback(std::ifstream& file, const std::string& primaryPath, const std::string& fallbackPath) {
    file.open(primaryPath);
    if (file.is_open()) {
        return true;
    }

    file.clear();
    file.open(fallbackPath);
    return file.is_open();
}

// Opens a file for appending using primary path first, then fallback path.
bool openAppendFileWithFallback(std::ofstream& file, const std::string& primaryPath, const std::string& fallbackPath) {
    file.open(primaryPath, std::ios::app);
    if (file.is_open()) {
        return true;
    }

    file.clear();
    file.open(fallbackPath, std::ios::app);
    return file.is_open();
}

// Creates a storage file with header if neither primary nor fallback path exists yet.
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

// Returns current local timestamp in a readable audit-log format.
std::string getCurrentTimestamp() {
    std::time_t now = std::time(NULL);
    std::tm localTime;
    localtime_s(&localTime, &now);

    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &localTime);
    return std::string(buffer);
}

// Appends a single audit log entry for traceability.
void logAuditAction(const std::string& action, const std::string& targetId, const std::string& actor) {
    std::ofstream file;
    if (!openAppendFileWithFallback(file, AUDIT_FILE_PATH_PRIMARY, AUDIT_FILE_PATH_FALLBACK)) {
        return;
    }

    file << getCurrentTimestamp() << '|' << action << '|' << targetId << '|' << actor << '\n';
    file.flush();
}

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

// Applies shared signup constraints for both citizen and admin accounts.
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

// Ensures the user storage file exists and starts with a known header.
void ensureUserDataFileExists() {
    // Prepares user/admin storage even when app is launched from src or root folder.
    ensureFileWithHeader(
        USERS_FILE_PATH_PRIMARY,
        USERS_FILE_PATH_FALLBACK,
        "userID|fullName|username|password"
    );

    ensureFileWithHeader(
        ADMINS_FILE_PATH_PRIMARY,
        ADMINS_FILE_PATH_FALLBACK,
        "adminID|fullName|username|password|role"
    );

    ensureFileWithHeader(
        DOCUMENTS_FILE_PATH_PRIMARY,
        DOCUMENTS_FILE_PATH_FALLBACK,
        "docID|title|category|department|dateUploaded|uploader|status|hashValue"
    );

    ensureFileWithHeader(
        AUDIT_FILE_PATH_PRIMARY,
        AUDIT_FILE_PATH_FALLBACK,
        "timestamp|action|targetID|actor"
    );

    ensureSampleDocumentsPresent();

    // Initializes budget storage with default categories if no file exists yet.
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

// Counts current user records (excluding the header row).
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

// Generates sequential IDs like U001, U002, ...
std::string generateNextUserId() {
    int nextNumber = countExistingUsers() + 1;

    std::ostringstream idBuilder;
    idBuilder << 'U' << std::setw(3) << std::setfill('0') << nextNumber;
    return idBuilder.str();
}

// Counts current admin records (excluding the header row).
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

// Generates sequential IDs like A001, A002, ...
std::string generateNextAdminId() {
    int nextNumber = countExistingAdmins() + 1;

    std::ostringstream idBuilder;
    idBuilder << 'A' << std::setw(3) << std::setfill('0') << nextNumber;
    return idBuilder.str();
}

// Checks if a username already exists in users.txt.
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

    std::ifstream adminFile;
    if (!openInputFileWithFallback(adminFile, ADMINS_FILE_PATH_PRIMARY, ADMINS_FILE_PATH_FALLBACK)) {
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

    if (isUsernameTaken(newUser.username)) {
        std::cout << "[!] Username is already taken.\n";
        return false;
    }

    std::cout << "Password : ";
    std::getline(std::cin, newUser.password);

    if (!hasValidAccountInput(newUser.fullName, newUser.username, newUser.password)) {
        return false;
    }

    // Assign ID only after validation passes.
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

    // Flush user data immediately so signup is persisted before returning.
    file.flush();

    logAuditAction("SIGNUP_CITIZEN", newUser.userId, newUser.username);

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

    // Assign ID only after all validations pass.
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

    // Flush admin data immediately so signup is persisted before returning.
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
            // Stores the logged-in citizen for dashboard/session use.
            loggedInUser.userId = userId;
            loggedInUser.fullName = fullName;
            loggedInUser.username = currentUsername;
            loggedInUser.password = currentPassword;
            CURRENT_CITIZEN_USERNAME = currentUsername;
            logAuditAction("LOGIN_SUCCESS", userId, currentUsername);
            std::cout << "[+] Welcome, " << fullName << " (" << userId << ").\n";
            return true;
        }
    }

    logAuditAction("LOGIN_FAILED", "N/A", username);
    std::cout << "[!] Invalid username or password.\n";
    return false;
}

// Backward-compatible wrapper for flows that do not need session user data.
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

        // Parse: adminID|fullName|username|password|role
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

// Backward-compatible wrapper for admin login when caller does not require session struct.
bool loginAdmin() {
    Admin tempAdmin;
    return loginAdmin(tempAdmin);
}

// Pauses the screen so users can read output before returning to a menu.
void waitForEnter() {
    std::cout << "\nPress Enter to continue...";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get();
}

// Converts letters to lowercase for simple case-insensitive checks.
std::string toLowerCopy(std::string value) {
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] >= 'A' && value[i] <= 'Z') {
            value[i] = static_cast<char>(value[i] + ('a' - 'A'));
        }
    }
    return value;
}

// Produces a deterministic beginner-level hash for record verification.
std::string computeSimpleHash(const std::string& text) {
    // This is intentionally non-cryptographic and used only for classroom simulation.
    unsigned long long hash = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        hash = (hash * 131ULL + static_cast<unsigned long long>(text[i])) % 0xFFFFFFFFULL;
    }

    std::ostringstream out;
    out << std::uppercase << std::hex << hash;
    return out.str();
}

// Adds starter documents when file only contains a header for easier citizen testing.
void ensureSampleDocumentsPresent() {
    std::ifstream reader;
    if (!openInputFileWithFallback(reader, DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK)) {
        return;
    }

    std::string line;
    int nonEmptyLines = 0;
    while (std::getline(reader, line)) {
        if (!line.empty()) {
            nonEmptyLines++;
        }
    }

    if (nonEmptyLines > 1) {
        return;
    }

    // Close the reader before opening the same file in append mode.
    reader.close();

    std::ofstream writer;
    if (!openAppendFileWithFallback(writer, DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK)) {
        return;
    }

    std::string row1Source = "PR001|Office Supplies Purchase|Purchase Request|Procurement Office|2026-04-09|proc_admin|published";
    std::string row2Source = "PO002|Road Repair Contract|Purchase Order|Engineering Department|2026-04-09|proc_admin|published";
    std::string row3Source = "CA003|Clinic Equipment Acquisition|Contract|Health Office|2026-04-09|proc_admin|pending";

    writer << row1Source << '|' << computeSimpleHash(row1Source) << '\n';
    writer << row2Source << '|' << computeSimpleHash(row2Source) << '\n';
    writer << row3Source << '|' << computeSimpleHash(row3Source) << '\n';
    writer.flush();
}

void showPublishedDocuments() {
    clearScreen();
    std::cout << "\n==============================================================\n";
    std::cout << "  PUBLISHED PROCUREMENT DOCUMENTS\n";
    std::cout << "==============================================================\n";

    std::ifstream file;
    if (!openInputFileWithFallback(file, DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK)) {
        std::cout << "[!] Unable to open documents file.\n";
        logAuditAction("VIEW_PUBLISHED_DOCS_FAILED", "N/A", CURRENT_CITIZEN_USERNAME);
        waitForEnter();
        return;
    }

    std::string line;
    std::getline(file, line); // Skip header.

    struct PublishedDocument {
        std::string docId;
        std::string title;
        std::string category;
        std::string department;
        std::string dateUploaded;
        std::string uploader;
        std::string status;
    };

    std::vector<PublishedDocument> publishedDocs;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        // Parse: docID|title|category|department|dateUploaded|uploader|status|hashValue
        std::stringstream parser(line);
        std::string docId;
        std::string title;
        std::string category;
        std::string department;
        std::string dateUploaded;
        std::string uploader;
        std::string status;
        std::string hashValue;

        std::getline(parser, docId, '|');
        std::getline(parser, title, '|');
        std::getline(parser, category, '|');
        std::getline(parser, department, '|');
        std::getline(parser, dateUploaded, '|');
        std::getline(parser, uploader, '|');
        std::getline(parser, status, '|');
        std::getline(parser, hashValue, '|');

        if (toLowerCopy(status) != "published") {
            continue;
        }

        PublishedDocument doc;
        doc.docId = docId;
        doc.title = title;
        doc.category = category;
        doc.department = department;
        doc.dateUploaded = dateUploaded;
        doc.uploader = uploader;
        doc.status = status;
        publishedDocs.push_back(doc);
    }

    std::sort(publishedDocs.begin(), publishedDocs.end(), [](const PublishedDocument& a, const PublishedDocument& b) {
        return a.docId < b.docId;
    });

    if (publishedDocs.empty()) {
        std::cout << "\n[!] No published documents available yet.\n";
        logAuditAction("VIEW_PUBLISHED_DOCS_EMPTY", "N/A", CURRENT_CITIZEN_USERNAME);
    } else {
        for (size_t i = 0; i < publishedDocs.size(); ++i) {
            std::cout << "\nDocument ID : " << publishedDocs[i].docId << '\n';
            std::cout << "Title       : " << publishedDocs[i].title << '\n';
            std::cout << "Category    : " << publishedDocs[i].category << '\n';
            std::cout << "Department  : " << publishedDocs[i].department << '\n';
            std::cout << "Uploaded On : " << publishedDocs[i].dateUploaded << '\n';
            std::cout << "Uploader    : " << publishedDocs[i].uploader << '\n';
            std::cout << "Status      : " << publishedDocs[i].status << '\n';
            std::cout << "--------------------------------------------------------------\n";
        }
        logAuditAction("VIEW_PUBLISHED_DOCS", "MULTI", CURRENT_CITIZEN_USERNAME);
    }

    waitForEnter();
}

void verifyDocumentIntegrity() {
    clearScreen();
    std::cout << "\n==============================================================\n";
    std::cout << "  DOCUMENT INTEGRITY VERIFICATION\n";
    std::cout << "==============================================================\n";

    clearInputBuffer();
    std::string targetDocId;
    std::cout << "Enter Document ID to verify: ";
    std::getline(std::cin, targetDocId);

    if (targetDocId.empty()) {
        std::cout << "[!] Document ID is required.\n";
        logAuditAction("VERIFY_DOC_INPUT_ERROR", "N/A", CURRENT_CITIZEN_USERNAME);
        waitForEnter();
        return;
    }

    std::ifstream file;
    if (!openInputFileWithFallback(file, DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK)) {
        std::cout << "[!] Unable to open documents file.\n";
        logAuditAction("VERIFY_DOC_FAILED", targetDocId, CURRENT_CITIZEN_USERNAME);
        waitForEnter();
        return;
    }

    std::string line;
    std::getline(file, line); // Skip header.

    bool found = false;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        std::stringstream parser(line);
        std::string docId;
        std::string title;
        std::string category;
        std::string department;
        std::string dateUploaded;
        std::string uploader;
        std::string status;
        std::string hashValue;

        std::getline(parser, docId, '|');
        std::getline(parser, title, '|');
        std::getline(parser, category, '|');
        std::getline(parser, department, '|');
        std::getline(parser, dateUploaded, '|');
        std::getline(parser, uploader, '|');
        std::getline(parser, status, '|');
        std::getline(parser, hashValue, '|');

        if (docId != targetDocId) {
            continue;
        }

        found = true;

        // Rebuilds the same source string used for this simplified integrity check.
        std::string source = docId + "|" + title + "|" + category + "|" + department + "|" +
                             dateUploaded + "|" + uploader + "|" + status;
        std::string computedHash = computeSimpleHash(source);

        std::cout << "\nDocument ID  : " << docId << '\n';
        std::cout << "Stored Hash  : " << hashValue << '\n';
        std::cout << "Computed Hash: " << computedHash << '\n';
        std::cout << "Note         : Simple classroom hash (not cryptographic).\n";

        if (hashValue == computedHash) {
            std::cout << "[+] Verification Result: VALID\n";
            logAuditAction("VERIFY_DOC_VALID", docId, CURRENT_CITIZEN_USERNAME);
        } else {
            std::cout << "[!] Verification Result: POTENTIALLY TAMPERED\n";
            logAuditAction("VERIFY_DOC_TAMPERED", docId, CURRENT_CITIZEN_USERNAME);
        }
        break;
    }

    if (!found) {
        std::cout << "\n[!] Document ID not found.\n";
        logAuditAction("VERIFY_DOC_NOT_FOUND", targetDocId, CURRENT_CITIZEN_USERNAME);
    }

    waitForEnter();
}

void viewBudgetAllocations() {
    clearScreen();
    std::cout << "\n==============================================================\n";
    std::cout << "  PROCUREMENT BUDGET ALLOCATIONS\n";
    std::cout << "==============================================================\n";

    std::ifstream file;
    if (!openInputFileWithFallback(file, BUDGETS_FILE_PATH_PRIMARY, BUDGETS_FILE_PATH_FALLBACK)) {
        std::cout << "[!] Unable to open budgets file.\n";
        logAuditAction("VIEW_BUDGETS_FAILED", "N/A", CURRENT_CITIZEN_USERNAME);
        waitForEnter();
        return;
    }

    std::string line;
    std::getline(file, line); // Skip header.

    bool hasBudgetRows = false;
    double totalBudget = 0.0;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        // Parse: category|amount
        std::stringstream parser(line);
        std::string category;
        std::string amount;
        std::getline(parser, category, '|');
        std::getline(parser, amount, '|');

        hasBudgetRows = true;
        double parsedAmount = 0.0;
        std::stringstream amountParser(amount);
        amountParser >> parsedAmount;
        totalBudget += parsedAmount;

        std::cout << "Category: " << category << '\n';
        std::cout << "Amount  : PHP " << std::fixed << std::setprecision(2) << parsedAmount << "\n";
        std::cout << "--------------------------------------------------------------\n";
    }

    if (!hasBudgetRows) {
        std::cout << "\n[!] No budget entries available.\n";
        logAuditAction("VIEW_BUDGETS_EMPTY", "N/A", CURRENT_CITIZEN_USERNAME);
    } else {
        std::cout << "Total   : PHP " << std::fixed << std::setprecision(2) << totalBudget << "\n";
        logAuditAction("VIEW_BUDGETS", "MULTI", CURRENT_CITIZEN_USERNAME);
    }

    waitForEnter();
}
