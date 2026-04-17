#include "../include/approvals.h"

#include "../include/audit.h"
#include "../include/auth.h"
#include "../include/blockchain.h"
#include "../include/delegation.h"
#include "../include/documents.h"
#include "../include/ui.h"

#include <ctime>
#include <cctype>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <vector>

namespace {
// Approval storage is file-based. Every operation here uses a primary path plus
// a fallback path so the program works from either project root or src folder.
const std::string APPROVALS_FILE_PATH_PRIMARY = "data/approvals.txt";
const std::string APPROVALS_FILE_PATH_FALLBACK = "../data/approvals.txt";
const std::string APPROVAL_RULES_FILE_PATH_PRIMARY = "data/approval_rules.txt";
const std::string APPROVAL_RULES_FILE_PATH_FALLBACK = "../data/approval_rules.txt";
const std::string APPROVAL_COMMENTS_FILE_PATH_PRIMARY = "data/approval_comments.txt";
const std::string APPROVAL_COMMENTS_FILE_PATH_FALLBACK = "../data/approval_comments.txt";
const std::string ADMINS_FILE_PATH_PRIMARY = "data/admins.txt";
const std::string ADMINS_FILE_PATH_FALLBACK = "../data/admins.txt";
const std::string DOCUMENTS_FILE_PATH_PRIMARY = "data/documents.txt";
const std::string DOCUMENTS_FILE_PATH_FALLBACK = "../data/documents.txt";

struct DocumentContext {
    std::string title;
    std::string category;
    std::string status;
    std::string uploader;
};

struct PendingApprovalSlaRow {
    std::string docId;
    std::string title;
    std::string category;
    std::string approverUsername;
    std::string role;
    std::string createdAt;
    int pendingDays;
    int slaDays;
    int overdueDays;
};

struct ApprovalComment {
    std::string docId;
    std::string commenterUsername;
    std::string commenterRole;
    std::string createdAt;
    std::string commentText;
};

bool openInputFileWithFallback(std::ifstream& file, const std::string& primaryPath, const std::string& fallbackPath) {
    // Constant-time path fallback: try primary, then fallback.
    file.open(primaryPath);
    if (file.is_open()) {
        return true;
    }

    file.clear();
    file.open(fallbackPath);
    return file.is_open();
}

std::string resolveDataPath(const std::string& primaryPath, const std::string& fallbackPath) {
    // Resolve the write target once so save routines stay deterministic.
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

std::vector<std::string> splitPipe(const std::string& line) {
    // Linear tokenize pass over one line: O(m), where m is line length.
    std::vector<std::string> tokens;
    std::stringstream parser(line);
    std::string token;
    while (std::getline(parser, token, '|')) {
        tokens.push_back(token);
    }
    return tokens;
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

std::string toLowerCopy(std::string value) {
    for (std::size_t i = 0; i < value.size(); ++i) {
        value[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(value[i])));
    }
    return value;
}

std::string sanitizeCommentText(const std::string& value) {
    std::string cleaned = trimCopy(value);

    for (std::size_t i = 0; i < cleaned.size(); ++i) {
        if (cleaned[i] == '|' || cleaned[i] == '\n' || cleaned[i] == '\r') {
            cleaned[i] = ' ';
        }
    }

    return trimCopy(cleaned);
}

std::vector<std::string> defaultRequiredRoles() {
    std::vector<std::string> roles;
    roles.push_back("Budget Officer");
    roles.push_back("Municipal Administrator");
    return roles;
}

std::vector<std::string> splitRolesCsv(const std::string& value) {
    std::vector<std::string> roles;
    std::stringstream parser(value);
    std::string token;

    while (std::getline(parser, token, ',')) {
        token = trimCopy(token);
        if (!token.empty()) {
            roles.push_back(token);
        }
    }

    return roles;
}

std::string joinRolesCsv(const std::vector<std::string>& roles) {
    std::string output;
    for (std::size_t i = 0; i < roles.size(); ++i) {
        if (!output.empty()) {
            output += ",";
        }
        output += roles[i];
    }

    return output;
}

bool roleExistsInRule(const std::vector<std::string>& requiredRoles, const std::string& role) {
    for (std::size_t i = 0; i < requiredRoles.size(); ++i) {
        if (requiredRoles[i] == role) {
            return true;
        }
    }
    return false;
}

ApprovalRule buildDefaultRule() {
    ApprovalRule rule;
    rule.category = "DEFAULT";
    rule.requiredRoles = defaultRequiredRoles();
    rule.maxDecisionDays = 7;
    return rule;
}

ApprovalRule parseApprovalRuleTokens(const std::vector<std::string>& tokens) {
    ApprovalRule rule = buildDefaultRule();
    rule.category = tokens.size() > 0 ? trimCopy(tokens[0]) : "DEFAULT";

    if (rule.category.empty()) {
        rule.category = "DEFAULT";
    }

    if (tokens.size() > 1) {
        const std::vector<std::string> parsedRoles = splitRolesCsv(tokens[1]);
        if (!parsedRoles.empty()) {
            rule.requiredRoles = parsedRoles;
        }
    }

    if (tokens.size() > 2) {
        std::stringstream in(tokens[2]);
        int parsedDays = 0;
        in >> parsedDays;
        if (!in.fail() && parsedDays > 0) {
            rule.maxDecisionDays = parsedDays;
        }
    }

    return rule;
}

std::string serializeApprovalRule(const ApprovalRule& rule) {
    std::ostringstream out;
    out << rule.category << "|" << joinRolesCsv(rule.requiredRoles) << "|" << rule.maxDecisionDays;
    return out.str();
}

bool loadApprovalRules(std::vector<ApprovalRule>& rules) {
    std::ifstream file;
    if (!openInputFileWithFallback(file, APPROVAL_RULES_FILE_PATH_PRIMARY, APPROVAL_RULES_FILE_PATH_FALLBACK)) {
        return false;
    }

    rules.clear();
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

        ApprovalRule rule = parseApprovalRuleTokens(splitPipe(line));
        if (rule.category.empty()) {
            continue;
        }

        rules.push_back(rule);
    }

    return true;
}

bool saveApprovalRules(const std::vector<ApprovalRule>& rules) {
    std::ofstream writer(resolveDataPath(APPROVAL_RULES_FILE_PATH_PRIMARY, APPROVAL_RULES_FILE_PATH_FALLBACK));
    if (!writer.is_open()) {
        return false;
    }

    writer << "category|requiredRoles|maxDecisionDays\n";
    for (std::size_t i = 0; i < rules.size(); ++i) {
        writer << serializeApprovalRule(rules[i]) << '\n';
    }
    writer.flush();
    return true;
}

ApprovalRule resolveRuleForCategory(const std::string& category) {
    std::vector<ApprovalRule> rules;
    ApprovalRule defaultRule = buildDefaultRule();

    if (!loadApprovalRules(rules) || rules.empty()) {
        return defaultRule;
    }

    const std::string categoryKey = toLowerCopy(trimCopy(category));

    for (std::size_t i = 0; i < rules.size(); ++i) {
        if (toLowerCopy(trimCopy(rules[i].category)) == "default") {
            defaultRule = rules[i];
            break;
        }
    }

    for (std::size_t i = 0; i < rules.size(); ++i) {
        if (toLowerCopy(trimCopy(rules[i].category)) == categoryKey) {
            return rules[i];
        }
    }

    return defaultRule;
}

Approval parseApprovalTokens(const std::vector<std::string>& tokens) {
    // Backward-compatible parser: missing trailing fields are defaulted.
    Approval row;
    row.docId = tokens.size() > 0 ? tokens[0] : "";
    row.approverUsername = tokens.size() > 1 ? tokens[1] : "";
    row.role = tokens.size() > 2 ? tokens[2] : "";
    row.status = tokens.size() > 3 ? tokens[3] : "pending";
    row.createdAt = tokens.size() > 4 ? tokens[4] : "";
    row.decidedAt = tokens.size() > 5 ? tokens[5] : "";
    row.note = tokens.size() > 6 ? tokens[6] : "";

    if (row.createdAt.empty()) {
        row.createdAt = getCurrentTimestamp();
    }

    return row;
}

std::string serializeApproval(const Approval& row) {
    // Keep schema order stable because other modules parse this exact layout.
    return row.docId + "|" + row.approverUsername + "|" + row.role + "|" + row.status + "|" + row.createdAt + "|" + row.decidedAt + "|" + row.note;
}

bool loadApprovals(std::vector<Approval>& rows) {
    // Full file scan: O(n) lines, O(n) memory for loaded rows.
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

        Approval parsed = parseApprovalTokens(splitPipe(line));
        if (parsed.docId.empty() || parsed.approverUsername.empty()) {
            continue;
        }

        rows.push_back(parsed);
    }

    return true;
}

bool saveApprovals(const std::vector<Approval>& rows) {
    // Rewrites the whole table each save for a canonical on-disk state: O(n).
    std::ofstream writer(resolveDataPath(APPROVALS_FILE_PATH_PRIMARY, APPROVALS_FILE_PATH_FALLBACK));
    if (!writer.is_open()) {
        return false;
    }

    writer << "docID|approverUsername|role|status|createdAt|decidedAt|note\n";
    for (size_t i = 0; i < rows.size(); ++i) {
        writer << serializeApproval(rows[i]) << '\n';
    }
    writer.flush();
    return true;
}

ApprovalComment parseApprovalCommentTokens(const std::vector<std::string>& tokens) {
    ApprovalComment row;
    row.docId = tokens.size() > 0 ? tokens[0] : "";
    row.commenterUsername = tokens.size() > 1 ? tokens[1] : "";
    row.commenterRole = tokens.size() > 2 ? tokens[2] : "";
    row.createdAt = tokens.size() > 3 ? tokens[3] : getCurrentTimestamp();
    row.commentText = tokens.size() > 4 ? tokens[4] : "";
    return row;
}

std::string serializeApprovalComment(const ApprovalComment& row) {
    return row.docId + "|" + row.commenterUsername + "|" + row.commenterRole + "|" + row.createdAt + "|" + row.commentText;
}

bool loadApprovalComments(std::vector<ApprovalComment>& rows) {
    std::ifstream file;
    if (!openInputFileWithFallback(file, APPROVAL_COMMENTS_FILE_PATH_PRIMARY, APPROVAL_COMMENTS_FILE_PATH_FALLBACK)) {
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

        ApprovalComment parsed = parseApprovalCommentTokens(splitPipe(line));
        if (parsed.docId.empty() || parsed.commenterUsername.empty()) {
            continue;
        }

        rows.push_back(parsed);
    }

    return true;
}

bool saveApprovalComments(const std::vector<ApprovalComment>& rows) {
    std::ofstream writer(resolveDataPath(APPROVAL_COMMENTS_FILE_PATH_PRIMARY, APPROVAL_COMMENTS_FILE_PATH_FALLBACK));
    if (!writer.is_open()) {
        return false;
    }

    writer << "docID|commenterUsername|commenterRole|createdAt|commentText\n";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        writer << serializeApprovalComment(rows[i]) << '\n';
    }
    writer.flush();
    return true;
}

bool appendApprovalComment(const std::string& docId,
                           const std::string& commenterUsername,
                           const std::string& commenterRole,
                           const std::string& commentText) {
    std::vector<ApprovalComment> rows;
    loadApprovalComments(rows);

    ApprovalComment newRow;
    newRow.docId = docId;
    newRow.commenterUsername = commenterUsername;
    newRow.commenterRole = commenterRole;
    newRow.createdAt = getCurrentTimestamp();
    newRow.commentText = sanitizeCommentText(commentText);

    if (newRow.commentText.empty()) {
        return false;
    }

    rows.push_back(newRow);
    return saveApprovalComments(rows);
}

void ensureApprovalCommentsDataFileExists() {
    std::ifstream checkFile;
    if (!openInputFileWithFallback(checkFile, APPROVAL_COMMENTS_FILE_PATH_PRIMARY, APPROVAL_COMMENTS_FILE_PATH_FALLBACK)) {
        std::ofstream createFile(APPROVAL_COMMENTS_FILE_PATH_PRIMARY);
        if (!createFile.is_open()) {
            createFile.clear();
            createFile.open(APPROVAL_COMMENTS_FILE_PATH_FALLBACK);
        }

        if (createFile.is_open()) {
            createFile << "docID|commenterUsername|commenterRole|createdAt|commentText\n";
        }
    }

    std::vector<ApprovalComment> existing;
    if (loadApprovalComments(existing)) {
        saveApprovalComments(existing);
    }
}

void seedApprovalsIfEmpty() {
    // Mock approval scenarios are maintained in data/approvals.txt.
    // When empty, keep the file normalized and let runtime operations populate it.
    std::vector<Approval> rows;
    if (!loadApprovals(rows)) {
        return;
    }

    saveApprovals(rows);
}

void clearInputBuffer() {
    // Removes unread input so getline calls are not skipped by stale newlines.
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

void printConsensusLegend() {
    std::cout << "  Consensus Legend: "
              << ui::consensusStatus("approved") << " | "
              << ui::consensusStatus("denied") << " | "
              << ui::consensusStatus("pending") << "\n";
}

std::string buildConsensusProgressText(int approved, int total) {
    if (total <= 0) {
        return "[ ] 0/0";
    }

    const int boundedApproved = std::max(0, std::min(approved, total));
    return "[" + std::string(static_cast<std::size_t>(boundedApproved), '#') +
           std::string(static_cast<std::size_t>(total - boundedApproved), '-') +
           "] " + std::to_string(boundedApproved) + "/" + std::to_string(total);
}

bool printPendingDecisionHints(const std::vector<Approval>& rows, const std::string& approverUsername) {
    std::vector<Approval> pendingRows;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].approverUsername == approverUsername && rows[i].status == "pending") {
            pendingRows.push_back(rows[i]);
        }
    }

    // Also include delegated pending items
    std::vector<Delegation> delegations = getActiveDelegationsFor(approverUsername);
    std::vector<Approval> delegatedRows;
    for (size_t d = 0; d < delegations.size(); ++d) {
        for (size_t i = 0; i < rows.size(); ++i) {
            if (rows[i].approverUsername == delegations[d].delegatorUsername && rows[i].status == "pending") {
                delegatedRows.push_back(rows[i]);
            }
        }
    }

    if (pendingRows.empty() && delegatedRows.empty()) {
        std::cout << "\n" << ui::warning("[!] You have no pending approvals right now.") << "\n";
        return false;
    }

    std::cout << "\n" << ui::bold("Available Pending Decisions") << "\n";
    const std::vector<std::string> headers = {"Document ID", "Role", "Created", "Via"};
    const std::vector<int> widths = {12, 24, 19, 16};
    ui::printTableHeader(headers, widths);

    for (std::size_t i = 0; i < pendingRows.size(); ++i) {
        ui::printTableRow({pendingRows[i].docId, pendingRows[i].role, pendingRows[i].createdAt, "Direct"}, widths);
    }
    for (std::size_t i = 0; i < delegatedRows.size(); ++i) {
        ui::printTableRow({delegatedRows[i].docId, delegatedRows[i].role, delegatedRows[i].createdAt, "Delegated"}, widths);
    }

    ui::printTableFooter(widths);
    std::cout << "  " << ui::muted("Tip: enter one of the listed Document IDs.") << "\n";
    return true;
}

bool resolvePendingDecisionOwner(const std::vector<Approval>& rows,
                                const std::string& approverUsername,
                                const std::string& targetDocId,
                                std::string& ownerUsername) {
    ownerUsername.clear();

    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].docId == targetDocId && rows[i].approverUsername == approverUsername && rows[i].status == "pending") {
            ownerUsername = approverUsername;
            return true;
        }
    }

    const std::vector<Delegation> delegations = getActiveDelegationsFor(approverUsername);
    for (std::size_t d = 0; d < delegations.size(); ++d) {
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (rows[i].docId == targetDocId &&
                rows[i].approverUsername == delegations[d].delegatorUsername &&
                rows[i].status == "pending") {
                ownerUsername = delegations[d].delegatorUsername;
                return true;
            }
        }
    }

    return false;
}

void printApprovalCommentThread(const std::string& docId) {
    std::vector<ApprovalComment> comments;
    if (!loadApprovalComments(comments)) {
        std::cout << "\n" << ui::warning("[!] Comment thread file unavailable.") << "\n";
        return;
    }

    std::vector<ApprovalComment> thread;
    for (std::size_t i = 0; i < comments.size(); ++i) {
        if (comments[i].docId == docId) {
            thread.push_back(comments[i]);
        }
    }

    std::sort(thread.begin(), thread.end(), [](const ApprovalComment& left, const ApprovalComment& right) {
        if (left.createdAt != right.createdAt) {
            return left.createdAt < right.createdAt;
        }

        if (left.commenterUsername != right.commenterUsername) {
            return left.commenterUsername < right.commenterUsername;
        }

        return left.commentText < right.commentText;
    });

    std::cout << "\n" << ui::bold("Approval Comment Thread") << "\n";
    if (thread.empty()) {
        std::cout << "  " << ui::muted("(no comments yet)") << "\n";
        return;
    }

    const std::vector<std::string> headers = {"Time", "User", "Role", "Comment"};
    const std::vector<int> widths = {19, 16, 24, 40};
    ui::printTableHeader(headers, widths);

    for (std::size_t i = 0; i < thread.size(); ++i) {
        ui::printTableRow({thread[i].createdAt,
                           thread[i].commenterUsername,
                           thread[i].commenterRole,
                           thread[i].commentText},
                          widths);
    }

    ui::printTableFooter(widths);
}

bool promptAndStoreCommentForDecision(const Admin& admin,
                                      const std::string& targetDocId,
                                      const std::string& actingOnBehalfOf) {
    printApprovalCommentThread(targetDocId);
    std::cout << "\nAdd a discussion comment before decision? (y/N): ";

    std::string answer;
    std::getline(std::cin, answer);
    answer = toLowerCopy(trimCopy(answer));

    if (answer != "y" && answer != "yes") {
        return false;
    }

    std::string commentText;
    std::cout << "Comment (max 220 chars): ";
    std::getline(std::cin, commentText);
    commentText = sanitizeCommentText(commentText);

    if (commentText.empty()) {
        std::cout << ui::warning("[!] Empty comment skipped.") << "\n";
        return false;
    }

    if (commentText.size() > 220) {
        commentText = commentText.substr(0, 220);
    }

    std::string roleLabel = admin.role;
    if (!actingOnBehalfOf.empty()) {
        roleLabel += " (delegate for " + actingOnBehalfOf + ")";
    }

    if (!appendApprovalComment(targetDocId, admin.username, roleLabel, commentText)) {
        std::cout << ui::warning("[!] Comment could not be saved.") << "\n";
        return false;
    }

    logAuditAction("APPROVAL_COMMENT_ADD", targetDocId, admin.username);
    std::cout << ui::success("[+] Comment added to thread.") << "\n";
    return true;
}

bool parseTimestamp(const std::string& text, std::time_t& outTime) {
    // Parses timestamps in the fixed "YYYY-MM-DD HH:MM:SS" format.
    if (text.empty()) {
        return false;
    }

    std::tm tmValue = {};
    std::istringstream in(text);
    in >> std::get_time(&tmValue, "%Y-%m-%d %H:%M:%S");
    if (in.fail()) {
        return false;
    }

    outTime = std::mktime(&tmValue);
    return outTime != static_cast<std::time_t>(-1);
}

std::string shortenText(const std::string& value, std::size_t maxLen) {
    if (value.size() <= maxLen) {
        return value;
    }

    if (maxLen <= 3) {
        return value.substr(0, maxLen);
    }

    return value.substr(0, maxLen - 3) + "...";
}

bool loadDocumentContexts(std::map<std::string, DocumentContext>& rows) {
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
        if (tokens.size() < 3) {
            continue;
        }

        DocumentContext row;
        row.title = tokens.size() > 1 ? tokens[1] : "";
        row.category = tokens.size() > 2 ? tokens[2] : "";
        row.status = tokens.size() > 7 ? tokens[7] : "pending_approval";
        row.uploader = tokens.size() > 6 ? tokens[6] : "";
        rows[tokens[0]] = row;
    }

    return true;
}

bool isUploaderConflictForDecision(const std::string& docId,
                                  const std::string& approvalOwnerUsername,
                                  std::string& outUploader) {
    outUploader.clear();

    std::map<std::string, DocumentContext> docs;
    if (!loadDocumentContexts(docs)) {
        return false;
    }

    std::map<std::string, DocumentContext>::const_iterator found = docs.find(docId);
    if (found == docs.end()) {
        return false;
    }

    outUploader = found->second.uploader;
    if (outUploader.empty()) {
        return false;
    }

    return toLowerCopy(outUploader) == toLowerCopy(approvalOwnerUsername);
}

int computePendingDaysFromCreatedAt(const std::string& createdAt) {
    std::time_t createdTime;
    if (!parseTimestamp(createdAt, createdTime)) {
        return 0;
    }

    const std::time_t now = std::time(NULL);
    const double elapsedSeconds = std::difftime(now, createdTime);
    if (elapsedSeconds <= 0.0) {
        return 0;
    }

    return static_cast<int>(elapsedSeconds / (24.0 * 3600.0));
}

std::vector<PendingApprovalSlaRow> buildPendingSlaRows(const std::vector<Approval>& approvals) {
    std::vector<PendingApprovalSlaRow> rows;
    std::map<std::string, DocumentContext> docById;
    loadDocumentContexts(docById);

    for (std::size_t i = 0; i < approvals.size(); ++i) {
        if (toLowerCopy(approvals[i].status) != "pending") {
            continue;
        }

        PendingApprovalSlaRow row;
        row.docId = approvals[i].docId;
        row.approverUsername = approvals[i].approverUsername;
        row.role = approvals[i].role;
        row.createdAt = approvals[i].createdAt;

        const std::map<std::string, DocumentContext>::const_iterator found = docById.find(approvals[i].docId);
        if (found != docById.end()) {
            row.title = found->second.title;
            row.category = found->second.category;
        } else {
            row.title = "(unknown)";
            row.category = "DEFAULT";
        }

        const ApprovalRule appliedRule = resolveRuleForCategory(row.category);
        row.slaDays = appliedRule.maxDecisionDays;
        row.pendingDays = computePendingDaysFromCreatedAt(row.createdAt);
        row.overdueDays = row.pendingDays - row.slaDays;
        if (row.overdueDays < 0) {
            row.overdueDays = 0;
        }

        rows.push_back(row);
    }

    return rows;
}

void applyApprovalDecision(const Admin& admin, bool approve) {
    // Main decision flow:
    // 1) update exactly one pending row for this approver,
    // 2) recompute aggregate decision status for the document,
    // 3) sync document status + append blockchain/audit entries.
    // Complexity is O(n) over total approval rows for the document scans.
    clearScreen();
    ui::printSectionTitle(approve ? "APPROVE DOCUMENT" : "REJECT DOCUMENT");
    printConsensusLegend();

    std::vector<Approval> rows;
    if (!loadApprovals(rows)) {
        std::cout << ui::error("[!] Unable to open approvals file.") << "\n";
        logAuditAction("APPROVAL_FAILED", "N/A", admin.username);
        waitForEnter();
        return;
    }

    if (!printPendingDecisionHints(rows, admin.username)) {
        logAuditAction("APPROVAL_NOT_FOUND", "N/A", admin.username);
        waitForEnter();
        return;
    }

    clearInputBuffer();
    std::string targetDocId;
    std::cout << "Enter Document ID: ";
    std::getline(std::cin, targetDocId);

    if (targetDocId.empty()) {
        std::cout << ui::error("[!] Document ID is required.") << "\n";
        logAuditAction("APPROVAL_INPUT_ERROR", "N/A", admin.username);
        waitForEnter();
        return;
    }

    std::string ownerUsername;
    if (!resolvePendingDecisionOwner(rows, admin.username, targetDocId, ownerUsername)) {
        std::cout << ui::error("[!] No pending approval record found for this document and account.") << "\n";
        logAuditAction("APPROVAL_NOT_FOUND", targetDocId, admin.username);
        waitForEnter();
        return;
    }

    std::string uploaderUsername;
    if (isUploaderConflictForDecision(targetDocId, ownerUsername, uploaderUsername)) {
        std::cout << ui::error("[!] Conflict of interest: uploader cannot approve or reject this document.") << "\n";
        std::cout << "  Document uploader: " << uploaderUsername << "\n";
        logAuditAction("COI_APPROVAL_BLOCKED", targetDocId, admin.username);
        waitForEnter();
        return;
    }

    std::vector<Approval> projectedRows = rows;
    for (std::size_t i = 0; i < projectedRows.size(); ++i) {
        if (projectedRows[i].docId == targetDocId &&
            projectedRows[i].approverUsername == ownerUsername &&
            projectedRows[i].status == "pending") {
            projectedRows[i].status = approve ? "approved" : "rejected";
            break;
        }
    }

    bool projectedHasAny = false;
    bool projectedHasPending = false;
    bool projectedHasRejected = false;
    int projectedApproved = 0;
    int projectedTotal = 0;

    for (std::size_t i = 0; i < projectedRows.size(); ++i) {
        if (projectedRows[i].docId != targetDocId) {
            continue;
        }

        projectedHasAny = true;
        projectedTotal++;
        if (projectedRows[i].status == "pending") {
            projectedHasPending = true;
        } else if (projectedRows[i].status == "rejected") {
            projectedHasRejected = true;
        } else if (projectedRows[i].status == "approved") {
            projectedApproved++;
        }
    }

    std::string projectedDocStatus = "pending_approval";
    if (projectedHasRejected) {
        projectedDocStatus = "rejected";
    } else if (projectedHasAny && !projectedHasPending) {
        projectedDocStatus = "published";
    }

    std::cout << "\nProjected consensus after this action: "
              << buildConsensusProgressText(projectedApproved, projectedTotal)
              << " -> " << ui::consensusStatus(projectedDocStatus) << "\n";

    std::string confirmQuestion = approve
        ? "Are you sure you want to approve this document?"
        : "Are you sure you want to reject this document?";
    if (projectedDocStatus == "published") {
        confirmQuestion = "All required approvals will be complete. Publish document to public portal?";
    }

    if (!ui::confirmAction(confirmQuestion,
                           approve ? "Confirm Decision" : "Confirm Rejection",
                           "Cancel")) {
        logAuditAction("APPROVAL_DECISION_CANCELLED", targetDocId, admin.username);
        std::cout << ui::warning("[!] Decision cancelled.") << "\n";
        waitForEnter();
        return;
    }

    std::string actingOnBehalfOf;
    if (ownerUsername != admin.username) {
        actingOnBehalfOf = ownerUsername;
    }

    promptAndStoreCommentForDecision(admin, targetDocId, actingOnBehalfOf);

    bool updated = false;
    const std::string now = getCurrentTimestamp();

    for (size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].docId == targetDocId && rows[i].approverUsername == ownerUsername && rows[i].status == "pending") {
            rows[i].status = approve ? "approved" : "rejected";
            rows[i].decidedAt = now;

            std::string note;
            std::cout << "  Add a note (optional, press Enter to skip): ";
            std::getline(std::cin, note);
            rows[i].note = sanitizeCommentText(note);

            updated = true;
            break;
        }
    }

    if (!updated) {
        std::cout << ui::error("[!] No pending approval record found for this document and account.") << "\n";
        logAuditAction("APPROVAL_NOT_FOUND", targetDocId, admin.username);
        waitForEnter();
        return;
    }

    if (!saveApprovals(rows)) {
        std::cout << ui::error("[!] Unable to update approvals file.") << "\n";
        logAuditAction("APPROVAL_FAILED", targetDocId, admin.username);
        waitForEnter();
        return;
    }

    bool hasAny = false;
    bool hasPending = false;
    bool hasRejected = false;

    for (size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].docId != targetDocId) {
            continue;
        }

        hasAny = true;
        if (rows[i].status == "pending") {
            hasPending = true;
        } else if (rows[i].status == "rejected") {
            hasRejected = true;
        }
    }

    std::string nextDocStatus = "pending_approval";
    if (hasRejected) {
        nextDocStatus = "rejected";
    } else if (hasAny && !hasPending) {
        nextDocStatus = "published";
    }

    const std::string syncNote = approve ? "Approval decision applied." : "Rejection decision applied.";
    if (!updateDocumentStatusBySystem(targetDocId, nextDocStatus, admin.username, syncNote)) {
        if (nextDocStatus == "published") {
            std::cout << ui::warning("[!] Decision saved, but publication was blocked by budget guardrail.") << "\n";
        } else {
            std::cout << ui::error("[!] Decision saved, but document status update failed.") << "\n";
        }
        logAuditAction("APPROVAL_STATUS_SYNC_FAILED", targetDocId, admin.username);
        waitForEnter();
        return;
    }

    std::cout << "\nDecision Saved: " << ui::consensusStatus(approve ? "approved" : "denied") << "\n";
    std::cout << "Consensus Result: " << ui::consensusStatus(nextDocStatus) << "\n";

    if (!actingOnBehalfOf.empty()) {
        std::cout << "  " << ui::muted("(Acting on behalf of " + actingOnBehalfOf + " via delegation)") << "\n";
    }

    const std::string auditActor = actingOnBehalfOf.empty() ? admin.username :
        admin.username + " (on behalf of " + actingOnBehalfOf + ")";
    if (approve) {
        const std::string action = actingOnBehalfOf.empty() ? "APPROVE" : "APPROVE_DELEGATED";
        const int chainIndex = appendBlockchainAction(action, targetDocId, admin.username);
        logAuditAction("APPROVE_DOC", targetDocId, auditActor, chainIndex);
        std::cout << ui::success("[+] Document approved successfully.") << "\n";
    } else {
        const std::string action = actingOnBehalfOf.empty() ? "REJECT" : "REJECT_DELEGATED";
        const int chainIndex = appendBlockchainAction(action, targetDocId, admin.username);
        logAuditAction("REJECT_DOC", targetDocId, auditActor, chainIndex);
        std::cout << ui::error("[+] Document denied successfully.") << "\n";
    }

    waitForEnter();
}
} // namespace

void ensureApprovalsDataFileExists() {
    // Ensures header presence, normalizes old rows, and seeds baseline requests.
    std::ifstream checkFile;
    if (!openInputFileWithFallback(checkFile, APPROVALS_FILE_PATH_PRIMARY, APPROVALS_FILE_PATH_FALLBACK)) {
        std::ofstream createFile(APPROVALS_FILE_PATH_PRIMARY);
        if (!createFile.is_open()) {
            createFile.clear();
            createFile.open(APPROVALS_FILE_PATH_FALLBACK);
        }

        if (createFile.is_open()) {
            createFile << "docID|approverUsername|role|status|createdAt|decidedAt|note\n";
        }
    }

    std::vector<Approval> existing;
    if (loadApprovals(existing)) {
        saveApprovals(existing);
    }

    seedApprovalsIfEmpty();
    ensureApprovalRulesDataFileExists();
    ensureApprovalCommentsDataFileExists();
}

void ensureApprovalRulesDataFileExists() {
    std::ifstream checkFile;
    if (!openInputFileWithFallback(checkFile, APPROVAL_RULES_FILE_PATH_PRIMARY, APPROVAL_RULES_FILE_PATH_FALLBACK)) {
        std::ofstream createFile(APPROVAL_RULES_FILE_PATH_PRIMARY);
        if (!createFile.is_open()) {
            createFile.clear();
            createFile.open(APPROVAL_RULES_FILE_PATH_FALLBACK);
        }

        if (createFile.is_open()) {
            createFile << "category|requiredRoles|maxDecisionDays\n";
            createFile << "DEFAULT|Budget Officer,Municipal Administrator|7\n";
            createFile << "Infrastructure Procurement|Budget Officer,Municipal Administrator,Procurement Officer|5\n";
            createFile << "Health Supplies|Budget Officer,Municipal Administrator|5\n";
            createFile << "IT and Digital Services|Budget Officer,Municipal Administrator,Procurement Officer|4\n";
        }
    }

    std::vector<ApprovalRule> rules;
    if (loadApprovalRules(rules) && !rules.empty()) {
        saveApprovalRules(rules);
    }
}

void createApprovalRequestsForDocument(const std::string& docId, const std::string& uploader, const std::string& category) {
    // Creates pending requests from category-specific rules with a safe DEFAULT fallback.
    // Complexity: O(a + n), where a=admin rows and n=existing approval rows.
    std::ifstream adminFile;
    if (!openInputFileWithFallback(adminFile, ADMINS_FILE_PATH_PRIMARY, ADMINS_FILE_PATH_FALLBACK)) {
        return;
    }

    std::string line;
    std::getline(adminFile, line); // Skip header.

    std::vector<Approval> existing;
    loadApprovals(existing);

    const std::string now = getCurrentTimestamp();
    ApprovalRule chosenRule = resolveRuleForCategory(category);
    const std::vector<std::string> fallbackRoles = defaultRequiredRoles();

    std::vector<Approval> requests;
    bool foundEligibleInChosenRule = false;

    const auto hasApprovalTuple = [](const std::vector<Approval>& rows,
                                     const std::string& targetDocId,
                                     const std::string& approverUsername,
                                     const std::string& approverRole) {
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (rows[i].docId == targetDocId &&
                rows[i].approverUsername == approverUsername &&
                rows[i].role == approverRole) {
                return true;
            }
        }

        return false;
    };

    while (std::getline(adminFile, line)) {
        if (line.empty()) {
            continue;
        }

        const std::vector<std::string> tokens = splitPipe(line);
        const std::string username = tokens.size() > 2 ? tokens[2] : "";
        const std::string role = tokens.size() > 4 ? tokens[4] : "";
        const std::string status = tokens.size() > 5 ? tokens[5] : "active";

        if (username.empty() || username == uploader || status != "active") {
            continue;
        }

        if (!roleExistsInRule(chosenRule.requiredRoles, role)) {
            continue;
        }

        foundEligibleInChosenRule = true;
        if (hasApprovalTuple(existing, docId, username, role) ||
            hasApprovalTuple(requests, docId, username, role)) {
            continue;
        }

        Approval request;
        request.docId = docId;
        request.approverUsername = username;
        request.role = role;
        request.status = "pending";
        request.createdAt = now;
        requests.push_back(request);
    }

    if (requests.empty() && !foundEligibleInChosenRule && toLowerCopy(trimCopy(chosenRule.category)) != "default") {
        adminFile.clear();
        adminFile.seekg(0);
        std::getline(adminFile, line); // Skip header.

        while (std::getline(adminFile, line)) {
            if (line.empty()) {
                continue;
            }

            const std::vector<std::string> tokens = splitPipe(line);
            const std::string username = tokens.size() > 2 ? tokens[2] : "";
            const std::string role = tokens.size() > 4 ? tokens[4] : "";
            const std::string status = tokens.size() > 5 ? tokens[5] : "active";

            if (username.empty() || username == uploader || status != "active") {
                continue;
            }

            if (!roleExistsInRule(fallbackRoles, role)) {
                continue;
            }

            if (hasApprovalTuple(existing, docId, username, role) ||
                hasApprovalTuple(requests, docId, username, role)) {
                continue;
            }

            Approval request;
            request.docId = docId;
            request.approverUsername = username;
            request.role = role;
            request.status = "pending";
            request.createdAt = now;
            requests.push_back(request);
        }
    }

    if (requests.empty()) {
        return;
    }

    for (size_t i = 0; i < requests.size(); ++i) {
        existing.push_back(requests[i]);
    }

    if (saveApprovals(existing)) {
        logAuditAction("APPROVAL_REQUESTS_CREATED", docId, uploader);
    }
}

bool getPendingDocumentApprovalIdsForApprover(const std::string& approverUsername,
                                              std::vector<std::string>& outDocIds) {
    outDocIds.clear();

    std::vector<Approval> rows;
    if (!loadApprovals(rows)) {
        return false;
    }

    std::map<std::string, bool> seenDocIds;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].approverUsername != approverUsername || toLowerCopy(rows[i].status) != "pending") {
            continue;
        }

        if (seenDocIds[rows[i].docId]) {
            continue;
        }

        seenDocIds[rows[i].docId] = true;
        outDocIds.push_back(rows[i].docId);
    }

    return true;
}

void viewPendingApprovalsForAdmin(const Admin& admin) {
    // Filters pending rows for one approver: O(n) scan on approval dataset.
    clearScreen();
    ui::printSectionTitle("PENDING APPROVALS");
    printConsensusLegend();

    std::vector<Approval> rows;
    if (!loadApprovals(rows)) {
        std::cout << ui::error("[!] Unable to open approvals file.") << "\n";
        logAuditAction("VIEW_PENDING_APPROVALS_FAILED", "N/A", admin.username);
        waitForEnter();
        return;
    }

    std::vector<Approval> pendingRows;
    for (size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].approverUsername == admin.username && rows[i].status == "pending") {
            pendingRows.push_back(rows[i]);
        }
    }

    std::vector<Delegation> delegations = getActiveDelegationsFor(admin.username);
    std::vector<Approval> delegatedRows;
    for (std::size_t d = 0; d < delegations.size(); ++d) {
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (rows[i].approverUsername == delegations[d].delegatorUsername && rows[i].status == "pending") {
                delegatedRows.push_back(rows[i]);
            }
        }
    }

    if (pendingRows.empty() && delegatedRows.empty()) {
        std::cout << ui::warning("[!] No pending approvals for your account.") << "\n";
        logAuditAction("VIEW_PENDING_APPROVALS_EMPTY", "N/A", admin.username);
    } else {
        const std::vector<std::string> headers = {"Document ID", "Role", "Status", "Created", "Via"};
        const std::vector<int> widths = {12, 24, 12, 19, 12};
        ui::printTableHeader(headers, widths);
        for (size_t i = 0; i < pendingRows.size(); ++i) {
            ui::printTableRow({pendingRows[i].docId, pendingRows[i].role, pendingRows[i].status, pendingRows[i].createdAt, "Direct"}, widths);
        }
        for (size_t i = 0; i < delegatedRows.size(); ++i) {
            ui::printTableRow({delegatedRows[i].docId, delegatedRows[i].role, delegatedRows[i].status, delegatedRows[i].createdAt, "Delegated"}, widths);
        }
        ui::printTableFooter(widths);

        std::cout << "\n" << ui::bold("Consensus Progress By Document") << "\n";
        const std::vector<std::string> progressHeaders = {"Document ID", "Progress", "Status"};
        const std::vector<int> progressWidths = {12, 16, 14};
        ui::printTableHeader(progressHeaders, progressWidths);

        std::map<std::string, bool> targetedDocIds;
        for (std::size_t i = 0; i < pendingRows.size(); ++i) {
            targetedDocIds[pendingRows[i].docId] = true;
        }
        for (std::size_t i = 0; i < delegatedRows.size(); ++i) {
            targetedDocIds[delegatedRows[i].docId] = true;
        }

        for (std::map<std::string, bool>::const_iterator it = targetedDocIds.begin(); it != targetedDocIds.end(); ++it) {
            int total = 0;
            int approved = 0;
            int pending = 0;
            int rejected = 0;

            for (std::size_t i = 0; i < rows.size(); ++i) {
                if (rows[i].docId != it->first) {
                    continue;
                }

                total++;
                if (rows[i].status == "approved") {
                    approved++;
                } else if (rows[i].status == "pending") {
                    pending++;
                } else if (rows[i].status == "rejected") {
                    rejected++;
                }
            }

            std::string statusLabel = "pending";
            if (rejected > 0) {
                statusLabel = "rejected";
            } else if (total > 0 && pending == 0) {
                statusLabel = "published";
            }

            ui::printTableRow({it->first, buildConsensusProgressText(approved, total), statusLabel}, progressWidths);
        }

        ui::printTableFooter(progressWidths);

        std::cout << "\n" << ui::bold("Pending Load Chart") << "\n";
        const std::size_t totalPending = pendingRows.size() + delegatedRows.size();
        ui::printBar("Pending approvals",
                     static_cast<double>(totalPending),
                     static_cast<double>(totalPending),
                     24,
                     ui::consensusColorCode("pending"));

        clearInputBuffer();
        std::string threadDocId;
        std::cout << "\nEnter Document ID to view comment thread (blank to skip): ";
        std::getline(std::cin, threadDocId);
        threadDocId = trimCopy(threadDocId);

        if (!threadDocId.empty()) {
            std::string ownerUsername;
            if (!resolvePendingDecisionOwner(rows, admin.username, threadDocId, ownerUsername)) {
                std::cout << ui::warning("[!] You do not have pending access to that document.") << "\n";
            } else {
                printApprovalCommentThread(threadDocId);
            }
        }

        logAuditAction("VIEW_PENDING_APPROVALS", "MULTI", admin.username);
    }

    waitForEnter();
}

void viewApprovalAnalyticsDashboard(const Admin& admin) {
    // Aggregates approval KPIs in one pass: O(n) time, O(r) extra memory where
    // r is number of distinct roles represented in decisions.
    clearScreen();
    ui::printSectionTitle("APPROVAL ANALYTICS DASHBOARD");

    std::vector<Approval> rows;
    if (!loadApprovals(rows)) {
        std::cout << ui::error("[!] Unable to open approvals file.") << "\n";
        logAuditAction("APPROVAL_ANALYTICS_FAILED", "N/A", admin.username);
        waitForEnter();
        return;
    }

    if (rows.empty()) {
        std::cout << ui::warning("[!] No approval rows available.") << "\n";
        waitForEnter();
        return;
    }

    int approvedCount = 0;
    int rejectedCount = 0;
    int pendingCount = 0;
    int decidedCount = 0;

    std::map<std::string, int> decisionsByRole;
    double totalDecisionHours = 0.0;
    int timedRows = 0;

    for (size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].status == "approved") {
            approvedCount++;
            decidedCount++;
            decisionsByRole[rows[i].role] += 1;
        } else if (rows[i].status == "rejected") {
            rejectedCount++;
            decidedCount++;
            decisionsByRole[rows[i].role] += 1;
        } else {
            pendingCount++;
        }

        std::time_t createdTime;
        std::time_t decidedTime;
        if (parseTimestamp(rows[i].createdAt, createdTime) && parseTimestamp(rows[i].decidedAt, decidedTime)) {
            const double elapsedHours = std::difftime(decidedTime, createdTime) / 3600.0;
            if (elapsedHours >= 0.0) {
                totalDecisionHours += elapsedHours;
                timedRows++;
            }
        }
    }

    const double rejectionRate = decidedCount > 0 ? (100.0 * rejectedCount / decidedCount) : 0.0;
    const double avgDecisionHours = timedRows > 0 ? (totalDecisionHours / timedRows) : 0.0;
    const std::vector<PendingApprovalSlaRow> pendingSlaRows = buildPendingSlaRows(rows);

    int overduePendingCount = 0;
    std::map<std::string, int> pendingByRole;
    std::map<std::string, int> overdueByRole;
    std::map<std::string, int> worstOverdueByRole;
    std::map<std::string, double> pendingAgeTotalsByRole;

    for (std::size_t i = 0; i < pendingSlaRows.size(); ++i) {
        pendingByRole[pendingSlaRows[i].role] += 1;
        pendingAgeTotalsByRole[pendingSlaRows[i].role] += static_cast<double>(pendingSlaRows[i].pendingDays);

        if (pendingSlaRows[i].overdueDays > 0) {
            overduePendingCount++;
            overdueByRole[pendingSlaRows[i].role] += 1;
        }

        if (pendingSlaRows[i].overdueDays > worstOverdueByRole[pendingSlaRows[i].role]) {
            worstOverdueByRole[pendingSlaRows[i].role] = pendingSlaRows[i].overdueDays;
        }
    }

    const double pendingSlaCompliance = pendingSlaRows.empty()
                                            ? 100.0
                                            : (100.0 * static_cast<double>(pendingSlaRows.size() - static_cast<std::size_t>(overduePendingCount)) /
                                               static_cast<double>(pendingSlaRows.size()));

    const std::vector<std::string> headers = {"Metric", "Value"};
    const std::vector<int> widths = {30, 16};
    ui::printTableHeader(headers, widths);

    std::ostringstream rejOut;
    rejOut << std::fixed << std::setprecision(2) << rejectionRate << "%";

    std::ostringstream avgOut;
    avgOut << std::fixed << std::setprecision(2) << avgDecisionHours << " hrs";

    std::ostringstream pendingSlaOut;
    pendingSlaOut << std::fixed << std::setprecision(1) << pendingSlaCompliance << "%";

    ui::printTableRow({"Total approval rows", std::to_string(rows.size())}, widths);
    ui::printTableRow({"Decided rows", std::to_string(decidedCount)}, widths);
    ui::printTableRow({"Pending rows", std::to_string(pendingCount)}, widths);
    ui::printTableRow({"Approved rows", std::to_string(approvedCount)}, widths);
    ui::printTableRow({"Rejected rows", std::to_string(rejectedCount)}, widths);
    ui::printTableRow({"Rejection rate", rejOut.str()}, widths);
    ui::printTableRow({"Average decision time", avgOut.str()}, widths);
    ui::printTableRow({"Rows with timing data", std::to_string(timedRows)}, widths);
    ui::printTableRow({"Overdue pending approvals", std::to_string(overduePendingCount)}, widths);
    ui::printTableRow({"Pending SLA compliance", pendingSlaOut.str()}, widths);
    ui::printTableFooter(widths);

    std::cout << "\n" << ui::bold("Throughput By Role") << "\n";
    int maxRoleCount = 1;
    for (std::map<std::string, int>::const_iterator it = decisionsByRole.begin(); it != decisionsByRole.end(); ++it) {
        if (it->second > maxRoleCount) {
            maxRoleCount = it->second;
        }
    }

    for (std::map<std::string, int>::const_iterator it = decisionsByRole.begin(); it != decisionsByRole.end(); ++it) {
        ui::printBar(it->first, static_cast<double>(it->second), static_cast<double>(maxRoleCount), 24);
    }

    if (!pendingByRole.empty()) {
        std::cout << "\n" << ui::bold("SLA Bottlenecks By Role") << "\n";
        const std::vector<std::string> slaHeaders = {"Role", "Pending", "Overdue", "Avg Age (d)", "Worst Overdue (d)"};
        const std::vector<int> slaWidths = {26, 9, 9, 13, 18};
        ui::printTableHeader(slaHeaders, slaWidths);

        std::vector<std::string> orderedRoles;
        for (std::map<std::string, int>::const_iterator it = pendingByRole.begin(); it != pendingByRole.end(); ++it) {
            orderedRoles.push_back(it->first);
        }

        std::sort(orderedRoles.begin(), orderedRoles.end(), [&overdueByRole, &pendingByRole](const std::string& left, const std::string& right) {
            const int leftOverdue = overdueByRole.find(left) != overdueByRole.end() ? overdueByRole.find(left)->second : 0;
            const int rightOverdue = overdueByRole.find(right) != overdueByRole.end() ? overdueByRole.find(right)->second : 0;
            if (leftOverdue != rightOverdue) {
                return leftOverdue > rightOverdue;
            }

            const int leftPending = pendingByRole.find(left) != pendingByRole.end() ? pendingByRole.find(left)->second : 0;
            const int rightPending = pendingByRole.find(right) != pendingByRole.end() ? pendingByRole.find(right)->second : 0;
            if (leftPending != rightPending) {
                return leftPending > rightPending;
            }

            return left < right;
        });

        int maxOverdueRoleCount = 1;
        for (std::size_t i = 0; i < orderedRoles.size(); ++i) {
            const int overdueCount = overdueByRole.find(orderedRoles[i]) != overdueByRole.end() ? overdueByRole[orderedRoles[i]] : 0;
            if (overdueCount > maxOverdueRoleCount) {
                maxOverdueRoleCount = overdueCount;
            }

            const int pendingCountByRole = pendingByRole[orderedRoles[i]];
            const double averageAge = pendingCountByRole > 0 ? (pendingAgeTotalsByRole[orderedRoles[i]] / static_cast<double>(pendingCountByRole)) : 0.0;

            std::ostringstream avgAgeOut;
            avgAgeOut << std::fixed << std::setprecision(1) << averageAge;

            ui::printTableRow({orderedRoles[i],
                               std::to_string(pendingCountByRole),
                               std::to_string(overdueCount),
                               avgAgeOut.str(),
                               std::to_string(worstOverdueByRole[orderedRoles[i]])},
                              slaWidths);
        }
        ui::printTableFooter(slaWidths);

        std::cout << "\n" << ui::bold("Overdue Queue By Role") << "\n";
        for (std::size_t i = 0; i < orderedRoles.size(); ++i) {
            const int overdueCount = overdueByRole.find(orderedRoles[i]) != overdueByRole.end() ? overdueByRole[orderedRoles[i]] : 0;
            ui::printBar(orderedRoles[i], static_cast<double>(overdueCount), static_cast<double>(maxOverdueRoleCount), 24, "33");
        }
    }

    std::cout << "\n" << ui::muted("[i] Legacy rows without timestamps are excluded from average-time computation.") << "\n";
    logAuditAction("APPROVAL_ANALYTICS_VIEW", "MULTI", admin.username);
    waitForEnter();
}

void viewEscalationQueueForAdmin(const Admin& admin) {
    clearScreen();
    ui::printSectionTitle("ESCALATION QUEUE (OVERDUE SLA)");

    if (admin.role != "Super Admin") {
        std::cout << ui::error("[!] Only Super Admin can access the escalation queue.") << "\n";
        logAuditAction("APPROVAL_ESCALATION_ACCESS_DENIED", "N/A", admin.username);
        waitForEnter();
        return;
    }

    std::vector<Approval> rows;
    if (!loadApprovals(rows)) {
        std::cout << ui::error("[!] Unable to open approvals file.") << "\n";
        logAuditAction("APPROVAL_ESCALATION_FAILED", "N/A", admin.username);
        waitForEnter();
        return;
    }

    const std::vector<PendingApprovalSlaRow> pendingRows = buildPendingSlaRows(rows);
    std::vector<PendingApprovalSlaRow> overdueRows;
    std::map<std::string, int> overdueByRole;

    for (std::size_t i = 0; i < pendingRows.size(); ++i) {
        if (pendingRows[i].overdueDays <= 0) {
            continue;
        }

        overdueRows.push_back(pendingRows[i]);
        overdueByRole[pendingRows[i].role] += 1;
    }

    if (overdueRows.empty()) {
        std::cout << "\n" << ui::success("[+] No overdue pending approvals. Escalation queue is clear.") << "\n";
        logAuditAction("APPROVAL_ESCALATION_QUEUE_EMPTY", "N/A", admin.username);
        waitForEnter();
        return;
    }

    std::sort(overdueRows.begin(), overdueRows.end(), [](const PendingApprovalSlaRow& left, const PendingApprovalSlaRow& right) {
        if (left.overdueDays != right.overdueDays) {
            return left.overdueDays > right.overdueDays;
        }

        if (left.pendingDays != right.pendingDays) {
            return left.pendingDays > right.pendingDays;
        }

        return left.docId < right.docId;
    });

    std::cout << "  " << ui::muted("Items are sorted by highest overdue days first.") << "\n\n";

    const std::vector<std::string> headers = {"Doc ID", "Title", "Role", "Approver", "Pending", "SLA", "Overdue"};
    const std::vector<int> widths = {10, 20, 22, 16, 8, 6, 8};
    ui::printTableHeader(headers, widths);
    for (std::size_t i = 0; i < overdueRows.size(); ++i) {
        ui::printTableRow({overdueRows[i].docId,
                           shortenText(overdueRows[i].title, 20),
                           shortenText(overdueRows[i].role, 22),
                           overdueRows[i].approverUsername,
                           std::to_string(overdueRows[i].pendingDays),
                           std::to_string(overdueRows[i].slaDays),
                           std::to_string(overdueRows[i].overdueDays)},
                          widths);
    }
    ui::printTableFooter(widths);

    std::cout << "\n" << ui::bold("Escalation Load By Role") << "\n";
    int maxOverdue = 1;
    for (std::map<std::string, int>::const_iterator it = overdueByRole.begin(); it != overdueByRole.end(); ++it) {
        if (it->second > maxOverdue) {
            maxOverdue = it->second;
        }
    }

    for (std::map<std::string, int>::const_iterator it = overdueByRole.begin(); it != overdueByRole.end(); ++it) {
        ui::printBar(it->first, static_cast<double>(it->second), static_cast<double>(maxOverdue), 24, "33");
    }

    logAuditAction("APPROVAL_ESCALATION_QUEUE_VIEW", "MULTI", admin.username);
    waitForEnter();
}

void approveDocumentAsAdmin(const Admin& admin) {
    applyApprovalDecision(admin, true);
}

void rejectDocumentAsAdmin(const Admin& admin) {
    applyApprovalDecision(admin, false);
}

void addApprovalCommentAsAdmin(const Admin& admin) {
    clearScreen();
    ui::printSectionTitle("REQUEST-FOR-COMMENT THREAD");

    std::vector<Approval> rows;
    if (!loadApprovals(rows)) {
        std::cout << ui::error("[!] Unable to open approvals file.") << "\n";
        logAuditAction("APPROVAL_COMMENT_FAILED", "N/A", admin.username);
        waitForEnter();
        return;
    }

    if (!printPendingDecisionHints(rows, admin.username)) {
        logAuditAction("APPROVAL_COMMENT_NO_PENDING", "N/A", admin.username);
        waitForEnter();
        return;
    }

    clearInputBuffer();
    std::string targetDocId;
    std::cout << "Enter Document ID for comment thread: ";
    std::getline(std::cin, targetDocId);
    targetDocId = trimCopy(targetDocId);

    if (targetDocId.empty()) {
        std::cout << ui::error("[!] Document ID is required.") << "\n";
        logAuditAction("APPROVAL_COMMENT_INPUT_ERROR", "N/A", admin.username);
        waitForEnter();
        return;
    }

    std::string ownerUsername;
    if (!resolvePendingDecisionOwner(rows, admin.username, targetDocId, ownerUsername)) {
        std::cout << ui::error("[!] No pending approval access for this document.") << "\n";
        logAuditAction("APPROVAL_COMMENT_ACCESS_DENIED", targetDocId, admin.username);
        waitForEnter();
        return;
    }

    std::string actingOnBehalfOf;
    if (ownerUsername != admin.username) {
        actingOnBehalfOf = ownerUsername;
    }

    printApprovalCommentThread(targetDocId);

    std::string commentText;
    std::cout << "\nComment text (required, max 220 chars): ";
    std::getline(std::cin, commentText);
    commentText = sanitizeCommentText(commentText);

    if (commentText.empty()) {
        std::cout << ui::error("[!] Comment text is required.") << "\n";
        logAuditAction("APPROVAL_COMMENT_INPUT_ERROR", targetDocId, admin.username);
        waitForEnter();
        return;
    }

    if (commentText.size() > 220) {
        commentText = commentText.substr(0, 220);
    }

    std::string roleLabel = admin.role;
    if (!actingOnBehalfOf.empty()) {
        roleLabel += " (delegate for " + actingOnBehalfOf + ")";
    }

    if (!appendApprovalComment(targetDocId, admin.username, roleLabel, commentText)) {
        std::cout << ui::error("[!] Unable to save comment.") << "\n";
        logAuditAction("APPROVAL_COMMENT_FAILED", targetDocId, admin.username);
        waitForEnter();
        return;
    }

    logAuditAction("APPROVAL_COMMENT_ADD", targetDocId, admin.username);
    std::cout << "\n" << ui::success("[+] Comment added successfully.") << "\n";
    printApprovalCommentThread(targetDocId);
    waitForEnter();
}

void manageApprovalRulesForAdmin(const Admin& admin) {
    if (admin.role != "Super Admin") {
        clearScreen();
        ui::printSectionTitle("APPROVAL RULES MANAGEMENT");
        std::cout << ui::error("[!] Only Super Admin can manage approval rules.") << "\n";
        logAuditAction("APPROVAL_RULES_ACCESS_DENIED", "N/A", admin.username);
        waitForEnter();
        return;
    }

    int choice = -1;
    do {
        clearScreen();
        ui::printSectionTitle("APPROVAL RULES MANAGEMENT");

        std::vector<ApprovalRule> rules;
        if (!loadApprovalRules(rules) || rules.empty()) {
            rules.push_back(buildDefaultRule());
            saveApprovalRules(rules);
        }

        const std::vector<std::string> headers = {"Category", "Required Roles", "SLA (days)"};
        const std::vector<int> widths = {28, 44, 12};
        ui::printTableHeader(headers, widths);
        for (std::size_t i = 0; i < rules.size(); ++i) {
            ui::printTableRow({rules[i].category, joinRolesCsv(rules[i].requiredRoles), std::to_string(rules[i].maxDecisionDays)}, widths);
        }
        ui::printTableFooter(widths);

        std::cout << "\n";
        std::cout << "  [1] Add or Update Rule\n";
        std::cout << "  [2] Delete Rule\n";
        std::cout << "  [0] Back\n";
        std::cout << "  Enter choice: ";

        std::cin >> choice;
        if (std::cin.fail()) {
            std::cin.clear();
            clearInputBuffer();
            std::cout << "\n" << ui::error("[!] Invalid input.") << "\n";
            waitForEnter();
            continue;
        }

        clearInputBuffer();

        if (choice == 1) {
            std::string category;
            std::string rolesInput;
            std::string daysInput;

            std::cout << "Category (or DEFAULT): ";
            std::getline(std::cin, category);
            category = trimCopy(category);

            std::cout << "Required roles (comma-separated): ";
            std::getline(std::cin, rolesInput);

            std::cout << "Max decision days: ";
            std::getline(std::cin, daysInput);

            std::vector<std::string> parsedRoles = splitRolesCsv(rolesInput);
            if (category.empty() || parsedRoles.empty()) {
                std::cout << "\n" << ui::error("[!] Category and required roles are required.") << "\n";
                waitForEnter();
                continue;
            }

            int maxDays = 7;
            std::stringstream in(daysInput);
            in >> maxDays;
            if (in.fail() || maxDays <= 0) {
                maxDays = 7;
            }

            bool replaced = false;
            const std::string key = toLowerCopy(category);
            for (std::size_t i = 0; i < rules.size(); ++i) {
                if (toLowerCopy(rules[i].category) == key) {
                    rules[i].requiredRoles = parsedRoles;
                    rules[i].maxDecisionDays = maxDays;
                    replaced = true;
                    break;
                }
            }

            if (!replaced) {
                ApprovalRule newRule;
                newRule.category = category;
                newRule.requiredRoles = parsedRoles;
                newRule.maxDecisionDays = maxDays;
                rules.push_back(newRule);
            }

            if (!saveApprovalRules(rules)) {
                std::cout << "\n" << ui::error("[!] Failed to save approval rules.") << "\n";
                waitForEnter();
                continue;
            }

            logAuditAction("APPROVAL_RULE_SAVED", category, admin.username);
            std::cout << "\n" << ui::success("[+] Approval rule saved.") << "\n";
            waitForEnter();
        } else if (choice == 2) {
            std::string category;
            std::cout << "Category to delete: ";
            std::getline(std::cin, category);
            category = trimCopy(category);

            if (toLowerCopy(category) == "default") {
                std::cout << "\n" << ui::warning("[!] DEFAULT rule cannot be deleted.") << "\n";
                waitForEnter();
                continue;
            }

            bool removed = false;
            for (std::size_t i = 0; i < rules.size(); ++i) {
                if (toLowerCopy(rules[i].category) == toLowerCopy(category)) {
                    rules.erase(rules.begin() + static_cast<std::vector<ApprovalRule>::difference_type>(i));
                    removed = true;
                    break;
                }
            }

            if (!removed) {
                std::cout << "\n" << ui::warning("[!] Rule not found.") << "\n";
                waitForEnter();
                continue;
            }

            if (!ui::confirmAction("Are you sure you want to delete this approval rule?",
                                   "Delete Rule",
                                   "Cancel")) {
                std::cout << "\n" << ui::warning("[!] Rule deletion cancelled.") << "\n";
                waitForEnter();
                continue;
            }

            if (!saveApprovalRules(rules)) {
                std::cout << "\n" << ui::error("[!] Failed to save approval rules.") << "\n";
                waitForEnter();
                continue;
            }

            logAuditAction("APPROVAL_RULE_DELETED", category, admin.username);
            std::cout << "\n" << ui::success("[+] Approval rule deleted.") << "\n";
            waitForEnter();
        }

    } while (choice != 0);
}
