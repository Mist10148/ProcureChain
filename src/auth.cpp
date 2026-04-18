#include "../include/auth.h"

#include "../include/approvals.h"
#include "../include/audit.h"
#include "../include/blockchain.h"
#include "../include/budget.h"
#include "../include/delegation.h"
#include "../include/documents.h"
#include "../include/storage_utils.h"
#include "../include/ui.h"
#include "../include/verification.h"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>

namespace {
// Account management persists rows in flat files. Helpers in this file keep
// parsing/writing consistent and backward-compatible with older row formats.
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
const std::string PASSWORD_FLAGS_FILE_PATH_PRIMARY = "data/password_flags.txt";
const std::string PASSWORD_FLAGS_FILE_PATH_FALLBACK = "../data/password_flags.txt";

const size_t MAX_FULLNAME_LENGTH = 60;
const size_t MAX_USERNAME_LENGTH = 30;
const size_t MAX_PASSWORD_LENGTH = 50;
const size_t MIN_PASSWORD_LENGTH = 4;
const std::size_t MAX_BLOCKED_IDS_TO_SHOW = 10;

struct SeedAdminAccount {
    // Seed records ensure role-based flows are testable on first startup.
    std::string fullName;
    std::string username;
    std::string password;
    std::string role;
};

std::vector<std::string> splitPipe(const std::string& line) {
    return storage::splitPipeRow(line);
}

bool openInputFileWithFallback(std::ifstream& file, const std::string& primaryPath, const std::string& fallbackPath) {
    // Resolves runtime working-directory differences without caller branching.
    file.open(primaryPath);
    if (file.is_open()) {
        return true;
    }

    file.clear();
    file.open(fallbackPath);
    return file.is_open();
}

bool openAppendFileWithFallback(std::ofstream& file, const std::string& primaryPath, const std::string& fallbackPath) {
    // Appending avoids rewriting full files for single-row insert events.
    file.open(primaryPath, std::ios::app);
    if (file.is_open()) {
        return true;
    }

    file.clear();
    file.open(fallbackPath, std::ios::app);
    return file.is_open();
}

std::string resolveDataPath(const std::string& primaryPath, const std::string& fallbackPath) {
    // Chooses existing path when possible so writes target current dataset.
    std::ifstream primary(primaryPath);
    if (primary.is_open()) {
        return primaryPath;
    }

    std::ifstream fallback(fallbackPath);
    if (fallback.is_open()) {
        return fallbackPath;
    }

    return primaryPath;
}

bool ensureFileWithHeader(const std::string& primaryPath, const std::string& fallbackPath, const std::string& headerLine) {
    // Creates missing data files with schema headers; O(1) file operations.
    std::ifstream checkFile;
    if (openInputFileWithFallback(checkFile, primaryPath, fallbackPath)) {
        return true;
    }

    return storage::writeTextFileWithFallback(primaryPath, fallbackPath, headerLine + "\n");
}

std::string normalizeStatus(const std::string& value) {
    // Restricts to known states so unexpected text defaults to active.
    if (value == "inactive" || value == "locked") {
        return value;
    }

    return "active";
}

User parseUserTokens(const std::vector<std::string>& tokens) {
    // Defensive parser for user rows with optional trailing fields.
    User row;
    row.userId = tokens.size() > 0 ? tokens[0] : "";
    row.fullName = tokens.size() > 1 ? tokens[1] : "";
    row.username = tokens.size() > 2 ? tokens[2] : "";
    row.password = tokens.size() > 3 ? tokens[3] : "";
    row.status = normalizeStatus(tokens.size() > 4 ? tokens[4] : "active");
    row.updatedAt = tokens.size() > 5 ? tokens[5] : "";

    if (row.updatedAt.empty()) {
        row.updatedAt = getCurrentTimestamp();
    }

    return row;
}

Admin parseAdminTokens(const std::vector<std::string>& tokens) {
    // Defensive parser for admin rows with optional trailing fields.
    Admin row;
    row.adminId = tokens.size() > 0 ? tokens[0] : "";
    row.fullName = tokens.size() > 1 ? tokens[1] : "";
    row.username = tokens.size() > 2 ? tokens[2] : "";
    row.password = tokens.size() > 3 ? tokens[3] : "";
    row.role = tokens.size() > 4 ? tokens[4] : "";
    row.status = normalizeStatus(tokens.size() > 5 ? tokens[5] : "active");
    row.updatedAt = tokens.size() > 6 ? tokens[6] : "";

    if (row.updatedAt.empty()) {
        row.updatedAt = getCurrentTimestamp();
    }

    return row;
}

bool loadUsers(std::vector<User>& users) {
    // Full scan read: O(u) time and O(u) memory, u = user rows.
    std::ifstream file;
    if (!openInputFileWithFallback(file, USERS_FILE_PATH_PRIMARY, USERS_FILE_PATH_FALLBACK)) {
        return false;
    }

    users.clear();
    std::string line;
    bool firstLine = true;

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        if (firstLine) {
            firstLine = false;
            if (line.find("userID|") == 0) {
                continue;
            }
        }

        const std::vector<std::string> tokens = splitPipe(line);
        User parsed = parseUserTokens(tokens);
        if (parsed.userId.empty() || parsed.username.empty()) {
            continue;
        }

        users.push_back(parsed);
    }

    return true;
}

bool loadAdmins(std::vector<Admin>& admins) {
    // Full scan read: O(a) time and O(a) memory, a = admin rows.
    std::ifstream file;
    if (!openInputFileWithFallback(file, ADMINS_FILE_PATH_PRIMARY, ADMINS_FILE_PATH_FALLBACK)) {
        return false;
    }

    admins.clear();
    std::string line;
    bool firstLine = true;

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        if (firstLine) {
            firstLine = false;
            if (line.find("adminID|") == 0) {
                continue;
            }
        }

        const std::vector<std::string> tokens = splitPipe(line);
        Admin parsed = parseAdminTokens(tokens);
        if (parsed.adminId.empty() || parsed.username.empty()) {
            continue;
        }

        admins.push_back(parsed);
    }

    return true;
}

bool saveUsers(const std::vector<User>& users) {
    std::vector<std::string> rows;
    rows.reserve(users.size());
    for (size_t i = 0; i < users.size(); ++i) {
        rows.push_back(storage::joinPipeRow({
            users[i].userId,
            storage::sanitizeSingleLineInput(users[i].fullName),
            storage::sanitizeSingleLineInput(users[i].username),
            users[i].password,
            normalizeStatus(users[i].status),
            users[i].updatedAt
        }));
    }
    return storage::writePipeFileWithFallback(USERS_FILE_PATH_PRIMARY,
                                              USERS_FILE_PATH_FALLBACK,
                                              "userID|fullName|username|password|status|updatedAt",
                                              rows);
}

bool saveAdmins(const std::vector<Admin>& admins) {
    std::vector<std::string> rows;
    rows.reserve(admins.size());
    for (size_t i = 0; i < admins.size(); ++i) {
        rows.push_back(storage::joinPipeRow({
            admins[i].adminId,
            storage::sanitizeSingleLineInput(admins[i].fullName),
            storage::sanitizeSingleLineInput(admins[i].username),
            admins[i].password,
            storage::sanitizeSingleLineInput(admins[i].role),
            normalizeStatus(admins[i].status),
            admins[i].updatedAt
        }));
    }
    return storage::writePipeFileWithFallback(ADMINS_FILE_PATH_PRIMARY,
                                              ADMINS_FILE_PATH_FALLBACK,
                                              "adminID|fullName|username|password|role|status|updatedAt",
                                              rows);
}

void migratePasswordsToHash(std::vector<User>& users, std::vector<Admin>& admins) {
    // SHA-256 hex output is always 64 characters.
    // Any stored password shorter than 64 chars is plaintext and needs hashing.
    for (size_t i = 0; i < users.size(); ++i) {
        if (users[i].password.size() != 64) {
            users[i].password = computeSimpleHash(users[i].password);
        }
    }
    for (size_t i = 0; i < admins.size(); ++i) {
        if (admins[i].password.size() != 64) {
            admins[i].password = computeSimpleHash(admins[i].password);
        }
    }
}

void migrateAccountFilesIfNeeded() {
    // Load+save pass upgrades legacy rows to current schema defaults.
    // Also migrates plaintext passwords to SHA-256 hashes.
    std::vector<User> users;
    std::vector<Admin> admins;
    bool usersLoaded = loadUsers(users);
    bool adminsLoaded = loadAdmins(admins);

    if (usersLoaded && adminsLoaded) {
        migratePasswordsToHash(users, admins);
    }

    if (usersLoaded) { saveUsers(users); }
    if (adminsLoaded) { saveAdmins(admins); }
}

bool containsUsername(const std::vector<std::string>& usernames, const std::string& username) {
    // Linear lookup because dataset size is small and stored in vectors.
    for (size_t i = 0; i < usernames.size(); ++i) {
        if (usernames[i] == username) {
            return true;
        }
    }

    return false;
}

void ensureSeedAdminAccounts() {
    // Idempotent seed: inserts only missing usernames, preserving existing data.
    std::vector<Admin> admins;
    if (!loadAdmins(admins)) {
        return;
    }

    std::vector<std::string> existingUsernames;
    for (size_t i = 0; i < admins.size(); ++i) {
        existingUsernames.push_back(admins[i].username);
    }

    const SeedAdminAccount seeds[] = {
        {"System Admin Test", "admin_test", "admin1234", "Super Admin"},
        {"Procurement Officer Demo", "proc_admin", "proc1234", "Procurement Officer"},
        {"Budget Officer Demo", "budget_admin", "budget1234", "Budget Officer"},
        {"Municipal Admin Demo", "mun_admin", "mun1234", "Municipal Administrator"}
    };

    int nextNumber = static_cast<int>(admins.size()) + 1;
    bool changed = false;

    for (size_t i = 0; i < (sizeof(seeds) / sizeof(seeds[0])); ++i) {
        if (containsUsername(existingUsernames, seeds[i].username)) {
            continue;
        }

        Admin newAdmin;
        std::ostringstream idBuilder;
        idBuilder << 'A' << std::setw(3) << std::setfill('0') << nextNumber;
        newAdmin.adminId = idBuilder.str();
        newAdmin.fullName = seeds[i].fullName;
        newAdmin.username = seeds[i].username;
        newAdmin.password = seeds[i].password;
        newAdmin.role = seeds[i].role;
        newAdmin.status = "active";
        newAdmin.updatedAt = getCurrentTimestamp();

        admins.push_back(newAdmin);
        nextNumber++;
        changed = true;
    }

    if (changed) {
        saveAdmins(admins);
    }
}

void clearInputBuffer() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

void printAuthPageHeader(const std::string& title) {
    clearScreen();
    ui::printSectionTitle(title);
}

bool hasValidAccountInput(const std::string& fullName, const std::string& username, const std::string& password) {
    // Centralized input guardrails to keep sign-up flows consistent.
    if (fullName.empty() || username.empty()) {
        std::cout << ui::warning("[!] Full name and username are required.") << "\n";
        return false;
    }

    if (fullName.size() > MAX_FULLNAME_LENGTH) {
        std::cout << ui::warning("[!] Full name is too long (max ") << MAX_FULLNAME_LENGTH
                  << ui::warning(" characters).") << "\n";
        return false;
    }

    if (username.size() > MAX_USERNAME_LENGTH) {
        std::cout << ui::warning("[!] Username is too long (max ") << MAX_USERNAME_LENGTH
                  << ui::warning(" characters).") << "\n";
        return false;
    }

    if (password.size() < MIN_PASSWORD_LENGTH) {
        std::cout << ui::warning("[!] Password must be at least ") << MIN_PASSWORD_LENGTH
                  << ui::warning(" characters.") << "\n";
        return false;
    }

    if (password.size() > MAX_PASSWORD_LENGTH) {
        std::cout << ui::warning("[!] Password is too long (max ") << MAX_PASSWORD_LENGTH
                  << ui::warning(" characters).") << "\n";
        return false;
    }

    return true;
}

std::string getAdminRoleByChoice(int choice) {
    // Maps menu indices to role names used throughout authorization checks.
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

std::string generateTemporaryPassword() {
    // Generates a simple random temporary password for reset workflows.
    // This is not cryptographic; it is suitable for classroom simulation.
    static bool seeded = false;
    if (!seeded) {
        std::srand(static_cast<unsigned int>(std::time(NULL)));
        seeded = true;
    }

    const std::string alphabet = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
    std::string out;
    const int desiredLength = 10;

    for (int i = 0; i < desiredLength; ++i) {
        const int idx = std::rand() % static_cast<int>(alphabet.size());
        out.push_back(alphabet[static_cast<size_t>(idx)]);
    }

    return out;
}

void printAccountLifecycleTable(const std::vector<User>& users, const std::vector<Admin>& admins) {
    // Renders merged user/admin account state into one support table.
    const std::vector<std::string> headers = {"Type", "ID", "Username", "Role/Name", "Status", "Updated"};
    const std::vector<int> widths = {8, 6, 16, 24, 10, 19};

    ui::printTableHeader(headers, widths);

    for (size_t i = 0; i < users.size(); ++i) {
        ui::printTableRow(
            {"Citizen", users[i].userId, users[i].username, users[i].fullName, users[i].status, users[i].updatedAt},
            widths);
    }

    for (size_t i = 0; i < admins.size(); ++i) {
        ui::printTableRow(
            {"Admin", admins[i].adminId, admins[i].username, admins[i].role, admins[i].status, admins[i].updatedAt},
            widths);
    }

    ui::printTableFooter(widths);
}

bool updateCitizenStatus(const std::string& username, const std::string& newStatus) {
    // Linear find-update-save pipeline: O(u) per status update.
    std::vector<User> users;
    if (!loadUsers(users)) {
        return false;
    }

    bool updated = false;
    for (size_t i = 0; i < users.size(); ++i) {
        if (users[i].username == username) {
            users[i].status = normalizeStatus(newStatus);
            users[i].updatedAt = getCurrentTimestamp();
            updated = true;
            break;
        }
    }

    if (!updated) {
        return false;
    }

    return saveUsers(users);
}

bool updateAdminStatus(const std::string& username, const std::string& newStatus) {
    // Linear find-update-save pipeline: O(a) per status update.
    std::vector<Admin> admins;
    if (!loadAdmins(admins)) {
        return false;
    }

    bool updated = false;
    for (size_t i = 0; i < admins.size(); ++i) {
        if (admins[i].username == username) {
            admins[i].status = normalizeStatus(newStatus);
            admins[i].updatedAt = getCurrentTimestamp();
            updated = true;
            break;
        }
    }

    if (!updated) {
        return false;
    }

    return saveAdmins(admins);
}

bool deleteAdminAccountByUsername(const std::string& username) {
    std::vector<Admin> admins;
    if (!loadAdmins(admins)) {
        return false;
    }

    bool removed = false;
    for (std::size_t i = 0; i < admins.size(); ++i) {
        if (admins[i].username != username) {
            continue;
        }

        admins.erase(admins.begin() + i);
        removed = true;
        break;
    }

    if (!removed) {
        return false;
    }

    if (!saveAdmins(admins)) {
        return false;
    }

    const bool nodeRemoved = removeBlockchainNodeForAdmin(username);
    ensureBlockchainNodeFilesExist();
    return nodeRemoved;
}

bool loadPendingApprovalIdsForAdminAccount(const std::string& username,
                                           std::vector<std::string>& outDocIds,
                                           std::vector<std::string>& outBudgetEntryIds) {
    outDocIds.clear();
    outBudgetEntryIds.clear();

    if (!getPendingDocumentApprovalIdsForApprover(username, outDocIds)) {
        return false;
    }

    if (!getPendingBudgetApprovalIdsForApprover(username, outBudgetEntryIds)) {
        return false;
    }

    return true;
}

std::string buildPendingApprovalBlockTarget(const std::string& username,
                                            std::size_t pendingDocCount,
                                            std::size_t pendingBudgetCount) {
    std::ostringstream target;
    target << username
           << " [docPending=" << pendingDocCount
           << ",budgetPending=" << pendingBudgetCount
           << "]";
    return target.str();
}

void printBlockedPendingIdGroup(const std::string& heading,
                                const std::vector<std::string>& ids) {
    if (ids.empty()) {
        return;
    }

    std::cout << "  " << heading << " (" << ids.size() << ")" << "\n";

    const std::size_t limit = ids.size() < MAX_BLOCKED_IDS_TO_SHOW ? ids.size() : MAX_BLOCKED_IDS_TO_SHOW;
    for (std::size_t i = 0; i < limit; ++i) {
        std::cout << "    - " << ids[i] << "\n";
    }

    if (ids.size() > limit) {
        std::cout << "    ... +" << (ids.size() - limit) << " more" << "\n";
    }
}

bool hasMustChangePasswordFlag(const std::string& username) {
    std::ifstream file;
    if (!openInputFileWithFallback(file, PASSWORD_FLAGS_FILE_PATH_PRIMARY, PASSWORD_FLAGS_FILE_PATH_FALLBACK)) {
        return false;
    }

    std::string line;
    bool firstLine = true;
    while (std::getline(file, line)) {
        if (line.empty()) { continue; }
        if (firstLine) { firstLine = false; if (line.find("username|") == 0) { continue; } }
        const std::vector<std::string> tokens = splitPipe(line);
        if (tokens.size() >= 2 && tokens[0] == username && tokens[1] == "yes") {
            return true;
        }
    }
    return false;
}

void setMustChangePasswordFlag(const std::string& username) {
    // Load all flags, set/add for username, rewrite file.
    std::vector<std::pair<std::string, std::string>> flags;
    std::ifstream file;
    if (openInputFileWithFallback(file, PASSWORD_FLAGS_FILE_PATH_PRIMARY, PASSWORD_FLAGS_FILE_PATH_FALLBACK)) {
        std::string line;
        bool firstLine = true;
        while (std::getline(file, line)) {
            if (line.empty()) { continue; }
            if (firstLine) { firstLine = false; if (line.find("username|") == 0) { continue; } }
            const std::vector<std::string> tokens = splitPipe(line);
            if (tokens.size() >= 2 && tokens[0] != username) {
                flags.push_back(std::make_pair(tokens[0], tokens[1]));
            }
        }
        file.close();
    }
    flags.push_back(std::make_pair(username, std::string("yes")));

    std::vector<std::string> rows;
    rows.reserve(flags.size());
    for (size_t i = 0; i < flags.size(); ++i) {
        rows.push_back(storage::joinPipeRow({flags[i].first, flags[i].second}));
    }
    storage::writePipeFileWithFallback(PASSWORD_FLAGS_FILE_PATH_PRIMARY,
                                       PASSWORD_FLAGS_FILE_PATH_FALLBACK,
                                       "username|mustChangePassword",
                                       rows);
}

void clearMustChangePasswordFlag(const std::string& username) {
    std::vector<std::pair<std::string, std::string>> flags;
    std::ifstream file;
    if (openInputFileWithFallback(file, PASSWORD_FLAGS_FILE_PATH_PRIMARY, PASSWORD_FLAGS_FILE_PATH_FALLBACK)) {
        std::string line;
        bool firstLine = true;
        while (std::getline(file, line)) {
            if (line.empty()) { continue; }
            if (firstLine) { firstLine = false; if (line.find("username|") == 0) { continue; } }
            const std::vector<std::string> tokens = splitPipe(line);
            if (tokens.size() >= 2 && tokens[0] != username) {
                flags.push_back(std::make_pair(tokens[0], tokens[1]));
            }
        }
        file.close();
    }

    std::vector<std::string> rows;
    rows.reserve(flags.size());
    for (size_t i = 0; i < flags.size(); ++i) {
        rows.push_back(storage::joinPipeRow({flags[i].first, flags[i].second}));
    }
    storage::writePipeFileWithFallback(PASSWORD_FLAGS_FILE_PATH_PRIMARY,
                                       PASSWORD_FLAGS_FILE_PATH_FALLBACK,
                                       "username|mustChangePassword",
                                       rows);
}

bool forcePasswordChange(const std::string& username, bool isAdmin) {
    clearScreen();
    ui::printSectionTitle("PASSWORD CHANGE REQUIRED");
    std::cout << ui::warning("[!] Your password was recently reset. You must set a new password.") << "\n\n";

    std::string newPassword;
    std::string confirmPassword;
    std::cout << "  New Password    : ";
    std::getline(std::cin, newPassword);
    std::cout << "  Confirm Password: ";
    std::getline(std::cin, confirmPassword);

    if (newPassword != confirmPassword) {
        std::cout << ui::error("[!] Passwords do not match.") << "\n";
        return false;
    }
    if (newPassword.size() < MIN_PASSWORD_LENGTH || newPassword.size() > MAX_PASSWORD_LENGTH) {
        std::cout << ui::error("[!] Password must be between 4 and 50 characters.") << "\n";
        return false;
    }

    const std::string hashed = computeSimpleHash(newPassword);

    if (isAdmin) {
        std::vector<Admin> admins;
        if (!loadAdmins(admins)) { return false; }
        for (size_t i = 0; i < admins.size(); ++i) {
            if (admins[i].username == username) {
                admins[i].password = hashed;
                admins[i].updatedAt = getCurrentTimestamp();
                break;
            }
        }
        if (!saveAdmins(admins)) { return false; }
    } else {
        std::vector<User> users;
        if (!loadUsers(users)) { return false; }
        for (size_t i = 0; i < users.size(); ++i) {
            if (users[i].username == username) {
                users[i].password = hashed;
                users[i].updatedAt = getCurrentTimestamp();
                break;
            }
        }
        if (!saveUsers(users)) { return false; }
    }

    clearMustChangePasswordFlag(username);
    logAuditAction("FORCED_PASSWORD_CHANGE", username, username);
    std::cout << ui::success("[+] Password changed successfully.") << "\n";
    waitForEnter();
    return true;
}

bool resetCitizenPassword(const std::string& username, std::string& tempPassword) {
    // Resets one matching user credential and records update timestamp.
    std::vector<User> users;
    if (!loadUsers(users)) {
        return false;
    }

    bool updated = false;
    for (size_t i = 0; i < users.size(); ++i) {
        if (users[i].username == username) {
            tempPassword = generateTemporaryPassword();
            users[i].password = computeSimpleHash(tempPassword);
            users[i].updatedAt = getCurrentTimestamp();
            updated = true;
            break;
        }
    }

    if (!updated) {
        return false;
    }

    if (!saveUsers(users)) { return false; }
    setMustChangePasswordFlag(username);
    return true;
}

bool resetAdminPassword(const std::string& username, std::string& tempPassword) {
    // Resets one matching admin credential and records update timestamp.
    std::vector<Admin> admins;
    if (!loadAdmins(admins)) {
        return false;
    }

    bool updated = false;
    for (size_t i = 0; i < admins.size(); ++i) {
        if (admins[i].username == username) {
            tempPassword = generateTemporaryPassword();
            admins[i].password = computeSimpleHash(tempPassword);
            admins[i].updatedAt = getCurrentTimestamp();
            updated = true;
            break;
        }
    }

    if (!updated) {
        return false;
    }

    if (!saveAdmins(admins)) { return false; }
    setMustChangePasswordFlag(username);
    return true;
}
} // namespace

bool checkAndForcePasswordChange(const std::string& username, bool isAdmin) {
    if (!hasMustChangePasswordFlag(username)) {
        return true;
    }
    return forcePasswordChange(username, isAdmin);
}

void clearScreen() {
    std::system("cls");
}

void waitForEnter() {
    std::cout << "\nPress Enter to continue...";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get();
}

void ensureUserDataFileExists() {
    // Startup bootstrap guarantees all dependent data files/schemas exist.
    ensureFileWithHeader(USERS_FILE_PATH_PRIMARY, USERS_FILE_PATH_FALLBACK, "userID|fullName|username|password|status|updatedAt");
    ensureFileWithHeader(ADMINS_FILE_PATH_PRIMARY, ADMINS_FILE_PATH_FALLBACK, "adminID|fullName|username|password|role|status|updatedAt");
    ensureFileWithHeader(DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK, "docID|title|category|description|department|dateUploaded|uploader|status|hashValue|fileName|fileType|filePath|fileSizeBytes|budgetCategory|amount");
    ensureFileWithHeader(BUDGETS_FILE_PATH_PRIMARY, BUDGETS_FILE_PATH_FALLBACK, "category|amount");
    ensureFileWithHeader(AUDIT_FILE_PATH_PRIMARY, AUDIT_FILE_PATH_FALLBACK, "timestamp|action|targetID|actor|chainIndex|previousHash|currentHash");
    ensureFileWithHeader(PASSWORD_FLAGS_FILE_PATH_PRIMARY, PASSWORD_FLAGS_FILE_PATH_FALLBACK, "username|mustChangePassword");

    migrateAccountFilesIfNeeded();
    ensureSeedAdminAccounts();
    ensureBudgetConsensusFilesExist();
    ensureSampleDocumentsPresent();
    ensureApprovalsDataFileExists();
    ensureAuditTrailHashChain();

    std::ifstream budgetCheck;
    if (!openInputFileWithFallback(budgetCheck, BUDGETS_FILE_PATH_PRIMARY, BUDGETS_FILE_PATH_FALLBACK)) {
        std::ofstream createBudgetFile(BUDGETS_FILE_PATH_PRIMARY);
        if (!createBudgetFile.is_open()) {
            createBudgetFile.clear();
            createBudgetFile.open(BUDGETS_FILE_PATH_FALLBACK);
        }

        if (createBudgetFile.is_open()) {
            createBudgetFile << "category|amount\n";
            createBudgetFile << "Infrastructure Procurement|450000\n";
            createBudgetFile << "Health Supplies|180000\n";
            createBudgetFile << "Educational Materials|160000\n";
            createBudgetFile << "Office Supplies|95000\n";
            createBudgetFile << "Emergency Procurement|120000\n";
        }
    }
}

int countExistingUsers() {
    // Count is derived from loaded rows, so ID generation is O(u).
    std::vector<User> users;
    if (!loadUsers(users)) {
        return 0;
    }

    return static_cast<int>(users.size());
}

std::string generateNextUserId() {
    // ID format is U### and increments from current row count.
    int nextNumber = countExistingUsers() + 1;
    std::ostringstream idBuilder;
    idBuilder << 'U' << std::setw(3) << std::setfill('0') << nextNumber;
    return idBuilder.str();
}

int countExistingAdmins() {
    // Count is derived from loaded rows, so ID generation is O(a).
    std::vector<Admin> admins;
    if (!loadAdmins(admins)) {
        return 0;
    }

    return static_cast<int>(admins.size());
}

std::string generateNextAdminId() {
    // ID format is A### and increments from current row count.
    int nextNumber = countExistingAdmins() + 1;
    std::ostringstream idBuilder;
    idBuilder << 'A' << std::setw(3) << std::setfill('0') << nextNumber;
    return idBuilder.str();
}

bool isUsernameTaken(const std::string& username) {
    // Global uniqueness check across both citizen and admin datasets.
    // Complexity: O(u + a).
    std::vector<User> users;
    if (loadUsers(users)) {
        for (size_t i = 0; i < users.size(); ++i) {
            if (users[i].username == username) {
                return true;
            }
        }
    }

    std::vector<Admin> admins;
    if (loadAdmins(admins)) {
        for (size_t i = 0; i < admins.size(); ++i) {
            if (admins[i].username == username) {
                return true;
            }
        }
    }

    return false;
}

bool signUpCitizen() {
    // Interactive create flow: validate input, assign ID, append logical record.
    clearInputBuffer();

    User newUser;
    printAuthPageHeader("CITIZEN SIGN UP");
    std::cout << "Full Name: ";
    std::getline(std::cin, newUser.fullName);

    std::cout << "Username : ";
    std::getline(std::cin, newUser.username);

    if (isUsernameTaken(newUser.username)) {
        std::cout << ui::warning("[!] Username is already taken.") << "\n";
        return false;
    }

    std::cout << "Password : ";
    std::getline(std::cin, newUser.password);

    if (!hasValidAccountInput(newUser.fullName, newUser.username, newUser.password)) {
        return false;
    }

    newUser.password = computeSimpleHash(newUser.password);
    newUser.userId = generateNextUserId();
    newUser.status = "active";
    newUser.updatedAt = getCurrentTimestamp();

    std::vector<User> users;
    loadUsers(users);
    users.push_back(newUser);

    if (!saveUsers(users)) {
        std::cout << ui::error("[!] Failed to save user account.") << "\n";
        return false;
    }

    logAuditAction("SIGNUP_CITIZEN", newUser.userId, newUser.username);
    std::cout << ui::success("[+] Account created successfully. Your User ID is ") << newUser.userId << ".\n";
    return true;
}

bool signUpAdmin() {
    // Mirrors citizen signup, with role selection and admin ID generation.
    clearInputBuffer();

    Admin newAdmin;
    printAuthPageHeader("ADMIN SIGN UP");
    std::cout << "Full Name: ";
    std::getline(std::cin, newAdmin.fullName);

    std::cout << "Username : ";
    std::getline(std::cin, newAdmin.username);

    if (isUsernameTaken(newAdmin.username)) {
        std::cout << ui::warning("[!] Username is already taken.") << "\n";
        return false;
    }

    std::cout << "Password : ";
    std::getline(std::cin, newAdmin.password);

    if (!hasValidAccountInput(newAdmin.fullName, newAdmin.username, newAdmin.password)) {
        return false;
    }

    newAdmin.password = computeSimpleHash(newAdmin.password);

    std::cout << "\n" << ui::bold("Select Admin Role") << "\n";
    std::cout << "  " << ui::info("[1]") << " Procurement Officer\n";
    std::cout << "  " << ui::info("[2]") << " Budget Officer\n";
    std::cout << "  " << ui::info("[3]") << " Municipal Administrator\n";
    std::cout << "  " << ui::info("[4]") << " Super Admin\n";
    std::cout << ui::muted("--------------------------------------------------------------") << "\n";
    std::cout << "  Enter role choice: ";

    int roleChoice = 0;
    std::cin >> roleChoice;
    if (std::cin.fail()) {
        std::cin.clear();
        clearInputBuffer();
        std::cout << ui::warning("[!] Invalid role input.") << "\n";
        return false;
    }

    newAdmin.role = getAdminRoleByChoice(roleChoice);
    if (newAdmin.role.empty()) {
        std::cout << ui::warning("[!] Invalid role option selected.") << "\n";
        return false;
    }

    newAdmin.adminId = generateNextAdminId();
    newAdmin.status = "active";
    newAdmin.updatedAt = getCurrentTimestamp();

    std::vector<Admin> admins;
    loadAdmins(admins);
    admins.push_back(newAdmin);

    if (!saveAdmins(admins)) {
        std::cout << ui::error("[!] Failed to save admin account.") << "\n";
        return false;
    }

    ensureBlockchainNodeFilesExist();

    logAuditAction("SIGNUP_ADMIN", newAdmin.adminId, newAdmin.username);
    std::cout << ui::success("[+] Admin account created successfully. Your Admin ID is ") << newAdmin.adminId << ".\n";
    std::cout << ui::success("[+] Assigned Role: ") << newAdmin.role << "\n";
    return true;
}

void signUpAccount() {
    clearScreen();
    ui::printSectionTitle("ACCOUNT SIGN UP TYPE");
    std::cout << "  " << ui::info("[1]") << " Citizen Account\n";
    std::cout << "  " << ui::info("[2]") << " Admin Account\n";
    std::cout << "  " << ui::info("[0]") << " Back to Home\n";
    std::cout << ui::muted("--------------------------------------------------------------") << "\n";
    std::cout << "  Enter your choice: ";

    int accountTypeChoice = -1;
    std::cin >> accountTypeChoice;

    if (std::cin.fail()) {
        std::cin.clear();
        clearInputBuffer();
        std::cout << "\n" << ui::warning("[!] Invalid input. Please enter a number from the menu.") << "\n";
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
            std::cout << "\n" << ui::warning("[!] Invalid choice. Please select a valid menu option.") << "\n";
            break;
    }
}

bool loginCitizen(User& loggedInUser) {
    // Credential match is a linear scan over current user records: O(u).
    clearInputBuffer();

    std::string username;
    std::string password;

    printAuthPageHeader("CITIZEN LOGIN");
    std::cout << "Username: ";
    std::getline(std::cin, username);
    std::cout << "Password: ";
    std::getline(std::cin, password);

    if (username.empty() || password.empty()) {
        std::cout << ui::warning("[!] Username and password are required.") << "\n";
        logAuditAction("LOGIN_FAILED", "N/A", username.empty() ? "anonymous" : username);
        return false;
    }

    const std::string hashedInput = computeSimpleHash(password);

    std::vector<User> users;
    if (!loadUsers(users)) {
        std::cout << ui::error("[!] Unable to open user account records.") << "\n";
        return false;
    }

    for (size_t i = 0; i < users.size(); ++i) {
        if (users[i].username != username || users[i].password != hashedInput) {
            continue;
        }

        if (users[i].status != "active") {
            logAuditAction("LOGIN_BLOCKED_INACTIVE", users[i].userId, username);
            std::cout << ui::error("[!] Account is inactive. Contact Super Admin.") << "\n";
            return false;
        }

        loggedInUser = users[i];
        logAuditAction("LOGIN_SUCCESS", users[i].userId, username);
        std::cout << ui::success("[+] Welcome, ") << users[i].fullName << " (" << users[i].userId << ").\n";
        return true;
    }

    logAuditAction("LOGIN_FAILED", "N/A", username);
    std::cout << ui::warning("[!] Invalid username or password.") << "\n";
    return false;
}

bool loginCitizen() {
    User tempUser;
    return loginCitizen(tempUser);
}

bool loginAdmin(Admin& loggedInAdmin) {
    // Credential match is a linear scan over current admin records: O(a).
    clearInputBuffer();

    std::string username;
    std::string password;

    printAuthPageHeader("ADMIN LOGIN");
    std::cout << "Username: ";
    std::getline(std::cin, username);
    std::cout << "Password: ";
    std::getline(std::cin, password);

    if (username.empty() || password.empty()) {
        std::cout << ui::warning("[!] Username and password are required.") << "\n";
        logAuditAction("ADMIN_LOGIN_FAILED", "N/A", username.empty() ? "anonymous" : username);
        return false;
    }

    const std::string hashedInput = computeSimpleHash(password);

    std::vector<Admin> admins;
    if (!loadAdmins(admins)) {
        std::cout << ui::error("[!] Unable to open admin account records.") << "\n";
        logAuditAction("ADMIN_LOGIN_FAILED", "N/A", username);
        return false;
    }

    for (size_t i = 0; i < admins.size(); ++i) {
        if (admins[i].username != username || admins[i].password != hashedInput) {
            continue;
        }

        if (admins[i].status != "active") {
            logAuditAction("ADMIN_LOGIN_BLOCKED_INACTIVE", admins[i].adminId, username);
            std::cout << ui::error("[!] Account is inactive. Contact Super Admin.") << "\n";
            return false;
        }

        loggedInAdmin = admins[i];
        logAuditAction("ADMIN_LOGIN_SUCCESS", admins[i].adminId, username);
        std::cout << ui::success("[+] Welcome, ") << admins[i].fullName
                  << " (" << admins[i].adminId << ") - " << admins[i].role << ".\n";
        return true;
    }

    logAuditAction("ADMIN_LOGIN_FAILED", "N/A", username);
    std::cout << ui::warning("[!] Invalid admin username or password.") << "\n";
    return false;
}

bool loginAdmin() {
    Admin tempAdmin;
    return loginAdmin(tempAdmin);
}

void manageAccountLifecycleForAdmin(const Admin& admin) {
    // Super Admin operations for status toggles and password resets.
    // Each mutation path performs load-search-save on the target file.
    if (admin.role != "Super Admin") {
        std::cout << ui::error("[!] Access denied. Super Admin only.") << "\n";
        waitForEnter();
        return;
    }

    int choice = -1;

    do {
        clearScreen();
        ui::printSectionTitle("ACCOUNT LIFECYCLE MANAGEMENT");
        std::cout << "  " << ui::info("[1]") << " List all accounts\n";
        std::cout << "  " << ui::info("[2]") << " Deactivate account\n";
        std::cout << "  " << ui::info("[3]") << " Reactivate account\n";
        std::cout << "  " << ui::info("[4]") << " Reset account password\n";
        std::cout << "  " << ui::info("[5]") << " Hard delete admin account\n";
        std::cout << "  " << ui::info("[0]") << " Back to Admin Dashboard\n";
        std::cout << ui::muted("--------------------------------------------------------------") << "\n";
        std::cout << "  Enter your choice: ";

        std::cin >> choice;
        if (std::cin.fail()) {
            std::cin.clear();
            clearInputBuffer();
            std::cout << ui::warning("[!] Invalid menu input.") << "\n";
            waitForEnter();
            continue;
        }

        if (choice == 1) {
            std::vector<User> users;
            std::vector<Admin> admins;
            if (!loadUsers(users) || !loadAdmins(admins)) {
                std::cout << ui::error("[!] Unable to load account data.") << "\n";
                waitForEnter();
                continue;
            }

            printAccountLifecycleTable(users, admins);
            logAuditAction("ACCOUNT_LIST_VIEW", "MULTI", admin.username);
            waitForEnter();
            continue;
        }

        if (choice == 2 || choice == 3 || choice == 4) {
            clearInputBuffer();
            std::cout << "Target type: " << ui::info("[1] Citizen") << "  " << ui::info("[2] Admin") << "\n";
            std::cout << "Enter type: ";

            int typeChoice = 0;
            std::cin >> typeChoice;
            if (std::cin.fail() || (typeChoice != 1 && typeChoice != 2)) {
                std::cin.clear();
                clearInputBuffer();
                std::cout << ui::warning("[!] Invalid account type.") << "\n";
                waitForEnter();
                continue;
            }

            clearInputBuffer();
            std::string username;
            std::cout << "Target username: ";
            std::getline(std::cin, username);

            if (username.empty()) {
                std::cout << ui::warning("[!] Username is required.") << "\n";
                waitForEnter();
                continue;
            }

            if (choice == 2 || choice == 3) {
                const std::string newStatus = (choice == 2) ? "inactive" : "active";

                if (typeChoice == 2 && username == admin.username && choice == 2) {
                    std::cout << ui::warning("[!] You cannot deactivate your current session account.") << "\n";
                    waitForEnter();
                    continue;
                }

                if (typeChoice == 2 && choice == 2) {
                    std::vector<std::string> pendingDocIds;
                    std::vector<std::string> pendingBudgetEntryIds;
                    if (!loadPendingApprovalIdsForAdminAccount(username, pendingDocIds, pendingBudgetEntryIds)) {
                        std::cout << ui::error("[!] Unable to verify pending approvals for this admin account.") << "\n";
                        logAuditAction("ACCOUNT_DEACTIVATE_BLOCKED_PENDING_CHECK_FAILED", username, admin.username);
                        waitForEnter();
                        continue;
                    }

                    if (!pendingDocIds.empty() || !pendingBudgetEntryIds.empty()) {
                        std::cout << ui::error("[!] Deactivation blocked: target admin has pending assigned approvals.") << "\n";
                        printBlockedPendingIdGroup("Pending document approvals", pendingDocIds);
                        printBlockedPendingIdGroup("Pending budget approvals", pendingBudgetEntryIds);
                        logAuditAction("ACCOUNT_DEACTIVATE_BLOCKED_PENDING_APPROVALS",
                                       buildPendingApprovalBlockTarget(username,
                                                                       pendingDocIds.size(),
                                                                       pendingBudgetEntryIds.size()),
                                       admin.username);
                        waitForEnter();
                        continue;
                    }
                }

                const std::string actionLabel = (choice == 2) ? "deactivate" : "reactivate";
                const std::string accountTypeLabel = (typeChoice == 1) ? "Citizen" : "Admin";
                if (!ui::confirmAction("Are you sure you want to " + actionLabel + " this " + accountTypeLabel + " account?",
                                       (choice == 2) ? "Confirm Deactivate" : "Confirm Reactivate",
                                       "Cancel")) {
                    std::cout << ui::warning("[!] Account status update cancelled.") << "\n";
                    waitForEnter();
                    continue;
                }

                bool ok = false;
                if (typeChoice == 1) {
                    ok = updateCitizenStatus(username, newStatus);
                } else {
                    ok = updateAdminStatus(username, newStatus);
                }

                if (!ok) {
                    std::cout << ui::error("[!] Account update failed or username not found.") << "\n";
                    waitForEnter();
                    continue;
                }

                if (typeChoice == 2 && choice == 2) {
                    const int revokedCount = revokeDelegationsForUsername(username);
                    if (revokedCount < 0) {
                        std::cout << ui::warning("[!] Account deactivated, but delegation cleanup failed.") << "\n";
                        logAuditAction("DELEGATION_REVOKE_FAILED", username, admin.username);
                    } else if (revokedCount > 0) {
                        const std::string target = username + " [count=" + std::to_string(revokedCount) + "]";
                        logAuditAction("DELEGATIONS_AUTO_REVOKED", target, admin.username);
                        std::cout << ui::muted("[i] Revoked ") << revokedCount
                                  << ui::muted(" active delegation(s) involving this account.") << "\n";
                    }
                }

                const std::string action = (choice == 2) ? "ACCOUNT_DEACTIVATED" : "ACCOUNT_REACTIVATED";
                logAuditAction(action, username, admin.username);
                std::cout << ui::success("[+] Account status updated successfully.") << "\n";
                waitForEnter();
                continue;
            }

            std::string generated;
            const std::string accountTypeLabel = (typeChoice == 1) ? "Citizen" : "Admin";
            if (!ui::confirmAction("Reset password for " + accountTypeLabel + " account '" + username + "'?",
                                   "Confirm Reset",
                                   "Cancel")) {
                std::cout << ui::warning("[!] Password reset cancelled.") << "\n";
                waitForEnter();
                continue;
            }

            bool ok = false;
            if (typeChoice == 1) {
                ok = resetCitizenPassword(username, generated);
            } else {
                ok = resetAdminPassword(username, generated);
            }

            if (!ok) {
                std::cout << ui::error("[!] Password reset failed or username not found.") << "\n";
                waitForEnter();
                continue;
            }

            logAuditAction("ACCOUNT_PASSWORD_RESET", username, admin.username);
            std::cout << ui::success("[+] Temporary password generated: ") << generated << "\n";
            std::cout << ui::muted("[i] Share this securely and ask the user to change it after login.") << "\n";
            waitForEnter();
            continue;
        }

        if (choice == 5) {
            clearInputBuffer();
            std::string username;
            std::cout << "Target admin username: ";
            std::getline(std::cin, username);

            if (username.empty()) {
                std::cout << ui::warning("[!] Username is required.") << "\n";
                waitForEnter();
                continue;
            }

            if (username == admin.username) {
                std::cout << ui::warning("[!] You cannot hard-delete your current session account.") << "\n";
                waitForEnter();
                continue;
            }

            std::vector<std::string> pendingDocIds;
            std::vector<std::string> pendingBudgetEntryIds;
            if (!loadPendingApprovalIdsForAdminAccount(username, pendingDocIds, pendingBudgetEntryIds)) {
                std::cout << ui::error("[!] Unable to verify pending approvals for this admin account.") << "\n";
                logAuditAction("ACCOUNT_DELETE_BLOCKED_PENDING_CHECK_FAILED", username, admin.username);
                waitForEnter();
                continue;
            }

            if (!pendingDocIds.empty() || !pendingBudgetEntryIds.empty()) {
                std::cout << ui::error("[!] Hard delete blocked: target admin has pending assigned approvals.") << "\n";
                printBlockedPendingIdGroup("Pending document approvals", pendingDocIds);
                printBlockedPendingIdGroup("Pending budget approvals", pendingBudgetEntryIds);
                logAuditAction("ACCOUNT_DELETE_BLOCKED_PENDING_APPROVALS",
                               buildPendingApprovalBlockTarget(username,
                                                               pendingDocIds.size(),
                                                               pendingBudgetEntryIds.size()),
                               admin.username);
                waitForEnter();
                continue;
            }

            if (!ui::confirmAction("Hard-delete admin account '" + username + "'? This cannot be undone.",
                                   "Confirm Delete",
                                   "Cancel")) {
                std::cout << ui::warning("[!] Hard delete cancelled.") << "\n";
                waitForEnter();
                continue;
            }

            if (!deleteAdminAccountByUsername(username)) {
                std::cout << ui::error("[!] Hard delete failed or username not found.") << "\n";
                waitForEnter();
                continue;
            }

            const int revokedCount = revokeDelegationsForUsername(username);
            if (revokedCount < 0) {
                std::cout << ui::warning("[!] Account deleted, but delegation cleanup failed.") << "\n";
                logAuditAction("DELEGATION_REVOKE_FAILED", username, admin.username);
            } else if (revokedCount > 0) {
                const std::string target = username + " [count=" + std::to_string(revokedCount) + "]";
                logAuditAction("DELEGATIONS_AUTO_REVOKED", target, admin.username);
                std::cout << ui::muted("[i] Revoked ") << revokedCount
                          << ui::muted(" active delegation(s) involving this account.") << "\n";
            }

            logAuditAction("ACCOUNT_DELETED", username, admin.username);
            std::cout << ui::success("[+] Admin account deleted successfully.") << "\n";
            waitForEnter();
            continue;
        }

        if (choice != 0) {
            std::cout << ui::warning("[!] Invalid menu choice.") << "\n";
            waitForEnter();
        }

    } while (choice != 0);
}
