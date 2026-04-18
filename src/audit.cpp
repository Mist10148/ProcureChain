#include "../include/audit.h"

#include "../include/auth.h"
#include "../include/documents.h"
#include "../include/storage_utils.h"
#include "../include/ui.h"
#include "../include/verification.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <vector>

namespace {
const std::string AUDIT_FILE_PATH_PRIMARY = "data/audit_log.txt";
const std::string AUDIT_FILE_PATH_FALLBACK = "../data/audit_log.txt";
const std::string ADMIN_FILE_PATH_PRIMARY = "data/admins.txt";
const std::string ADMIN_FILE_PATH_FALLBACK = "../data/admins.txt";
const std::string USER_FILE_PATH_PRIMARY = "data/users.txt";
const std::string USER_FILE_PATH_FALLBACK = "../data/users.txt";
const std::string DOCUMENT_FILE_PATH_PRIMARY = "data/documents.txt";
const std::string DOCUMENT_FILE_PATH_FALLBACK = "../data/documents.txt";
const std::string BUDGET_ENTRY_FILE_PATH_PRIMARY = "data/budget_entries.txt";
const std::string BUDGET_ENTRY_FILE_PATH_FALLBACK = "../data/budget_entries.txt";
const std::string AUDIT_HEADER = "timestamp|action|targetType|targetID|actorUsername|actorRole|outcome|visibility|chainIndex|previousHash|currentHash";

struct AuditEntry {
    std::string timestamp;
    std::string action;
    std::string targetType;
    std::string targetId;
    std::string actorUsername;
    std::string actorRole;
    std::string outcome;
    std::string visibility;
    std::string chainIndex;
    std::string previousHash;
    std::string currentHash;
};

struct PublicAuditFilter {
    std::string domain;
    std::string fromDate;
    std::string toDate;
    std::string outcome;
};

struct InternalAuditFilter {
    std::string fromDate;
    std::string toDate;
    std::string action;
    std::string actor;
    std::string role;
    std::string targetType;
    std::string targetId;
    std::string outcome;
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

std::vector<std::string> splitPipe(const std::string& line) {
    return storage::splitPipeRow(line);
}

void clearInputBuffer() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

bool looksLikeHeader(const std::string& firstLine) {
    return toLowerCopy(firstLine).find("timestamp|") == 0;
}

bool containsCaseInsensitive(const std::string& text, const std::string& token) {
    if (token.empty()) {
        return true;
    }

    return toLowerCopy(text).find(toLowerCopy(token)) != std::string::npos;
}

bool isDateTextValid(const std::string& value) {
    return storage::isDateTextValidStrict(value, true);
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

std::string csvEscape(const std::string& raw) {
    std::string escaped = "\"";
    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\"') {
            escaped += "\"\"";
        } else {
            escaped += raw[i];
        }
    }
    escaped += "\"";
    return escaped;
}

std::string shortHash(const std::string& value, std::size_t keep) {
    if (value.size() <= keep) {
        return value;
    }
    return value.substr(0, keep) + "...";
}

bool isSafeExportFileName(const std::string& fileName) {
    if (fileName.empty() || fileName.size() > 80) {
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

std::string defaultExportFileName(const std::string& prefix) {
    std::string stamp = getCurrentTimestamp();
    for (std::size_t i = 0; i < stamp.size(); ++i) {
        if (stamp[i] == ' ' || stamp[i] == ':') {
            stamp[i] = '_';
        }
    }
    return prefix + "_" + sanitizeFileToken(stamp) + ".csv";
}

std::string extractBaseActorUsername(const std::string& actor) {
    std::string base = actor;
    const std::size_t marker = base.find(" (");
    if (marker != std::string::npos) {
        base = base.substr(0, marker);
    }
    return trimCopy(base);
}
void loadActorRoleLookup(std::map<std::string, std::string>& roleByUsername) {
    roleByUsername.clear();

    std::ifstream adminFile;
    if (openInputFileWithFallback(adminFile, ADMIN_FILE_PATH_PRIMARY, ADMIN_FILE_PATH_FALLBACK)) {
        std::string line;
        bool firstLine = true;
        while (std::getline(adminFile, line)) {
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
            if (tokens.size() > 4) {
                roleByUsername[tokens[2]] = tokens[4];
            }
        }
    }

    std::ifstream userFile;
    if (openInputFileWithFallback(userFile, USER_FILE_PATH_PRIMARY, USER_FILE_PATH_FALLBACK)) {
        std::string line;
        bool firstLine = true;
        while (std::getline(userFile, line)) {
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
            if (tokens.size() > 2 && roleByUsername.find(tokens[2]) == roleByUsername.end()) {
                roleByUsername[tokens[2]] = "Citizen";
            }
        }
    }
}

void loadKnownTargetTypes(std::map<std::string, std::string>& targetTypeById) {
    targetTypeById.clear();

    std::ifstream documentFile;
    if (openInputFileWithFallback(documentFile, DOCUMENT_FILE_PATH_PRIMARY, DOCUMENT_FILE_PATH_FALLBACK)) {
        std::string line;
        bool firstLine = true;
        while (std::getline(documentFile, line)) {
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
            if (!tokens.empty()) {
                targetTypeById[tokens[0]] = "DOCUMENT";
            }
        }
    }

    std::ifstream budgetFile;
    if (openInputFileWithFallback(budgetFile, BUDGET_ENTRY_FILE_PATH_PRIMARY, BUDGET_ENTRY_FILE_PATH_FALLBACK)) {
        std::string line;
        bool firstLine = true;
        while (std::getline(budgetFile, line)) {
            if (line.empty()) {
                continue;
            }

            if (firstLine) {
                firstLine = false;
                if (line.find("entryID|") == 0) {
                    continue;
                }
            }

            const std::vector<std::string> tokens = splitPipe(line);
            if (!tokens.empty()) {
                targetTypeById[tokens[0]] = "BUDGET";
            }
        }
    }
}

std::string resolveActorRole(const std::string& actor, const std::map<std::string, std::string>& roleByUsername) {
    const std::string base = extractBaseActorUsername(actor);
    if (base.empty() || base == "N/A") {
        return "System";
    }

    if (toLowerCopy(base) == "anonymous") {
        return "Anonymous";
    }

    const std::map<std::string, std::string>::const_iterator found = roleByUsername.find(base);
    if (found != roleByUsername.end()) {
        return found->second;
    }

    return "System";
}

std::string inferTargetType(const std::string& action,
                            const std::string& targetId,
                            const std::map<std::string, std::string>& targetTypeById) {
    const std::string lowerAction = toLowerCopy(action);
    const std::string trimmedTarget = trimCopy(targetId);

    const std::map<std::string, std::string>::const_iterator known = targetTypeById.find(trimmedTarget);
    if (known != targetTypeById.end()) {
        return known->second;
    }

    if (lowerAction.find("budget") != std::string::npos || lowerAction.find("fiscal") != std::string::npos) {
        return "BUDGET";
    }

    if (lowerAction.find("doc") != std::string::npos ||
        lowerAction.find("approval") != std::string::npos ||
        lowerAction.find("published_docs") != std::string::npos) {
        return "DOCUMENT";
    }

    if (lowerAction.find("login") != std::string::npos ||
        lowerAction.find("signup") != std::string::npos ||
        lowerAction.find("password") != std::string::npos ||
        lowerAction.find("account") != std::string::npos ||
        lowerAction.find("deactivate") != std::string::npos ||
        lowerAction.find("reactivate") != std::string::npos) {
        return "ACCOUNT";
    }

    if (lowerAction.find("notification") != std::string::npos ||
        lowerAction.find("help") != std::string::npos ||
        lowerAction.find("analytics") != std::string::npos ||
        lowerAction.find("blockchain") != std::string::npos ||
        lowerAction.find("integrity") != std::string::npos ||
        lowerAction.find("backup") != std::string::npos ||
        lowerAction.find("restore") != std::string::npos ||
        lowerAction.find("audit") != std::string::npos ||
        lowerAction.find("delegation") != std::string::npos) {
        return "SYSTEM";
    }

    if (!trimmedTarget.empty() && trimmedTarget != "MULTI" && trimmedTarget != "N/A" && trimmedTarget[0] == 'B') {
        return "BUDGET";
    }

    if (!trimmedTarget.empty() && trimmedTarget != "MULTI" && trimmedTarget != "N/A") {
        return "DOCUMENT";
    }

    return "SYSTEM";
}

std::string inferOutcomeFromAction(const std::string& action, const std::string& targetType) {
    const std::string lowerAction = toLowerCopy(action);

    if (action == "UPLOAD_DOC") {
        return "PENDING";
    }
    if (action == "APPROVE_DOC") {
        return "APPROVED";
    }
    if (action == "REJECT_DOC") {
        return "REJECTED";
    }
    if (action == "BUDGET_ENTRY_SUBMIT") {
        return "PENDING";
    }
    if (action == "BUDGET_ENTRY_APPROVED" || action == "BUDGET_APPROVE") {
        return "APPROVED";
    }
    if (action == "BUDGET_ENTRY_REJECTED" || action == "BUDGET_REJECT") {
        return "REJECTED";
    }
    if (action == "BUDGET_ENTRY_PUBLISH" || action == "BUDGET_PUBLISH") {
        return "PUBLISHED";
    }

    if (lowerAction.find("failed") != std::string::npos ||
        lowerAction.find("fail") != std::string::npos ||
        lowerAction.find("error") != std::string::npos ||
        lowerAction.find("blocked") != std::string::npos ||
        lowerAction.find("denied") != std::string::npos ||
        lowerAction.find("not_found") != std::string::npos ||
        lowerAction.find("alert") != std::string::npos) {
        return "FAIL";
    }

    if (lowerAction.find("publish") != std::string::npos) {
        return "PUBLISHED";
    }
    if (lowerAction.find("approve") != std::string::npos) {
        return "APPROVED";
    }
    if (lowerAction.find("reject") != std::string::npos) {
        return "REJECTED";
    }
    if (lowerAction.find("upload") != std::string::npos && targetType == "DOCUMENT") {
        return "PENDING";
    }

    return "SUCCESS";
}

bool isPublicTrailAction(const std::string& action, const std::string& targetType, const std::string& actorRole) {
    if (action == "UPLOAD_DOC" ||
        action == "APPROVE_DOC" ||
        action == "REJECT_DOC" ||
        action == "BUDGET_ENTRY_SUBMIT" ||
        action == "BUDGET_ENTRY_APPROVED" ||
        action == "BUDGET_ENTRY_REJECTED" ||
        action == "BUDGET_ENTRY_PUBLISH" ||
        action == "BUDGET_APPROVE" ||
        action == "BUDGET_REJECT" ||
        action == "BUDGET_PUBLISH") {
        return true;
    }

    if (action == "UPDATE_STATUS") {
        return actorRole == "Super Admin" && (targetType == "DOCUMENT" || targetType == "BUDGET");
    }

    return false;
}

std::string inferVisibility(const std::string& action, const std::string& targetType, const std::string& actorRole) {
    return isPublicTrailAction(action, targetType, actorRole) ? "PUBLIC" : "INTERNAL";
}

std::string formatPublicActionLabel(const AuditEntry& row) {
    if (row.action == "UPLOAD_DOC") {
        return "Document Submitted";
    }
    if (row.action == "APPROVE_DOC") {
        return "Document Approved";
    }
    if (row.action == "REJECT_DOC") {
        return "Document Rejected";
    }
    if (row.action == "BUDGET_ENTRY_SUBMIT") {
        return "Budget Submitted";
    }
    if (row.action == "BUDGET_ENTRY_APPROVED" || row.action == "BUDGET_APPROVE") {
        return "Budget Approved";
    }
    if (row.action == "BUDGET_ENTRY_REJECTED" || row.action == "BUDGET_REJECT") {
        return "Budget Rejected";
    }
    if (row.action == "BUDGET_ENTRY_PUBLISH" || row.action == "BUDGET_PUBLISH") {
        return "Budget Published";
    }
    if (row.action == "UPDATE_STATUS") {
        if (row.outcome == "PUBLISHED") {
            return "Status Override: Published";
        }
        if (row.outcome == "REJECTED") {
            return "Status Override: Rejected";
        }
        if (row.outcome == "PENDING") {
            return "Status Override: Pending";
        }
        return "Status Override";
    }

    return row.action;
}

std::string formatOutcomeBadge(const std::string& outcome) {
    const std::string lower = toLowerCopy(outcome);
    if (lower == "approved") {
        return ui::consensusStatus("approved");
    }
    if (lower == "rejected") {
        return ui::consensusStatus("rejected");
    }
    if (lower == "published") {
        return ui::consensusStatus("published");
    }
    if (lower == "pending") {
        return ui::consensusStatus("pending");
    }
    if (lower == "fail") {
        return ui::error("Fail");
    }
    return ui::success(outcome.empty() ? "Success" : outcome);
}

std::string buildAuditHashPayload(const AuditEntry& row) {
    return row.timestamp + "|" + row.action + "|" + row.targetType + "|" + row.targetId + "|" +
           row.actorUsername + "|" + row.actorRole + "|" + row.outcome + "|" + row.visibility + "|" +
           row.chainIndex + "|" + row.previousHash;
}

bool validateEntryHash(const AuditEntry& row) {
    if (row.currentHash.empty()) {
        return false;
    }
    return computeSimpleHash(buildAuditHashPayload(row)) == row.currentHash;
}

AuditEntry finalizeEntry(const std::string& action,
                        const std::string& targetId,
                        const std::string& actor,
                        const AuditLogMetadata& metadata,
                        const std::map<std::string, std::string>& roleByUsername,
                        const std::map<std::string, std::string>& targetTypeById) {
    AuditEntry row;
    row.timestamp = getCurrentTimestamp();
    row.action = action;
    row.targetId = targetId;
    row.actorUsername = actor;
    row.actorRole = metadata.actorRole.empty() ? resolveActorRole(actor, roleByUsername) : metadata.actorRole;
    row.targetType = metadata.targetType.empty() ? inferTargetType(action, targetId, targetTypeById) : metadata.targetType;
    row.outcome = metadata.outcome.empty() ? inferOutcomeFromAction(action, row.targetType) : metadata.outcome;
    row.visibility = metadata.visibility.empty() ? inferVisibility(action, row.targetType, row.actorRole) : metadata.visibility;
    row.chainIndex = metadata.chainIndex >= 0 ? std::to_string(metadata.chainIndex) : "";
    row.previousHash = "";
    row.currentHash = "";
    return row;
}

bool parseAuditRow(const std::vector<std::string>& tokens,
                   AuditEntry& row,
                   const std::map<std::string, std::string>& roleByUsername,
                   const std::map<std::string, std::string>& targetTypeById) {
    if (tokens.size() >= 11) {
        row.timestamp = tokens[0];
        row.action = tokens[1];
        row.targetType = tokens[2];
        row.targetId = tokens[3];
        row.actorUsername = tokens[4];
        row.actorRole = tokens[5];
        row.outcome = tokens[6];
        row.visibility = tokens[7];
        row.chainIndex = tokens[8];
        row.previousHash = tokens[9];
        row.currentHash = tokens[10];
        return !row.timestamp.empty() && !row.action.empty();
    }

    if (tokens.size() >= 7) {
        row.timestamp = tokens[0];
        row.action = tokens[1];
        row.targetId = tokens[2];
        row.actorUsername = tokens[3];
        row.chainIndex = tokens[4];
        row.previousHash = tokens[5];
        row.currentHash = tokens[6];
        row.actorRole = resolveActorRole(row.actorUsername, roleByUsername);
        row.targetType = inferTargetType(row.action, row.targetId, targetTypeById);
        row.outcome = inferOutcomeFromAction(row.action, row.targetType);
        row.visibility = inferVisibility(row.action, row.targetType, row.actorRole);
        return !row.timestamp.empty() && !row.action.empty();
    }

    return false;
}
bool loadAuditEntries(std::vector<AuditEntry>& entries) {
    std::ifstream file;
    if (!openInputFileWithFallback(file, AUDIT_FILE_PATH_PRIMARY, AUDIT_FILE_PATH_FALLBACK)) {
        return false;
    }

    std::map<std::string, std::string> roleByUsername;
    std::map<std::string, std::string> targetTypeById;
    loadActorRoleLookup(roleByUsername);
    loadKnownTargetTypes(targetTypeById);

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

        AuditEntry row;
        if (!parseAuditRow(splitPipe(line), row, roleByUsername, targetTypeById)) {
            continue;
        }

        entries.push_back(row);
    }

    return true;
}

bool saveAuditEntries(const std::vector<AuditEntry>& rows) {
    std::vector<std::string> serializedRows;
    serializedRows.reserve(rows.size());
    for (std::size_t i = 0; i < rows.size(); ++i) {
        serializedRows.push_back(storage::joinPipeRow({
            rows[i].timestamp,
            rows[i].action,
            rows[i].targetType,
            rows[i].targetId,
            rows[i].actorUsername,
            rows[i].actorRole,
            rows[i].outcome,
            rows[i].visibility,
            rows[i].chainIndex,
            rows[i].previousHash,
            rows[i].currentHash
        }));
    }

    return storage::writePipeFileWithFallback(AUDIT_FILE_PATH_PRIMARY,
                                              AUDIT_FILE_PATH_FALLBACK,
                                              AUDIT_HEADER,
                                              serializedRows);
}

std::vector<AuditEntry> collectPublicEntries(const std::vector<AuditEntry>& entries) {
    std::vector<AuditEntry> filtered;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (toLowerCopy(entries[i].visibility) == "public") {
            filtered.push_back(entries[i]);
        }
    }
    return filtered;
}

std::vector<AuditEntry> applyPublicFilters(const std::vector<AuditEntry>& entries, const PublicAuditFilter& filter) {
    std::vector<AuditEntry> filtered;

    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (!isWithinDateRange(entries[i].timestamp, filter.fromDate, filter.toDate)) {
            continue;
        }

        if (filter.domain == "DOCUMENTS" && entries[i].targetType != "DOCUMENT") {
            continue;
        }

        if (filter.domain == "BUDGETS" && entries[i].targetType != "BUDGET") {
            continue;
        }

        if (!containsCaseInsensitive(entries[i].outcome, filter.outcome)) {
            continue;
        }

        filtered.push_back(entries[i]);
    }

    return filtered;
}

std::vector<AuditEntry> applyInternalFilters(const std::vector<AuditEntry>& entries, const InternalAuditFilter& filter) {
    std::vector<AuditEntry> filtered;

    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (!isWithinDateRange(entries[i].timestamp, filter.fromDate, filter.toDate)) {
            continue;
        }
        if (!containsCaseInsensitive(entries[i].action, filter.action)) {
            continue;
        }
        if (!containsCaseInsensitive(entries[i].actorUsername, filter.actor)) {
            continue;
        }
        if (!containsCaseInsensitive(entries[i].actorRole, filter.role)) {
            continue;
        }
        if (!containsCaseInsensitive(entries[i].targetType, filter.targetType)) {
            continue;
        }
        if (!containsCaseInsensitive(entries[i].targetId, filter.targetId)) {
            continue;
        }
        if (!containsCaseInsensitive(entries[i].outcome, filter.outcome)) {
            continue;
        }

        filtered.push_back(entries[i]);
    }

    return filtered;
}

void printPublicWhyThisMatters() {
    std::cout << "  " << ui::muted("Each visible event is hash-linked for traceability, and blockchain-backed actions include a chain reference.") << "\n";
    std::cout << "  " << ui::muted("This public trail focuses on procurement and budget governance events only.") << "\n";
}

void printPublicIntegritySummary(const std::vector<AuditEntry>& entries) {
    std::string latestChain = "N/A";
    std::string latestHash = "N/A";
    bool valid = !entries.empty();

    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (!entries[i].chainIndex.empty()) {
            latestChain = entries[i].chainIndex;
        }
        if (!entries[i].currentHash.empty()) {
            latestHash = shortHash(entries[i].currentHash, 18);
        }
        valid = valid && validateEntryHash(entries[i]);
    }

    std::cout << "\n" << ui::bold("Public Integrity Summary") << "\n";
    std::cout << "  Total public events : " << entries.size() << "\n";
    std::cout << "  Latest chain index  : " << latestChain << "\n";
    std::cout << "  Latest event hash   : " << latestHash << "\n";
    std::cout << "  Chain status        : " << (valid ? ui::success("OK") : ui::warning("Needs Review")) << "\n";
}

void printPublicFilterSuggestions(const std::vector<AuditEntry>& entries) {
    std::string oldestDate;
    std::string newestDate;
    std::map<std::string, int> outcomes;

    for (std::size_t i = 0; i < entries.size(); ++i) {
        outcomes[entries[i].outcome] += 1;
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
    std::cout << "  Domains  : All, Documents, Budgets\n";
    if (!oldestDate.empty()) {
        std::cout << "  Date span: " << oldestDate << " to " << newestDate << "\n";
    }
    std::cout << "  Outcomes : ";
    bool wroteAny = false;
    for (std::map<std::string, int>::const_iterator it = outcomes.begin(); it != outcomes.end(); ++it) {
        if (it->first.empty()) {
            continue;
        }
        if (wroteAny) {
            std::cout << ", ";
        }
        std::cout << it->first;
        wroteAny = true;
    }
    if (!wroteAny) {
        std::cout << "(none)";
    }
    std::cout << "\n";
}

void printInternalFilterSuggestions(const std::vector<AuditEntry>& entries) {
    std::string oldestDate;
    std::string newestDate;
    std::map<std::string, int> actions;
    std::map<std::string, int> roles;
    std::map<std::string, int> targetTypes;

    for (std::size_t i = 0; i < entries.size(); ++i) {
        actions[entries[i].action] += 1;
        roles[entries[i].actorRole] += 1;
        targetTypes[entries[i].targetType] += 1;
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
    if (!oldestDate.empty()) {
        std::cout << "  Date span  : " << oldestDate << " to " << newestDate << "\n";
    }
    std::cout << "  Action hint: " << (actions.empty() ? "(none)" : actions.begin()->first) << "\n";
    std::cout << "  Role hint  : " << (roles.empty() ? "(none)" : roles.begin()->first) << "\n";
    std::cout << "  Type hint  : " << (targetTypes.empty() ? "(none)" : targetTypes.begin()->first) << "\n";
}
bool promptPublicFilters(PublicAuditFilter& filter, const std::vector<AuditEntry>& entries) {
    clearScreen();
    ui::printSectionTitle("PUBLIC AUDIT TRAIL FILTERS");
    printPublicWhyThisMatters();
    printPublicFilterSuggestions(entries);

    std::cout << "\nSelect domain:\n";
    std::cout << "  " << ui::info("[1]") << " All\n";
    std::cout << "  " << ui::info("[2]") << " Documents\n";
    std::cout << "  " << ui::info("[3]") << " Budgets\n";
    std::cout << "  Enter choice: ";

    int domainChoice = 1;
    std::cin >> domainChoice;
    if (std::cin.fail()) {
        std::cin.clear();
        clearInputBuffer();
        domainChoice = 1;
    } else {
        clearInputBuffer();
    }

    if (domainChoice == 2) {
        filter.domain = "DOCUMENTS";
    } else if (domainChoice == 3) {
        filter.domain = "BUDGETS";
    } else {
        filter.domain = "ALL";
    }

    std::cout << "From date (YYYY-MM-DD, optional): ";
    std::getline(std::cin, filter.fromDate);
    std::cout << "To date (YYYY-MM-DD, optional): ";
    std::getline(std::cin, filter.toDate);
    std::cout << "Outcome contains (optional): ";
    std::getline(std::cin, filter.outcome);

    filter.fromDate = trimCopy(filter.fromDate);
    filter.toDate = trimCopy(filter.toDate);
    filter.outcome = trimCopy(filter.outcome);

    return isDateTextValid(filter.fromDate) && isDateTextValid(filter.toDate) &&
           (filter.fromDate.empty() || filter.toDate.empty() || filter.fromDate <= filter.toDate);
}

bool promptInternalFilters(InternalAuditFilter& filter, const std::vector<AuditEntry>& entries) {
    clearScreen();
    ui::printSectionTitle("INTERNAL AUDIT LOG FILTERS");
    printInternalFilterSuggestions(entries);

    std::cout << "From date (YYYY-MM-DD, optional): ";
    clearInputBuffer();
    std::getline(std::cin, filter.fromDate);
    std::cout << "To date (YYYY-MM-DD, optional): ";
    std::getline(std::cin, filter.toDate);
    std::cout << "Action contains (optional): ";
    std::getline(std::cin, filter.action);
    std::cout << "Actor username contains (optional): ";
    std::getline(std::cin, filter.actor);
    std::cout << "Actor role contains (optional): ";
    std::getline(std::cin, filter.role);
    std::cout << "Target type contains (optional): ";
    std::getline(std::cin, filter.targetType);
    std::cout << "Target ID contains (optional): ";
    std::getline(std::cin, filter.targetId);
    std::cout << "Outcome contains (optional): ";
    std::getline(std::cin, filter.outcome);

    filter.fromDate = trimCopy(filter.fromDate);
    filter.toDate = trimCopy(filter.toDate);
    filter.action = trimCopy(filter.action);
    filter.actor = trimCopy(filter.actor);
    filter.role = trimCopy(filter.role);
    filter.targetType = trimCopy(filter.targetType);
    filter.targetId = trimCopy(filter.targetId);
    filter.outcome = trimCopy(filter.outcome);

    return isDateTextValid(filter.fromDate) && isDateTextValid(filter.toDate) &&
           (filter.fromDate.empty() || filter.toDate.empty() || filter.fromDate <= filter.toDate);
}

void printPublicTimeline(const std::vector<AuditEntry>& entries) {
    std::cout << "\n" << ui::bold("Transparency Timeline") << "\n";

    for (std::size_t i = 0; i < entries.size(); ++i) {
        const std::string targetTypeLower = toLowerCopy(entries[i].targetType);
        const std::string publicDocumentId =
            (targetTypeLower == "documents" || targetTypeLower == "document") ? entries[i].targetId : "N/A";

        std::cout << ui::info("--------------------------------------------------------------") << "\n";
        std::cout << "  " << ui::bold(formatPublicActionLabel(entries[i])) << "  " << formatOutcomeBadge(entries[i].outcome) << "\n";
        std::cout << "  Target       : " << entries[i].targetId << " (" << entries[i].targetType << ")\n";
        std::cout << "  Document ID  : " << publicDocumentId << "\n";
        std::cout << "  Actor Role   : " << ui::roleLabel(entries[i].actorRole) << "\n";
        std::cout << "  Timestamp    : " << entries[i].timestamp << "\n";
        std::cout << "  Chain Index  : " << (entries[i].chainIndex.empty() ? "N/A" : ("#" + entries[i].chainIndex)) << "\n";
        std::cout << "  Prev Hash    : " << shortHash(entries[i].previousHash, 18) << "\n";
        std::cout << "  Curr Hash    : " << shortHash(entries[i].currentHash, 18) << "\n";
        if (i + 1 < entries.size()) {
            std::cout << "      |\n";
            std::cout << "      v\n";
        }
    }
    std::cout << ui::info("--------------------------------------------------------------") << "\n";
}

std::vector<std::string> collectPublicDocumentTargets(const std::vector<AuditEntry>& entries) {
    std::vector<std::string> out;

    for (std::size_t i = 0; i < entries.size(); ++i) {
        const std::string targetType = toLowerCopy(entries[i].targetType);
        if (targetType != "documents" && targetType != "document") {
            continue;
        }

        if (entries[i].targetId.empty()) {
            continue;
        }

        bool exists = false;
        for (std::size_t j = 0; j < out.size(); ++j) {
            if (out[j] == entries[i].targetId) {
                exists = true;
                break;
            }
        }

        if (!exists) {
            out.push_back(entries[i].targetId);
        }
    }

    std::sort(out.begin(), out.end());
    return out;
}

void printInternalAuditTable(const std::vector<AuditEntry>& entries) {
    const std::vector<std::string> headers = {"Timestamp", "Action", "Type", "Target", "Actor", "Role", "Outcome", "Visibility", "Chain", "Hash"};
    const std::vector<int> widths = {19, 20, 10, 12, 18, 18, 10, 10, 7, 14};

    ui::printTableHeader(headers, widths);
    for (std::size_t i = 0; i < entries.size(); ++i) {
        ui::printTableRow({entries[i].timestamp,
                           entries[i].action,
                           entries[i].targetType,
                           entries[i].targetId,
                           entries[i].actorUsername,
                           entries[i].actorRole,
                           entries[i].outcome,
                           entries[i].visibility,
                           entries[i].chainIndex,
                           shortHash(entries[i].currentHash, 14)},
                          widths);
    }
    ui::printTableFooter(widths);
}

bool writePublicCsvRows(const std::string& outputPath, const std::vector<AuditEntry>& rows) {
    std::ofstream out(outputPath);
    if (!out.is_open()) {
        return false;
    }

    out << "timestamp,publicLabel,targetType,targetID,actorRole,outcome,chainIndex,previousHash,currentHash\n";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        out << csvEscape(rows[i].timestamp) << ','
            << csvEscape(formatPublicActionLabel(rows[i])) << ','
            << csvEscape(rows[i].targetType) << ','
            << csvEscape(rows[i].targetId) << ','
            << csvEscape(rows[i].actorRole) << ','
            << csvEscape(rows[i].outcome) << ','
            << csvEscape(rows[i].chainIndex) << ','
            << csvEscape(rows[i].previousHash) << ','
            << csvEscape(rows[i].currentHash) << '\n';
    }

    out.flush();
    return true;
}

bool writeInternalCsvRows(const std::string& outputPath, const std::vector<AuditEntry>& rows) {
    std::ofstream out(outputPath);
    if (!out.is_open()) {
        return false;
    }

    out << "timestamp,action,targetType,targetID,actorUsername,actorRole,outcome,visibility,chainIndex,previousHash,currentHash\n";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        out << csvEscape(rows[i].timestamp) << ','
            << csvEscape(rows[i].action) << ','
            << csvEscape(rows[i].targetType) << ','
            << csvEscape(rows[i].targetId) << ','
            << csvEscape(rows[i].actorUsername) << ','
            << csvEscape(rows[i].actorRole) << ','
            << csvEscape(rows[i].outcome) << ','
            << csvEscape(rows[i].visibility) << ','
            << csvEscape(rows[i].chainIndex) << ','
            << csvEscape(rows[i].previousHash) << ','
            << csvEscape(rows[i].currentHash) << '\n';
    }

    out.flush();
    return true;
}

void exportDisplayedPublicRows(const std::string& actor, const std::vector<AuditEntry>& rows) {
    clearInputBuffer();
    std::string fileName;
    std::cout << "Output filename (.csv, blank for default): ";
    std::getline(std::cin, fileName);

    fileName = trimCopy(fileName);
    if (fileName.empty()) {
        fileName = defaultExportFileName("public_audit_export");
    } else if (!isSafeExportFileName(fileName)) {
        std::cout << ui::error("[!] Invalid filename. Use letters, numbers, '_', '-', and '.'.") << "\n";
        waitForEnter();
        return;
    }

    if (fileName.size() < 4 || fileName.substr(fileName.size() - 4) != ".csv") {
        fileName += ".csv";
    }

    const std::string outputPath = chooseExportPath(fileName);
    if (outputPath.empty() || !writePublicCsvRows(outputPath, rows)) {
        std::cout << ui::error("[!] Unable to write export file.") << "\n";
        waitForEnter();
        return;
    }

    logAuditAction("PUBLIC_AUDIT_EXPORT_CSV", fileName, actor);
    std::cout << ui::success("[+] Export completed: ") << outputPath << "\n";
    std::cout << ui::info("[i] Rows exported: ") << rows.size() << "\n";
    waitForEnter();
}

void exportDisplayedInternalRows(const std::string& actor, const std::vector<AuditEntry>& rows) {
    clearInputBuffer();
    std::string fileName;
    std::cout << "Output filename (.csv, blank for default): ";
    std::getline(std::cin, fileName);

    fileName = trimCopy(fileName);
    if (fileName.empty()) {
        fileName = defaultExportFileName("internal_audit_export");
    } else if (!isSafeExportFileName(fileName)) {
        std::cout << ui::error("[!] Invalid filename. Use letters, numbers, '_', '-', and '.'.") << "\n";
        waitForEnter();
        return;
    }

    if (fileName.size() < 4 || fileName.substr(fileName.size() - 4) != ".csv") {
        fileName += ".csv";
    }

    const std::string outputPath = chooseExportPath(fileName);
    if (outputPath.empty() || !writeInternalCsvRows(outputPath, rows)) {
        std::cout << ui::error("[!] Unable to write export file.") << "\n";
        waitForEnter();
        return;
    }

    logAuditAction("INTERNAL_AUDIT_EXPORT_CSV", fileName, actor);
    std::cout << ui::success("[+] Export completed: ") << outputPath << "\n";
    std::cout << ui::info("[i] Rows exported: ") << rows.size() << "\n";
    waitForEnter();
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

void ensureAuditTrailHashChain() {
    std::vector<AuditEntry> entries;
    if (!loadAuditEntries(entries)) {
        return;
    }

    std::string previousHash = "0000";
    for (std::size_t i = 0; i < entries.size(); ++i) {
        entries[i].previousHash = previousHash;
        entries[i].currentHash = computeSimpleHash(buildAuditHashPayload(entries[i]));
        previousHash = entries[i].currentHash;
    }

    saveAuditEntries(entries);
}

bool tryLogAuditAction(const std::string& action, const std::string& targetId, const std::string& actor, int chainIndex) {
    AuditLogMetadata metadata;
    metadata.chainIndex = chainIndex;
    return tryLogAuditActionDetailed(action, targetId, actor, metadata);
}

bool tryLogAuditActionDetailed(const std::string& action,
                               const std::string& targetId,
                               const std::string& actor,
                               const AuditLogMetadata& metadata) {
    std::vector<AuditEntry> entries;
    if (!loadAuditEntries(entries)) {
        entries.clear();
    }

    std::map<std::string, std::string> roleByUsername;
    std::map<std::string, std::string> targetTypeById;
    loadActorRoleLookup(roleByUsername);
    loadKnownTargetTypes(targetTypeById);

    AuditEntry row = finalizeEntry(action, targetId, actor, metadata, roleByUsername, targetTypeById);
    row.previousHash = entries.empty() ? "0000" : entries.back().currentHash;
    if (row.previousHash.empty()) {
        row.previousHash = "0000";
    }
    row.currentHash = computeSimpleHash(buildAuditHashPayload(row));

    entries.push_back(row);
    return saveAuditEntries(entries);
}

void logAuditAction(const std::string& action, const std::string& targetId, const std::string& actor, int chainIndex) {
    tryLogAuditAction(action, targetId, actor, chainIndex);
}

void logAuditActionDetailed(const std::string& action,
                           const std::string& targetId,
                           const std::string& actor,
                           const AuditLogMetadata& metadata) {
    tryLogAuditActionDetailed(action, targetId, actor, metadata);
}

void exportAuditTrailCsv(const std::string& actor) {
    std::vector<AuditEntry> entries;
    if (!loadAuditEntries(entries)) {
        std::cout << ui::error("[!] Unable to open audit log file.") << "\n";
        waitForEnter();
        return;
    }

    std::vector<AuditEntry> publicEntries = collectPublicEntries(entries);
    if (publicEntries.empty()) {
        std::cout << ui::warning("[!] No public audit entries available to export.") << "\n";
        waitForEnter();
        return;
    }

    exportDisplayedPublicRows(actor, publicEntries);
}

void viewAuditTrail(const std::string& actor) {
    viewPublicAuditTrail(actor);
}

void viewPublicAuditTrail(const std::string& actorUsername) {
    PublicAuditFilter filter;
    filter.domain = "ALL";
    bool shouldPromptFilters = true;

    while (true) {
        std::vector<AuditEntry> entries;
        if (!loadAuditEntries(entries)) {
            clearScreen();
            ui::printSectionTitle("PUBLIC AUDIT TRAIL");
            std::cout << ui::error("[!] Unable to open audit log file.") << "\n";
            waitForEnter();
            return;
        }

        const std::vector<AuditEntry> publicEntries = collectPublicEntries(entries);
        if (publicEntries.empty()) {
            clearScreen();
            ui::printSectionTitle("PUBLIC AUDIT TRAIL");
            std::cout << ui::warning("[!] No public audit entries available yet.") << "\n";
            logAuditAction("VIEW_PUBLIC_AUDIT_TRAIL_EMPTY", "PUBLIC", actorUsername);
            waitForEnter();
            return;
        }

        if (shouldPromptFilters) {
            if (!promptPublicFilters(filter, publicEntries)) {
                std::cout << ui::error("[!] Invalid date range. Use YYYY-MM-DD.") << "\n";
                waitForEnter();
                shouldPromptFilters = true;
                continue;
            }
            shouldPromptFilters = false;
        }

        const std::vector<AuditEntry> filtered = applyPublicFilters(publicEntries, filter);

        clearScreen();
        ui::printSectionTitle("PUBLIC AUDIT TRAIL");
        printPublicWhyThisMatters();
        printPublicIntegritySummary(filtered);

        if (filtered.empty()) {
            std::cout << "\n" << ui::warning("[!] No public rows matched the selected filters.") << "\n";
        } else {
            printPublicTimeline(filtered);
        }

        logAuditAction("VIEW_PUBLIC_AUDIT_TRAIL", filtered.empty() ? "EMPTY" : "MULTI", actorUsername);

        std::cout << "\n" << ui::info("[1]") << " Export displayed public CSV\n";
        std::cout << "  " << ui::info("[2]") << " Change filters\n";
        std::cout << "  " << ui::info("[3]") << " Open document detail\n";
        std::cout << "  " << ui::info("[0]") << " Back\n";
        std::cout << ui::muted("--------------------------------------------------------------") << "\n";
        std::cout << "  Enter your choice: ";

        int choice = -1;
        std::cin >> choice;
        if (std::cin.fail()) {
            std::cin.clear();
            clearInputBuffer();
            continue;
        }

        if (choice == 1 && !filtered.empty()) {
            exportDisplayedPublicRows(actorUsername, filtered);
            continue;
        }

        if (choice == 2) {
            shouldPromptFilters = true;
            continue;
        }

        if (choice == 3) {
            const std::vector<std::string> documentTargets = collectPublicDocumentTargets(filtered);
            if (documentTargets.empty()) {
                std::cout << ui::warning("[!] No document IDs are available in the current timeline view.") << "\n";
                waitForEnter();
                continue;
            }

            std::cout << "\n" << ui::bold("Available Document IDs") << "\n";
            for (std::size_t i = 0; i < documentTargets.size(); ++i) {
                std::cout << "  - " << documentTargets[i] << "\n";
            }

            clearInputBuffer();
            std::string docId;
            std::cout << "Enter Document ID to open detail (blank to cancel): ";
            std::getline(std::cin, docId);

            docId = trimCopy(docId);
            if (docId.empty()) {
                continue;
            }

            viewPublishedDocumentDetailForCitizenById(docId, actorUsername);
            continue;
        }

        break;
    }
}

void viewInternalAuditLog(const Admin& admin) {
    if (admin.role != "Super Admin") {
        clearScreen();
        ui::printSectionTitle("INTERNAL AUDIT LOG");
        std::cout << ui::error("[!] Only Super Admin can access the internal audit log.") << "\n";
        logAuditAction("ACCESS_DENIED_INTERNAL_AUDIT_LOG", "N/A", admin.username);
        waitForEnter();
        return;
    }

    InternalAuditFilter filter;
    bool shouldPromptFilters = true;

    while (true) {
        std::vector<AuditEntry> entries;
        if (!loadAuditEntries(entries)) {
            clearScreen();
            ui::printSectionTitle("INTERNAL AUDIT LOG");
            std::cout << ui::error("[!] Unable to open audit log file.") << "\n";
            waitForEnter();
            return;
        }

        if (shouldPromptFilters) {
            if (!promptInternalFilters(filter, entries)) {
                std::cout << ui::error("[!] Invalid date range. Use YYYY-MM-DD.") << "\n";
                waitForEnter();
                shouldPromptFilters = true;
                continue;
            }
            shouldPromptFilters = false;
        }

        const std::vector<AuditEntry> filtered = applyInternalFilters(entries, filter);

        clearScreen();
        ui::printSectionTitle("INTERNAL AUDIT LOG");
        std::cout << "  " << ui::muted("Super Admin view: complete operational, security, and governance history.") << "\n";

        if (filtered.empty()) {
            std::cout << "\n" << ui::warning("[!] No internal rows matched the selected filters.") << "\n";
        } else {
            printInternalAuditTable(filtered);
        }

        logAuditAction("VIEW_INTERNAL_AUDIT_LOG", filtered.empty() ? "EMPTY" : "MULTI", admin.username);

        std::cout << "\n" << ui::info("[1]") << " Export displayed internal CSV\n";
        std::cout << "  " << ui::info("[2]") << " Change filters\n";
        std::cout << "  " << ui::info("[0]") << " Back\n";
        std::cout << ui::muted("--------------------------------------------------------------") << "\n";
        std::cout << "  Enter your choice: ";

        int choice = -1;
        std::cin >> choice;
        if (std::cin.fail()) {
            std::cin.clear();
            clearInputBuffer();
            continue;
        }

        if (choice == 1 && !filtered.empty()) {
            exportDisplayedInternalRows(admin.username, filtered);
            continue;
        }

        if (choice == 2) {
            shouldPromptFilters = true;
            continue;
        }

        break;
    }
}
