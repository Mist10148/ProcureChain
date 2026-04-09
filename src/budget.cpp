#include "../include/budget.h"

#include "../include/audit.h"
#include "../include/auth.h"
#include "../include/blockchain.h"
#include "../include/ui.h"

#include <algorithm>
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

    std::ofstream createPrimary(primaryPath);
    if (createPrimary.is_open()) {
        createPrimary << headerLine << '\n';
        return true;
    }

    std::ofstream createFallback(fallbackPath);
    if (createFallback.is_open()) {
        createFallback << headerLine << '\n';
        return true;
    }

    return false;
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
    std::ofstream writer(resolveDataPath(BUDGETS_FILE_PATH_PRIMARY, BUDGETS_FILE_PATH_FALLBACK));
    if (!writer.is_open()) {
        return false;
    }

    writer << "category|amount\n";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        writer << rows[i].category << '|' << std::fixed << std::setprecision(2) << rows[i].amount << '\n';
    }
    writer.flush();

    return true;
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

    return entry.entryId + "|" + entry.entryType + "|" + entry.fiscalYear + "|" + entry.category + "|" +
           amountOut.str() + "|" + entry.description + "|" + entry.createdAt + "|" + entry.createdBy + "|" +
           entry.status + "|" + entry.publishedAt;
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
    std::ofstream writer(resolveDataPath(BUDGET_ENTRIES_FILE_PATH_PRIMARY, BUDGET_ENTRIES_FILE_PATH_FALLBACK));
    if (!writer.is_open()) {
        return false;
    }

    writer << "entryID|entryType|fiscalYear|category|allocatedAmount|description|createdAt|createdBy|status|publishedAt\n";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        writer << serializeBudgetEntry(rows[i]) << '\n';
    }

    writer.flush();
    return true;
}

BudgetApproval parseBudgetApprovalTokens(const std::vector<std::string>& tokens) {
    BudgetApproval row;
    row.entryId = tokens.size() > 0 ? tokens[0] : "";
    row.approverUsername = tokens.size() > 1 ? tokens[1] : "";
    row.role = tokens.size() > 2 ? tokens[2] : "";
    row.status = tokens.size() > 3 ? tokens[3] : "pending";
    row.createdAt = tokens.size() > 4 ? tokens[4] : "";
    row.decidedAt = tokens.size() > 5 ? tokens[5] : "";
    return row;
}

std::string serializeBudgetApproval(const BudgetApproval& row) {
    return row.entryId + "|" + row.approverUsername + "|" + row.role + "|" + row.status + "|" + row.createdAt + "|" + row.decidedAt;
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
    std::ofstream writer(resolveDataPath(BUDGET_APPROVALS_FILE_PATH_PRIMARY, BUDGET_APPROVALS_FILE_PATH_FALLBACK));
    if (!writer.is_open()) {
        return false;
    }

    writer << "entryID|approverUsername|role|status|createdAt|decidedAt\n";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        writer << serializeBudgetApproval(rows[i]) << '\n';
    }

    writer.flush();
    return true;
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

    const std::string budgetOfficer = findActiveAdminUsernameByRole("Budget Officer");
    const std::string municipalAdmin = findActiveAdminUsernameByRole("Municipal Administrator");

    if (budgetOfficer.empty() || municipalAdmin.empty()) {
        return false;
    }

    const std::string now = getCurrentTimestamp();

    BudgetApproval a;
    a.entryId = entryId;
    a.approverUsername = budgetOfficer;
    a.role = "Budget Officer";
    a.status = "pending";
    a.createdAt = now;
    approvals.push_back(a);

    a.entryId = entryId;
    a.approverUsername = municipalAdmin;
    a.role = "Municipal Administrator";
    a.status = "pending";
    a.createdAt = now;
    approvals.push_back(a);

    return saveBudgetApprovals(approvals);
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

    std::vector<std::vector<std::string>> tableRows;
    for (std::size_t i = 0; i < approvals.size(); ++i) {
        if (approvals[i].approverUsername != admin.username || approvals[i].status != "pending") {
            continue;
        }

        std::map<std::string, BudgetEntry>::const_iterator entryIt = entryLookup.find(approvals[i].entryId);
        if (entryIt == entryLookup.end()) {
            continue;
        }

        std::ostringstream amountOut;
        amountOut << std::fixed << std::setprecision(2) << entryIt->second.allocatedAmount;

        tableRows.push_back({approvals[i].entryId,
                             entryIt->second.entryType,
                             entryIt->second.fiscalYear,
                             entryIt->second.category,
                             amountOut.str(),
                             approvals[i].status});
    }

    if (tableRows.empty()) {
        std::cout << ui::warning("[!] No pending budget approvals for your account.") << "\n";
        logAuditAction("VIEW_PENDING_BUDGET_APPROVALS_EMPTY", "N/A", admin.username);
        waitForEnter();
        return;
    }

    const std::vector<std::string> headers = {"Entry ID", "Type", "Fiscal Year", "Category", "Amount", "Status"};
    const std::vector<int> widths = {10, 18, 12, 22, 12, 10};
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

    std::vector<BudgetApproval> pendingForUser;
    for (std::size_t i = 0; i < approvals.size(); ++i) {
        if (approvals[i].approverUsername == admin.username && approvals[i].status == "pending") {
            pendingForUser.push_back(approvals[i]);
        }
    }

    if (pendingForUser.empty()) {
        std::cout << ui::warning("[!] No pending budget approvals for your account.") << "\n";
        logAuditAction("BUDGET_APPROVAL_NOT_FOUND", "N/A", admin.username);
        waitForEnter();
        return;
    }

    std::cout << "\n" << ui::bold("Available Pending Budget Entry IDs") << "\n";
    for (std::size_t i = 0; i < pendingForUser.size(); ++i) {
        std::cout << "  - " << pendingForUser[i].entryId << "\n";
    }

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

    bool updated = false;
    const std::string now = getCurrentTimestamp();
    for (std::size_t i = 0; i < approvals.size(); ++i) {
        if (approvals[i].entryId == entryId && approvals[i].approverUsername == admin.username && approvals[i].status == "pending") {
            approvals[i].status = approve ? "approved" : "rejected";
            approvals[i].decidedAt = now;
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

    if (approve) {
        const int chainIndex = appendBlockchainAction("BUDGET_APPROVE", entryId, admin.username);
        logAuditAction("BUDGET_APPROVE", entryId, admin.username, chainIndex);
    } else {
        const int chainIndex = appendBlockchainAction("BUDGET_REJECT", entryId, admin.username);
        logAuditAction("BUDGET_REJECT", entryId, admin.username, chainIndex);
    }

    if (nextStatus == "published") {
        const int publishChainIndex = appendBlockchainAction("BUDGET_PUBLISH", entryId, admin.username);
        logAuditAction("BUDGET_PUBLISH", entryId, admin.username, publishChainIndex);
    }

    waitForEnter();
}
} // namespace

void ensureBudgetConsensusFilesExist() {
    ensureFileWithHeader(BUDGET_ENTRIES_FILE_PATH_PRIMARY,
                         BUDGET_ENTRIES_FILE_PATH_FALLBACK,
                         "entryID|entryType|fiscalYear|category|allocatedAmount|description|createdAt|createdBy|status|publishedAt");
    ensureFileWithHeader(BUDGET_APPROVALS_FILE_PATH_PRIMARY,
                         BUDGET_APPROVALS_FILE_PATH_FALLBACK,
                         "entryID|approverUsername|role|status|createdAt|decidedAt");

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
