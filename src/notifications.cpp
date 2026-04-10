#include "../include/notifications.h"
#include "../include/ui.h"
#include "../include/audit.h"

#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace {

const std::string APPROVALS_FILE_PATH_PRIMARY = "data/approvals.txt";
const std::string APPROVALS_FILE_PATH_FALLBACK = "../data/approvals.txt";
const std::string BUDGET_APPROVALS_FILE_PATH_PRIMARY = "data/budget_approvals.txt";
const std::string BUDGET_APPROVALS_FILE_PATH_FALLBACK = "../data/budget_approvals.txt";
const std::string DOCUMENTS_FILE_PATH_PRIMARY = "data/documents.txt";
const std::string DOCUMENTS_FILE_PATH_FALLBACK = "../data/documents.txt";
const std::string APPROVAL_RULES_FILE_PATH_PRIMARY = "data/approval_rules.txt";
const std::string APPROVAL_RULES_FILE_PATH_FALLBACK = "../data/approval_rules.txt";

bool openInputFileWithFallback(std::ifstream& file, const std::string& primary, const std::string& fallback) {
    file.open(primary);
    if (file.is_open()) { return true; }
    file.clear();
    file.open(fallback);
    return file.is_open();
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

std::string toLowerCopy(std::string value) {
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] >= 'A' && value[i] <= 'Z') {
            value[i] = static_cast<char>(value[i] + ('a' - 'A'));
        }
    }
    return value;
}

std::string trimCopy(const std::string& s) {
    std::size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t')) { ++start; }
    std::size_t end = s.size();
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t')) { --end; }
    return s.substr(start, end - start);
}

int getMaxDecisionDaysForCategory(const std::string& category) {
    std::ifstream file;
    if (!openInputFileWithFallback(file, APPROVAL_RULES_FILE_PATH_PRIMARY, APPROVAL_RULES_FILE_PATH_FALLBACK)) {
        return 7;
    }
    const std::string catKey = toLowerCopy(trimCopy(category));
    int defaultDays = 7;
    std::string line;
    bool firstLine = true;
    while (std::getline(file, line)) {
        if (line.empty()) { continue; }
        if (firstLine) { firstLine = false; if (line.find("category|") == 0) { continue; } }
        const std::vector<std::string> tokens = splitPipe(line);
        if (tokens.size() < 3) { continue; }
        const std::string ruleKey = toLowerCopy(trimCopy(tokens[0]));
        if (ruleKey == "default") { defaultDays = std::atoi(tokens[2].c_str()); }
        if (ruleKey == catKey) { return std::atoi(tokens[2].c_str()); }
    }
    return defaultDays;
}

double daysSinceTimestamp(const std::string& ts) {
    if (ts.size() < 10) { return 0; }
    struct std::tm tm = {};
    tm.tm_year = std::atoi(ts.substr(0, 4).c_str()) - 1900;
    tm.tm_mon  = std::atoi(ts.substr(5, 2).c_str()) - 1;
    tm.tm_mday = std::atoi(ts.substr(8, 2).c_str());
    std::time_t then = std::mktime(&tm);
    std::time_t now  = std::time(NULL);
    double diff = std::difftime(now, then) / 86400.0;
    return diff > 0 ? diff : 0;
}

std::string getDocCategoryForDocId(const std::string& docId) {
    std::ifstream file;
    if (!openInputFileWithFallback(file, DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK)) {
        return "";
    }
    std::string line;
    bool firstLine = true;
    while (std::getline(file, line)) {
        if (line.empty()) { continue; }
        if (firstLine) { firstLine = false; if (line.find("docID|") == 0) { continue; } }
        const std::vector<std::string> tokens = splitPipe(line);
        if (tokens.size() >= 3 && tokens[0] == docId) {
            return tokens[2];
        }
    }
    return "";
}

int countRecentPublishedDocuments(int withinDays) {
    std::ifstream file;
    if (!openInputFileWithFallback(file, DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK)) {
        return 0;
    }
    int count = 0;
    std::string line;
    bool firstLine = true;
    while (std::getline(file, line)) {
        if (line.empty()) { continue; }
        if (firstLine) { firstLine = false; if (line.find("docID|") == 0) { continue; } }
        const std::vector<std::string> tokens = splitPipe(line);
        if (tokens.size() >= 8 && tokens[7] == "published") {
            if (tokens.size() >= 6) {
                double days = daysSinceTimestamp(tokens[5]);
                if (days <= withinDays) {
                    count++;
                }
            }
        }
    }
    return count;
}

} // namespace

void showAdminNotificationInbox(const Admin& admin) {
    clearScreen();
    ui::printSectionTitle("NOTIFICATION INBOX");
    ui::printBreadcrumb({"ADMIN", "NOTIFICATIONS"});
    std::cout << "  " << ui::muted("Notifications for: ") << admin.fullName << " (" << admin.role << ")\n\n";

    int pendingDocs = 0;
    int overdueDocs = 0;
    int pendingBudgets = 0;
    int overdueBudgets = 0;

    // Count pending document approvals
    {
        std::ifstream file;
        if (openInputFileWithFallback(file, APPROVALS_FILE_PATH_PRIMARY, APPROVALS_FILE_PATH_FALLBACK)) {
            std::string line;
            bool firstLine = true;
            while (std::getline(file, line)) {
                if (line.empty()) { continue; }
                if (firstLine) { firstLine = false; if (line.find("docID|") == 0) { continue; } }
                const std::vector<std::string> tokens = splitPipe(line);
                if (tokens.size() >= 4 && tokens[1] == admin.username && tokens[3] == "pending") {
                    pendingDocs++;
                    if (tokens.size() >= 5 && !tokens[4].empty()) {
                        const std::string category = getDocCategoryForDocId(tokens[0]);
                        int maxDays = getMaxDecisionDaysForCategory(category);
                        if (daysSinceTimestamp(tokens[4]) > maxDays) {
                            overdueDocs++;
                        }
                    }
                }
            }
        }
    }

    // Count pending budget approvals
    {
        std::ifstream file;
        if (openInputFileWithFallback(file, BUDGET_APPROVALS_FILE_PATH_PRIMARY, BUDGET_APPROVALS_FILE_PATH_FALLBACK)) {
            std::string line;
            bool firstLine = true;
            while (std::getline(file, line)) {
                if (line.empty()) { continue; }
                if (firstLine) { firstLine = false; if (line.find("entryID|") == 0) { continue; } }
                const std::vector<std::string> tokens = splitPipe(line);
                if (tokens.size() >= 4 && tokens[1] == admin.username && tokens[3] == "pending") {
                    pendingBudgets++;
                    if (tokens.size() >= 5 && !tokens[4].empty()) {
                        if (daysSinceTimestamp(tokens[4]) > 7) {
                            overdueBudgets++;
                        }
                    }
                }
            }
        }
    }

    bool hasNotifications = false;

    if (pendingDocs > 0) {
        hasNotifications = true;
        std::cout << "  " << ui::warning("[!]") << " " << pendingDocs << " document(s) awaiting your approval";
        if (overdueDocs > 0) {
            std::cout << " (" << ui::error(std::to_string(overdueDocs) + " overdue") << ")";
        }
        std::cout << "\n";
    }

    if (pendingBudgets > 0) {
        hasNotifications = true;
        std::cout << "  " << ui::warning("[!]") << " " << pendingBudgets << " budget entry(ies) awaiting your approval";
        if (overdueBudgets > 0) {
            std::cout << " (" << ui::error(std::to_string(overdueBudgets) + " overdue") << ")";
        }
        std::cout << "\n";
    }

    if (!hasNotifications) {
        std::cout << "  " << ui::success("[+] No pending actions. You are all caught up!") << "\n";
    }

    std::cout << "\n";
    logAuditAction("VIEW_NOTIFICATIONS", "INBOX", admin.username);
    waitForEnter();
}

void showCitizenNotificationInbox(const User& citizen) {
    clearScreen();
    ui::printSectionTitle("NOTIFICATION INBOX");
    std::cout << "  " << ui::muted("Notifications for: ") << citizen.fullName << "\n\n";

    int recentPublished = countRecentPublishedDocuments(7);

    if (recentPublished > 0) {
        std::cout << "  " << ui::info("[i]") << " " << recentPublished << " new document(s) published in the last 7 days.\n";
    } else {
        std::cout << "  " << ui::success("[+] No new updates. You are all caught up!") << "\n";
    }

    std::cout << "\n";
    logAuditAction("VIEW_NOTIFICATIONS", "INBOX", citizen.username);
    waitForEnter();
}
