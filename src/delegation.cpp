#include "../include/delegation.h"
#include "../include/ui.h"
#include "../include/audit.h"

#include <ctime>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>

namespace {

const std::string DELEGATIONS_FILE_PATH_PRIMARY = "data/delegations.txt";
const std::string DELEGATIONS_FILE_PATH_FALLBACK = "../data/delegations.txt";
const std::string ADMINS_FILE_PATH_PRIMARY = "data/admins.txt";
const std::string ADMINS_FILE_PATH_FALLBACK = "../data/admins.txt";

bool openInputFileWithFallback(std::ifstream& file, const std::string& primary, const std::string& fallback) {
    file.open(primary);
    if (file.is_open()) { return true; }
    file.clear();
    file.open(fallback);
    return file.is_open();
}

std::string resolveDataPath(const std::string& primary, const std::string& fallback) {
    std::ifstream p(primary);
    if (p.is_open()) { return primary; }
    std::ifstream f(fallback);
    if (f.is_open()) { return fallback; }
    return primary;
}

std::vector<std::string> splitPipe(const std::string& line) {
    std::vector<std::string> tokens;
    std::stringstream parser(line);
    std::string token;
    while (std::getline(parser, token, '|')) {
        tokens.push_back(token);
    }
    return tokens;
}

void clearInputBuffer() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

std::string getCurrentTimestamp() {
    std::time_t now = std::time(NULL);
    struct std::tm* local = std::localtime(&now);
    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", local);
    return std::string(buffer);
}

std::string getTodayDate() {
    std::time_t now = std::time(NULL);
    struct std::tm* local = std::localtime(&now);
    char buffer[11];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", local);
    return std::string(buffer);
}

Delegation parseDelegationTokens(const std::vector<std::string>& tokens) {
    Delegation d;
    d.delegatorUsername = tokens.size() > 0 ? tokens[0] : "";
    d.delegateeUsername = tokens.size() > 1 ? tokens[1] : "";
    d.startDate = tokens.size() > 2 ? tokens[2] : "";
    d.endDate = tokens.size() > 3 ? tokens[3] : "";
    d.status = tokens.size() > 4 ? tokens[4] : "active";
    return d;
}

std::string serializeDelegation(const Delegation& d) {
    return d.delegatorUsername + "|" + d.delegateeUsername + "|" + d.startDate + "|" + d.endDate + "|" + d.status;
}

bool loadDelegations(std::vector<Delegation>& delegations) {
    std::ifstream file;
    if (!openInputFileWithFallback(file, DELEGATIONS_FILE_PATH_PRIMARY, DELEGATIONS_FILE_PATH_FALLBACK)) {
        return false;
    }
    delegations.clear();
    std::string line;
    bool firstLine = true;
    while (std::getline(file, line)) {
        if (line.empty()) { continue; }
        if (firstLine) { firstLine = false; if (line.find("delegatorUsername|") == 0) { continue; } }
        Delegation d = parseDelegationTokens(splitPipe(line));
        if (!d.delegatorUsername.empty() && !d.delegateeUsername.empty()) {
            delegations.push_back(d);
        }
    }
    return true;
}

bool saveDelegations(const std::vector<Delegation>& delegations) {
    std::ofstream writer(resolveDataPath(DELEGATIONS_FILE_PATH_PRIMARY, DELEGATIONS_FILE_PATH_FALLBACK));
    if (!writer.is_open()) { return false; }
    writer << "delegatorUsername|delegateeUsername|startDate|endDate|status\n";
    for (size_t i = 0; i < delegations.size(); ++i) {
        writer << serializeDelegation(delegations[i]) << "\n";
    }
    writer.flush();
    return true;
}

bool isAdminActiveWithUsername(const std::string& username);

bool isDelegationActive(const Delegation& d) {
    if (d.status != "active") { return false; }
    const std::string today = getTodayDate();
    if (!(today >= d.startDate && today <= d.endDate)) { return false; }
    if (!isAdminActiveWithUsername(d.delegatorUsername)) { return false; }
    if (!isAdminActiveWithUsername(d.delegateeUsername)) { return false; }
    return true;
}

bool isApproverRole(const std::string& role) {
    return role == "Budget Officer" || role == "Municipal Administrator";
}

bool isAdminActiveWithUsername(const std::string& username) {
    std::ifstream file;
    if (!openInputFileWithFallback(file, ADMINS_FILE_PATH_PRIMARY, ADMINS_FILE_PATH_FALLBACK)) {
        return false;
    }
    std::string line;
    bool firstLine = true;
    while (std::getline(file, line)) {
        if (line.empty()) { continue; }
        if (firstLine) { firstLine = false; if (line.find("adminID|") == 0) { continue; } }
        const std::vector<std::string> tokens = splitPipe(line);
        if (tokens.size() >= 6 && tokens[2] == username && tokens[5] == "active") {
            return true;
        }
    }
    return false;
}

bool isDateFormatValid(const std::string& date) {
    if (date.size() != 10) { return false; }
    if (date[4] != '-' || date[7] != '-') { return false; }
    for (int i = 0; i < 10; ++i) {
        if (i == 4 || i == 7) { continue; }
        if (date[static_cast<size_t>(i)] < '0' || date[static_cast<size_t>(i)] > '9') { return false; }
    }
    return true;
}

void createDelegation(const Admin& admin) {
    clearScreen();
    ui::printSectionTitle("CREATE DELEGATION");

    if (!isApproverRole(admin.role)) {
        std::cout << ui::error("[!] Only Budget Officers and Municipal Administrators can delegate.") << "\n";
        waitForEnter();
        return;
    }

    clearInputBuffer();
    std::string delegateeUsername;
    std::cout << "  Delegatee admin username: ";
    std::getline(std::cin, delegateeUsername);

    if (delegateeUsername.empty()) {
        std::cout << ui::error("[!] Username is required.") << "\n";
        waitForEnter();
        return;
    }

    if (delegateeUsername == admin.username) {
        std::cout << ui::error("[!] Cannot delegate to yourself.") << "\n";
        waitForEnter();
        return;
    }

    if (!isAdminActiveWithUsername(delegateeUsername)) {
        std::cout << ui::error("[!] Admin account not found or inactive.") << "\n";
        waitForEnter();
        return;
    }

    std::string startDate;
    std::string endDate;
    std::cout << "  Start date (YYYY-MM-DD): ";
    std::getline(std::cin, startDate);
    std::cout << "  End date   (YYYY-MM-DD): ";
    std::getline(std::cin, endDate);

    if (!isDateFormatValid(startDate) || !isDateFormatValid(endDate)) {
        std::cout << ui::error("[!] Invalid date format. Use YYYY-MM-DD.") << "\n";
        waitForEnter();
        return;
    }

    if (endDate < startDate) {
        std::cout << ui::error("[!] End date must be on or after start date.") << "\n";
        waitForEnter();
        return;
    }

    Delegation newDel;
    newDel.delegatorUsername = admin.username;
    newDel.delegateeUsername = delegateeUsername;
    newDel.startDate = startDate;
    newDel.endDate = endDate;
    newDel.status = "active";

    std::vector<Delegation> delegations;
    loadDelegations(delegations);
    delegations.push_back(newDel);
    saveDelegations(delegations);

    logAuditAction("DELEGATION_CREATED", delegateeUsername, admin.username);
    std::cout << "\n" << ui::success("[+] Delegation created successfully.") << "\n";
    std::cout << "  " << admin.username << " -> " << delegateeUsername << " (" << startDate << " to " << endDate << ")\n";
    waitForEnter();
}

void viewDelegations(const Admin& admin) {
    clearScreen();
    ui::printSectionTitle("MY DELEGATIONS");

    std::vector<Delegation> delegations;
    if (!loadDelegations(delegations)) {
        std::cout << ui::error("[!] Unable to open delegations file.") << "\n";
        waitForEnter();
        return;
    }

    std::vector<Delegation> relevant;
    for (size_t i = 0; i < delegations.size(); ++i) {
        if (delegations[i].delegatorUsername == admin.username || delegations[i].delegateeUsername == admin.username) {
            relevant.push_back(delegations[i]);
        }
    }

    if (relevant.empty()) {
        std::cout << "\n" << ui::muted("  No delegations found.") << "\n";
        waitForEnter();
        return;
    }

    const std::vector<std::string> headers = {"Delegator", "Delegatee", "Start", "End", "Status"};
    const std::vector<int> widths = {18, 18, 12, 12, 10};
    ui::printTableHeader(headers, widths);
    for (size_t i = 0; i < relevant.size(); ++i) {
        std::string displayStatus = relevant[i].status;
        if (relevant[i].status == "active" && !isDelegationActive(relevant[i])) {
            displayStatus = "expired";
        }
        ui::printTableRow({relevant[i].delegatorUsername, relevant[i].delegateeUsername,
                           relevant[i].startDate, relevant[i].endDate, displayStatus}, widths);
    }
    ui::printTableFooter(widths);

    waitForEnter();
}

void revokeDelegation(const Admin& admin) {
    clearScreen();
    ui::printSectionTitle("REVOKE DELEGATION");

    std::vector<Delegation> delegations;
    if (!loadDelegations(delegations)) {
        std::cout << ui::error("[!] Unable to open delegations file.") << "\n";
        waitForEnter();
        return;
    }

    std::vector<size_t> activeIndices;
    for (size_t i = 0; i < delegations.size(); ++i) {
        if (delegations[i].delegatorUsername == admin.username && delegations[i].status == "active") {
            activeIndices.push_back(i);
        }
    }

    if (activeIndices.empty()) {
        std::cout << "\n" << ui::muted("  No active delegations to revoke.") << "\n";
        waitForEnter();
        return;
    }

    std::cout << "\n" << ui::bold("Active Delegations You Created") << "\n";
    for (size_t j = 0; j < activeIndices.size(); ++j) {
        const Delegation& d = delegations[activeIndices[j]];
        std::cout << "  " << ui::info("[" + std::to_string(j + 1) + "]")
                  << " -> " << d.delegateeUsername << " (" << d.startDate << " to " << d.endDate << ")\n";
    }
    std::cout << "  " << ui::info("[0]") << " Cancel\n";
    std::cout << "\n  Select delegation to revoke: ";

    int revokeChoice = -1;
    std::cin >> revokeChoice;
    if (std::cin.fail() || revokeChoice < 0 || revokeChoice > static_cast<int>(activeIndices.size())) {
        std::cin.clear();
        clearInputBuffer();
        std::cout << ui::error("[!] Invalid selection.") << "\n";
        waitForEnter();
        return;
    }

    if (revokeChoice == 0) { return; }

    size_t targetIndex = activeIndices[static_cast<size_t>(revokeChoice - 1)];
    if (!ui::confirmAction("Are you sure you want to revoke this delegation?",
                           "Confirm Revoke",
                           "Cancel")) {
        std::cout << "\n" << ui::warning("[!] Delegation revoke cancelled.") << "\n";
        waitForEnter();
        return;
    }

    delegations[targetIndex].status = "revoked";
    saveDelegations(delegations);

    logAuditAction("DELEGATION_REVOKED", delegations[targetIndex].delegateeUsername, admin.username);
    std::cout << "\n" << ui::success("[+] Delegation revoked.") << "\n";
    waitForEnter();
}

} // namespace

void ensureDelegationFileExists() {
    std::ifstream check;
    if (!openInputFileWithFallback(check, DELEGATIONS_FILE_PATH_PRIMARY, DELEGATIONS_FILE_PATH_FALLBACK)) {
        std::ofstream createFile(DELEGATIONS_FILE_PATH_PRIMARY);
        if (!createFile.is_open()) {
            createFile.clear();
            createFile.open(DELEGATIONS_FILE_PATH_FALLBACK);
        }
        if (createFile.is_open()) {
            createFile << "delegatorUsername|delegateeUsername|startDate|endDate|status\n";
        }
    }
}

std::vector<Delegation> getActiveDelegationsFor(const std::string& delegateeUsername) {
    std::vector<Delegation> all;
    std::vector<Delegation> result;
    if (!loadDelegations(all)) { return result; }
    for (size_t i = 0; i < all.size(); ++i) {
        if (all[i].delegateeUsername == delegateeUsername && isDelegationActive(all[i])) {
            result.push_back(all[i]);
        }
    }
    return result;
}

int revokeDelegationsForUsername(const std::string& username) {
    if (username.empty()) {
        return 0;
    }

    std::vector<Delegation> all;
    if (!loadDelegations(all)) {
        return -1;
    }

    int revokedCount = 0;
    for (size_t i = 0; i < all.size(); ++i) {
        if (all[i].status != "active") {
            continue;
        }

        if (all[i].delegatorUsername == username || all[i].delegateeUsername == username) {
            all[i].status = "revoked";
            revokedCount++;
        }
    }

    if (revokedCount == 0) {
        return 0;
    }

    if (!saveDelegations(all)) {
        return -1;
    }

    return revokedCount;
}

void runDelegationManagement(const Admin& admin) {
    if (!isApproverRole(admin.role) && admin.role != "Super Admin") {
        std::cout << ui::error("[!] Only approver roles can manage delegations.") << "\n";
        waitForEnter();
        return;
    }

    int choice = -1;
    do {
        clearScreen();
        ui::printSectionTitle("DELEGATION MANAGEMENT");
        ui::printBreadcrumb({"ADMIN", "APPROVALS", "DELEGATIONS"});
        std::cout << "  " << ui::info("[1]") << " Create Delegation\n";
        std::cout << "  " << ui::info("[2]") << " View My Delegations\n";
        std::cout << "  " << ui::info("[3]") << " Revoke Delegation\n";
        std::cout << "  " << ui::info("[0]") << " Back\n";
        std::cout << ui::muted("--------------------------------------------------------------") << "\n";
        std::cout << "  Enter your choice: ";

        std::cin >> choice;
        if (std::cin.fail()) {
            std::cin.clear();
            clearInputBuffer();
            std::cout << "\n" << ui::error("[!] Invalid input.") << "\n";
            waitForEnter();
            continue;
        }

        switch (choice) {
            case 1: createDelegation(admin); break;
            case 2: viewDelegations(admin); break;
            case 3: revokeDelegation(admin); break;
            case 0: break;
            default:
                std::cout << "\n" << ui::error("[!] Invalid choice.") << "\n";
                waitForEnter();
                break;
        }
    } while (choice != 0);
}
