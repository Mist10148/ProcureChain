#include "../include/audit.h"

#include "../include/auth.h"
#include "../include/ui.h"
#include "../include/verification.h"

#include <cctype>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <ctime>
#include <vector>

namespace {
// Audit storage is append-only in practice; readers parse rows defensively so
// legacy/malformed lines do not break the view/export flows.
const std::string AUDIT_FILE_PATH_PRIMARY = "data/audit_log.txt";
const std::string AUDIT_FILE_PATH_FALLBACK = "../data/audit_log.txt";

struct AuditEntry {
    std::string timestamp;
    std::string action;
    std::string target;
    std::string actor;
    std::string chainIndex;
    std::string previousHash;
    std::string currentHash;
};

bool openInputFileWithFallback(std::ifstream& file, const std::string& primaryPath, const std::string& fallbackPath) {
    // Constant-time fallback open strategy for read paths.
    file.open(primaryPath);
    if (file.is_open()) {
        return true;
    }

    file.clear();
    file.open(fallbackPath);
    return file.is_open();
}

bool openAppendFileWithFallback(std::ofstream& file, const std::string& primaryPath, const std::string& fallbackPath) {
    // Append mode preserves historical records while avoiding in-place rewrites.
    file.open(primaryPath, std::ios::app);
    if (file.is_open()) {
        return true;
    }

    file.clear();
    file.open(fallbackPath, std::ios::app);
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

std::string toLowerCopy(std::string value) {
    // ASCII-only lowercase conversion for case-insensitive filters.
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] >= 'A' && value[i] <= 'Z') {
            value[i] = static_cast<char>(value[i] + ('a' - 'A'));
        }
    }
    return value;
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

bool containsCaseInsensitive(const std::string& text, const std::string& token) {
    // O(n) substring check after lowercasing both sides.
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
    // Full log parse: O(n) rows and O(n) memory for in-memory table operations.
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

        std::vector<std::string> tokens = splitPipe(line);
        AuditEntry row;
        row.timestamp = tokens.size() > 0 ? tokens[0] : "";
        row.action = tokens.size() > 1 ? tokens[1] : "";
        row.target = tokens.size() > 2 ? tokens[2] : "";
        row.actor = tokens.size() > 3 ? tokens[3] : "";
        row.chainIndex = tokens.size() > 4 ? tokens[4] : "";
        row.previousHash = tokens.size() > 5 ? tokens[5] : "";
        row.currentHash = tokens.size() > 6 ? tokens[6] : "";

        if (row.timestamp.empty() || row.action.empty()) {
            continue;
        }

        entries.push_back(row);
    }

    return true;
}

std::string csvEscape(const std::string& raw) {
    // CSV-safe quoting: wraps field and doubles embedded quotes.
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
    // Uses lexical compare on YYYY-MM-DD, which preserves chronological order.
    const std::string day = timestamp.size() >= 10 ? timestamp.substr(0, 10) : "";

    if (!fromDate.empty() && day < fromDate) {
        return false;
    }

    if (!toDate.empty() && day > toDate) {
        return false;
    }

    return true;
}

std::string shortHash(const std::string& value, std::size_t keep) {
    if (value.size() <= keep) {
        return value;
    }

    return value.substr(0, keep) + "...";
}

void printAuditTimeline(const std::vector<AuditEntry>& entries, std::size_t maxRows) {
    if (entries.empty()) {
        return;
    }

    std::size_t start = 0;
    if (entries.size() > maxRows) {
        start = entries.size() - maxRows;
    }

    std::cout << "\n" << ui::bold("Audit Timeline (Chain View)") << "\n";
    for (std::size_t i = start; i < entries.size(); ++i) {
        std::cout << "\n[" << entries[i].timestamp << "] " << entries[i].action << "\n";
        std::cout << "  User   : " << entries[i].actor << "\n";
        std::cout << "  Target : " << entries[i].target << "\n";
        std::cout << "  Chain  : #" << entries[i].chainIndex << "\n";
        std::cout << "  Tx Hash: " << shortHash(entries[i].currentHash, 10) << "\n";

        if (i + 1 < entries.size()) {
            std::cout << "      |\n";
            std::cout << "      v\n";
        }
    }
}

bool isDateTextValid(const std::string& value) {
    if (value.empty()) {
        return true;
    }

    if (value.size() != 10) {
        return false;
    }

    return std::isdigit(static_cast<unsigned char>(value[0])) &&
           std::isdigit(static_cast<unsigned char>(value[1])) &&
           std::isdigit(static_cast<unsigned char>(value[2])) &&
           std::isdigit(static_cast<unsigned char>(value[3])) &&
           value[4] == '-' &&
           std::isdigit(static_cast<unsigned char>(value[5])) &&
           std::isdigit(static_cast<unsigned char>(value[6])) &&
           value[7] == '-' &&
           std::isdigit(static_cast<unsigned char>(value[8])) &&
           std::isdigit(static_cast<unsigned char>(value[9]));
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

bool isSafeExportFileName(const std::string& fileName) {
    if (fileName.empty() || fileName.size() > 80) {
        return false;
    }

    if (fileName.find("..") != std::string::npos) {
        return false;
    }

    for (size_t i = 0; i < fileName.size(); ++i) {
        const char c = fileName[i];
        const bool safe = std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '-' || c == '.';
        if (!safe) {
            return false;
        }
    }

    return true;
}

std::string sanitizeFileToken(std::string token) {
    // Restricts generated export filenames to a safe character subset.
    for (size_t i = 0; i < token.size(); ++i) {
        const char c = token[i];
        const bool safe = std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '-' || c == '.';
        if (!safe) {
            token[i] = '_';
        }
    }
    return token;
}

std::string defaultExportFileName() {
    // Timestamp-based default avoids collisions for repeated exports.
    std::string stamp = getCurrentTimestamp();
    for (size_t i = 0; i < stamp.size(); ++i) {
        if (stamp[i] == ' ' || stamp[i] == ':') {
            stamp[i] = '_';
        }
    }

    return "audit_export_" + sanitizeFileToken(stamp) + ".csv";
}

std::string chooseExportPath(const std::string& fileName) {
    // Probes writable location by opening the target file once.
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

std::string joinTopKeys(const std::map<std::string, int>& values, std::size_t maxItems) {
    std::string out;
    std::size_t shown = 0;

    for (std::map<std::string, int>::const_iterator it = values.begin(); it != values.end(); ++it) {
        if (it->first.empty()) {
            continue;
        }

        if (!out.empty()) {
            out += ", ";
        }

        out += it->first;
        shown++;

        if (shown >= maxItems) {
            break;
        }
    }

    return out.empty() ? "(none)" : out;
}

void printAuditFilterSuggestions(const std::vector<AuditEntry>& entries) {
    std::map<std::string, int> actionCounts;
    std::map<std::string, int> actorCounts;
    std::map<std::string, int> targetCounts;
    std::string oldestDate;
    std::string newestDate;

    for (std::size_t i = 0; i < entries.size(); ++i) {
        actionCounts[entries[i].action] += 1;
        actorCounts[entries[i].actor] += 1;
        targetCounts[entries[i].target] += 1;

        const std::string day = entries[i].timestamp.size() >= 10 ? entries[i].timestamp.substr(0, 10) : "";
        if (!day.empty()) {
            if (oldestDate.empty() || day < oldestDate) {
                oldestDate = day;
            }
            if (newestDate.empty() || day > newestDate) {
                newestDate = day;
            }
        }
    }

    std::cout << "\n" << ui::bold("Filter Suggestions") << "\n";
    std::cout << "  Actions : " << joinTopKeys(actionCounts, 8) << "\n";
    std::cout << "  Actors  : " << joinTopKeys(actorCounts, 8) << "\n";
    std::cout << "  Targets : " << joinTopKeys(targetCounts, 8) << "\n";
    if (!oldestDate.empty() && !newestDate.empty()) {
        std::cout << "  Date span: " << oldestDate << " to " << newestDate << "\n";
    }
    std::cout << "  " << ui::muted("Tip: leave filters blank to keep more rows.") << "\n";
}

std::string buildAuditHashPayload(const AuditEntry& row) {
    return row.timestamp + "|" + row.action + "|" + row.target + "|" + row.actor + "|" + row.chainIndex + "|" + row.previousHash;
}

void seedAuditRowsIfEmpty(std::vector<AuditEntry>& rows) {
    // Audit showcase rows are maintained in data/audit_log.txt.
    (void)rows;
}

bool saveAuditEntries(const std::vector<AuditEntry>& rows) {
    std::ofstream writer(resolveDataPath(AUDIT_FILE_PATH_PRIMARY, AUDIT_FILE_PATH_FALLBACK));
    if (!writer.is_open()) {
        return false;
    }

    writer << "timestamp|action|targetID|actor|chainIndex|previousHash|currentHash\n";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        writer << rows[i].timestamp << '|'
               << rows[i].action << '|'
               << rows[i].target << '|'
               << rows[i].actor << '|'
               << rows[i].chainIndex << '|'
               << rows[i].previousHash << '|'
               << rows[i].currentHash << '\n';
    }

    writer.flush();
    return true;
}

void writeCsvRows(const std::string& outputPath, const std::vector<AuditEntry>& rows) {
    // Linear write pipeline: O(n) where n is exported row count.
    std::ofstream out(outputPath);
    if (!out.is_open()) {
        std::cout << ui::error("[!] Unable to write export file.") << "\n";
        return;
    }

    out << "timestamp,action,targetID,actor,chainIndex,previousHash,currentHash\n";
    for (size_t i = 0; i < rows.size(); ++i) {
        out << csvEscape(rows[i].timestamp) << ','
            << csvEscape(rows[i].action) << ','
            << csvEscape(rows[i].target) << ','
            << csvEscape(rows[i].actor) << ','
            << csvEscape(rows[i].chainIndex) << ','
            << csvEscape(rows[i].previousHash) << ','
            << csvEscape(rows[i].currentHash) << '\n';
    }
    out.flush();
}
} // namespace

std::string getCurrentTimestamp() {
    // Central timestamp format shared by audit, approvals, and blockchain rows.
    std::time_t now = std::time(NULL);
    std::tm localTime;
    localtime_s(&localTime, &now);

    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &localTime);
    return std::string(buffer);
}

void ensureAuditTrailHashChain() {
    std::vector<AuditEntry> entries;
    if (!loadAuditEntries(entries)) {
        return;
    }

    seedAuditRowsIfEmpty(entries);

    std::string previousHash = "0000";
    for (std::size_t i = 0; i < entries.size(); ++i) {
        entries[i].previousHash = previousHash;
        entries[i].currentHash = computeSimpleHash(buildAuditHashPayload(entries[i]));
        previousHash = entries[i].currentHash;
    }

    saveAuditEntries(entries);
}

void logAuditAction(const std::string& action, const std::string& targetId, const std::string& actor, int chainIndex) {
    std::vector<AuditEntry> entries;
    if (!loadAuditEntries(entries)) {
        return;
    }

    AuditEntry row;
    row.timestamp = getCurrentTimestamp();
    row.action = action;
    row.target = targetId;
    row.actor = actor;

    if (chainIndex >= 0) {
        row.chainIndex = std::to_string(chainIndex);
    } else {
        row.chainIndex = "";
    }

    row.previousHash = entries.empty() ? "0000" : entries.back().currentHash;
    if (row.previousHash.empty()) {
        row.previousHash = "0000";
    }

    row.currentHash = computeSimpleHash(buildAuditHashPayload(row));
    entries.push_back(row);

    saveAuditEntries(entries);
}

void exportAuditTrailCsv(const std::string& actor) {
    // Export flow supports full dump or interactive filtered dump in one screen.
    // Worst-case complexity is O(n) for filtering + O(k) for writing output.
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

        printAuditFilterSuggestions(filtered);

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

        fromDate = trimCopy(fromDate);
        toDate = trimCopy(toDate);

        if (!isDateTextValid(fromDate) || !isDateTextValid(toDate)) {
            std::cout << ui::error("[!] Invalid date format. Use YYYY-MM-DD.") << "\n";
            waitForEnter();
            return;
        }

        if (!fromDate.empty() && !toDate.empty() && fromDate > toDate) {
            std::cout << ui::error("[!] Invalid date range: fromDate is after toDate.") << "\n";
            waitForEnter();
            return;
        }

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

    fileName = trimCopy(fileName);

    if (fileName.empty()) {
        fileName = defaultExportFileName();
    } else if (!isSafeExportFileName(fileName)) {
        std::cout << ui::error("[!] Invalid filename. Use letters, numbers, '_', '-', and '.'.") << "\n";
        waitForEnter();
        return;
    }

    if (fileName.size() < 4 || fileName.substr(fileName.size() - 4) != ".csv") {
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
    // Displays tabular audit rows and an action-frequency chart from the same
    // in-memory snapshot to keep screen data internally consistent.
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

    printAuditTimeline(entries, 12);

    std::cout << "\n" << ui::bold("Audit Table (Detailed)") << "\n";
    const std::vector<std::string> headers = {"Timestamp", "Action", "Target", "Actor", "ChainIdx", "PrevHash", "CurrHash"};
    const std::vector<int> widths = {19, 20, 12, 12, 8, 14, 14};

    ui::printTableHeader(headers, widths);
    for (size_t i = 0; i < entries.size(); ++i) {
        const std::string prevShort = entries[i].previousHash.size() > 14 ? entries[i].previousHash.substr(0, 14) : entries[i].previousHash;
        const std::string currShort = entries[i].currentHash.size() > 14 ? entries[i].currentHash.substr(0, 14) : entries[i].currentHash;
        ui::printTableRow(
            {entries[i].timestamp, entries[i].action, entries[i].target, entries[i].actor, entries[i].chainIndex, prevShort, currShort},
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
