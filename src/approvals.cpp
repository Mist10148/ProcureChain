#include "../include/approvals.h"

#include "../include/audit.h"
#include "../include/auth.h"
#include "../include/blockchain.h"
#include "../include/documents.h"
#include "../include/ui.h"

#include <ctime>
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
const std::string ADMINS_FILE_PATH_PRIMARY = "data/admins.txt";
const std::string ADMINS_FILE_PATH_FALLBACK = "../data/admins.txt";
const std::string DOCUMENTS_FILE_PATH_PRIMARY = "data/documents.txt";
const std::string DOCUMENTS_FILE_PATH_FALLBACK = "../data/documents.txt";

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

bool isRequiredApproverRole(const std::string& role) {
    // Approval flow currently requires exactly two governance roles.
    return role == "Budget Officer" || role == "Municipal Administrator";
}

std::string findAdminUsernameByRole(const std::string& targetRole) {
    // Linear search over admin records: O(a), a = admin row count.
    std::ifstream file;
    if (!openInputFileWithFallback(file, ADMINS_FILE_PATH_PRIMARY, ADMINS_FILE_PATH_FALLBACK)) {
        return "";
    }

    std::string line;
    std::getline(file, line); // Skip header.

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        const std::vector<std::string> tokens = splitPipe(line);
        const std::string username = tokens.size() > 2 ? tokens[2] : "";
        const std::string role = tokens.size() > 4 ? tokens[4] : "";

        if (role == targetRole && !username.empty()) {
            return username;
        }
    }

    return "";
}

std::string findPendingDocumentIdForSeeding() {
    // Picks the first pending document to attach initial approval rows.
    std::ifstream file;
    if (!openInputFileWithFallback(file, DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK)) {
        return "";
    }

    std::string line;
    std::getline(file, line); // Skip header.

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        const std::vector<std::string> tokens = splitPipe(line);
        const std::string docId = tokens.size() > 0 ? tokens[0] : "";
        const std::string status = tokens.size() > 6 ? tokens[6] : "";

        if (status == "pending_approval" || status == "pending") {
            return docId;
        }
    }

    return "";
}

void seedApprovalsIfEmpty() {
    // Seeds only when no approval rows exist; this keeps startup idempotent.
    std::vector<Approval> rows;
    if (!loadApprovals(rows)) {
        return;
    }

    if (!rows.empty()) {
        saveApprovals(rows);
        return;
    }

    std::string docId = findPendingDocumentIdForSeeding();
    if (docId.empty()) {
        docId = "CA003";
    }

    const std::string budgetOfficer = findAdminUsernameByRole("Budget Officer");
    const std::string municipalAdmin = findAdminUsernameByRole("Municipal Administrator");
    const std::string now = getCurrentTimestamp();

    if (!budgetOfficer.empty()) {
        Approval a;
        a.docId = docId;
        a.approverUsername = budgetOfficer;
        a.role = "Budget Officer";
        a.status = "pending";
        a.createdAt = now;
        rows.push_back(a);
    }

    if (!municipalAdmin.empty()) {
        Approval a;
        a.docId = docId;
        a.approverUsername = municipalAdmin;
        a.role = "Municipal Administrator";
        a.status = "pending";
        a.createdAt = now;
        rows.push_back(a);
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

    std::vector<Approval> rows;
    if (!loadApprovals(rows)) {
        std::cout << ui::error("[!] Unable to open approvals file.") << "\n";
        logAuditAction("APPROVAL_FAILED", targetDocId, admin.username);
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
}

void createApprovalRequestsForDocument(const std::string& docId, const std::string& uploader) {
    // Creates one pending request per required active admin role.
    // Complexity: O(a + n), where a=admin rows and n=existing approval rows.
    std::ifstream adminFile;
    if (!openInputFileWithFallback(adminFile, ADMINS_FILE_PATH_PRIMARY, ADMINS_FILE_PATH_FALLBACK)) {
        return;
    }

    std::string line;
    std::getline(adminFile, line); // Skip header.

    std::vector<Approval> existing;
    loadApprovals(existing);

    std::vector<Approval> requests;
    const std::string now = getCurrentTimestamp();

    while (std::getline(adminFile, line)) {
        if (line.empty()) {
            continue;
        }

        const std::vector<std::string> tokens = splitPipe(line);
        const std::string username = tokens.size() > 2 ? tokens[2] : "";
        const std::string role = tokens.size() > 4 ? tokens[4] : "";
        const std::string status = tokens.size() > 5 ? tokens[5] : "active";

        if (username == uploader || !isRequiredApproverRole(role) || status != "active") {
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
