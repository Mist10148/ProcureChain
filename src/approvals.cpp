#include "../include/approvals.h"

#include "../include/audit.h"
#include "../include/auth.h"
#include "../include/blockchain.h"
#include "../include/documents.h"
#include "../include/ui.h"

#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>

namespace {
const std::string APPROVALS_FILE_PATH_PRIMARY = "data/approvals.txt";
const std::string APPROVALS_FILE_PATH_FALLBACK = "../data/approvals.txt";
const std::string ADMINS_FILE_PATH_PRIMARY = "data/admins.txt";
const std::string ADMINS_FILE_PATH_FALLBACK = "../data/admins.txt";
const std::string DOCUMENTS_FILE_PATH_PRIMARY = "data/documents.txt";
const std::string DOCUMENTS_FILE_PATH_FALLBACK = "../data/documents.txt";

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

bool isRequiredApproverRole(const std::string& role) {
    return role == "Budget Officer" || role == "Municipal Administrator";
}

std::string findAdminUsernameByRole(const std::string& targetRole) {
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

        std::stringstream parser(line);
        std::string adminId;
        std::string fullName;
        std::string username;
        std::string password;
        std::string role;

        std::getline(parser, adminId, '|');
        std::getline(parser, fullName, '|');
        std::getline(parser, username, '|');
        std::getline(parser, password, '|');
        std::getline(parser, role, '|');

        if (role == targetRole && !username.empty()) {
            return username;
        }
    }

    return "";
}

std::string findPendingDocumentIdForSeeding() {
    std::ifstream file;
    if (!openInputFileWithFallback(file, DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK)) {
        return "";
    }

    std::string line;
    std::getline(file, line); // Skip header.

    // Prefer an actual pending document so seeded approvals point to a real record.
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        std::stringstream parser(line);
        std::string docId;
        std::string title;
        std::string category;
        std::string department;
        std::string dateUploaded;
        std::string uploader;
        std::string status;
        std::string hashValue;

        std::getline(parser, docId, '|');
        std::getline(parser, title, '|');
        std::getline(parser, category, '|');
        std::getline(parser, department, '|');
        std::getline(parser, dateUploaded, '|');
        std::getline(parser, uploader, '|');
        std::getline(parser, status, '|');
        std::getline(parser, hashValue, '|');

        if (status == "pending_approval" || status == "pending") {
            return docId;
        }
    }

    return "";
}

void seedApprovalsIfEmpty() {
    std::ifstream reader;
    if (!openInputFileWithFallback(reader, APPROVALS_FILE_PATH_PRIMARY, APPROVALS_FILE_PATH_FALLBACK)) {
        return;
    }

    std::string line;
    std::getline(reader, line); // Skip header.
    while (std::getline(reader, line)) {
        if (!line.empty()) {
            return;
        }
    }

    // Seed only once when approvals data has no rows at all.
    std::string docId = findPendingDocumentIdForSeeding();
    if (docId.empty()) {
        docId = "CA003";
    }

    std::string budgetOfficer = findAdminUsernameByRole("Budget Officer");
    std::string municipalAdmin = findAdminUsernameByRole("Municipal Administrator");

    if (budgetOfficer.empty() && municipalAdmin.empty()) {
        return;
    }

    std::ofstream writer;
    if (!openAppendFileWithFallback(writer, APPROVALS_FILE_PATH_PRIMARY, APPROVALS_FILE_PATH_FALLBACK)) {
        return;
    }

    if (!budgetOfficer.empty()) {
        writer << docId << '|'
               << budgetOfficer << '|'
               << "Budget Officer" << '|'
               << "pending" << '\n';
    }

    if (!municipalAdmin.empty()) {
        writer << docId << '|'
               << municipalAdmin << '|'
               << "Municipal Administrator" << '|'
               << "pending" << '\n';
    }

    writer.flush();
}

void clearInputBuffer() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

void printConsensusLegend() {
    std::cout << "  Consensus Legend: "
              << ui::consensusStatus("approved") << " | "
              << ui::consensusStatus("denied") << " | "
              << ui::consensusStatus("pending") << "\n";
}

void applyApprovalDecision(const Admin& admin, bool approve) {
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

    std::ifstream file;
    if (!openInputFileWithFallback(file, APPROVALS_FILE_PATH_PRIMARY, APPROVALS_FILE_PATH_FALLBACK)) {
        std::cout << ui::error("[!] Unable to open approvals file.") << "\n";
        logAuditAction("APPROVAL_FAILED", targetDocId, admin.username);
        waitForEnter();
        return;
    }

    std::string header;
    std::getline(file, header);

    // Load all rows, update only the current approver's pending record for the target doc.
    std::vector<Approval> rows;
    std::string line;
    bool updated = false;

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        std::stringstream parser(line);
        Approval row;
        std::getline(parser, row.docId, '|');
        std::getline(parser, row.approverUsername, '|');
        std::getline(parser, row.role, '|');
        std::getline(parser, row.status, '|');

        if (row.docId == targetDocId && row.approverUsername == admin.username && row.status == "pending") {
            row.status = approve ? "approved" : "rejected";
            updated = true;
        }

        rows.push_back(row);
    }

    if (!updated) {
        std::cout << ui::error("[!] No pending approval record found for this document and account.") << "\n";
        logAuditAction("APPROVAL_NOT_FOUND", targetDocId, admin.username);
        waitForEnter();
        return;
    }

    std::string targetPath = resolveDataPath(APPROVALS_FILE_PATH_PRIMARY, APPROVALS_FILE_PATH_FALLBACK);
    std::ofstream writer(targetPath);
    if (!writer.is_open()) {
        std::cout << ui::error("[!] Unable to update approvals file.") << "\n";
        logAuditAction("APPROVAL_FAILED", targetDocId, admin.username);
        waitForEnter();
        return;
    }

    writer << header << '\n';
    for (size_t i = 0; i < rows.size(); ++i) {
        writer << rows[i].docId << '|'
               << rows[i].approverUsername << '|'
               << rows[i].role << '|'
               << rows[i].status << '\n';
    }
    writer.flush();

    // Determine document status after this decision.
    // rejected  -> any approver rejects
    // published -> all required approvers have non-pending status and none rejected
    // pending   -> at least one required approver is still pending
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
        appendBlockchainAction("APPROVE", targetDocId, admin.username);
        logAuditAction("APPROVE_DOC", targetDocId, admin.username);
        std::cout << ui::success("[+] Document approved successfully.") << "\n";
    } else {
        appendBlockchainAction("REJECT", targetDocId, admin.username);
        logAuditAction("REJECT_DOC", targetDocId, admin.username);
        std::cout << ui::error("[+] Document denied successfully.") << "\n";
    }

    waitForEnter();
}
} // namespace

void ensureApprovalsDataFileExists() {
    std::ifstream checkFile;
    if (!openInputFileWithFallback(checkFile, APPROVALS_FILE_PATH_PRIMARY, APPROVALS_FILE_PATH_FALLBACK)) {
        std::ofstream createFile(APPROVALS_FILE_PATH_PRIMARY);
        if (!createFile.is_open()) {
            createFile.clear();
            createFile.open(APPROVALS_FILE_PATH_FALLBACK);
        }

        if (createFile.is_open()) {
            createFile << "docID|approverUsername|role|status\n";
        }
    }

    seedApprovalsIfEmpty();
}

void createApprovalRequestsForDocument(const std::string& docId, const std::string& uploader) {
    std::ifstream adminFile;
    if (!openInputFileWithFallback(adminFile, ADMINS_FILE_PATH_PRIMARY, ADMINS_FILE_PATH_FALLBACK)) {
        return;
    }

    std::string line;
    std::getline(adminFile, line); // Skip header.

    std::vector<Approval> requests;
    while (std::getline(adminFile, line)) {
        if (line.empty()) {
            continue;
        }

        std::stringstream parser(line);
        std::string adminId;
        std::string fullName;
        std::string username;
        std::string password;
        std::string role;

        std::getline(parser, adminId, '|');
        std::getline(parser, fullName, '|');
        std::getline(parser, username, '|');
        std::getline(parser, password, '|');
        std::getline(parser, role, '|');

        // Uploader should not approve their own upload.
        if (username == uploader) {
            continue;
        }

        // Only designated approver roles are part of the consensus requirement.
        if (!isRequiredApproverRole(role)) {
            continue;
        }

        Approval request;
        request.docId = docId;
        request.approverUsername = username;
        request.role = role;
        request.status = "pending";
        requests.push_back(request);
    }

    if (requests.empty()) {
        return;
    }

    std::ofstream file;
    if (!openAppendFileWithFallback(file, APPROVALS_FILE_PATH_PRIMARY, APPROVALS_FILE_PATH_FALLBACK)) {
        return;
    }

    for (size_t i = 0; i < requests.size(); ++i) {
        file << requests[i].docId << '|'
             << requests[i].approverUsername << '|'
             << requests[i].role << '|'
             << requests[i].status << '\n';
    }
    file.flush();

    logAuditAction("APPROVAL_REQUESTS_CREATED", docId, uploader);
}

void viewPendingApprovalsForAdmin(const Admin& admin) {
    clearScreen();
    ui::printSectionTitle("PENDING APPROVALS");
    printConsensusLegend();

    std::ifstream file;
    if (!openInputFileWithFallback(file, APPROVALS_FILE_PATH_PRIMARY, APPROVALS_FILE_PATH_FALLBACK)) {
        std::cout << ui::error("[!] Unable to open approvals file.") << "\n";
        logAuditAction("VIEW_PENDING_APPROVALS_FAILED", "N/A", admin.username);
        waitForEnter();
        return;
    }

    std::string line;
    std::getline(file, line); // Skip header.

    bool hasRows = false;
    std::vector<Approval> pendingRows;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        std::stringstream parser(line);
        Approval row;
        std::getline(parser, row.docId, '|');
        std::getline(parser, row.approverUsername, '|');
        std::getline(parser, row.role, '|');
        std::getline(parser, row.status, '|');

        if (row.approverUsername != admin.username || row.status != "pending") {
            continue;
        }

        hasRows = true;
        pendingRows.push_back(row);
    }

    if (!hasRows) {
        std::cout << ui::warning("[!] No pending approvals for your account.") << "\n";
        logAuditAction("VIEW_PENDING_APPROVALS_EMPTY", "N/A", admin.username);
    } else {
        const std::vector<std::string> headers = {"Document ID", "Role", "Status"};
        const std::vector<int> widths = {12, 24, 12};
        ui::printTableHeader(headers, widths);
        for (size_t i = 0; i < pendingRows.size(); ++i) {
            ui::printTableRow({pendingRows[i].docId, pendingRows[i].role, pendingRows[i].status}, widths);
        }
        ui::printTableFooter(widths);
        std::cout << "\n  Displayed status color: " << ui::consensusStatus("pending") << "\n";

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

void approveDocumentAsAdmin(const Admin& admin) {
    applyApprovalDecision(admin, true);
}

void rejectDocumentAsAdmin(const Admin& admin) {
    applyApprovalDecision(admin, false);
}
