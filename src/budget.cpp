#include "../include/budget.h"

#include "../include/audit.h"
#include "../include/auth.h"
#include "../include/blockchain.h"
#include "../include/delegation.h"
#include "../include/storage_utils.h"
#include "../include/ui.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <vector>

namespace {
const std::string BUDGETS_FILE_PATH_PRIMARY = "data/budgets.txt";
const std::string BUDGETS_FILE_PATH_FALLBACK = "../data/budgets.txt";
const std::string BUDGET_ENTRIES_FILE_PATH_PRIMARY = "data/budget_entries.txt";
const std::string BUDGET_ENTRIES_FILE_PATH_FALLBACK = "../data/budget_entries.txt";
const std::string BUDGET_APPROVALS_FILE_PATH_PRIMARY = "data/budget_approvals.txt";
const std::string BUDGET_APPROVALS_FILE_PATH_FALLBACK = "../data/budget_approvals.txt";
const std::string APPROVALS_FILE_PATH_PRIMARY = "data/approvals.txt";
const std::string APPROVALS_FILE_PATH_FALLBACK = "../data/approvals.txt";
const std::string ADMINS_FILE_PATH_PRIMARY = "data/admins.txt";
const std::string ADMINS_FILE_PATH_FALLBACK = "../data/admins.txt";
const std::string DOCUMENTS_FILE_PATH_PRIMARY = "data/documents.txt";
const std::string DOCUMENTS_FILE_PATH_FALLBACK = "../data/documents.txt";

struct BudgetRow {
    std::string category;
    double amount;
};

struct VarianceRow {
    std::string category;
    double allocated;
    double actual;
    double variance;
    double utilization;
};

struct BudgetEntry {
    std::string entryId;
    std::string entryType;
    std::string fiscalYear;
    std::string category;
    double allocatedAmount;
    std::string description;
    std::string createdAt;
    std::string createdBy;
    std::string status;
    std::string publishedAt;
};

struct BudgetApproval {
    std::string entryId;
    std::string approverUsername;
    std::string role;
    std::string status;
    std::string createdAt;
    std::string decidedAt;
    std::string note;
};

struct OverrunGuardrailCheck {
    bool hasBudget;
    bool warn;
    bool block;
    std::string category;
    double allocation;
    double actual;
    double utilization;
};

struct MonthlyVarianceRow {
    std::string category;
    double allocated;
    double monthlyActual;
    double variance;
    double utilization;
};

bool openInputFileWithFallback(std::ifstream& file, const std::string& primaryPath, const std::string& fallbackPath) {
    file.open(primaryPath);
    if (file.is_open()) {
        return true;
    }

    file.clear();
    file.open(fallbackPath);
    return file.is_open();
}

std::string resolveDataPath(const std::string& primaryPath, const std::string& fallbackPath) {
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
    std::ifstream primary(primaryPath);
    if (primary.is_open()) {
        return true;
    }

    std::ifstream fallback(fallbackPath);
    if (fallback.is_open()) {
        return true;
    }

    return storage::writeTextFileWithFallback(primaryPath, fallbackPath, headerLine + "\n");
}

std::vector<std::string> splitPipe(const std::string& line) {
    return storage::splitPipeRow(line);
}

void clearInputBuffer() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

std::string toLowerCopy(std::string value) {
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] >= 'A' && value[i] <= 'Z') {
            value[i] = static_cast<char>(value[i] + ('a' - 'A'));
        }
    }
    return value;
}

std::string trimCopy(const std::string& value) {
    const std::string whitespace = " \t\r\n";
    const std::size_t start = value.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return "";
    }

    const std::size_t end = value.find_last_not_of(whitespace);
    return value.substr(start, end - start + 1);
}

bool isSafeExportFileName(const std::string& fileName) {
    if (fileName.empty() || fileName.size() > 90) {
        return false;
    }

    if (fileName.find("..") != std::string::npos) {
        return false;
    }

    for (std::size_t i = 0; i < fileName.size(); ++i) {
        const char c = fileName[i];
        const bool safe = std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '-' || c == '.';
        if (!safe) {
            return false;
        }
    }

    return true;
}

std::string sanitizeFileToken(std::string token) {
    for (std::size_t i = 0; i < token.size(); ++i) {
        const char c = token[i];
        const bool safe = std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '-' || c == '.';
        if (!safe) {
            token[i] = '_';
        }
    }
    return token;
}

std::string chooseExportPath(const std::string& fileName) {
    std::ofstream primary("data/" + fileName);
    if (primary.is_open()) {
        primary.close();
        return "data/" + fileName;
    }

    std::ofstream fallback("../data/" + fileName);
    if (fallback.is_open()) {
        fallback.close();
        return "../data/" + fileName;
    }

    return "";
}

bool isYearMonthTextValid(const std::string& yearMonth) {
    if (yearMonth.size() != 7 || yearMonth[4] != '-') {
        return false;
    }

    for (std::size_t i = 0; i < yearMonth.size(); ++i) {
        if (i == 4) {
            continue;
        }
        if (yearMonth[i] < '0' || yearMonth[i] > '9') {
            return false;
        }
    }

    const int month = std::atoi(yearMonth.substr(5, 2).c_str());
    return month >= 1 && month <= 12;
}

bool isTimestampInYearMonth(const std::string& timestamp, const std::string& yearMonth) {
    return timestamp.size() >= 7 && timestamp.substr(0, 7) == yearMonth;
}

bool isBudgetApproverRole(const Admin& admin) {
    return admin.role == "Budget Officer" || admin.role == "Municipal Administrator";
}

bool canSubmitBudgetEntry(const Admin& admin) {
    return admin.role == "Budget Officer" || admin.role == "Super Admin";
}

bool loadBudgetRows(std::vector<BudgetRow>& rows) {
    std::ifstream file;
    if (!openInputFileWithFallback(file, BUDGETS_FILE_PATH_PRIMARY, BUDGETS_FILE_PATH_FALLBACK)) {
        return false;
    }

    rows.clear();
    std::string line;
    bool firstLine = true;

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        if (firstLine) {
            firstLine = false;
            if (line.find("category|") == 0) {
                continue;
            }
        }

        const std::vector<std::string> tokens = splitPipe(line);
        if (tokens.empty()) {
            continue;
        }

        BudgetRow row;
        row.category = tokens[0];
        row.amount = 0.0;

        if (tokens.size() > 1) {
            std::stringstream amountParser(tokens[1]);
            amountParser >> row.amount;
        }

        if (!row.category.empty()) {
            rows.push_back(row);
        }
    }

    return true;
}

bool saveBudgetRows(const std::vector<BudgetRow>& rows) {
    std::vector<std::string> serializedRows;
    serializedRows.reserve(rows.size());
    for (std::size_t i = 0; i < rows.size(); ++i) {
        std::ostringstream amountOut;
        amountOut << std::fixed << std::setprecision(2) << rows[i].amount;
        serializedRows.push_back(storage::joinPipeRow({rows[i].category, amountOut.str()}));
    }
    return storage::writePipeFileWithFallback(BUDGETS_FILE_PATH_PRIMARY,
                                              BUDGETS_FILE_PATH_FALLBACK,
                                              "category|amount",
                                              serializedRows);
}

BudgetEntry parseBudgetEntryTokens(const std::vector<std::string>& tokens) {
    BudgetEntry entry;
    entry.entryId = tokens.size() > 0 ? tokens[0] : "";
    entry.entryType = tokens.size() > 1 ? tokens[1] : "NEW";
    entry.fiscalYear = tokens.size() > 2 ? tokens[2] : "";
    entry.category = tokens.size() > 3 ? tokens[3] : "";
    entry.allocatedAmount = 0.0;
    entry.description = tokens.size() > 5 ? tokens[5] : "";
    entry.createdAt = tokens.size() > 6 ? tokens[6] : "";
    entry.createdBy = tokens.size() > 7 ? tokens[7] : "";
    entry.status = tokens.size() > 8 ? tokens[8] : "pending_approval";
    entry.publishedAt = tokens.size() > 9 ? tokens[9] : "";

    if (tokens.size() > 4) {
        std::stringstream amountIn(tokens[4]);
        amountIn >> entry.allocatedAmount;
    }

    return entry;
}

std::string serializeBudgetEntry(const BudgetEntry& entry) {
    std::ostringstream amountOut;
    amountOut << std::fixed << std::setprecision(2) << entry.allocatedAmount;

    return storage::joinPipeRow({
        entry.entryId,
        entry.entryType,
        entry.fiscalYear,
        entry.category,
        amountOut.str(),
        entry.description,
        entry.createdAt,
        entry.createdBy,
        entry.status,
        entry.publishedAt
    });
}

bool loadBudgetEntries(std::vector<BudgetEntry>& rows) {
    std::ifstream file;
    if (!openInputFileWithFallback(file, BUDGET_ENTRIES_FILE_PATH_PRIMARY, BUDGET_ENTRIES_FILE_PATH_FALLBACK)) {
        return false;
    }

    rows.clear();
    std::string line;
    bool firstLine = true;

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        if (firstLine) {
            firstLine = false;
            if (line.find("entryID|") == 0) {
                continue;
            }
        }

        const BudgetEntry entry = parseBudgetEntryTokens(splitPipe(line));
        if (!entry.entryId.empty()) {
            rows.push_back(entry);
        }
    }

    return true;
}

bool saveBudgetEntries(const std::vector<BudgetEntry>& rows) {
    std::vector<std::string> serializedRows;
    serializedRows.reserve(rows.size());
    for (std::size_t i = 0; i < rows.size(); ++i) {
        serializedRows.push_back(serializeBudgetEntry(rows[i]));
    }
    return storage::writePipeFileWithFallback(BUDGET_ENTRIES_FILE_PATH_PRIMARY,
                                              BUDGET_ENTRIES_FILE_PATH_FALLBACK,
                                              "entryID|entryType|fiscalYear|category|allocatedAmount|description|createdAt|createdBy|status|publishedAt",
                                              serializedRows);
}

BudgetApproval parseBudgetApprovalTokens(const std::vector<std::string>& tokens) {
    BudgetApproval row;
    row.entryId = tokens.size() > 0 ? tokens[0] : "";
    row.approverUsername = tokens.size() > 1 ? tokens[1] : "";
    row.role = tokens.size() > 2 ? tokens[2] : "";
    row.status = tokens.size() > 3 ? tokens[3] : "pending";
    row.createdAt = tokens.size() > 4 ? tokens[4] : "";
    row.decidedAt = tokens.size() > 5 ? tokens[5] : "";
    row.note = tokens.size() > 6 ? tokens[6] : "";
    return row;
}

std::string serializeBudgetApproval(const BudgetApproval& row) {
    return storage::joinPipeRow({
        row.entryId,
        row.approverUsername,
        row.role,
        row.status,
        row.createdAt,
        row.decidedAt,
        row.note
    });
}

bool loadBudgetApprovals(std::vector<BudgetApproval>& rows) {
    std::ifstream file;
    if (!openInputFileWithFallback(file, BUDGET_APPROVALS_FILE_PATH_PRIMARY, BUDGET_APPROVALS_FILE_PATH_FALLBACK)) {
        return false;
    }

    rows.clear();
    std::string line;
    bool firstLine = true;

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        if (firstLine) {
            firstLine = false;
            if (line.find("entryID|") == 0) {
                continue;
            }
        }

        const BudgetApproval row = parseBudgetApprovalTokens(splitPipe(line));
        if (!row.entryId.empty() && !row.approverUsername.empty()) {
            rows.push_back(row);
        }
    }

    return true;
}

bool saveBudgetApprovals(const std::vector<BudgetApproval>& rows) {
    std::vector<std::string> serializedRows;
    serializedRows.reserve(rows.size());
    for (std::size_t i = 0; i < rows.size(); ++i) {
        serializedRows.push_back(serializeBudgetApproval(rows[i]));
    }
    return storage::writePipeFileWithFallback(BUDGET_APPROVALS_FILE_PATH_PRIMARY,
                                              BUDGET_APPROVALS_FILE_PATH_FALLBACK,
                                              "entryID|approverUsername|role|status|createdAt|decidedAt|note",
                                              serializedRows);
}

std::string findActiveAdminUsernameByRole(const std::string& targetRole) {
    std::ifstream file;
    if (!openInputFileWithFallback(file, ADMINS_FILE_PATH_PRIMARY, ADMINS_FILE_PATH_FALLBACK)) {
        return "";
    }

    std::string line;
    std::getline(file, line);

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        const std::vector<std::string> tokens = splitPipe(line);
        const std::string username = tokens.size() > 2 ? tokens[2] : "";
        const std::string role = tokens.size() > 4 ? tokens[4] : "";
        const std::string status = tokens.size() > 5 ? tokens[5] : "active";

        if (role == targetRole && status == "active" && !username.empty()) {
            return username;
        }
    }

    return "";
}

std::vector<std::string> findAllActiveAdminUsernamesByRole(const std::string& targetRole) {
    std::vector<std::string> usernames;

    std::ifstream file;
    if (!openInputFileWithFallback(file, ADMINS_FILE_PATH_PRIMARY, ADMINS_FILE_PATH_FALLBACK)) {
        return usernames;
    }

    std::string line;
    std::getline(file, line);

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        const std::vector<std::string> tokens = splitPipe(line);
        const std::string username = tokens.size() > 2 ? tokens[2] : "";
        const std::string role = tokens.size() > 4 ? tokens[4] : "";
        const std::string status = tokens.size() > 5 ? tokens[5] : "active";

        if (role == targetRole && status == "active" && !username.empty()) {
            usernames.push_back(username);
        }
    }

    return usernames;
}

bool hasBudgetApprovalTuple(const std::vector<BudgetApproval>& rows,
                            const std::string& targetEntryId,
                            const std::string& approverUsername,
                            const std::string& approverRole) {
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].entryId == targetEntryId &&
            rows[i].approverUsername == approverUsername &&
            rows[i].role == approverRole) {
            return true;
        }
    }

    return false;
}

std::string generateNextBudgetEntryId(const std::vector<BudgetEntry>& entries) {
    int maxNumber = 0;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].entryId.size() < 3) {
            continue;
        }

        const int value = std::atoi(entries[i].entryId.substr(2).c_str());
        if (value > maxNumber) {
            maxNumber = value;
        }
    }

    std::ostringstream out;
    out << "BA" << std::setw(3) << std::setfill('0') << (maxNumber + 1);
    return out.str();
}

bool createBudgetApprovalRequests(const std::string& entryId) {
    std::vector<BudgetApproval> approvals;
    if (!loadBudgetApprovals(approvals)) {
        return false;
    }

    std::vector<BudgetEntry> entries;
    if (!loadBudgetEntries(entries)) {
        return false;
    }

    std::string submitter;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].entryId == entryId) {
            submitter = entries[i].createdBy;
            break;
        }
    }

    if (submitter.empty()) {
        return false;
    }

    const std::vector<std::string> budgetOfficers = findAllActiveAdminUsernamesByRole("Budget Officer");
    const std::vector<std::string> municipalAdmins = findAllActiveAdminUsernamesByRole("Municipal Administrator");

    std::vector<std::string> eligibleBudgetOfficers;
    std::vector<std::string> eligibleMunicipalAdmins;

    for (std::size_t i = 0; i < budgetOfficers.size(); ++i) {
        if (budgetOfficers[i] != submitter) {
            eligibleBudgetOfficers.push_back(budgetOfficers[i]);
        }
    }

    for (std::size_t i = 0; i < municipalAdmins.size(); ++i) {
        if (municipalAdmins[i] != submitter) {
            eligibleMunicipalAdmins.push_back(municipalAdmins[i]);
        }
    }

    if (eligibleBudgetOfficers.empty() || eligibleMunicipalAdmins.empty()) {
        return false;
    }

    const std::string now = getCurrentTimestamp();
    std::vector<BudgetApproval> toCreate;

    for (std::size_t i = 0; i < eligibleBudgetOfficers.size(); ++i) {
        if (hasBudgetApprovalTuple(approvals, entryId, eligibleBudgetOfficers[i], "Budget Officer") ||
            hasBudgetApprovalTuple(toCreate, entryId, eligibleBudgetOfficers[i], "Budget Officer")) {
            continue;
        }

        BudgetApproval a;
        a.entryId = entryId;
        a.approverUsername = eligibleBudgetOfficers[i];
        a.role = "Budget Officer";
        a.status = "pending";
        a.createdAt = now;
        toCreate.push_back(a);
    }

    for (std::size_t i = 0; i < eligibleMunicipalAdmins.size(); ++i) {
        if (hasBudgetApprovalTuple(approvals, entryId, eligibleMunicipalAdmins[i], "Municipal Administrator") ||
            hasBudgetApprovalTuple(toCreate, entryId, eligibleMunicipalAdmins[i], "Municipal Administrator")) {
            continue;
        }

        BudgetApproval a;
        a.entryId = entryId;
        a.approverUsername = eligibleMunicipalAdmins[i];
        a.role = "Municipal Administrator";
        a.status = "pending";
        a.createdAt = now;
        toCreate.push_back(a);
    }

    for (std::size_t i = 0; i < toCreate.size(); ++i) {
        approvals.push_back(toCreate[i]);
    }

    if (toCreate.empty()) {
        return true;
    }

    return saveBudgetApprovals(approvals);
}

bool getPendingBudgetApprovalIdsForApproverInternal(const std::string& approverUsername,
                                                    std::vector<std::string>& outEntryIds) {
    outEntryIds.clear();

    std::vector<BudgetApproval> rows;
    if (!loadBudgetApprovals(rows)) {
        return false;
    }

    std::map<std::string, bool> seenEntryIds;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].approverUsername != approverUsername || toLowerCopy(rows[i].status) != "pending") {
            continue;
        }

        if (seenEntryIds[rows[i].entryId]) {
            continue;
        }

        seenEntryIds[rows[i].entryId] = true;
        outEntryIds.push_back(rows[i].entryId);
    }

    return true;
}

void collectPendingBudgetRowsForDecision(const std::vector<BudgetApproval>& rows,
                                         const std::string& approverUsername,
                                         std::vector<BudgetApproval>& directRows,
                                         std::vector<BudgetApproval>& delegatedRows) {
    directRows.clear();
    delegatedRows.clear();

    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].approverUsername == approverUsername && rows[i].status == "pending") {
            directRows.push_back(rows[i]);
        }
    }

    const std::vector<Delegation> delegations = getActiveDelegationsFor(approverUsername);
    std::set<std::string> delegatedKeys;
    for (std::size_t d = 0; d < delegations.size(); ++d) {
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (rows[i].approverUsername != delegations[d].delegatorUsername || rows[i].status != "pending") {
                continue;
            }

            const std::string key = rows[i].entryId + "|" + rows[i].approverUsername + "|" + rows[i].role;
            if (!delegatedKeys.insert(key).second) {
                continue;
            }

            delegatedRows.push_back(rows[i]);
        }
    }
}

bool resolvePendingBudgetDecisionOwner(const std::vector<BudgetApproval>& rows,
                                       const std::string& approverUsername,
                                       const std::string& entryId,
                                       std::string& ownerUsername) {
    ownerUsername.clear();

    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].entryId == entryId && rows[i].approverUsername == approverUsername && rows[i].status == "pending") {
            ownerUsername = approverUsername;
            return true;
        }
    }

    const std::vector<Delegation> delegations = getActiveDelegationsFor(approverUsername);
    for (std::size_t d = 0; d < delegations.size(); ++d) {
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (rows[i].entryId == entryId &&
                rows[i].approverUsername == delegations[d].delegatorUsername &&
                rows[i].status == "pending") {
                ownerUsername = delegations[d].delegatorUsername;
                return true;
            }
        }
    }

    return false;
}

bool isFiscalYearValid(const std::string& fiscalYear) {
    if (fiscalYear.size() != 4) {
        return false;
    }

    for (std::size_t i = 0; i < fiscalYear.size(); ++i) {
        if (fiscalYear[i] < '0' || fiscalYear[i] > '9') {
            return false;
        }
    }

    return true;
}

std::vector<std::string> buildBudgetCategoryChoices() {
    std::set<std::string> categories;
    categories.insert("Infrastructure Procurement");
    categories.insert("Health Supplies");
    categories.insert("Educational Materials");
    categories.insert("Office Supplies");
    categories.insert("Emergency Procurement");

    std::vector<BudgetRow> existing;
    if (loadBudgetRows(existing)) {
        for (std::size_t i = 0; i < existing.size(); ++i) {
            if (!existing[i].category.empty()) {
                categories.insert(existing[i].category);
            }
        }
    }

    return std::vector<std::string>(categories.begin(), categories.end());
}

bool promptBudgetCategoryChoice(std::string& outCategory) {
    const std::vector<std::string> choices = buildBudgetCategoryChoices();
    if (choices.empty()) {
        return false;
    }

    std::cout << "\n" << ui::bold("Choose Budget Category") << "\n";
    for (std::size_t i = 0; i < choices.size(); ++i) {
        std::cout << "  [" << (i + 1) << "] " << choices[i] << "\n";
    }
    std::cout << "  [" << (choices.size() + 1) << "] Other (type custom category)\n";
    std::cout << "  Enter choice: ";

    int selected = 0;
    std::cin >> selected;
    if (std::cin.fail()) {
        std::cin.clear();
        clearInputBuffer();
        return false;
    }

    if (selected >= 1 && static_cast<std::size_t>(selected) <= choices.size()) {
        outCategory = choices[static_cast<std::size_t>(selected - 1)];
        clearInputBuffer();
        return true;
    }

    if (selected == static_cast<int>(choices.size()) + 1) {
        clearInputBuffer();
        std::cout << "  Custom category: ";
        std::getline(std::cin, outCategory);
        outCategory = trimCopy(outCategory);
        return !outCategory.empty();
    }

    clearInputBuffer();
    return false;
}

void seedBudgetsIfEmpty() {
    // Budget showcase rows should come from data/budgets.txt.
    std::vector<BudgetRow> rows;
    if (!loadBudgetRows(rows)) {
        return;
    }

    saveBudgetRows(rows);
}

void seedBudgetConsensusIfEmpty() {
    // Budget entry and approval showcase rows should come from data/*.txt files.
    std::vector<BudgetEntry> entries;
    if (!loadBudgetEntries(entries)) {
        return;
    }

    saveBudgetEntries(entries);

    std::vector<BudgetApproval> approvals;
    if (!loadBudgetApprovals(approvals)) {
        return;
    }

    saveBudgetApprovals(approvals);
}

void printBudgetTable(const std::vector<BudgetRow>& rows) {
    const std::vector<std::string> headers = {"Category", "Amount (PHP)", "Share"};
    const std::vector<int> widths = {30, 14, 8};

    double total = 0.0;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        total += rows[i].amount;
    }

    ui::printTableHeader(headers, widths);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        std::ostringstream amountOut;
        amountOut << std::fixed << std::setprecision(2) << rows[i].amount;

        std::ostringstream shareOut;
        const double share = total > 0.0 ? (rows[i].amount / total) * 100.0 : 0.0;
        shareOut << std::fixed << std::setprecision(1) << share << "%";

        ui::printTableRow({rows[i].category, amountOut.str(), shareOut.str()}, widths);
    }
    ui::printTableFooter(widths);
}

void printBudgetChart(const std::vector<BudgetRow>& rows) {
    if (rows.empty()) {
        return;
    }

    double maxAmount = 0.0;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].amount > maxAmount) {
            maxAmount = rows[i].amount;
        }
    }

    std::cout << "\n" << ui::bold("Budget Allocation Chart") << "\n";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        ui::printBar(rows[i].category, rows[i].amount, maxAmount, 24);
    }
}

std::map<std::string, double> loadActualSpendByCategory() {
    std::map<std::string, double> totals;

    std::ifstream file;
    if (!openInputFileWithFallback(file, DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK)) {
        return totals;
    }

    std::string line;
    bool firstLine = true;

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        if (firstLine) {
            firstLine = false;
            if (line.find("docID|") == 0) {
                continue;
            }
        }

        const std::vector<std::string> tokens = splitPipe(line);
        if (tokens.size() < 8) {
            continue;
        }

        const std::string status = toLowerCopy(tokens.size() >= 15 ? tokens[7] : tokens[6]);
        if (status != "approved" && status != "published") {
            continue;
        }

        const std::string fallbackCategory = tokens.size() >= 15 ? tokens[2] : (tokens.size() > 2 ? tokens[2] : "");
        const std::string budgetCategory = tokens.size() >= 15
            ? (tokens[13].empty() ? fallbackCategory : tokens[13])
            : (tokens.size() > 8 && !tokens[8].empty() ? tokens[8] : fallbackCategory);

        double amount = 0.0;
        if (tokens.size() >= 15) {
            std::stringstream amountIn(tokens[14]);
            amountIn >> amount;
        } else if (tokens.size() > 9) {
            std::stringstream amountIn(tokens[9]);
            amountIn >> amount;
        }

        if (!budgetCategory.empty() && amount > 0.0) {
            totals[budgetCategory] += amount;
        }
    }

    return totals;
}

bool applyPublishedBudgetChange(const BudgetEntry& entry) {
    std::vector<BudgetRow> rows;
    if (!loadBudgetRows(rows)) {
        return false;
    }

    bool found = false;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].category == entry.category) {
            rows[i].amount = entry.allocatedAmount;
            found = true;
            break;
        }
    }

    if (!found) {
        BudgetRow row;
        row.category = entry.category;
        row.amount = entry.allocatedAmount;
        rows.push_back(row);
    }

    return saveBudgetRows(rows);
}

OverrunGuardrailCheck buildBudgetEntryPublishGuardrailCheck(const BudgetEntry& entry) {
    OverrunGuardrailCheck check;
    check.hasBudget = false;
    check.warn = false;
    check.block = false;
    check.category = entry.category;
    check.allocation = entry.allocatedAmount;
    check.actual = 0.0;
    check.utilization = 0.0;

    if (check.category.empty()) {
        return check;
    }

    check.hasBudget = true;
    const std::map<std::string, double> actuals = loadActualSpendByCategory();
    std::map<std::string, double>::const_iterator actualFound = actuals.find(check.category);
    if (actualFound != actuals.end()) {
        check.actual = actualFound->second;
    }

    if (check.allocation <= 0.0) {
        check.block = check.actual > 0.0;
        return check;
    }

    check.utilization = (check.actual / check.allocation) * 100.0;
    if (check.utilization > 100.0) {
        check.block = true;
    } else if (check.utilization >= 90.0) {
        check.warn = true;
    }

    return check;
}

bool confirmBudgetEntryPublishGuardrail(const BudgetEntry& entry, const std::string& actorUsername) {
    const OverrunGuardrailCheck check = buildBudgetEntryPublishGuardrailCheck(entry);
    if (!check.hasBudget) {
        return true;
    }

    std::ostringstream allocationOut;
    allocationOut << std::fixed << std::setprecision(2) << check.allocation;
    std::ostringstream actualOut;
    actualOut << std::fixed << std::setprecision(2) << check.actual;
    std::ostringstream utilizationOut;
    utilizationOut << std::fixed << std::setprecision(1) << check.utilization;

    if (check.block) {
        std::cout << ui::error("[!] Budget overrun guardrail blocked budget publication.") << "\n";
        std::cout << "  Category             : " << check.category << "\n";
        std::cout << "  Projected allocation : PHP " << allocationOut.str() << "\n";
        std::cout << "  Current actual spend : PHP " << actualOut.str() << "\n";
        if (check.allocation > 0.0) {
            std::cout << "  Utilization          : " << utilizationOut.str() << "%\n";
        }

        logAuditAction("BUDGET_OVERRUN_BLOCKED", entry.entryId, actorUsername);
        return false;
    }

    if (!check.warn) {
        return true;
    }

    std::cout << ui::warning("[!] Budget overrun warning (>=90% utilization).") << "\n";
    std::cout << "  Category             : " << check.category << "\n";
    std::cout << "  Projected allocation : PHP " << allocationOut.str() << "\n";
    std::cout << "  Current actual spend : PHP " << actualOut.str() << "\n";
    std::cout << "  Utilization          : " << utilizationOut.str() << "%\n";
    logAuditAction("BUDGET_OVERRUN_WARNING", entry.entryId, actorUsername);

    if (!ui::confirmAction("Proceed with publish despite high utilization warning?",
                           "Proceed",
                           "Cancel Publish")) {
        logAuditAction("BUDGET_OVERRUN_WARNING_CANCELLED", entry.entryId, actorUsername);
        return false;
    }

    return true;
}

void submitBudgetEntry(const Admin& admin, const std::string& entryType) {
    clearScreen();
    ui::printSectionTitle("SUBMIT BUDGET ENTRY");

    clearInputBuffer();

    std::string fiscalYear;
    std::string category;
    std::string description;
    double allocatedAmount = 0.0;

    std::cout << "Fiscal Year (YYYY): ";
    std::getline(std::cin, fiscalYear);

    // Category list keeps data more consistent while still allowing custom entries.
    if (!promptBudgetCategoryChoice(category)) {
        std::cout << ui::error("[!] Invalid category selection.") << "\n";
        logAuditAction("BUDGET_ENTRY_SUBMIT_FAILED", "N/A", admin.username);
        waitForEnter();
        return;
    }

    std::cout << "Allocated Amount   : ";
    std::cin >> allocatedAmount;

    if (std::cin.fail()) {
        std::cin.clear();
        clearInputBuffer();
        std::cout << ui::error("[!] Invalid amount.") << "\n";
        logAuditAction("BUDGET_ENTRY_SUBMIT_FAILED", "N/A", admin.username);
        waitForEnter();
        return;
    }

    clearInputBuffer();
    std::cout << "Description        : ";
    std::getline(std::cin, description);

    if (!isFiscalYearValid(fiscalYear) || category.empty() || description.empty() || allocatedAmount < 0.0) {
        std::cout << ui::error("[!] Fiscal year, category, description, and non-negative amount are required.") << "\n";
        logAuditAction("BUDGET_ENTRY_SUBMIT_FAILED", "N/A", admin.username);
        waitForEnter();
        return;
    }

    std::vector<BudgetEntry> entries;
    if (!loadBudgetEntries(entries)) {
        std::cout << ui::error("[!] Unable to open budget entries file.") << "\n";
        logAuditAction("BUDGET_ENTRY_SUBMIT_FAILED", "N/A", admin.username);
        waitForEnter();
        return;
    }

    BudgetEntry entry;
    entry.entryId = generateNextBudgetEntryId(entries);
    entry.entryType = entryType;
    entry.fiscalYear = fiscalYear;
    entry.category = category;
    entry.allocatedAmount = allocatedAmount;
    entry.description = description;
    entry.createdAt = getCurrentTimestamp();
    entry.createdBy = admin.username;
    entry.status = "pending_approval";
    entry.publishedAt = "";
    entries.push_back(entry);

    if (!saveBudgetEntries(entries)) {
        std::cout << ui::error("[!] Failed to save budget entry.") << "\n";
        logAuditAction("BUDGET_ENTRY_SUBMIT_FAILED", "N/A", admin.username);
        waitForEnter();
        return;
    }

    if (!createBudgetApprovalRequests(entry.entryId)) {
        std::cout << ui::error("[!] Failed to create budget approval requests.") << "\n";
        logAuditAction("BUDGET_ENTRY_SUBMIT_FAILED", entry.entryId, admin.username);
        waitForEnter();
        return;
    }

    const int chainIndex = appendBlockchainAction("BUDGET_SUBMIT", entry.entryId, admin.username);
    logAuditAction("BUDGET_ENTRY_SUBMIT", entry.entryId, admin.username, chainIndex);

    std::cout << ui::success("[+] Budget entry submitted for unanimous approval.") << "\n";
    std::cout << "  Entry ID     : " << entry.entryId << "\n";
    std::cout << "  Entry Type   : " << entry.entryType << "\n";
    std::cout << "  Fiscal Year  : " << entry.fiscalYear << "\n";
    std::cout << "  Category     : " << entry.category << "\n";
    std::cout << "  Amount (PHP) : " << std::fixed << std::setprecision(2) << entry.allocatedAmount << "\n";
    waitForEnter();
}

void viewPendingBudgetEntriesForApprover(const Admin& admin) {
    clearScreen();
    ui::printSectionTitle("PENDING BUDGET ENTRIES");

    std::vector<BudgetApproval> approvals;
    std::vector<BudgetEntry> entries;

    if (!loadBudgetApprovals(approvals) || !loadBudgetEntries(entries)) {
        std::cout << ui::error("[!] Unable to open budget approval datasets.") << "\n";
        waitForEnter();
        return;
    }

    std::map<std::string, BudgetEntry> entryLookup;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        entryLookup[entries[i].entryId] = entries[i];
    }

    std::vector<BudgetApproval> directRows;
    std::vector<BudgetApproval> delegatedRows;
    collectPendingBudgetRowsForDecision(approvals, admin.username, directRows, delegatedRows);

    std::vector<std::vector<std::string>> tableRows;
    for (std::size_t i = 0; i < directRows.size(); ++i) {
        std::map<std::string, BudgetEntry>::const_iterator entryIt = entryLookup.find(directRows[i].entryId);
        if (entryIt == entryLookup.end()) {
            continue;
        }

        std::ostringstream amountOut;
        amountOut << std::fixed << std::setprecision(2) << entryIt->second.allocatedAmount;

        tableRows.push_back({directRows[i].entryId,
                             directRows[i].role,
                             entryIt->second.entryType,
                             entryIt->second.fiscalYear,
                             entryIt->second.category,
                             amountOut.str(),
                             "Direct"});
    }

    for (std::size_t i = 0; i < delegatedRows.size(); ++i) {
        std::map<std::string, BudgetEntry>::const_iterator entryIt = entryLookup.find(delegatedRows[i].entryId);
        if (entryIt == entryLookup.end()) {
            continue;
        }

        std::ostringstream amountOut;
        amountOut << std::fixed << std::setprecision(2) << entryIt->second.allocatedAmount;

        tableRows.push_back({delegatedRows[i].entryId,
                             delegatedRows[i].role,
                             entryIt->second.entryType,
                             entryIt->second.fiscalYear,
                             entryIt->second.category,
                             amountOut.str(),
                             "Delegated"});
    }

    if (tableRows.empty()) {
        std::cout << ui::warning("[!] No pending budget approvals for your account.") << "\n";
        logAuditAction("VIEW_PENDING_BUDGET_APPROVALS_EMPTY", "N/A", admin.username);
        waitForEnter();
        return;
    }

    const std::vector<std::string> headers = {"Entry ID", "Role", "Type", "Fiscal Year", "Category", "Amount", "Via"};
    const std::vector<int> widths = {10, 24, 18, 12, 22, 12, 10};
    ui::printTableHeader(headers, widths);
    for (std::size_t i = 0; i < tableRows.size(); ++i) {
        ui::printTableRow(tableRows[i], widths);
    }
    ui::printTableFooter(widths);

    logAuditAction("VIEW_PENDING_BUDGET_APPROVALS", "MULTI", admin.username);
    waitForEnter();
}

void applyBudgetApprovalDecision(const Admin& admin, bool approve) {
    clearScreen();
    ui::printSectionTitle(approve ? "APPROVE BUDGET ENTRY" : "REJECT BUDGET ENTRY");

    std::vector<BudgetApproval> approvals;
    std::vector<BudgetEntry> entries;

    if (!loadBudgetApprovals(approvals) || !loadBudgetEntries(entries)) {
        std::cout << ui::error("[!] Unable to open budget approval datasets.") << "\n";
        logAuditAction("BUDGET_APPROVAL_FAILED", "N/A", admin.username);
        waitForEnter();
        return;
    }

    std::vector<BudgetApproval> directRows;
    std::vector<BudgetApproval> delegatedRows;
    collectPendingBudgetRowsForDecision(approvals, admin.username, directRows, delegatedRows);

    if (directRows.empty() && delegatedRows.empty()) {
        std::cout << ui::warning("[!] No pending budget approvals for your account.") << "\n";
        logAuditAction("BUDGET_APPROVAL_NOT_FOUND", "N/A", admin.username);
        waitForEnter();
        return;
    }

    std::cout << "\n" << ui::bold("Available Pending Budget Decisions") << "\n";
    const std::vector<std::string> pendingHeaders = {"Entry ID", "Role", "Created", "Via"};
    const std::vector<int> pendingWidths = {10, 24, 19, 12};
    ui::printTableHeader(pendingHeaders, pendingWidths);
    for (std::size_t i = 0; i < directRows.size(); ++i) {
        ui::printTableRow({directRows[i].entryId, directRows[i].role, directRows[i].createdAt, "Direct"}, pendingWidths);
    }
    for (std::size_t i = 0; i < delegatedRows.size(); ++i) {
        ui::printTableRow({delegatedRows[i].entryId, delegatedRows[i].role, delegatedRows[i].createdAt, "Delegated"}, pendingWidths);
    }
    ui::printTableFooter(pendingWidths);

    clearInputBuffer();
    std::string entryId;
    std::cout << "Entry ID: ";
    std::getline(std::cin, entryId);

    if (entryId.empty()) {
        std::cout << ui::error("[!] Entry ID is required.") << "\n";
        logAuditAction("BUDGET_APPROVAL_INPUT_ERROR", "N/A", admin.username);
        waitForEnter();
        return;
    }

    BudgetEntry selectedEntry;
    bool selectedEntryFound = false;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].entryId == entryId) {
            selectedEntry = entries[i];
            selectedEntryFound = true;
            break;
        }
    }

    if (!selectedEntryFound) {
        std::cout << ui::error("[!] Budget entry not found.") << "\n";
        logAuditAction("BUDGET_APPROVAL_NOT_FOUND", entryId, admin.username);
        waitForEnter();
        return;
    }

    std::string ownerUsername;
    if (!resolvePendingBudgetDecisionOwner(approvals, admin.username, entryId, ownerUsername)) {
        std::cout << ui::error("[!] No pending approval found for this budget entry and account.") << "\n";
        logAuditAction("BUDGET_APPROVAL_NOT_FOUND", entryId, admin.username);
        waitForEnter();
        return;
    }

    std::vector<BudgetApproval> projectedApprovals = approvals;
    for (std::size_t i = 0; i < projectedApprovals.size(); ++i) {
        if (projectedApprovals[i].entryId == entryId &&
            projectedApprovals[i].approverUsername == ownerUsername &&
            projectedApprovals[i].status == "pending") {
            projectedApprovals[i].status = approve ? "approved" : "rejected";
            break;
        }
    }

    bool projectedHasAny = false;
    bool projectedHasPending = false;
    bool projectedHasRejected = false;
    int projectedApproved = 0;
    int projectedTotal = 0;

    for (std::size_t i = 0; i < projectedApprovals.size(); ++i) {
        if (projectedApprovals[i].entryId != entryId) {
            continue;
        }

        projectedHasAny = true;
        projectedTotal++;
        if (projectedApprovals[i].status == "pending") {
            projectedHasPending = true;
        } else if (projectedApprovals[i].status == "rejected") {
            projectedHasRejected = true;
        } else if (projectedApprovals[i].status == "approved") {
            projectedApproved++;
        }
    }

    std::string projectedStatus = "pending_approval";
    if (projectedHasRejected) {
        projectedStatus = "rejected";
    } else if (projectedHasAny && !projectedHasPending) {
        projectedStatus = "published";
    }

    const std::string projectedProgress = "[" + std::string(static_cast<std::size_t>(projectedApproved), '#') +
                                         std::string(static_cast<std::size_t>(std::max(0, projectedTotal - projectedApproved)), '-') +
                                         "] " + std::to_string(projectedApproved) + "/" + std::to_string(projectedTotal);
    std::cout << "\nProjected consensus after this action: "
              << projectedProgress << " -> " << ui::consensusStatus(projectedStatus) << "\n";

    std::string confirmQuestion = approve
        ? "Are you sure you want to approve this budget entry?"
        : "Are you sure you want to reject this budget entry?";
    if (projectedStatus == "published") {
        if (!confirmBudgetEntryPublishGuardrail(selectedEntry, admin.username)) {
            waitForEnter();
            return;
        }
        confirmQuestion = "All required approvals are complete. Publish this budget entry now?";
    }

    if (!ui::confirmAction(confirmQuestion,
                           approve ? "Confirm Approval" : "Confirm Rejection",
                           "Cancel")) {
        logAuditAction("BUDGET_APPROVAL_CANCELLED", entryId, admin.username);
        std::cout << ui::warning("[!] Budget decision cancelled.") << "\n";
        waitForEnter();
        return;
    }

    std::string actingOnBehalfOf;
    if (ownerUsername != admin.username) {
        actingOnBehalfOf = ownerUsername;
    }

    bool updated = false;
    const std::string now = getCurrentTimestamp();
    for (std::size_t i = 0; i < approvals.size(); ++i) {
        if (approvals[i].entryId == entryId && approvals[i].approverUsername == ownerUsername && approvals[i].status == "pending") {
            approvals[i].status = approve ? "approved" : "rejected";
            approvals[i].decidedAt = now;

            std::string note;
            std::cout << "  Add a note (optional, press Enter to skip): ";
            std::getline(std::cin, note);
            approvals[i].note = note;

            updated = true;
            break;
        }
    }

    if (!updated) {
        std::cout << ui::error("[!] No pending approval found for this budget entry and account.") << "\n";
        logAuditAction("BUDGET_APPROVAL_NOT_FOUND", entryId, admin.username);
        waitForEnter();
        return;
    }

    if (!saveBudgetApprovals(approvals)) {
        std::cout << ui::error("[!] Unable to save budget approvals.") << "\n";
        logAuditAction("BUDGET_APPROVAL_FAILED", entryId, admin.username);
        waitForEnter();
        return;
    }

    bool hasAny = false;
    bool hasPending = false;
    bool hasRejected = false;
    for (std::size_t i = 0; i < approvals.size(); ++i) {
        if (approvals[i].entryId != entryId) {
            continue;
        }

        hasAny = true;
        if (approvals[i].status == "pending") {
            hasPending = true;
        } else if (approvals[i].status == "rejected") {
            hasRejected = true;
        }
    }

    std::string nextStatus = "pending_approval";
    if (hasRejected) {
        nextStatus = "rejected";
    } else if (hasAny && !hasPending) {
        nextStatus = "published";
    }

    bool entryUpdated = false;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].entryId == entryId) {
            entries[i].status = nextStatus;
            if (nextStatus == "published") {
                entries[i].publishedAt = getCurrentTimestamp();
                if (!applyPublishedBudgetChange(entries[i])) {
                    std::cout << ui::error("[!] Budget decision saved, but published budget update failed.") << "\n";
                    logAuditAction("BUDGET_PUBLISH_FAILED", entryId, admin.username);
                    waitForEnter();
                    return;
                }
            }
            entryUpdated = true;
            break;
        }
    }

    if (!entryUpdated || !saveBudgetEntries(entries)) {
        std::cout << ui::error("[!] Decision saved, but budget entry status update failed.") << "\n";
        logAuditAction("BUDGET_STATUS_SYNC_FAILED", entryId, admin.username);
        waitForEnter();
        return;
    }

    std::cout << "\nDecision Saved: " << ui::consensusStatus(approve ? "approved" : "rejected") << "\n";
    std::cout << "Consensus Result: " << ui::consensusStatus(nextStatus) << "\n";

    if (!actingOnBehalfOf.empty()) {
        std::cout << "  " << ui::muted("(Acting on behalf of " + actingOnBehalfOf + " via delegation)") << "\n";
    }

    const std::string auditActor = actingOnBehalfOf.empty()
        ? admin.username
        : admin.username + " (on behalf of " + actingOnBehalfOf + ")";

    if (approve) {
        const std::string action = actingOnBehalfOf.empty() ? "BUDGET_APPROVE" : "BUDGET_APPROVE_DELEGATED";
        const std::string auditAction = actingOnBehalfOf.empty() ? "BUDGET_ENTRY_APPROVED" : "BUDGET_ENTRY_APPROVED_DELEGATED";
        const int chainIndex = appendBlockchainAction(action, entryId, admin.username);
        logAuditAction(auditAction, entryId, auditActor, chainIndex);
    } else {
        const std::string action = actingOnBehalfOf.empty() ? "BUDGET_REJECT" : "BUDGET_REJECT_DELEGATED";
        const std::string auditAction = actingOnBehalfOf.empty() ? "BUDGET_ENTRY_REJECTED" : "BUDGET_ENTRY_REJECTED_DELEGATED";
        const int chainIndex = appendBlockchainAction(action, entryId, admin.username);
        logAuditAction(auditAction, entryId, auditActor, chainIndex);
    }

    if (nextStatus == "published") {
        const int publishChainIndex = appendBlockchainAction("BUDGET_PUBLISH", entryId, admin.username);
        logAuditAction("BUDGET_ENTRY_PUBLISH", entryId, auditActor, publishChainIndex);
    }

    waitForEnter();
}

void loadMonthlyPublishedDocumentMetrics(const std::string& yearMonth,
                                         std::map<std::string, double>& amountByCategory,
                                         int& publishedCount) {
    amountByCategory.clear();
    publishedCount = 0;

    std::ifstream file;
    if (!openInputFileWithFallback(file, DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK)) {
        return;
    }

    std::string line;
    bool firstLine = true;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        if (firstLine) {
            firstLine = false;
            if (line.find("docID|") == 0) {
                continue;
            }
        }

        const std::vector<std::string> tokens = splitPipe(line);
        if (tokens.size() < 8) {
            continue;
        }

        const std::string status = toLowerCopy(tokens.size() > 7 ? tokens[7] : "");
        const std::string uploadDate = tokens.size() > 5 ? tokens[5] : "";
        if (status != "published" || !isTimestampInYearMonth(uploadDate, yearMonth)) {
            continue;
        }

        const std::string category = tokens.size() > 13 && !tokens[13].empty() ? tokens[13] : (tokens.size() > 2 ? tokens[2] : "");
        double amount = 0.0;
        if (tokens.size() > 14) {
            std::stringstream amountIn(tokens[14]);
            amountIn >> amount;
        }

        if (!category.empty()) {
            amountByCategory[category] += amount;
        }
        publishedCount++;
    }
}

void loadMonthlyDecisionCounts(const std::string& yearMonth,
                               const std::string& filePrimary,
                               const std::string& fileFallback,
                               int& approvedCount,
                               int& rejectedCount) {
    approvedCount = 0;
    rejectedCount = 0;

    std::ifstream file;
    if (!openInputFileWithFallback(file, filePrimary, fileFallback)) {
        return;
    }

    std::string line;
    bool firstLine = true;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        if (firstLine) {
            firstLine = false;
            if (line.find("docID|") == 0 || line.find("entryID|") == 0) {
                continue;
            }
        }

        const std::vector<std::string> tokens = splitPipe(line);
        if (tokens.size() < 6) {
            continue;
        }

        const std::string status = toLowerCopy(tokens[3]);
        const std::string decidedAt = tokens[5];
        if (!isTimestampInYearMonth(decidedAt, yearMonth)) {
            continue;
        }

        if (status == "approved") {
            approvedCount++;
        } else if (status == "rejected") {
            rejectedCount++;
        }
    }
}

std::vector<MonthlyVarianceRow> buildMonthlyVarianceRows(const std::vector<BudgetRow>& budgets,
                                                         const std::map<std::string, double>& monthlyActuals) {
    std::vector<MonthlyVarianceRow> rows;

    for (std::size_t i = 0; i < budgets.size(); ++i) {
        MonthlyVarianceRow row;
        row.category = budgets[i].category;
        row.allocated = budgets[i].amount;

        std::map<std::string, double>::const_iterator found = monthlyActuals.find(row.category);
        row.monthlyActual = (found != monthlyActuals.end()) ? found->second : 0.0;
        row.variance = row.allocated - row.monthlyActual;
        row.utilization = row.allocated > 0.0 ? (row.monthlyActual / row.allocated) * 100.0 : 0.0;
        rows.push_back(row);
    }

    std::sort(rows.begin(), rows.end(), [](const MonthlyVarianceRow& left, const MonthlyVarianceRow& right) {
        return left.category < right.category;
    });
    return rows;
}

bool writeMonthlyTransparencyTxt(const std::string& outputPath,
                                 const std::string& yearMonth,
                                 const Admin& admin,
                                 const std::vector<MonthlyVarianceRow>& varianceRows,
                                 int publishedDocCount,
                                 int docApprovedCount,
                                 int docRejectedCount,
                                 int budgetApprovedCount,
                                 int budgetRejectedCount) {
    std::ofstream out(outputPath);
    if (!out.is_open()) {
        return false;
    }

    out << "PROCURECHAIN MONTHLY TRANSPARENCY REPORT\n";
    out << "Period       : " << yearMonth << "\n";
    out << "Generated at : " << getCurrentTimestamp() << "\n";
    out << "Generated by : " << admin.fullName << " (" << admin.username << ")\n";
    out << "==============================================================\n";
    out << "Published documents: " << publishedDocCount << "\n";
    out << "Document approvals : " << docApprovedCount << "\n";
    out << "Document rejections: " << docRejectedCount << "\n";
    out << "Budget approvals   : " << budgetApprovedCount << "\n";
    out << "Budget rejections  : " << budgetRejectedCount << "\n";
    out << "==============================================================\n";
    out << "Category|Allocated|MonthlyActualPublished|Variance|UtilizationPercent\n";

    for (std::size_t i = 0; i < varianceRows.size(); ++i) {
        out << varianceRows[i].category << "|"
            << std::fixed << std::setprecision(2) << varianceRows[i].allocated << "|"
            << std::fixed << std::setprecision(2) << varianceRows[i].monthlyActual << "|"
            << std::fixed << std::setprecision(2) << varianceRows[i].variance << "|"
            << std::fixed << std::setprecision(1) << varianceRows[i].utilization << "\n";
    }

    out.flush();
    return out.good();
}

bool writeMonthlyTransparencyCsv(const std::string& outputPath,
                                 const std::string& yearMonth,
                                 const std::vector<MonthlyVarianceRow>& varianceRows,
                                 int publishedDocCount,
                                 int docApprovedCount,
                                 int docRejectedCount,
                                 int budgetApprovedCount,
                                 int budgetRejectedCount) {
    std::ofstream out(outputPath);
    if (!out.is_open()) {
        return false;
    }

    out << "period,category,allocated,monthlyActualPublished,variance,utilizationPercent,publishedDocs,documentApprovals,documentRejections,budgetApprovals,budgetRejections\n";
    for (std::size_t i = 0; i < varianceRows.size(); ++i) {
        out << yearMonth << ","
            << varianceRows[i].category << ","
            << std::fixed << std::setprecision(2) << varianceRows[i].allocated << ","
            << std::fixed << std::setprecision(2) << varianceRows[i].monthlyActual << ","
            << std::fixed << std::setprecision(2) << varianceRows[i].variance << ","
            << std::fixed << std::setprecision(1) << varianceRows[i].utilization << ","
            << publishedDocCount << ","
            << docApprovedCount << ","
            << docRejectedCount << ","
            << budgetApprovedCount << ","
            << budgetRejectedCount << "\n";
    }

    out.flush();
    return out.good();
}

void generateMonthlyTransparencyReportForAdmin(const Admin& admin) {
    clearScreen();
    ui::printSectionTitle("MONTHLY TRANSPARENCY REPORT");

    clearInputBuffer();
    std::string yearMonth;
    std::cout << "Reporting period (YYYY-MM): ";
    std::getline(std::cin, yearMonth);
    yearMonth = trimCopy(yearMonth);

    if (!isYearMonthTextValid(yearMonth)) {
        std::cout << ui::error("[!] Invalid period format. Use YYYY-MM.") << "\n";
        logAuditAction("MONTHLY_TRANSPARENCY_REPORT_FAILED", "BAD_PERIOD", admin.username);
        waitForEnter();
        return;
    }

    std::vector<BudgetRow> budgets;
    if (!loadBudgetRows(budgets) || budgets.empty()) {
        std::cout << ui::error("[!] No published budget allocations available.") << "\n";
        logAuditAction("MONTHLY_TRANSPARENCY_REPORT_FAILED", yearMonth, admin.username);
        waitForEnter();
        return;
    }

    std::map<std::string, double> monthlyActualByCategory;
    int publishedDocCount = 0;
    loadMonthlyPublishedDocumentMetrics(yearMonth, monthlyActualByCategory, publishedDocCount);

    int docApprovedCount = 0;
    int docRejectedCount = 0;
    loadMonthlyDecisionCounts(yearMonth,
                              APPROVALS_FILE_PATH_PRIMARY,
                              APPROVALS_FILE_PATH_FALLBACK,
                              docApprovedCount,
                              docRejectedCount);

    int budgetApprovedCount = 0;
    int budgetRejectedCount = 0;
    loadMonthlyDecisionCounts(yearMonth,
                              BUDGET_APPROVALS_FILE_PATH_PRIMARY,
                              BUDGET_APPROVALS_FILE_PATH_FALLBACK,
                              budgetApprovedCount,
                              budgetRejectedCount);

    const std::vector<MonthlyVarianceRow> varianceRows = buildMonthlyVarianceRows(budgets, monthlyActualByCategory);

    std::string stamp = getCurrentTimestamp();
    for (std::size_t i = 0; i < stamp.size(); ++i) {
        if (stamp[i] == ' ' || stamp[i] == ':') {
            stamp[i] = '_';
        }
    }

    std::string ymToken = yearMonth;
    for (std::size_t i = 0; i < ymToken.size(); ++i) {
        if (ymToken[i] == '-') {
            ymToken[i] = '_';
        }
    }

    const std::string txtName = "monthly_transparency_" + sanitizeFileToken(ymToken + "_" + stamp) + ".txt";
    const std::string csvName = "monthly_transparency_" + sanitizeFileToken(ymToken + "_" + stamp) + ".csv";

    if (!isSafeExportFileName(txtName) || !isSafeExportFileName(csvName)) {
        std::cout << ui::error("[!] Unable to build safe export filenames.") << "\n";
        logAuditAction("MONTHLY_TRANSPARENCY_REPORT_FAILED", yearMonth, admin.username);
        waitForEnter();
        return;
    }

    const std::string txtPath = chooseExportPath(txtName);
    const std::string csvPath = chooseExportPath(csvName);
    if (txtPath.empty() || csvPath.empty()) {
        std::cout << ui::error("[!] Unable to resolve report export paths.") << "\n";
        logAuditAction("MONTHLY_TRANSPARENCY_REPORT_FAILED", yearMonth, admin.username);
        waitForEnter();
        return;
    }

    if (!writeMonthlyTransparencyTxt(txtPath,
                                     yearMonth,
                                     admin,
                                     varianceRows,
                                     publishedDocCount,
                                     docApprovedCount,
                                     docRejectedCount,
                                     budgetApprovedCount,
                                     budgetRejectedCount) ||
        !writeMonthlyTransparencyCsv(csvPath,
                                     yearMonth,
                                     varianceRows,
                                     publishedDocCount,
                                     docApprovedCount,
                                     docRejectedCount,
                                     budgetApprovedCount,
                                     budgetRejectedCount)) {
        std::cout << ui::error("[!] Failed to generate transparency report files.") << "\n";
        logAuditAction("MONTHLY_TRANSPARENCY_REPORT_FAILED", yearMonth, admin.username);
        waitForEnter();
        return;
    }

    std::cout << ui::success("[+] Monthly transparency report generated.") << "\n";
    std::cout << "  Period              : " << yearMonth << "\n";
    std::cout << "  Published documents : " << publishedDocCount << "\n";
    std::cout << "  Document approvals  : " << docApprovedCount << "\n";
    std::cout << "  Document rejections : " << docRejectedCount << "\n";
    std::cout << "  Budget approvals    : " << budgetApprovedCount << "\n";
    std::cout << "  Budget rejections   : " << budgetRejectedCount << "\n";
    std::cout << "  TXT report          : " << txtPath << "\n";
    std::cout << "  CSV report          : " << csvPath << "\n";

    logAuditAction("MONTHLY_TRANSPARENCY_REPORT_EXPORT", yearMonth, admin.username);
    waitForEnter();
}
} // namespace

bool getPendingBudgetApprovalIdsForApprover(const std::string& approverUsername,
                                            std::vector<std::string>& outEntryIds) {
    return getPendingBudgetApprovalIdsForApproverInternal(approverUsername, outEntryIds);
}

void ensureBudgetConsensusFilesExist() {
    ensureFileWithHeader(BUDGET_ENTRIES_FILE_PATH_PRIMARY,
                         BUDGET_ENTRIES_FILE_PATH_FALLBACK,
                         "entryID|entryType|fiscalYear|category|allocatedAmount|description|createdAt|createdBy|status|publishedAt");
    ensureFileWithHeader(BUDGET_APPROVALS_FILE_PATH_PRIMARY,
                         BUDGET_APPROVALS_FILE_PATH_FALLBACK,
                         "entryID|approverUsername|role|status|createdAt|decidedAt|note");

    std::vector<BudgetEntry> entries;
    if (loadBudgetEntries(entries)) {
        saveBudgetEntries(entries);
    }

    std::vector<BudgetApproval> approvals;
    if (loadBudgetApprovals(approvals)) {
        saveBudgetApprovals(approvals);
    }

    seedBudgetsIfEmpty();
    seedBudgetConsensusIfEmpty();
}

void viewBudgetAllocations(const std::string& actor) {
    clearScreen();
    ui::printSectionTitle("PROCUREMENT BUDGET ALLOCATIONS (PUBLISHED)");

    std::vector<BudgetRow> rows;
    if (!loadBudgetRows(rows)) {
        std::cout << ui::error("[!] Unable to open budgets file.") << "\n";
        logAuditAction("VIEW_BUDGETS_FAILED", "N/A", actor);
        waitForEnter();
        return;
    }

    if (rows.empty()) {
        std::cout << "\n" << ui::warning("[!] No published budget entries available.") << "\n";
        logAuditAction("VIEW_BUDGETS_EMPTY", "N/A", actor);
        waitForEnter();
        return;
    }

    double totalBudget = 0.0;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        totalBudget += rows[i].amount;
    }

    std::sort(rows.begin(), rows.end(), [](const BudgetRow& a, const BudgetRow& b) {
        return a.amount > b.amount;
    });

    printBudgetTable(rows);
    printBudgetChart(rows);

    std::cout << "\n" << ui::success("Total Published Budget: PHP ") << std::fixed << std::setprecision(2) << totalBudget << "\n";
    logAuditAction("VIEW_BUDGETS", "MULTI", actor);

    waitForEnter();
}

void viewBudgetVarianceReport(const std::string& actor) {
    clearScreen();
    ui::printSectionTitle("BUDGET VARIANCE REPORT");

    std::vector<BudgetRow> budgets;
    if (!loadBudgetRows(budgets)) {
        std::cout << ui::error("[!] Unable to open budgets file.") << "\n";
        logAuditAction("BUDGET_VARIANCE_FAILED", "N/A", actor);
        waitForEnter();
        return;
    }

    if (budgets.empty()) {
        std::cout << ui::warning("[!] No budget rows available.") << "\n";
        waitForEnter();
        return;
    }

    const std::map<std::string, double> actuals = loadActualSpendByCategory();
    std::vector<VarianceRow> report;

    for (std::size_t i = 0; i < budgets.size(); ++i) {
        VarianceRow row;
        row.category = budgets[i].category;
        row.allocated = budgets[i].amount;

        std::map<std::string, double>::const_iterator found = actuals.find(row.category);
        row.actual = (found != actuals.end()) ? found->second : 0.0;
        row.variance = row.allocated - row.actual;
        row.utilization = row.allocated > 0.0 ? (row.actual / row.allocated) * 100.0 : 0.0;
        report.push_back(row);
    }

    const std::vector<std::string> headers = {"Category", "Allocated", "Actual", "Variance", "Utilization"};
    const std::vector<int> widths = {24, 12, 12, 12, 12};
    ui::printTableHeader(headers, widths);

    for (std::size_t i = 0; i < report.size(); ++i) {
        std::ostringstream allocated;
        std::ostringstream actual;
        std::ostringstream variance;
        std::ostringstream utilization;

        allocated << std::fixed << std::setprecision(2) << report[i].allocated;
        actual << std::fixed << std::setprecision(2) << report[i].actual;
        variance << std::fixed << std::setprecision(2) << report[i].variance;
        utilization << std::fixed << std::setprecision(1) << report[i].utilization << "%";

        ui::printTableRow({report[i].category, allocated.str(), actual.str(), variance.str(), utilization.str()}, widths);
    }
    ui::printTableFooter(widths);

    double maxActual = 1.0;
    for (std::size_t i = 0; i < report.size(); ++i) {
        if (report[i].actual > maxActual) {
            maxActual = report[i].actual;
        }
    }

    std::cout << "\n" << ui::bold("Actual Spend By Category") << "\n";
    for (std::size_t i = 0; i < report.size(); ++i) {
        ui::printBar(report[i].category, report[i].actual, maxActual, 24);
    }

    logAuditAction("BUDGET_VARIANCE_VIEW", "MULTI", actor);
    waitForEnter();
}

void manageBudgetsForAdmin(const Admin& admin) {
    int choice = -1;

    do {
        clearScreen();
        ui::printSectionTitle("ADMIN BUDGET CONSENSUS WORKSPACE");
        std::cout << "  " << ui::info("[1]") << " View Published Budget Allocations\n";
        if (canSubmitBudgetEntry(admin)) {
            std::cout << "  " << ui::info("[2]") << " Submit New Budget Entry\n";
            std::cout << "  " << ui::info("[3]") << " Submit Allocation Update\n";
        }
        if (isBudgetApproverRole(admin)) {
            std::cout << "  " << ui::info("[4]") << " View Pending Budget Entries\n";
            std::cout << "  " << ui::info("[5]") << " Approve Budget Entry\n";
            std::cout << "  " << ui::info("[6]") << " Reject Budget Entry\n";
        }
        std::cout << "  " << ui::info("[7]") << " View Budget Variance Report\n";
        std::cout << "  " << ui::info("[8]") << " Generate Monthly Transparency Report (TXT + CSV)\n";
        std::cout << "  " << ui::info("[0]") << " Back to Admin Dashboard\n";
        std::cout << ui::muted("--------------------------------------------------------------") << "\n";
        std::cout << "  Enter your choice: ";

        std::cin >> choice;
        if (std::cin.fail()) {
            std::cin.clear();
            clearInputBuffer();
            std::cout << "\n" << ui::warning("[!] Invalid input. Please enter a number from the menu.") << "\n";
            waitForEnter();
            continue;
        }

        if (choice == 0) {
            break;
        }

        if (choice == 1) {
            viewBudgetAllocations(admin.username);
            continue;
        }

        if (choice == 7) {
            viewBudgetVarianceReport(admin.username);
            continue;
        }

        if (choice == 8) {
            generateMonthlyTransparencyReportForAdmin(admin);
            continue;
        }

        if ((choice == 2 || choice == 3) && !canSubmitBudgetEntry(admin)) {
            std::cout << ui::warning("[!] Access denied for this role.") << "\n";
            logAuditAction("ACCESS_DENIED_BUDGET_SUBMIT", "N/A", admin.username);
            waitForEnter();
            continue;
        }

        if ((choice == 4 || choice == 5 || choice == 6) && !isBudgetApproverRole(admin)) {
            std::cout << ui::warning("[!] Access denied for this role.") << "\n";
            logAuditAction("ACCESS_DENIED_BUDGET_APPROVE", "N/A", admin.username);
            waitForEnter();
            continue;
        }

        if (choice == 2) {
            submitBudgetEntry(admin, "NEW");
            continue;
        }

        if (choice == 3) {
            submitBudgetEntry(admin, "ALLOCATION_CHANGE");
            continue;
        }

        if (choice == 4) {
            viewPendingBudgetEntriesForApprover(admin);
            continue;
        }

        if (choice == 5) {
            applyBudgetApprovalDecision(admin, true);
            continue;
        }

        if (choice == 6) {
            applyBudgetApprovalDecision(admin, false);
            continue;
        }

        std::cout << "\n" << ui::warning("[!] Invalid menu choice.") << "\n";
        waitForEnter();
    } while (choice != 0);
}

