#include "../include/audit.h"

#include "../include/auth.h"
#include "../include/ui.h"

#include <cctype>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <ctime>
#include <vector>

namespace {
const std::string AUDIT_FILE_PATH_PRIMARY = "data/audit_log.txt";
const std::string AUDIT_FILE_PATH_FALLBACK = "../data/audit_log.txt";

struct AuditEntry {
    std::string timestamp;
    std::string action;
    std::string target;
    std::string actor;
    std::string chainIndex;
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

bool openAppendFileWithFallback(std::ofstream& file, const std::string& primaryPath, const std::string& fallbackPath) {
    file.open(primaryPath, std::ios::app);
    if (file.is_open()) {
        return true;
    }

    file.clear();
    file.open(fallbackPath, std::ios::app);
    return file.is_open();
}

std::string toLowerCopy(std::string value) {
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] >= 'A' && value[i] <= 'Z') {
            value[i] = static_cast<char>(value[i] + ('a' - 'A'));
        }
    }
    return value;
}

bool containsCaseInsensitive(const std::string& text, const std::string& token) {
    if (token.empty()) {
        return true;
    }

    const std::string lowerText = toLowerCopy(text);
    const std::string lowerToken = toLowerCopy(token);
    return lowerText.find(lowerToken) != std::string::npos;
}

void clearInputBuffer() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

bool looksLikeHeader(const std::string& firstLine) {
    const std::string lower = toLowerCopy(firstLine);
    return lower.find("timestamp|") == 0;
}

bool loadAuditEntries(std::vector<AuditEntry>& entries) {
    std::ifstream file;
    if (!openInputFileWithFallback(file, AUDIT_FILE_PATH_PRIMARY, AUDIT_FILE_PATH_FALLBACK)) {
        return false;
    }

    entries.clear();
    std::string line;
    bool firstLineHandled = false;

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        if (!firstLineHandled) {
            firstLineHandled = true;
            if (looksLikeHeader(line)) {
                continue;
            }
        }

        std::stringstream parser(line);
        AuditEntry row;
        std::getline(parser, row.timestamp, '|');
        std::getline(parser, row.action, '|');
        std::getline(parser, row.target, '|');
        std::getline(parser, row.actor, '|');
        std::getline(parser, row.chainIndex, '|');

        if (row.timestamp.empty() || row.action.empty()) {
            continue;
        }

        entries.push_back(row);
    }

    return true;
}

std::string csvEscape(const std::string& raw) {
    std::string escaped = "\"";
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\"') {
            escaped += "\"\"";
        } else {
            escaped += raw[i];
        }
    }
    escaped += "\"";
    return escaped;
}

bool isWithinDateRange(const std::string& timestamp, const std::string& fromDate, const std::string& toDate) {
    const std::string day = timestamp.size() >= 10 ? timestamp.substr(0, 10) : "";

    if (!fromDate.empty() && day < fromDate) {
        return false;
    }

    if (!toDate.empty() && day > toDate) {
        return false;
    }

    return true;
}

std::string sanitizeFileToken(std::string token) {
    for (size_t i = 0; i < token.size(); ++i) {
        const char c = token[i];
        const bool safe = std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '-';
        if (!safe) {
            token[i] = '_';
        }
    }
    return token;
}

std::string defaultExportFileName() {
    std::string stamp = getCurrentTimestamp();
    for (size_t i = 0; i < stamp.size(); ++i) {
        if (stamp[i] == ' ' || stamp[i] == ':') {
            stamp[i] = '_';
        }
    }

    return "audit_export_" + sanitizeFileToken(stamp) + ".csv";
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

void writeCsvRows(const std::string& outputPath, const std::vector<AuditEntry>& rows) {
    std::ofstream out(outputPath);
    if (!out.is_open()) {
        std::cout << ui::error("[!] Unable to write export file.") << "\n";
        return;
    }

    out << "timestamp,action,targetID,actor,chainIndex\n";
    for (size_t i = 0; i < rows.size(); ++i) {
        out << csvEscape(rows[i].timestamp) << ','
            << csvEscape(rows[i].action) << ','
            << csvEscape(rows[i].target) << ','
            << csvEscape(rows[i].actor) << ','
            << csvEscape(rows[i].chainIndex) << '\n';
    }
    out.flush();
}
} // namespace

std::string getCurrentTimestamp() {
    std::time_t now = std::time(NULL);
    std::tm localTime;
    localtime_s(&localTime, &now);

    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &localTime);
    return std::string(buffer);
}

void logAuditAction(const std::string& action, const std::string& targetId, const std::string& actor, int chainIndex) {
    std::ofstream file;
    if (!openAppendFileWithFallback(file, AUDIT_FILE_PATH_PRIMARY, AUDIT_FILE_PATH_FALLBACK)) {
        return;
    }

    // Chain index is optional so non-blockchain actions still reuse one unified audit schema.
    file << getCurrentTimestamp() << '|'
         << action << '|'
         << targetId << '|'
         << actor << '|';

    if (chainIndex >= 0) {
        file << chainIndex;
    }

    file << '\n';
    file.flush();
}

void exportAuditTrailCsv(const std::string& actor) {
    clearScreen();
    ui::printSectionTitle("EXPORT AUDIT TRAIL (CSV)");

    std::vector<AuditEntry> entries;
    if (!loadAuditEntries(entries)) {
        std::cout << ui::error("[!] Unable to open audit log file.") << "\n";
        waitForEnter();
        return;
    }

    if (entries.empty()) {
        std::cout << ui::warning("[!] No audit entries available to export.") << "\n";
        waitForEnter();
        return;
    }

    std::cout << "  " << ui::info("[1]") << " Export all rows\n";
    std::cout << "  " << ui::info("[2]") << " Export filtered rows\n";
    std::cout << "  " << ui::info("[0]") << " Cancel\n";
    std::cout << ui::muted("--------------------------------------------------------------") << "\n";
    std::cout << "  Enter your choice: ";

    int choice = -1;
    std::cin >> choice;
    if (std::cin.fail()) {
        std::cin.clear();
        clearInputBuffer();
        std::cout << ui::warning("[!] Invalid choice.") << "\n";
        waitForEnter();
        return;
    }

    if (choice == 0) {
        return;
    }

    std::vector<AuditEntry> filtered = entries;
    if (choice == 2) {
        clearInputBuffer();

        std::string fromDate;
        std::string toDate;
        std::string actionFilter;
        std::string actorFilter;
        std::string targetFilter;

        std::cout << "From date (YYYY-MM-DD, optional): ";
        std::getline(std::cin, fromDate);
        std::cout << "To date (YYYY-MM-DD, optional): ";
        std::getline(std::cin, toDate);
        std::cout << "Action contains (optional): ";
        std::getline(std::cin, actionFilter);
        std::cout << "Actor contains (optional): ";
        std::getline(std::cin, actorFilter);
        std::cout << "Target contains (optional): ";
        std::getline(std::cin, targetFilter);

        std::vector<AuditEntry> result;
        // Filter pipeline keeps each criterion optional so one prompt can handle ad hoc audits.
        for (size_t i = 0; i < filtered.size(); ++i) {
            if (!isWithinDateRange(filtered[i].timestamp, fromDate, toDate)) {
                continue;
            }
            if (!containsCaseInsensitive(filtered[i].action, actionFilter)) {
                continue;
            }
            if (!containsCaseInsensitive(filtered[i].actor, actorFilter)) {
                continue;
            }
            if (!containsCaseInsensitive(filtered[i].target, targetFilter)) {
                continue;
            }

            result.push_back(filtered[i]);
        }

        filtered = result;
    } else if (choice != 1) {
        std::cout << ui::warning("[!] Invalid choice.") << "\n";
        waitForEnter();
        return;
    }

    if (filtered.empty()) {
        std::cout << ui::warning("[!] No rows matched the selected export criteria.") << "\n";
        waitForEnter();
        return;
    }

    clearInputBuffer();
    std::string fileName;
    std::cout << "Output filename (.csv, blank for default): ";
    std::getline(std::cin, fileName);

    if (fileName.empty()) {
        fileName = defaultExportFileName();
    }

    if (fileName.find(".csv") == std::string::npos) {
        fileName += ".csv";
    }

    const std::string outputPath = chooseExportPath(fileName);
    if (outputPath.empty()) {
        std::cout << ui::error("[!] Unable to create export path.") << "\n";
        waitForEnter();
        return;
    }

    writeCsvRows(outputPath, filtered);
    logAuditAction("AUDIT_EXPORT_CSV", fileName, actor);

    std::cout << ui::success("[+] Export completed: ") << outputPath << "\n";
    std::cout << ui::info("[i] Rows exported: ") << filtered.size() << "\n";
    waitForEnter();
}

void viewAuditTrail(const std::string& actor) {
    clearScreen();
    ui::printSectionTitle("AUDIT TRAIL");

    std::vector<AuditEntry> entries;
    if (!loadAuditEntries(entries)) {
        std::cout << ui::error("[!] Unable to open audit log file.") << "\n";
        waitForEnter();
        return;
    }

    if (entries.empty()) {
        std::cout << ui::warning("[!] No audit entries available yet.") << "\n";
        logAuditAction("VIEW_AUDIT_TRAIL", "EMPTY", actor);
        waitForEnter();
        return;
    }

    std::map<std::string, double> actionCounts;
    for (size_t i = 0; i < entries.size(); ++i) {
        actionCounts[entries[i].action] += 1.0;
    }

    const std::vector<std::string> headers = {"Timestamp", "Action", "Target", "Actor", "ChainIdx"};
    const std::vector<int> widths = {19, 24, 14, 14, 8};

    ui::printTableHeader(headers, widths);
    for (size_t i = 0; i < entries.size(); ++i) {
        ui::printTableRow(
            {entries[i].timestamp, entries[i].action, entries[i].target, entries[i].actor, entries[i].chainIndex},
            widths);
    }
    ui::printTableFooter(widths);

    std::cout << "\n" << ui::bold("Action Frequency Chart") << "\n";
    double maxCount = 0.0;
    for (std::map<std::string, double>::const_iterator it = actionCounts.begin(); it != actionCounts.end(); ++it) {
        if (it->second > maxCount) {
            maxCount = it->second;
        }
    }

    for (std::map<std::string, double>::const_iterator it = actionCounts.begin(); it != actionCounts.end(); ++it) {
        ui::printBar(it->first, it->second, maxCount, 24);
    }

    logAuditAction("VIEW_AUDIT_TRAIL", "MULTI", actor);

    std::cout << "\n" << ui::info("[1]") << " Export audit CSV\n";
    std::cout << "  " << ui::info("[0]") << " Back\n";
    std::cout << ui::muted("--------------------------------------------------------------") << "\n";
    std::cout << "  Enter your choice: ";

    int choice = -1;
    std::cin >> choice;
    if (std::cin.fail()) {
        std::cin.clear();
        clearInputBuffer();
        waitForEnter();
        return;
    }

    if (choice == 1) {
        exportAuditTrailCsv(actor);
        return;
    }

    waitForEnter();
}
