#include "../include/approvals.h"

#include "../include/audit.h"
#include "../include/auth.h"
#include "../include/blockchain.h"
#include "../include/documents.h"
#include "../include/ui.h"

#include <ctime>
#include <cctype>
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
const std::string ADMINS_FILE_PATH_PRIMARY = "data/admins.txt";
const std::string ADMINS_FILE_PATH_FALLBACK = "../data/admins.txt";

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

    if (row.createdAt.empty()) {
        row.createdAt = getCurrentTimestamp();
    }

    return row;
}

std::string serializeApproval(const Approval& row) {
    // Keep schema order stable because other modules parse this exact layout.
    return row.docId + "|" + row.approverUsername + "|" + row.role + "|" + row.status + "|" + row.createdAt + "|" + row.decidedAt;
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

    writer << "docID|approverUsername|role|status|createdAt|decidedAt\n";
    for (size_t i = 0; i < rows.size(); ++i) {
        writer << serializeApproval(rows[i]) << '\n';
    }
    writer.flush();
    return true;
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

bool printPendingDecisionHints(const std::vector<Approval>& rows, const std::string& approverUsername) {
    std::vector<Approval> pendingRows;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].approverUsername == approverUsername && rows[i].status == "pending") {
            pendingRows.push_back(rows[i]);
        }
    }

    if (pendingRows.empty()) {
        std::cout << "\n" << ui::warning("[!] You have no pending approvals right now.") << "\n";
        return false;
    }

    std::cout << "\n" << ui::bold("Available Pending Decisions") << "\n";
    const std::vector<std::string> headers = {"Document ID", "Role", "Created"};
    const std::vector<int> widths = {12, 24, 19};
    ui::printTableHeader(headers, widths);

    for (std::size_t i = 0; i < pendingRows.size(); ++i) {
        ui::printTableRow({pendingRows[i].docId, pendingRows[i].role, pendingRows[i].createdAt}, widths);
    }

    ui::printTableFooter(widths);
    std::cout << "  " << ui::muted("Tip: enter one of the listed Document IDs.") << "\n";
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

    bool updated = false;
    const std::string now = getCurrentTimestamp();

    for (size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].docId == targetDocId && rows[i].approverUsername == admin.username && rows[i].status == "pending") {
            rows[i].status = approve ? "approved" : "rejected";
            rows[i].decidedAt = now;
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

    if (!updateDocumentStatusBySystem(targetDocId, nextDocStatus)) {
        std::cout << ui::error("[!] Decision saved, but document status update failed.") << "\n";
        logAuditAction("APPROVAL_STATUS_SYNC_FAILED", targetDocId, admin.username);
        waitForEnter();
        return;
    }

    std::cout << "\nDecision Saved: " << ui::consensusStatus(approve ? "approved" : "denied") << "\n";
    std::cout << "Consensus Result: " << ui::consensusStatus(nextDocStatus) << "\n";

    if (approve) {
        const int chainIndex = appendBlockchainAction("APPROVE", targetDocId, admin.username);
        logAuditAction("APPROVE_DOC", targetDocId, admin.username, chainIndex);
        std::cout << ui::success("[+] Document approved successfully.") << "\n";
    } else {
        const int chainIndex = appendBlockchainAction("REJECT", targetDocId, admin.username);
        logAuditAction("REJECT_DOC", targetDocId, admin.username, chainIndex);
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
            createFile << "docID|approverUsername|role|status|createdAt|decidedAt\n";
        }
    }

    std::vector<Approval> existing;
    if (loadApprovals(existing)) {
        saveApprovals(existing);
    }

    seedApprovalsIfEmpty();
    ensureApprovalRulesDataFileExists();
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

        Approval request;
        request.docId = docId;
        request.approverUsername = username;
        request.role = role;
        request.status = "pending";
        request.createdAt = now;
        requests.push_back(request);
    }

    if (requests.empty() && toLowerCopy(trimCopy(chosenRule.category)) != "default") {
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

    if (pendingRows.empty()) {
        std::cout << ui::warning("[!] No pending approvals for your account.") << "\n";
        logAuditAction("VIEW_PENDING_APPROVALS_EMPTY", "N/A", admin.username);
    } else {
        const std::vector<std::string> headers = {"Document ID", "Role", "Status", "Created"};
        const std::vector<int> widths = {12, 24, 12, 19};
        ui::printTableHeader(headers, widths);
        for (size_t i = 0; i < pendingRows.size(); ++i) {
            ui::printTableRow({pendingRows[i].docId, pendingRows[i].role, pendingRows[i].status, pendingRows[i].createdAt}, widths);
        }
        ui::printTableFooter(widths);

        std::cout << "\n" << ui::bold("Pending Load Chart") << "\n";
        ui::printBar("Pending approvals",
                     static_cast<double>(pendingRows.size()),
                     static_cast<double>(pendingRows.size()),
                     24,
                     ui::consensusColorCode("pending"));
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

    const std::vector<std::string> headers = {"Metric", "Value"};
    const std::vector<int> widths = {30, 16};
    ui::printTableHeader(headers, widths);

    std::ostringstream rejOut;
    rejOut << std::fixed << std::setprecision(2) << rejectionRate << "%";

    std::ostringstream avgOut;
    avgOut << std::fixed << std::setprecision(2) << avgDecisionHours << " hrs";

    ui::printTableRow({"Total approval rows", std::to_string(rows.size())}, widths);
    ui::printTableRow({"Decided rows", std::to_string(decidedCount)}, widths);
    ui::printTableRow({"Pending rows", std::to_string(pendingCount)}, widths);
    ui::printTableRow({"Approved rows", std::to_string(approvedCount)}, widths);
    ui::printTableRow({"Rejected rows", std::to_string(rejectedCount)}, widths);
    ui::printTableRow({"Rejection rate", rejOut.str()}, widths);
    ui::printTableRow({"Average decision time", avgOut.str()}, widths);
    ui::printTableRow({"Rows with timing data", std::to_string(timedRows)}, widths);
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

    std::cout << "\n" << ui::muted("[i] Legacy rows without timestamps are excluded from average-time computation.") << "\n";
    logAuditAction("APPROVAL_ANALYTICS_VIEW", "MULTI", admin.username);
    waitForEnter();
}

void approveDocumentAsAdmin(const Admin& admin) {
    applyApprovalDecision(admin, true);
}

void rejectDocumentAsAdmin(const Admin& admin) {
    applyApprovalDecision(admin, false);
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
