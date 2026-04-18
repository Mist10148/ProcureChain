#include "../include/analytics.h"

#include "../include/audit.h"
#include "../include/auth.h"
#include "../include/blockchain.h"
#include "../include/storage_utils.h"
#include "../include/ui.h"
#include "../include/verification.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {
const std::string DOCUMENTS_FILE_PATH_PRIMARY = "data/documents.txt";
const std::string DOCUMENTS_FILE_PATH_FALLBACK = "../data/documents.txt";
const std::string APPROVALS_FILE_PATH_PRIMARY = "data/approvals.txt";
const std::string APPROVALS_FILE_PATH_FALLBACK = "../data/approvals.txt";
const std::string APPROVAL_RULES_FILE_PATH_PRIMARY = "data/approval_rules.txt";
const std::string APPROVAL_RULES_FILE_PATH_FALLBACK = "../data/approval_rules.txt";
const std::string BUDGETS_FILE_PATH_PRIMARY = "data/budgets.txt";
const std::string BUDGETS_FILE_PATH_FALLBACK = "../data/budgets.txt";
const std::string AUDIT_FILE_PATH_PRIMARY = "data/audit_log.txt";
const std::string AUDIT_FILE_PATH_FALLBACK = "../data/audit_log.txt";

struct DocumentRow {
    std::string docId;
    std::string status;
    std::string dateUploaded;
    std::string category;
    std::string department;
    std::string budgetCategory;
    double amount;
};

struct ApprovalRow {
    std::string docId;
    std::string role;
    std::string status;
    std::string createdAt;
    std::string decidedAt;
};

struct ApprovalRuleRow {
    std::string category;
    std::vector<std::string> requiredRoles;
    int maxDecisionDays;
};

struct ComplianceCheckResult {
    std::string checkName;
    bool passed;
    std::string detail;
};

struct ComplianceViolationRow {
    std::string type;
    std::string docId;
    std::string detail;
};

struct DepartmentWorkloadMetric {
    int pendingApprovals;
    int overduePendingApprovals;
    int decisionsInWindow;
    int uploadsInWindow;
    double decisionDaysTotal;
    int decisionDaysCount;
};

struct AuditRow {
    std::string timestamp;
    std::string action;
};

struct BudgetRow {
    std::string category;
    double amount;
};

struct AnalyticsWindow {
    bool allTime;
    std::string fromDate;
    std::string toDate;
    std::string label;
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

std::vector<std::string> splitPipe(const std::string& line) {
    return storage::splitPipeRow(line);
}

std::string trimCopy(const std::string& value) {
    const std::string whitespace = " \t\r\n";
    const size_t start = value.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return "";
    }

    const size_t end = value.find_last_not_of(whitespace);
    return value.substr(start, end - start + 1);
}

std::vector<std::string> splitRolesCsv(const std::string& text) {
    std::vector<std::string> roles;
    std::stringstream parser(text);
    std::string token;

    while (std::getline(parser, token, ',')) {
        token = trimCopy(token);
        if (!token.empty()) {
            roles.push_back(token);
        }
    }

    return roles;
}

bool roleExistsInList(const std::vector<std::string>& roles, const std::string& targetRole) {
    for (std::size_t i = 0; i < roles.size(); ++i) {
        if (roles[i] == targetRole) {
            return true;
        }
    }

    return false;
}

std::string toLowerCopy(std::string value) {
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] >= 'A' && value[i] <= 'Z') {
            value[i] = static_cast<char>(value[i] + ('a' - 'A'));
        }
    }
    return value;
}

void clearInputBuffer() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

bool isDateTextValid(const std::string& value) {
    return storage::isDateTextValidStrict(value, false);
}

bool parseDateText(const std::string& dateText, std::tm& tmValue) {
    return storage::parseDateTextStrict(dateText, tmValue);
}

bool parseDateTimeText(const std::string& dateTimeText, std::tm& tmValue) {
    return storage::parseDateTimeTextStrict(dateTimeText, tmValue);
}

std::string formatDate(std::tm tmValue) {
    char buffer[11];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tmValue);
    return std::string(buffer);
}

std::string shiftDateByDays(const std::string& dateText, int dayOffset) {
    std::tm tmValue = {};
    if (!parseDateText(dateText, tmValue)) {
        return dateText;
    }

    std::time_t base = std::mktime(&tmValue);
    if (base == static_cast<std::time_t>(-1)) {
        return dateText;
    }

    base += static_cast<std::time_t>(dayOffset) * 24 * 60 * 60;

    std::tm adjusted;
    localtime_s(&adjusted, &base);
    return formatDate(adjusted);
}

std::string todayDate() {
    return getCurrentTimestamp().substr(0, 10);
}

std::string extractDate(const std::string& text) {
    if (text.size() >= 10) {
        return text.substr(0, 10);
    }
    return "";
}

int daysBetweenDates(const std::string& fromDate, const std::string& toDate) {
    std::tm fromTm = {};
    std::tm toTm = {};
    if (!parseDateText(fromDate, fromTm) || !parseDateText(toDate, toTm)) {
        return 0;
    }

    std::time_t fromTime = std::mktime(&fromTm);
    std::time_t toTime = std::mktime(&toTm);
    if (fromTime == static_cast<std::time_t>(-1) || toTime == static_cast<std::time_t>(-1)) {
        return 0;
    }

    const double deltaSeconds = std::difftime(toTime, fromTime);
    return static_cast<int>(deltaSeconds / (24.0 * 60.0 * 60.0));
}

bool computeDecisionDurationDays(const ApprovalRow& row, int& outDays) {
    outDays = 0;

    std::tm createdTm = {};
    std::tm decidedTm = {};
    if (parseDateTimeText(row.createdAt, createdTm) && parseDateTimeText(row.decidedAt, decidedTm)) {
        std::time_t createdTime = std::mktime(&createdTm);
        std::time_t decidedTime = std::mktime(&decidedTm);
        if (createdTime != static_cast<std::time_t>(-1) && decidedTime != static_cast<std::time_t>(-1) && decidedTime >= createdTime) {
            const double deltaSeconds = std::difftime(decidedTime, createdTime);
            outDays = static_cast<int>(deltaSeconds / (24.0 * 60.0 * 60.0));
            return true;
        }
    }

    const std::string createdDate = extractDate(row.createdAt);
    const std::string decidedDate = extractDate(row.decidedAt);
    if (!createdDate.empty() && !decidedDate.empty()) {
        const int deltaDays = daysBetweenDates(createdDate, decidedDate);
        if (deltaDays >= 0) {
            outDays = deltaDays;
            return true;
        }
    }

    return false;
}

bool matchesWindow(const std::string& dateText, const AnalyticsWindow& window) {
    if (!window.allTime && dateText.empty()) {
        return false;
    }

    if (window.allTime) {
        return true;
    }

    return dateText >= window.fromDate && dateText <= window.toDate;
}

bool loadDocuments(std::vector<DocumentRow>& rows) {
    std::ifstream file;
    if (!openInputFileWithFallback(file, DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK)) {
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
            if (line.find("docID|") == 0) {
                continue;
            }
        }

        const std::vector<std::string> tokens = splitPipe(line);
        if (tokens.empty()) {
            continue;
        }

        DocumentRow row;
        row.docId = tokens.size() > 0 ? tokens[0] : "";
        row.category = tokens.size() > 2 ? tokens[2] : "";
        if (tokens.size() >= 15) {
            row.department = tokens[4];
            row.dateUploaded = tokens[5];
            row.status = tokens[7];
            row.budgetCategory = tokens[13].empty() ? row.category : tokens[13];
        } else {
            row.department = tokens.size() > 3 ? tokens[3] : "General";
            row.dateUploaded = tokens.size() > 4 ? tokens[4] : "";
            row.status = tokens.size() > 6 ? tokens[6] : "";
            row.budgetCategory = tokens.size() > 8 && !tokens[8].empty() ? tokens[8] : row.category;
        }

        if (row.department.empty()) {
            row.department = "General";
        }
        row.amount = 0.0;

        if (tokens.size() >= 15) {
            std::stringstream amountIn(tokens[14]);
            amountIn >> row.amount;
        } else if (tokens.size() > 9) {
            std::stringstream amountIn(tokens[9]);
            amountIn >> row.amount;
        }

        if (!row.docId.empty()) {
            rows.push_back(row);
        }
    }

    return true;
}

bool loadApprovals(std::vector<ApprovalRow>& rows) {
    std::ifstream file;
    if (!openInputFileWithFallback(file, APPROVALS_FILE_PATH_PRIMARY, APPROVALS_FILE_PATH_FALLBACK)) {
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
            if (line.find("docID|") == 0) {
                continue;
            }
        }

        const std::vector<std::string> tokens = splitPipe(line);
        if (tokens.size() < 4) {
            continue;
        }

        ApprovalRow row;
        row.docId = tokens.size() > 0 ? tokens[0] : "";
        row.role = tokens.size() > 2 ? tokens[2] : "";
        row.status = tokens.size() > 3 ? tokens[3] : "pending";
        row.createdAt = tokens.size() > 4 ? tokens[4] : "";
        row.decidedAt = tokens.size() > 5 ? tokens[5] : "";
        if (!row.docId.empty()) {
            rows.push_back(row);
        }
    }

    return true;
}

bool loadAuditRows(std::vector<AuditRow>& rows) {
    std::ifstream file;
    if (!openInputFileWithFallback(file, AUDIT_FILE_PATH_PRIMARY, AUDIT_FILE_PATH_FALLBACK)) {
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
            if (toLowerCopy(line).find("timestamp|") == 0) {
                continue;
            }
        }

        const std::vector<std::string> tokens = splitPipe(line);
        if (tokens.size() < 2) {
            continue;
        }

        AuditRow row;
        row.timestamp = tokens[0];
        row.action = tokens[1];
        rows.push_back(row);
    }

    return true;
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
            std::stringstream amountIn(tokens[1]);
            amountIn >> row.amount;
        }

        if (!row.category.empty()) {
            rows.push_back(row);
        }
    }

    return true;
}

ApprovalRuleRow buildDefaultApprovalRule() {
    ApprovalRuleRow rule;
    rule.category = "DEFAULT";
    rule.requiredRoles.push_back("Budget Officer");
    rule.requiredRoles.push_back("Municipal Administrator");
    rule.maxDecisionDays = 7;
    return rule;
}

bool loadApprovalRules(std::vector<ApprovalRuleRow>& rows) {
    std::ifstream file;
    if (!openInputFileWithFallback(file, APPROVAL_RULES_FILE_PATH_PRIMARY, APPROVAL_RULES_FILE_PATH_FALLBACK)) {
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

        ApprovalRuleRow row = buildDefaultApprovalRule();
        row.category = trimCopy(tokens[0]);
        if (row.category.empty()) {
            row.category = "DEFAULT";
        }

        if (tokens.size() > 1) {
            const std::vector<std::string> parsedRoles = splitRolesCsv(tokens[1]);
            if (!parsedRoles.empty()) {
                row.requiredRoles = parsedRoles;
            }
        }

        if (tokens.size() > 2) {
            std::stringstream in(tokens[2]);
            int parsedDays = 0;
            in >> parsedDays;
            if (!in.fail() && parsedDays > 0) {
                row.maxDecisionDays = parsedDays;
            }
        }

        rows.push_back(row);
    }

    return true;
}

ApprovalRuleRow resolveApprovalRuleForCategory(const std::vector<ApprovalRuleRow>& rules, const std::string& category) {
    ApprovalRuleRow fallback = buildDefaultApprovalRule();
    const std::string key = toLowerCopy(trimCopy(category));

    for (std::size_t i = 0; i < rules.size(); ++i) {
        if (toLowerCopy(trimCopy(rules[i].category)) == "default") {
            fallback = rules[i];
            break;
        }
    }

    for (std::size_t i = 0; i < rules.size(); ++i) {
        if (toLowerCopy(trimCopy(rules[i].category)) == key) {
            return rules[i];
        }
    }

    return fallback;
}

std::vector<std::string> buildDateRange(const std::string& fromDate, const std::string& toDate, std::size_t maxDays) {
    std::vector<std::string> days;
    if (!isDateTextValid(fromDate) || !isDateTextValid(toDate) || fromDate > toDate) {
        return days;
    }

    std::string cursor = fromDate;
    while (cursor <= toDate && days.size() < maxDays) {
        days.push_back(cursor);
        cursor = shiftDateByDays(cursor, 1);
    }

    return days;
}

std::vector<std::pair<std::string, double>> buildDailySeries(const std::vector<std::string>& sourceDates,
                                                             const AnalyticsWindow& window,
                                                             std::size_t maxAllTimeDays) {
    std::map<std::string, double> counts;
    for (std::size_t i = 0; i < sourceDates.size(); ++i) {
        const std::string day = extractDate(sourceDates[i]);
        if (!matchesWindow(day, window)) {
            continue;
        }
        counts[day] += 1.0;
    }

    std::vector<std::pair<std::string, double>> series;
    if (counts.empty()) {
        return series;
    }

    if (window.allTime) {
        std::vector<std::string> keys;
        for (std::map<std::string, double>::const_iterator it = counts.begin(); it != counts.end(); ++it) {
            keys.push_back(it->first);
        }

        if (keys.size() > maxAllTimeDays) {
            keys.erase(keys.begin(), keys.begin() + static_cast<long long>(keys.size() - maxAllTimeDays));
        }

        for (std::size_t i = 0; i < keys.size(); ++i) {
            series.push_back(std::make_pair(keys[i], counts[keys[i]]));
        }

        return series;
    }

    const std::vector<std::string> days = buildDateRange(window.fromDate, window.toDate, 90);
    for (std::size_t i = 0; i < days.size(); ++i) {
        const std::map<std::string, double>::const_iterator found = counts.find(days[i]);
        const double value = (found != counts.end()) ? found->second : 0.0;
        series.push_back(std::make_pair(days[i], value));
    }

    return series;
}

void printWindowLabel(const AnalyticsWindow& window) {
    if (window.allTime) {
        std::cout << "  " << ui::muted("Window: All Time") << "\n";
        return;
    }

    std::cout << "  " << ui::muted("Window: ") << window.fromDate << " to " << window.toDate << "\n";
}

bool parseBlockLine(const std::string& line,
                    std::string& index,
                    std::string& timestamp,
                    std::string& action,
                    std::string& documentId,
                    std::string& actor,
                    std::string& previousHash,
                    std::string& currentHash) {
    std::stringstream parser(line);
    std::getline(parser, index, '|');
    std::getline(parser, timestamp, '|');
    std::getline(parser, action, '|');
    std::getline(parser, documentId, '|');
    std::getline(parser, actor, '|');
    std::getline(parser, previousHash, '|');
    std::getline(parser, currentHash, '|');

    return !index.empty() && !timestamp.empty() && !action.empty() &&
           !documentId.empty() && !actor.empty() && !previousHash.empty() && !currentHash.empty();
}

bool validateSingleChain(const std::string& primaryPath, const std::string& fallbackPath) {
    std::ifstream file;
    if (!openInputFileWithFallback(file, primaryPath, fallbackPath)) {
        return false;
    }

    std::string line;
    std::getline(file, line); // Skip header.

    std::string expectedPreviousHash = "0000";

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        std::string index;
        std::string timestamp;
        std::string action;
        std::string documentId;
        std::string actor;
        std::string previousHash;
        std::string currentHash;

        if (!parseBlockLine(line, index, timestamp, action, documentId, actor, previousHash, currentHash)) {
            return false;
        }

        if (previousHash != expectedPreviousHash) {
            return false;
        }

        const std::string hashSource = index + "|" + timestamp + "|" + action + "|" + documentId + "|" + actor + "|" + previousHash;
        if (computeSimpleHash(hashSource) != currentHash) {
            return false;
        }

        expectedPreviousHash = currentHash;
    }

    return true;
}

std::string readFullFileWithFallback(const std::string& primaryPath, const std::string& fallbackPath) {
    std::ifstream file;
    if (!openInputFileWithFallback(file, primaryPath, fallbackPath)) {
        return "";
    }

    std::ostringstream out;
    out << file.rdbuf();
    return out.str();
}

int countDocumentHashMismatches() {
    std::ifstream file;
    if (!openInputFileWithFallback(file, DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK)) {
        return 0;
    }

    std::string line;
    bool firstLine = true;
    int mismatches = 0;

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

        std::string storedHash;
        std::string computed;

        if (tokens.size() >= 15) {
            storedHash = tokens[8];
            const std::string filePath = tokens[11];

            if (!filePath.empty() && std::filesystem::exists(filePath)) {
                computed = computeFileHashSha256(filePath);
            } else {
                const std::string tags = tokens.size() > 17 ? tokens[17] : "";
                const std::string metadataSource = tokens[0] + "|" + tokens[1] + "|" + tokens[2] + "|" + tokens[3] + "|" +
                                                   tokens[4] + "|" + tokens[5] + "|" + tokens[6] + "|" + tags;
                computed = computeSimpleHash(metadataSource);
            }
        } else {
            storedHash = tokens[7];
            const std::string metadataSource = tokens[0] + "|" + tokens[1] + "|" + tokens[2] + "|" + "" + "|" +
                                               tokens[3] + "|" + tokens[4] + "|" + tokens[5];
            computed = computeSimpleHash(metadataSource);
        }

        if (storedHash != computed) {
            mismatches++;
        }
    }

    return mismatches;
}

int countStalePendingApprovals(int staleDays) {
    std::vector<ApprovalRow> approvals;
    if (!loadApprovals(approvals)) {
        return 0;
    }

    const std::string staleBefore = shiftDateByDays(todayDate(), -staleDays);
    int staleCount = 0;

    for (std::size_t i = 0; i < approvals.size(); ++i) {
        if (toLowerCopy(approvals[i].status) != "pending") {
            continue;
        }

        const std::string day = extractDate(approvals[i].createdAt);
        if (!day.empty() && day < staleBefore) {
            staleCount++;
        }
    }

    return staleCount;
}

void renderSeriesTable(const std::vector<std::pair<std::string, double>>& series,
                       const std::string& columnName,
                       const std::string& valueName,
                       int maxRows) {
    if (series.empty()) {
        return;
    }

    const std::vector<std::string> headers = {columnName, valueName};
    const std::vector<int> widths = {12, 10};
    ui::printTableHeader(headers, widths);

    int startIndex = 0;
    if (static_cast<int>(series.size()) > maxRows) {
        startIndex = static_cast<int>(series.size()) - maxRows;
    }

    for (int i = startIndex; i < static_cast<int>(series.size()); ++i) {
        std::ostringstream valueOut;
        valueOut << std::fixed << std::setprecision(0) << series[static_cast<std::size_t>(i)].second;
        ui::printTableRow({series[static_cast<std::size_t>(i)].first, valueOut.str()}, widths);
    }

    ui::printTableFooter(widths);
}

void runPagedTableView(const std::string& title,
                       const std::vector<std::string>& breadcrumb,
                       const std::vector<std::string>& headers,
                       const std::vector<int>& widths,
                       const std::vector<std::vector<std::string>>& rows) {
    if (rows.empty()) {
        return;
    }

    const int pageSize = ui::tablePageSize();
    const int totalPages = static_cast<int>((rows.size() + static_cast<std::size_t>(pageSize) - 1) /
                                            static_cast<std::size_t>(pageSize));

    if (totalPages <= 1) {
        clearScreen();
        ui::printSectionTitle(title);
        ui::printBreadcrumb(breadcrumb);
        ui::printTableHeader(headers, widths);
        for (std::size_t i = 0; i < rows.size(); ++i) {
            ui::printTableRow(rows[i], widths);
        }
        ui::printTableFooter(widths);
        waitForEnter();
        return;
    }

    int page = 0;
    clearInputBuffer();

    while (true) {
        const std::size_t start = static_cast<std::size_t>(page * pageSize);
        const std::size_t end = std::min(rows.size(), start + static_cast<std::size_t>(pageSize));

        clearScreen();
        ui::printSectionTitle(title);
        ui::printBreadcrumb(breadcrumb);
        std::cout << "  " << ui::muted("Layout: ") << ui::layoutModeLabel() << "\n";

        ui::printTableHeader(headers, widths);
        for (std::size_t i = start; i < end; ++i) {
            ui::printTableRow(rows[i], widths);
        }
        ui::printTableFooter(widths);

        std::cout << "\n  " << ui::muted("Page ") << (page + 1) << "/" << totalPages;
        std::cout << "  " << ui::info("[N]") << " Next  " << ui::info("[P]") << " Previous  " << ui::info("[Q]") << " Close\n";
        std::cout << "  Enter command: ";

        std::string command;
        std::getline(std::cin, command);
        if (command.empty()) {
            return;
        }

        const char key = static_cast<char>(std::tolower(static_cast<unsigned char>(command[0])));
        if (key == 'n') {
            if (page + 1 < totalPages) {
                page++;
            }
        } else if (key == 'p') {
            if (page > 0) {
                page--;
            }
        } else {
            return;
        }
    }
}

bool promptPagedDetails(const std::string& label) {
    std::cout << "\n  " << ui::info("[1]") << " Open paged detail table for " << label << "\n";
    std::cout << "  " << ui::info("[0]") << " Continue\n";
    std::cout << "  Enter your choice: ";

    int choice = -1;
    std::cin >> choice;
    if (std::cin.fail()) {
        std::cin.clear();
        clearInputBuffer();
        return false;
    }

    return choice == 1;
}

void renderApprovalFunnelAndTrend(const Admin& admin, const AnalyticsWindow& window) {
    clearScreen();
    ui::printSectionTitle("ANALYTICS - APPROVAL FUNNEL AND THROUGHPUT");
    ui::printBreadcrumb({"ADMIN", "ANALYTICS HUB", "APPROVALS"});
    printWindowLabel(window);

    std::vector<DocumentRow> docs;
    std::vector<ApprovalRow> approvals;
    if (!loadDocuments(docs) || !loadApprovals(approvals)) {
        std::cout << "\n" << ui::error("[!] Unable to load analytics datasets.") << "\n";
        waitForEnter();
        return;
    }

    int pendingCount = 0;
    int approvedCount = 0;
    int rejectedCount = 0;

    for (std::size_t i = 0; i < docs.size(); ++i) {
        if (!matchesWindow(docs[i].dateUploaded, window)) {
            continue;
        }

        const std::string status = toLowerCopy(docs[i].status);
        if (status == "pending" || status == "pending_approval") {
            pendingCount++;
        } else if (status == "approved" || status == "published") {
            approvedCount++;
        } else if (status == "rejected" || status == "denied") {
            rejectedCount++;
        }
    }

    const int total = pendingCount + approvedCount + rejectedCount;
    ui::printKpiTiles({
        std::make_pair("Total tracked docs", std::to_string(total)),
        std::make_pair("Pending", std::to_string(pendingCount)),
        std::make_pair("Approved/Published", std::to_string(approvedCount)),
        std::make_pair("Rejected", std::to_string(rejectedCount))
    });

    const std::vector<std::string> headers = {"Stage", "Count", "Share"};
    const std::vector<int> widths = ui::isCompactLayout() ? std::vector<int>{16, 8, 8} : std::vector<int>{22, 10, 10};
    ui::printTableHeader(headers, widths);

    const double denominator = total > 0 ? static_cast<double>(total) : 1.0;
    std::ostringstream pendingShare;
    std::ostringstream approvedShare;
    std::ostringstream rejectedShare;
    pendingShare << std::fixed << std::setprecision(1) << (100.0 * pendingCount / denominator) << "%";
    approvedShare << std::fixed << std::setprecision(1) << (100.0 * approvedCount / denominator) << "%";
    rejectedShare << std::fixed << std::setprecision(1) << (100.0 * rejectedCount / denominator) << "%";

    ui::printTableRow({"Pending", std::to_string(pendingCount), pendingShare.str()}, widths);
    ui::printTableRow({"Approved/Published", std::to_string(approvedCount), approvedShare.str()}, widths);
    ui::printTableRow({"Rejected", std::to_string(rejectedCount), rejectedShare.str()}, widths);
    ui::printTableFooter(widths);

    std::cout << "\n" << ui::bold("Approval Funnel") << "\n";
    const double maxStage = static_cast<double>(std::max(std::max(pendingCount, approvedCount), rejectedCount));
    const int barWidth = ui::preferredBarWidth();
    ui::printBar("Pending", static_cast<double>(pendingCount), maxStage <= 0.0 ? 1.0 : maxStage, barWidth, ui::consensusColorCode("pending"));
    ui::printBar("Approved/Published", static_cast<double>(approvedCount), maxStage <= 0.0 ? 1.0 : maxStage, barWidth, ui::consensusColorCode("approved"));
    ui::printBar("Rejected", static_cast<double>(rejectedCount), maxStage <= 0.0 ? 1.0 : maxStage, barWidth, ui::consensusColorCode("rejected"));

    std::vector<std::string> decidedDates;
    for (std::size_t i = 0; i < approvals.size(); ++i) {
        const std::string status = toLowerCopy(approvals[i].status);
        if (status == "approved" || status == "rejected") {
            if (matchesWindow(extractDate(approvals[i].decidedAt), window)) {
                decidedDates.push_back(approvals[i].decidedAt);
            }
        }
    }

    const std::vector<std::pair<std::string, double>> series = buildDailySeries(decidedDates, window, 14);
    if (!series.empty()) {
        std::vector<std::string> labels;
        std::vector<double> values;
        for (std::size_t i = 0; i < series.size(); ++i) {
            labels.push_back(series[i].first);
            values.push_back(series[i].second);
        }

        ui::printLineChart("Decisions Per Day", labels, values, ui::preferredChartHeight(), ui::preferredChartWidth());
        renderSeriesTable(series, "Date", "Decisions", ui::tablePageSize());
    } else {
        std::cout << "\n" << ui::muted("[i] No decided approvals in selected window.") << "\n";
    }

    logAuditAction("ANALYTICS_APPROVAL_FUNNEL", window.label, admin.username);
    waitForEnter();
}

void renderDepartmentWorkload(const Admin& admin, const AnalyticsWindow& window) {
    clearScreen();
    ui::printSectionTitle("ANALYTICS - DEPARTMENT WORKLOAD");
    ui::printBreadcrumb({"ADMIN", "ANALYTICS HUB", "DEPARTMENT WORKLOAD"});
    printWindowLabel(window);

    std::vector<DocumentRow> docs;
    std::vector<ApprovalRow> approvals;
    if (!loadDocuments(docs) || !loadApprovals(approvals)) {
        std::cout << "\n" << ui::error("[!] Unable to load analytics datasets.") << "\n";
        waitForEnter();
        return;
    }

    std::map<std::string, std::string> docDepartment;
    std::map<std::string, DepartmentWorkloadMetric> metrics;

    for (std::size_t i = 0; i < docs.size(); ++i) {
        const std::string department = docs[i].department.empty() ? "General" : docs[i].department;
        docDepartment[docs[i].docId] = department;

        if (metrics.find(department) == metrics.end()) {
            metrics[department] = DepartmentWorkloadMetric();
        }

        if (matchesWindow(docs[i].dateUploaded, window)) {
            metrics[department].uploadsInWindow += 1;
        }
    }

    const std::string today = todayDate();
    for (std::size_t i = 0; i < approvals.size(); ++i) {
        std::map<std::string, std::string>::const_iterator docFound = docDepartment.find(approvals[i].docId);
        const std::string department = (docFound != docDepartment.end()) ? docFound->second : "General";

        if (metrics.find(department) == metrics.end()) {
            metrics[department] = DepartmentWorkloadMetric();
        }

        DepartmentWorkloadMetric& metric = metrics[department];
        const std::string status = toLowerCopy(approvals[i].status);

        if (status == "pending") {
            metric.pendingApprovals += 1;

            const std::string createdDate = extractDate(approvals[i].createdAt);
            if (!createdDate.empty() && daysBetweenDates(createdDate, today) > 7) {
                metric.overduePendingApprovals += 1;
            }
            continue;
        }

        if (status == "approved" || status == "rejected") {
            if (!matchesWindow(extractDate(approvals[i].decidedAt), window)) {
                continue;
            }

            metric.decisionsInWindow += 1;

            int decisionDays = 0;
            if (computeDecisionDurationDays(approvals[i], decisionDays)) {
                metric.decisionDaysTotal += static_cast<double>(decisionDays);
                metric.decisionDaysCount += 1;
            }
        }
    }

    std::vector<std::pair<std::string, DepartmentWorkloadMetric>> ordered;
    for (std::map<std::string, DepartmentWorkloadMetric>::const_iterator it = metrics.begin(); it != metrics.end(); ++it) {
        const DepartmentWorkloadMetric& metric = it->second;
        if (metric.pendingApprovals == 0 &&
            metric.overduePendingApprovals == 0 &&
            metric.decisionsInWindow == 0 &&
            metric.uploadsInWindow == 0) {
            continue;
        }

        ordered.push_back(std::make_pair(it->first, metric));
    }

    if (ordered.empty()) {
        std::cout << "\n" << ui::warning("[!] No department workload rows in selected context.") << "\n";
        waitForEnter();
        return;
    }

    std::sort(ordered.begin(), ordered.end(), [](const std::pair<std::string, DepartmentWorkloadMetric>& left,
                                                 const std::pair<std::string, DepartmentWorkloadMetric>& right) {
        if (left.second.pendingApprovals != right.second.pendingApprovals) {
            return left.second.pendingApprovals > right.second.pendingApprovals;
        }

        if (left.second.decisionsInWindow != right.second.decisionsInWindow) {
            return left.second.decisionsInWindow > right.second.decisionsInWindow;
        }

        return left.first < right.first;
    });

    int totalPending = 0;
    int totalOverdue = 0;
    int totalDecisions = 0;
    int totalUploads = 0;
    double totalDecisionDays = 0.0;
    int totalDecisionCount = 0;

    for (std::size_t i = 0; i < ordered.size(); ++i) {
        totalPending += ordered[i].second.pendingApprovals;
        totalOverdue += ordered[i].second.overduePendingApprovals;
        totalDecisions += ordered[i].second.decisionsInWindow;
        totalUploads += ordered[i].second.uploadsInWindow;
        totalDecisionDays += ordered[i].second.decisionDaysTotal;
        totalDecisionCount += ordered[i].second.decisionDaysCount;
    }

    std::ostringstream avgOverallOut;
    const double avgOverall = totalDecisionCount > 0 ? (totalDecisionDays / static_cast<double>(totalDecisionCount)) : 0.0;
    avgOverallOut << std::fixed << std::setprecision(1) << avgOverall;

    ui::printKpiTiles({
        std::make_pair("Departments tracked", std::to_string(ordered.size())),
        std::make_pair("Pending approvals", std::to_string(totalPending)),
        std::make_pair("Overdue pending (>7d)", std::to_string(totalOverdue)),
        std::make_pair("Throughput (decisions)", std::to_string(totalDecisions)),
        std::make_pair("Avg decision days", avgOverallOut.str()),
        std::make_pair("Docs uploaded in window", std::to_string(totalUploads))
    });

    const std::vector<std::string> headers = {"Department", "Pending", "Overdue", "Decisions", "Avg Days", "Uploads"};
    const std::vector<int> widths = ui::isCompactLayout() ? std::vector<int>{18, 8, 8, 9, 9, 8}
                                                           : std::vector<int>{24, 10, 10, 12, 10, 10};
    ui::printTableHeader(headers, widths);

    std::vector<std::vector<std::string>> tableRows;
    double maxPending = 1.0;
    std::vector<std::string> deptLabels;
    std::vector<double> throughputSeries;

    for (std::size_t i = 0; i < ordered.size(); ++i) {
        const DepartmentWorkloadMetric& metric = ordered[i].second;

        if (metric.pendingApprovals > maxPending) {
            maxPending = static_cast<double>(metric.pendingApprovals);
        }

        std::ostringstream avgOut;
        const double avgDays = metric.decisionDaysCount > 0
            ? (metric.decisionDaysTotal / static_cast<double>(metric.decisionDaysCount))
            : 0.0;
        avgOut << std::fixed << std::setprecision(1) << avgDays;

        std::vector<std::string> row = {
            ordered[i].first,
            std::to_string(metric.pendingApprovals),
            std::to_string(metric.overduePendingApprovals),
            std::to_string(metric.decisionsInWindow),
            avgOut.str(),
            std::to_string(metric.uploadsInWindow)
        };

        ui::printTableRow(row, widths);
        tableRows.push_back(row);

        deptLabels.push_back(ordered[i].first);
        throughputSeries.push_back(static_cast<double>(metric.decisionsInWindow));
    }

    ui::printTableFooter(widths);

    std::cout << "\n" << ui::bold("Pending Approval Load By Department") << "\n";
    for (std::size_t i = 0; i < ordered.size(); ++i) {
        ui::printBar(ordered[i].first,
                     static_cast<double>(ordered[i].second.pendingApprovals),
                     maxPending,
                     ui::preferredBarWidth(),
                     "36");
    }

    if (!deptLabels.empty()) {
        ui::printLineChart("Department Throughput Profile", deptLabels, throughputSeries,
                           ui::preferredChartHeight(), ui::preferredChartWidth());
    }

    if (tableRows.size() > static_cast<std::size_t>(ui::tablePageSize()) && promptPagedDetails("Department Workload")) {
        clearInputBuffer();
        runPagedTableView("DEPARTMENT WORKLOAD - PAGED DETAIL",
                          {"ADMIN", "ANALYTICS HUB", "DEPARTMENT WORKLOAD", "PAGED DETAIL"},
                          headers,
                          widths,
                          tableRows);
    }

    logAuditAction("ANALYTICS_DEPARTMENT_WORKLOAD", window.label, admin.username);
    waitForEnter();
}

void renderComplianceAuditReport(const Admin& admin, const AnalyticsWindow& window) {
    clearScreen();
    ui::printSectionTitle("ANALYTICS - COMPLIANCE AUDIT REPORT");
    ui::printBreadcrumb({"ADMIN", "ANALYTICS HUB", "COMPLIANCE"});
    printWindowLabel(window);

    std::vector<DocumentRow> docs;
    std::vector<ApprovalRow> approvals;
    std::vector<ApprovalRuleRow> rules;
    std::vector<BudgetRow> budgets;

    if (!loadDocuments(docs) || !loadApprovals(approvals) || !loadBudgetRows(budgets)) {
        std::cout << "\n" << ui::error("[!] Unable to load compliance datasets.") << "\n";
        waitForEnter();
        return;
    }

    if (!loadApprovalRules(rules) || rules.empty()) {
        rules.push_back(buildDefaultApprovalRule());
    }

    std::vector<DocumentRow> scopedDocs;
    std::map<std::string, DocumentRow> docById;
    for (std::size_t i = 0; i < docs.size(); ++i) {
        if (!matchesWindow(docs[i].dateUploaded, window)) {
            continue;
        }

        scopedDocs.push_back(docs[i]);
        docById[docs[i].docId] = docs[i];
    }

    if (scopedDocs.empty()) {
        std::cout << "\n" << ui::warning("[!] No documents found in selected window.") << "\n";
        waitForEnter();
        return;
    }

    std::map<std::string, std::vector<ApprovalRow>> approvalsByDoc;
    for (std::size_t i = 0; i < approvals.size(); ++i) {
        approvalsByDoc[approvals[i].docId].push_back(approvals[i]);
    }

    std::vector<ComplianceViolationRow> violations;

    int finalizedDocs = 0;
    int publishedDocs = 0;
    int approvalRowsCheckedForSla = 0;

    int consensusViolations = 0;
    int publicationViolations = 0;
    int slaViolations = 0;

    for (std::size_t i = 0; i < scopedDocs.size(); ++i) {
        const DocumentRow& doc = scopedDocs[i];
        const std::string docStatus = toLowerCopy(doc.status);
        const std::vector<ApprovalRow>& docApprovals = approvalsByDoc[doc.docId];
        const ApprovalRuleRow rule = resolveApprovalRuleForCategory(rules, doc.category);

        if (docStatus == "published" || docStatus == "approved") {
            finalizedDocs++;

            for (std::size_t r = 0; r < rule.requiredRoles.size(); ++r) {
                bool hasApprovedForRole = false;
                for (std::size_t a = 0; a < docApprovals.size(); ++a) {
                    const std::string status = toLowerCopy(docApprovals[a].status);
                    if (docApprovals[a].role == rule.requiredRoles[r] && status == "approved") {
                        hasApprovedForRole = true;
                        break;
                    }
                }

                if (!hasApprovedForRole) {
                    ComplianceViolationRow row;
                    row.type = "Unanimous Approval";
                    row.docId = doc.docId;
                    row.detail = "Missing required approval from role: " + rule.requiredRoles[r];
                    violations.push_back(row);
                    consensusViolations++;
                }
            }

            for (std::size_t a = 0; a < docApprovals.size(); ++a) {
                if (toLowerCopy(docApprovals[a].status) == "rejected") {
                    ComplianceViolationRow row;
                    row.type = "Unanimous Approval";
                    row.docId = doc.docId;
                    row.detail = "Finalized document still has rejected approval row.";
                    violations.push_back(row);
                    consensusViolations++;
                    break;
                }
            }
        }

        if (docStatus == "published") {
            publishedDocs++;

            for (std::size_t a = 0; a < docApprovals.size(); ++a) {
                if (toLowerCopy(docApprovals[a].status) == "rejected") {
                    ComplianceViolationRow row;
                    row.type = "Published Safety";
                    row.docId = doc.docId;
                    row.detail = "Published document has a rejected approval decision.";
                    violations.push_back(row);
                    publicationViolations++;
                    break;
                }
            }
        }

        for (std::size_t a = 0; a < docApprovals.size(); ++a) {
            const std::string status = toLowerCopy(docApprovals[a].status);
            if (status == "approved" || status == "rejected") {
                if (!matchesWindow(extractDate(docApprovals[a].decidedAt), window)) {
                    continue;
                }

                int days = 0;
                if (computeDecisionDurationDays(docApprovals[a], days)) {
                    approvalRowsCheckedForSla++;
                    if (days > rule.maxDecisionDays) {
                        ComplianceViolationRow row;
                        row.type = "SLA Breach";
                        row.docId = doc.docId;
                        row.detail = "Decision time " + std::to_string(days) + "d exceeded SLA " + std::to_string(rule.maxDecisionDays) + "d.";
                        violations.push_back(row);
                        slaViolations++;
                    }
                }
                continue;
            }

            if (status == "pending") {
                const std::string createdDate = extractDate(docApprovals[a].createdAt);
                if (createdDate.empty()) {
                    continue;
                }

                const int ageDays = daysBetweenDates(createdDate, todayDate());
                if (ageDays < 0) {
                    continue;
                }

                approvalRowsCheckedForSla++;
                if (ageDays > rule.maxDecisionDays) {
                    ComplianceViolationRow row;
                    row.type = "SLA Breach";
                    row.docId = doc.docId;
                    row.detail = "Pending age " + std::to_string(ageDays) + "d exceeded SLA " + std::to_string(rule.maxDecisionDays) + "d.";
                    violations.push_back(row);
                    slaViolations++;
                }
            }
        }
    }

    std::map<std::string, double> allocatedByCategory;
    for (std::size_t i = 0; i < budgets.size(); ++i) {
        allocatedByCategory[budgets[i].category] = budgets[i].amount;
    }

    std::map<std::string, double> actualByCategory;
    for (std::size_t i = 0; i < scopedDocs.size(); ++i) {
        const std::string status = toLowerCopy(scopedDocs[i].status);
        if (status != "approved" && status != "published") {
            continue;
        }

        actualByCategory[scopedDocs[i].budgetCategory] += scopedDocs[i].amount;
    }

    int budgetViolations = 0;
    double totalAllocated = 0.0;
    double totalActual = 0.0;

    for (std::map<std::string, double>::const_iterator it = allocatedByCategory.begin(); it != allocatedByCategory.end(); ++it) {
        totalAllocated += it->second;
    }

    for (std::map<std::string, double>::const_iterator it = actualByCategory.begin(); it != actualByCategory.end(); ++it) {
        totalActual += it->second;

        const double allocated = allocatedByCategory.find(it->first) != allocatedByCategory.end()
            ? allocatedByCategory[it->first]
            : 0.0;

        if (it->second > allocated + 0.005) {
            ComplianceViolationRow row;
            row.type = "Budget Overspend";
            row.docId = "N/A";

            std::ostringstream detail;
            detail << it->first << " actual=" << std::fixed << std::setprecision(2) << it->second
                   << " exceeds allocated=" << allocated;
            row.detail = detail.str();

            violations.push_back(row);
            budgetViolations++;
        }
    }

    if (totalActual > totalAllocated + 0.005) {
        ComplianceViolationRow row;
        row.type = "Budget Overspend";
        row.docId = "N/A";

        std::ostringstream detail;
        detail << "Overall actual=" << std::fixed << std::setprecision(2) << totalActual
               << " exceeds allocated=" << totalAllocated;
        row.detail = detail.str();

        violations.push_back(row);
        budgetViolations++;
    }

    std::vector<ComplianceCheckResult> checks;

    ComplianceCheckResult c1;
    c1.checkName = "Unanimous approvals enforced";
    c1.passed = consensusViolations == 0;
    c1.detail = std::to_string(finalizedDocs) + " finalized docs checked; violations=" + std::to_string(consensusViolations);
    checks.push_back(c1);

    ComplianceCheckResult c2;
    c2.checkName = "Rejected docs not published";
    c2.passed = publicationViolations == 0;
    c2.detail = std::to_string(publishedDocs) + " published docs checked; violations=" + std::to_string(publicationViolations);
    checks.push_back(c2);

    ComplianceCheckResult c3;
    c3.checkName = "Approval SLA compliance";
    c3.passed = slaViolations == 0;
    c3.detail = std::to_string(approvalRowsCheckedForSla) + " rows checked; violations=" + std::to_string(slaViolations);
    checks.push_back(c3);

    ComplianceCheckResult c4;
    c4.checkName = "Budget balance maintained";
    c4.passed = budgetViolations == 0;
    c4.detail = "allocated=" + std::to_string(totalAllocated) + ", actual=" + std::to_string(totalActual) + ", violations=" + std::to_string(budgetViolations);
    checks.push_back(c4);

    int passedChecks = 0;
    for (std::size_t i = 0; i < checks.size(); ++i) {
        if (checks[i].passed) {
            passedChecks++;
        }
    }

    std::set<std::string> affectedDocs;
    for (std::size_t i = 0; i < violations.size(); ++i) {
        if (!violations[i].docId.empty() && violations[i].docId != "N/A") {
            affectedDocs.insert(violations[i].docId);
        }
    }

    ui::printKpiTiles({
        std::make_pair("Checks passed", std::to_string(passedChecks) + "/" + std::to_string(checks.size())),
        std::make_pair("Total violations", std::to_string(violations.size())),
        std::make_pair("Affected documents", std::to_string(affectedDocs.size())),
        std::make_pair("Compliance status", passedChecks == static_cast<int>(checks.size()) ? "PASS" : "REVIEW REQUIRED")
    });

    const std::vector<std::string> checkHeaders = {"Check", "Result", "Detail"};
    const std::vector<int> checkWidths = ui::isCompactLayout() ? std::vector<int>{22, 8, 22}
                                                                : std::vector<int>{34, 10, 40};
    ui::printTableHeader(checkHeaders, checkWidths);
    for (std::size_t i = 0; i < checks.size(); ++i) {
        ui::printTableRow({checks[i].checkName,
                           checks[i].passed ? "PASS" : "FAIL",
                           checks[i].detail},
                          checkWidths);
    }
    ui::printTableFooter(checkWidths);

    if (!violations.empty()) {
        std::sort(violations.begin(), violations.end(), [](const ComplianceViolationRow& left, const ComplianceViolationRow& right) {
            if (left.type != right.type) {
                return left.type < right.type;
            }

            if (left.docId != right.docId) {
                return left.docId < right.docId;
            }

            return left.detail < right.detail;
        });

        std::cout << "\n" << ui::bold("Compliance Violations") << "\n";
        const std::vector<std::string> violationHeaders = {"Type", "Doc ID", "Detail"};
        const std::vector<int> violationWidths = ui::isCompactLayout() ? std::vector<int>{16, 10, 28}
                                                                        : std::vector<int>{20, 12, 52};
        ui::printTableHeader(violationHeaders, violationWidths);

        std::vector<std::vector<std::string>> violationRows;
        for (std::size_t i = 0; i < violations.size(); ++i) {
            std::vector<std::string> row = {violations[i].type, violations[i].docId, violations[i].detail};
            ui::printTableRow(row, violationWidths);
            violationRows.push_back(row);
        }
        ui::printTableFooter(violationWidths);

        if (violationRows.size() > static_cast<std::size_t>(ui::tablePageSize()) && promptPagedDetails("Compliance Violations")) {
            clearInputBuffer();
            runPagedTableView("COMPLIANCE VIOLATIONS - PAGED DETAIL",
                              {"ADMIN", "ANALYTICS HUB", "COMPLIANCE", "PAGED DETAIL"},
                              violationHeaders,
                              violationWidths,
                              violationRows);
        }
    } else {
        std::cout << "\n" << ui::success("[+] No compliance violations detected in selected window.") << "\n";
    }

    logAuditAction("ANALYTICS_COMPLIANCE_AUDIT", window.label, admin.username);
    waitForEnter();
}

void renderBudgetUtilization(const Admin& admin, const AnalyticsWindow& window) {
    clearScreen();
    ui::printSectionTitle("ANALYTICS - BUDGET UTILIZATION");
    ui::printBreadcrumb({"ADMIN", "ANALYTICS HUB", "BUDGETS"});
    printWindowLabel(window);

    std::vector<BudgetRow> budgets;
    std::vector<DocumentRow> docs;
    if (!loadBudgetRows(budgets) || !loadDocuments(docs)) {
        std::cout << "\n" << ui::error("[!] Unable to load analytics datasets.") << "\n";
        waitForEnter();
        return;
    }

    std::map<std::string, double> allocated;
    std::map<std::string, double> actual;

    for (std::size_t i = 0; i < budgets.size(); ++i) {
        allocated[budgets[i].category] = budgets[i].amount;
        actual[budgets[i].category] += 0.0;
    }

    for (std::size_t i = 0; i < docs.size(); ++i) {
        const std::string status = toLowerCopy(docs[i].status);
        if (status != "approved" && status != "published") {
            continue;
        }

        if (!matchesWindow(docs[i].dateUploaded, window)) {
            continue;
        }

        actual[docs[i].budgetCategory] += docs[i].amount;
        if (allocated.find(docs[i].budgetCategory) == allocated.end()) {
            allocated[docs[i].budgetCategory] = 0.0;
        }
    }

    std::vector<std::string> categories;
    for (std::map<std::string, double>::const_iterator it = allocated.begin(); it != allocated.end(); ++it) {
        categories.push_back(it->first);
    }

    std::sort(categories.begin(), categories.end());

    double totalAllocated = 0.0;
    double totalActual = 0.0;
    std::vector<double> utilizationValues;

    for (std::size_t i = 0; i < categories.size(); ++i) {
        totalAllocated += allocated[categories[i]];
        totalActual += actual[categories[i]];
        const double util = allocated[categories[i]] > 0.0 ? (100.0 * actual[categories[i]] / allocated[categories[i]]) : 0.0;
        utilizationValues.push_back(util);
    }

    const double totalUtilization = totalAllocated > 0.0 ? (100.0 * totalActual / totalAllocated) : 0.0;
    std::ostringstream utilOut;
    utilOut << std::fixed << std::setprecision(1) << totalUtilization << "%";

    std::ostringstream allocOut;
    allocOut << std::fixed << std::setprecision(2) << totalAllocated;

    std::ostringstream actualOut;
    actualOut << std::fixed << std::setprecision(2) << totalActual;

    ui::printKpiTiles({
        std::make_pair("Total allocated (PHP)", allocOut.str()),
        std::make_pair("Total actual (PHP)", actualOut.str()),
        std::make_pair("Overall utilization", utilOut.str())
    });

    const std::vector<std::string> headers = {"Category", "Allocated", "Actual", "Variance", "Utilization"};
    const std::vector<int> widths = ui::isCompactLayout() ? std::vector<int>{18, 10, 10, 10, 10} : std::vector<int>{24, 12, 12, 12, 12};
    ui::printTableHeader(headers, widths);

    std::vector<std::vector<std::string>> tableRows;

    double maxActual = 1.0;
    for (std::size_t i = 0; i < categories.size(); ++i) {
        if (actual[categories[i]] > maxActual) {
            maxActual = actual[categories[i]];
        }

        const double variance = allocated[categories[i]] - actual[categories[i]];
        const double util = allocated[categories[i]] > 0.0 ? (100.0 * actual[categories[i]] / allocated[categories[i]]) : 0.0;

        std::ostringstream allocCell;
        std::ostringstream actualCell;
        std::ostringstream varianceCell;
        std::ostringstream utilCell;

        allocCell << std::fixed << std::setprecision(2) << allocated[categories[i]];
        actualCell << std::fixed << std::setprecision(2) << actual[categories[i]];
        varianceCell << std::fixed << std::setprecision(2) << variance;
        utilCell << std::fixed << std::setprecision(1) << util << "%";

        std::vector<std::string> row = {categories[i], allocCell.str(), actualCell.str(), varianceCell.str(), utilCell.str()};
        ui::printTableRow(row, widths);
        tableRows.push_back(row);
    }
    ui::printTableFooter(widths);

    std::cout << "\n" << ui::bold("Actual Spend By Category") << "\n";
    for (std::size_t i = 0; i < categories.size(); ++i) {
        ui::printBar(categories[i], actual[categories[i]], maxActual, ui::preferredBarWidth(), "34");
    }

    if (!categories.empty()) {
        ui::printLineChart("Utilization Profile (%)", categories, utilizationValues, ui::preferredChartHeight(), ui::preferredChartWidth());
    }

    if (tableRows.size() > static_cast<std::size_t>(ui::tablePageSize()) && promptPagedDetails("Budget Utilization")) {
        clearInputBuffer();
        runPagedTableView("BUDGET UTILIZATION - PAGED DETAIL",
                          {"ADMIN", "ANALYTICS HUB", "BUDGETS", "PAGED DETAIL"},
                          headers,
                          widths,
                          tableRows);
    }

    logAuditAction("ANALYTICS_BUDGET_UTILIZATION", window.label, admin.username);
    waitForEnter();
}

void renderAuditActivity(const Admin& admin, const AnalyticsWindow& window) {
    clearScreen();
    ui::printSectionTitle("ANALYTICS - AUDIT ACTIVITY");
    ui::printBreadcrumb({"ADMIN", "ANALYTICS HUB", "AUDIT"});
    printWindowLabel(window);

    std::vector<AuditRow> rows;
    if (!loadAuditRows(rows)) {
        std::cout << "\n" << ui::error("[!] Unable to load audit dataset.") << "\n";
        waitForEnter();
        return;
    }

    std::vector<AuditRow> filtered;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (matchesWindow(extractDate(rows[i].timestamp), window)) {
            filtered.push_back(rows[i]);
        }
    }

    if (filtered.empty()) {
        std::cout << "\n" << ui::warning("[!] No audit rows in selected window.") << "\n";
        waitForEnter();
        return;
    }

    std::map<std::string, double> actionCounts;
    std::vector<std::string> dateSeriesSource;
    for (std::size_t i = 0; i < filtered.size(); ++i) {
        actionCounts[filtered[i].action] += 1.0;
        dateSeriesSource.push_back(filtered[i].timestamp);
    }

    ui::printKpiTiles({
        std::make_pair("Events", std::to_string(filtered.size())),
        std::make_pair("Unique actions", std::to_string(actionCounts.size())),
        std::make_pair("Most recent date", extractDate(filtered.back().timestamp))
    });

    std::vector<std::pair<std::string, double>> sortedActions;
    for (std::map<std::string, double>::const_iterator it = actionCounts.begin(); it != actionCounts.end(); ++it) {
        sortedActions.push_back(std::make_pair(it->first, it->second));
    }

    std::sort(sortedActions.begin(), sortedActions.end(), [](const std::pair<std::string, double>& a,
                                                             const std::pair<std::string, double>& b) {
        return a.second > b.second;
    });

    const std::vector<std::string> headers = {"Action", "Count", "Share"};
    const std::vector<int> widths = ui::isCompactLayout() ? std::vector<int>{20, 8, 8} : std::vector<int>{30, 10, 10};
    ui::printTableHeader(headers, widths);
    std::vector<std::vector<std::string>> tableRows;

    const double total = static_cast<double>(filtered.size());
    for (std::size_t i = 0; i < sortedActions.size(); ++i) {
        std::ostringstream countOut;
        std::ostringstream shareOut;
        countOut << std::fixed << std::setprecision(0) << sortedActions[i].second;
        shareOut << std::fixed << std::setprecision(1) << (100.0 * sortedActions[i].second / total) << "%";
        std::vector<std::string> row = {sortedActions[i].first, countOut.str(), shareOut.str()};
        ui::printTableRow(row, widths);
        tableRows.push_back(row);
    }
    ui::printTableFooter(widths);

    std::cout << "\n" << ui::bold("Action Frequency") << "\n";
    const double maxCount = sortedActions.empty() ? 1.0 : sortedActions[0].second;
    for (std::size_t i = 0; i < sortedActions.size(); ++i) {
        ui::printBar(sortedActions[i].first, sortedActions[i].second, maxCount, ui::preferredBarWidth(), "36");
    }

    const std::vector<std::pair<std::string, double>> series = buildDailySeries(dateSeriesSource, window, 14);
    if (!series.empty()) {
        std::vector<std::string> labels;
        std::vector<double> values;
        for (std::size_t i = 0; i < series.size(); ++i) {
            labels.push_back(series[i].first);
            values.push_back(series[i].second);
        }
        ui::printLineChart("Audit Events Per Day", labels, values, ui::preferredChartHeight(), ui::preferredChartWidth());
        renderSeriesTable(series, "Date", "Events", ui::tablePageSize());
    }

    if (tableRows.size() > static_cast<std::size_t>(ui::tablePageSize()) && promptPagedDetails("Audit Action Frequency")) {
        clearInputBuffer();
        runPagedTableView("AUDIT ACTIONS - PAGED DETAIL",
                          {"ADMIN", "ANALYTICS HUB", "AUDIT", "PAGED DETAIL"},
                          headers,
                          widths,
                          tableRows);
    }

    logAuditAction("ANALYTICS_AUDIT_ACTIVITY", window.label, admin.username);
    waitForEnter();
}

void renderIntegrityStatus(const Admin& admin, const AnalyticsWindow& window) {
    clearScreen();
    ui::printSectionTitle("ANALYTICS - INTEGRITY STATUS");
    ui::printBreadcrumb({"ADMIN", "ANALYTICS HUB", "INTEGRITY"});
    printWindowLabel(window);

    std::vector<DocumentRow> docs;
    std::vector<ApprovalRow> approvals;
    std::vector<AuditRow> auditRows;
    if (!loadDocuments(docs) || !loadApprovals(approvals) || !loadAuditRows(auditRows)) {
        std::cout << "\n" << ui::error("[!] Integrity analytics datasets are unavailable.") << "\n";
        waitForEnter();
        return;
    }

    std::vector<bool> nodeValid;
    std::vector<std::string> nodeData;
    const std::vector<BlockchainNodePath> nodePaths = getBlockchainNodePaths();

    for (std::size_t i = 0; i < nodePaths.size(); ++i) {
        nodeValid.push_back(validateSingleChain(nodePaths[i].primaryPath, nodePaths[i].fallbackPath));
        nodeData.push_back(readFullFileWithFallback(nodePaths[i].primaryPath, nodePaths[i].fallbackPath));
    }

    bool sameContent = !nodeData.empty() && !nodeData[0].empty();
    for (std::size_t i = 1; i < nodeData.size() && sameContent; ++i) {
        if (nodeData[i] != nodeData[0]) {
            sameContent = false;
        }
    }

    int invalidNodes = 0;
    for (std::size_t i = 0; i < nodeValid.size(); ++i) {
        if (!nodeValid[i]) {
            invalidNodes++;
        }
    }

    const int hashMismatches = countDocumentHashMismatches();
    const int stalePending = countStalePendingApprovals(7);

    std::vector<std::string> riskDates;

    for (std::size_t i = 0; i < auditRows.size(); ++i) {
        if (!matchesWindow(extractDate(auditRows[i].timestamp), window)) {
            continue;
        }

        const std::string action = toLowerCopy(auditRows[i].action);
        if (action.find("fail") != std::string::npos || action.find("tampered") != std::string::npos) {
            riskDates.push_back(auditRows[i].timestamp);
        }
    }

    const int riskAlerts = static_cast<int>(riskDates.size());

    ui::printKpiTiles({
        std::make_pair("Invalid blockchain nodes", std::to_string(invalidNodes)),
        std::make_pair("Node content consistency", sameContent ? "PASS" : "FAIL"),
        std::make_pair("Document hash mismatches", std::to_string(hashMismatches)),
        std::make_pair("Stale pending approvals", std::to_string(stalePending))
    });

    const std::vector<std::string> headers = {"Check", "Status", "Detail"};
    const std::vector<int> widths = ui::isCompactLayout() ? std::vector<int>{20, 8, 14} : std::vector<int>{30, 10, 20};
    ui::printTableHeader(headers, widths);

    std::vector<std::vector<std::string>> integrityRows;
    integrityRows.push_back({"Blockchain node integrity", invalidNodes == 0 ? "PASS" : "FAIL", std::to_string(invalidNodes) + " invalid"});
    integrityRows.push_back({"Node content consistency", sameContent ? "PASS" : "FAIL", sameContent ? "all synchronized" : "divergence detected"});
    integrityRows.push_back({"Document hash verification", hashMismatches == 0 ? "PASS" : "FAIL", std::to_string(hashMismatches) + " mismatches"});
    integrityRows.push_back({"Pending age threshold (7d)", stalePending == 0 ? "PASS" : "WARN", std::to_string(stalePending) + " stale"});

    for (std::size_t i = 0; i < integrityRows.size(); ++i) {
        ui::printTableRow(integrityRows[i], widths);
    }
    ui::printTableFooter(widths);

    const double maxIssues = static_cast<double>(std::max(std::max(invalidNodes, hashMismatches), std::max(stalePending, riskAlerts)));
    std::cout << "\n" << ui::bold("Integrity Risk Indicators") << "\n";
    ui::printBar("Invalid nodes", static_cast<double>(invalidNodes), maxIssues <= 0.0 ? 1.0 : maxIssues, ui::preferredBarWidth(), "31");
    ui::printBar("Hash mismatches", static_cast<double>(hashMismatches), maxIssues <= 0.0 ? 1.0 : maxIssues, ui::preferredBarWidth(), "31");
    ui::printBar("Stale pending approvals", static_cast<double>(stalePending), maxIssues <= 0.0 ? 1.0 : maxIssues, ui::preferredBarWidth(), "33");
    ui::printBar("Risk alerts in window", static_cast<double>(riskAlerts), maxIssues <= 0.0 ? 1.0 : maxIssues, ui::preferredBarWidth(), "35");

    const std::vector<std::pair<std::string, double>> riskSeries = buildDailySeries(riskDates, window, 14);
    if (!riskSeries.empty()) {
        std::vector<std::string> labels;
        std::vector<double> values;
        for (std::size_t i = 0; i < riskSeries.size(); ++i) {
            labels.push_back(riskSeries[i].first);
            values.push_back(riskSeries[i].second);
        }
        ui::printLineChart("Integrity Alerts Per Day", labels, values, ui::preferredChartHeight(), ui::preferredChartWidth());
        renderSeriesTable(riskSeries, "Date", "Alerts", ui::tablePageSize());
    }

    if (promptPagedDetails("Integrity Status Checks")) {
        clearInputBuffer();
        runPagedTableView("INTEGRITY STATUS - PAGED DETAIL",
                          {"ADMIN", "ANALYTICS HUB", "INTEGRITY", "PAGED DETAIL"},
                          headers,
                          widths,
                          integrityRows);
    }

    logAuditAction("ANALYTICS_INTEGRITY_STATUS", window.label, admin.username);
    waitForEnter();
}

void renderExecutiveSnapshot(const Admin& admin, const AnalyticsWindow& window) {
    clearScreen();
    ui::printSectionTitle("ANALYTICS - EXECUTIVE SNAPSHOT");
    ui::printBreadcrumb({"ADMIN", "ANALYTICS HUB", "EXECUTIVE"});
    printWindowLabel(window);

    std::vector<DocumentRow> docs;
    std::vector<ApprovalRow> approvals;
    std::vector<BudgetRow> budgets;
    std::vector<AuditRow> audits;

    if (!loadDocuments(docs) || !loadApprovals(approvals) || !loadBudgetRows(budgets) || !loadAuditRows(audits)) {
        std::cout << "\n" << ui::error("[!] Unable to load analytics datasets.") << "\n";
        waitForEnter();
        return;
    }

    int pendingDocs = 0;
    int approvedDocs = 0;
    int rejectedDocs = 0;
    double totalActual = 0.0;

    for (std::size_t i = 0; i < docs.size(); ++i) {
        if (!matchesWindow(docs[i].dateUploaded, window)) {
            continue;
        }

        const std::string status = toLowerCopy(docs[i].status);
        if (status == "pending" || status == "pending_approval") {
            pendingDocs++;
        } else if (status == "approved" || status == "published") {
            approvedDocs++;
            totalActual += docs[i].amount;
        } else if (status == "rejected" || status == "denied") {
            rejectedDocs++;
        }
    }

    double totalAllocated = 0.0;
    for (std::size_t i = 0; i < budgets.size(); ++i) {
        totalAllocated += budgets[i].amount;
    }

    int decidedApprovals = 0;
    for (std::size_t i = 0; i < approvals.size(); ++i) {
        const std::string status = toLowerCopy(approvals[i].status);
        if ((status == "approved" || status == "rejected") && matchesWindow(extractDate(approvals[i].decidedAt), window)) {
            decidedApprovals++;
        }
    }

    int auditEvents = 0;
    std::vector<std::string> auditDates;
    for (std::size_t i = 0; i < audits.size(); ++i) {
        if (matchesWindow(extractDate(audits[i].timestamp), window)) {
            auditEvents++;
            auditDates.push_back(audits[i].timestamp);
        }
    }

    const double utilization = totalAllocated > 0.0 ? (100.0 * totalActual / totalAllocated) : 0.0;

    std::ostringstream utilOut;
    utilOut << std::fixed << std::setprecision(1) << utilization << "%";

    ui::printKpiTiles({
        std::make_pair("Pending docs", std::to_string(pendingDocs)),
        std::make_pair("Approved docs", std::to_string(approvedDocs)),
        std::make_pair("Rejected docs", std::to_string(rejectedDocs)),
        std::make_pair("Decided approvals", std::to_string(decidedApprovals)),
        std::make_pair("Budget utilization", utilOut.str()),
        std::make_pair("Audit events", std::to_string(auditEvents))
    });

    const std::vector<std::string> headers = {"Metric", "Value"};
    const std::vector<int> widths = ui::isCompactLayout() ? std::vector<int>{22, 14} : std::vector<int>{28, 18};
    ui::printTableHeader(headers, widths);

    std::vector<std::vector<std::string>> executiveRows;
    executiveRows.push_back({"Pending documents", std::to_string(pendingDocs)});
    executiveRows.push_back({"Approved documents", std::to_string(approvedDocs)});
    executiveRows.push_back({"Rejected documents", std::to_string(rejectedDocs)});
    executiveRows.push_back({"Decided approvals", std::to_string(decidedApprovals)});
    executiveRows.push_back({"Total allocated budget", std::to_string(totalAllocated)});
    executiveRows.push_back({"Total actual spend", std::to_string(totalActual)});
    executiveRows.push_back({"Audit events", std::to_string(auditEvents)});

    for (std::size_t i = 0; i < executiveRows.size(); ++i) {
        ui::printTableRow(executiveRows[i], widths);
    }
    ui::printTableFooter(widths);

    const double maxDoc = static_cast<double>(std::max(std::max(pendingDocs, approvedDocs), rejectedDocs));
    std::cout << "\n" << ui::bold("Document Lifecycle") << "\n";
    const int barWidth = ui::preferredBarWidth();
    ui::printBar("Pending", static_cast<double>(pendingDocs), maxDoc <= 0.0 ? 1.0 : maxDoc, barWidth, ui::consensusColorCode("pending"));
    ui::printBar("Approved", static_cast<double>(approvedDocs), maxDoc <= 0.0 ? 1.0 : maxDoc, barWidth, ui::consensusColorCode("approved"));
    ui::printBar("Rejected", static_cast<double>(rejectedDocs), maxDoc <= 0.0 ? 1.0 : maxDoc, barWidth, ui::consensusColorCode("rejected"));

    const std::vector<std::pair<std::string, double>> auditSeries = buildDailySeries(auditDates, window, 14);
    if (!auditSeries.empty()) {
        std::vector<std::string> labels;
        std::vector<double> values;
        for (std::size_t i = 0; i < auditSeries.size(); ++i) {
            labels.push_back(auditSeries[i].first);
            values.push_back(auditSeries[i].second);
        }
        ui::printLineChart("Audit Activity Trend", labels, values, ui::preferredChartHeight(), ui::preferredChartWidth());
        renderSeriesTable(auditSeries, "Date", "Events", ui::tablePageSize());
    }

    if (promptPagedDetails("Executive Metrics")) {
        clearInputBuffer();
        runPagedTableView("EXECUTIVE SNAPSHOT - PAGED DETAIL",
                          {"ADMIN", "ANALYTICS HUB", "EXECUTIVE", "PAGED DETAIL"},
                          headers,
                          widths,
                          executiveRows);
    }

    logAuditAction("ANALYTICS_EXECUTIVE_SNAPSHOT", window.label, admin.username);
    waitForEnter();
}

bool promptWindowSelection(AnalyticsWindow& window) {
    clearInputBuffer();

    clearScreen();
    ui::printSectionTitle("ANALYTICS WINDOW");
    ui::printBreadcrumb({"ADMIN", "ANALYTICS HUB", "WINDOW"});
    std::cout << "  " << ui::info("[1]") << " All Time\n";
    std::cout << "  " << ui::info("[2]") << " Last 7 Days\n";
    std::cout << "  " << ui::info("[3]") << " Last 30 Days\n";
    std::cout << "  " << ui::info("[4]") << " Custom Date Range\n";
    std::cout << "  " << ui::info("[0]") << " Cancel\n";
    std::cout << ui::muted("--------------------------------------------------------------") << "\n";
    std::cout << "  Enter your choice: ";

    int choice = -1;
    std::cin >> choice;
    if (std::cin.fail()) {
        std::cin.clear();
        clearInputBuffer();
        std::cout << "\n" << ui::warning("[!] Invalid input.") << "\n";
        waitForEnter();
        return false;
    }

    if (choice == 0) {
        return false;
    }

    if (choice == 1) {
        window.allTime = true;
        window.fromDate.clear();
        window.toDate.clear();
        window.label = "all_time";
        return true;
    }

    if (choice == 2 || choice == 3) {
        window.allTime = false;
        const std::string today = todayDate();
        const int span = (choice == 2) ? 6 : 29;
        window.fromDate = shiftDateByDays(today, -span);
        window.toDate = today;
        window.label = (choice == 2) ? "last_7_days" : "last_30_days";
        return true;
    }

    if (choice == 4) {
        clearInputBuffer();

        std::cout << "From date (YYYY-MM-DD): ";
        std::getline(std::cin, window.fromDate);
        std::cout << "To date (YYYY-MM-DD): ";
        std::getline(std::cin, window.toDate);

        if (!isDateTextValid(window.fromDate) || !isDateTextValid(window.toDate)) {
            std::cout << "\n" << ui::error("[!] Invalid date format.") << "\n";
            waitForEnter();
            return false;
        }

        if (window.fromDate > window.toDate) {
            std::cout << "\n" << ui::error("[!] Invalid range: fromDate is after toDate.") << "\n";
            waitForEnter();
            return false;
        }

        window.allTime = false;
        window.label = "custom_" + window.fromDate + "_to_" + window.toDate;
        return true;
    }

    std::cout << "\n" << ui::warning("[!] Invalid choice.") << "\n";
    waitForEnter();
    return false;
}

void renderOverviewCards(const Admin& admin) {
    std::vector<DocumentRow> docs;
    std::vector<ApprovalRow> approvals;
    std::vector<BudgetRow> budgets;
    std::vector<AuditRow> audits;

    if (!loadDocuments(docs) || !loadApprovals(approvals) || !loadBudgetRows(budgets) || !loadAuditRows(audits)) {
        std::cout << "\n" << ui::error("[!] Overview datasets are unavailable.") << "\n";
        return;
    }

    int pendingDocs = 0;
    int approvedDocs = 0;
    int rejectedDocs = 0;

    for (std::size_t i = 0; i < docs.size(); ++i) {
        const std::string status = toLowerCopy(docs[i].status);
        if (status == "pending" || status == "pending_approval") {
            pendingDocs++;
        } else if (status == "approved" || status == "published") {
            approvedDocs++;
        } else if (status == "rejected" || status == "denied") {
            rejectedDocs++;
        }
    }

    double allocated = 0.0;
    for (std::size_t i = 0; i < budgets.size(); ++i) {
        allocated += budgets[i].amount;
    }

    double actual = 0.0;
    for (std::size_t i = 0; i < docs.size(); ++i) {
        const std::string status = toLowerCopy(docs[i].status);
        if (status == "approved" || status == "published") {
            actual += docs[i].amount;
        }
    }

    int decidedApprovals = 0;
    for (std::size_t i = 0; i < approvals.size(); ++i) {
        const std::string status = toLowerCopy(approvals[i].status);
        if (status == "approved" || status == "rejected") {
            decidedApprovals++;
        }
    }

    const double utilization = allocated > 0.0 ? (100.0 * actual / allocated) : 0.0;
    std::ostringstream utilizationOut;
    utilizationOut << std::fixed << std::setprecision(1) << utilization << "%";

    ui::printKpiTiles({
        std::make_pair("Role", admin.role),
        std::make_pair("Pending docs", std::to_string(pendingDocs)),
        std::make_pair("Approved docs", std::to_string(approvedDocs)),
        std::make_pair("Rejected docs", std::to_string(rejectedDocs)),
        std::make_pair("Decided approvals", std::to_string(decidedApprovals)),
        std::make_pair("Budget utilization", utilizationOut.str()),
        std::make_pair("Audit events", std::to_string(audits.size()))
    });

    const double maxDoc = static_cast<double>(std::max(std::max(pendingDocs, approvedDocs), rejectedDocs));
    std::cout << "\n" << ui::bold("Document State Snapshot") << "\n";
    ui::printBar("Pending", static_cast<double>(pendingDocs), maxDoc <= 0.0 ? 1.0 : maxDoc, ui::preferredBarWidth(), ui::consensusColorCode("pending"));
    ui::printBar("Approved", static_cast<double>(approvedDocs), maxDoc <= 0.0 ? 1.0 : maxDoc, ui::preferredBarWidth(), ui::consensusColorCode("approved"));
    ui::printBar("Rejected", static_cast<double>(rejectedDocs), maxDoc <= 0.0 ? 1.0 : maxDoc, ui::preferredBarWidth(), ui::consensusColorCode("rejected"));
}
} // namespace

void runAnalyticsHub(const Admin& admin) {
    int choice = -1;

    do {
        clearScreen();
        ui::printSectionTitle("ANALYTICS HUB");
        ui::printBreadcrumb({"ADMIN", "OVERVIEW", "ANALYTICS HUB"});

        std::cout << "  " << ui::info("[1]") << " Approval Funnel + Throughput Trends\n";
        std::cout << "  " << ui::info("[2]") << " Budget Utilization Comparisons\n";
        std::cout << "  " << ui::info("[3]") << " Audit Activity Timeline/Frequency\n";
        std::cout << "  " << ui::info("[4]") << " Integrity Status Cards\n";
        std::cout << "  " << ui::info("[5]") << " Executive Snapshot\n";
        std::cout << "  " << ui::info("[6]") << " Department Workload Report\n";
        std::cout << "  " << ui::info("[7]") << " Compliance Audit Report\n";
        std::cout << "  " << ui::info("[8]") << " Toggle Layout Mode (Current: " << ui::layoutModeLabel() << ")\n";
        std::cout << "  " << ui::info("[0]") << " Back to Overview\n";
        std::cout << ui::muted("--------------------------------------------------------------") << "\n";
        std::cout << "  Enter your choice: ";

        std::cin >> choice;
        if (std::cin.fail()) {
            std::cin.clear();
            clearInputBuffer();
            std::cout << "\n" << ui::warning("[!] Invalid menu input.") << "\n";
            waitForEnter();
            continue;
        }

        if (choice == 0) {
            break;
        }

        if (choice == 8) {
            ui::toggleCompactLayout();
            logAuditAction("ANALYTICS_LAYOUT_TOGGLE", ui::layoutModeLabel(), admin.username);
            continue;
        }

        AnalyticsWindow window;
        if (!promptWindowSelection(window)) {
            continue;
        }

        switch (choice) {
            case 1:
                renderApprovalFunnelAndTrend(admin, window);
                break;
            case 2:
                renderBudgetUtilization(admin, window);
                break;
            case 3:
                renderAuditActivity(admin, window);
                break;
            case 4:
                renderIntegrityStatus(admin, window);
                break;
            case 5:
                renderExecutiveSnapshot(admin, window);
                break;
            case 6:
                renderDepartmentWorkload(admin, window);
                break;
            case 7:
                renderComplianceAuditReport(admin, window);
                break;
            default:
                std::cout << "\n" << ui::warning("[!] Invalid menu choice.") << "\n";
                waitForEnter();
                break;
        }

    } while (choice != 0);
}

void viewIntegritySnapshot(const Admin& admin) {
    AnalyticsWindow window;
    window.allTime = false;
    window.fromDate = shiftDateByDays(todayDate(), -6);
    window.toDate = todayDate();
    window.label = "last_7_days";

    renderIntegrityStatus(admin, window);
}

void viewAdminOverviewDashboard(const Admin& admin) {
    int choice = -1;

    do {
        clearScreen();
        ui::printSectionTitle("ADMIN OVERVIEW DASHBOARD");
        ui::printBreadcrumb({"ADMIN", "OVERVIEW"});

        renderOverviewCards(admin);

        std::cout << "\n";
        std::cout << "  " << ui::info("[1]") << " Open Analytics Hub\n";
        std::cout << "  " << ui::info("[2]") << " Quick Integrity Snapshot (Last 7 Days)\n";
        std::cout << "  " << ui::info("[3]") << " Refresh Overview\n";
        std::cout << "  " << ui::info("[4]") << " Toggle Layout Mode (Current: " << ui::layoutModeLabel() << ")\n";
        std::cout << "  " << ui::info("[0]") << " Back to Admin Workspace\n";
        std::cout << ui::muted("--------------------------------------------------------------") << "\n";
        std::cout << "  Enter your choice: ";

        std::cin >> choice;
        if (std::cin.fail()) {
            std::cin.clear();
            clearInputBuffer();
            std::cout << "\n" << ui::warning("[!] Invalid menu input.") << "\n";
            waitForEnter();
            continue;
        }

        if (choice == 1) {
            runAnalyticsHub(admin);
        } else if (choice == 2) {
            viewIntegritySnapshot(admin);
        } else if (choice == 3) {
            continue;
        } else if (choice == 4) {
            ui::toggleCompactLayout();
            logAuditAction("OVERVIEW_LAYOUT_TOGGLE", ui::layoutModeLabel(), admin.username);
            continue;
        } else if (choice == 0) {
            break;
        } else {
            std::cout << "\n" << ui::warning("[!] Invalid menu choice.") << "\n";
            waitForEnter();
        }

    } while (choice != 0);
}
