#include "../include/approvals.h"

#include "../include/audit.h"
#include "../include/auth.h"
#include "../include/documents.h"

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

void clearInputBuffer() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

void applyApprovalDecision(const Admin& admin, bool approve) {
    clearScreen();
    std::cout << "\n==============================================================\n";
    std::cout << "  " << (approve ? "APPROVE DOCUMENT" : "REJECT DOCUMENT") << "\n";
    std::cout << "==============================================================\n";

    clearInputBuffer();
    std::string targetDocId;
    std::cout << "Enter Document ID: ";
    std::getline(std::cin, targetDocId);

    if (targetDocId.empty()) {
        std::cout << "[!] Document ID is required.\n";
        logAuditAction("APPROVAL_INPUT_ERROR", "N/A", admin.username);
        waitForEnter();
        return;
    }

    std::ifstream file;
    if (!openInputFileWithFallback(file, APPROVALS_FILE_PATH_PRIMARY, APPROVALS_FILE_PATH_FALLBACK)) {
        std::cout << "[!] Unable to open approvals file.\n";
        logAuditAction("APPROVAL_FAILED", targetDocId, admin.username);
        waitForEnter();
        return;
    }

    std::string header;
    std::getline(file, header);

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
        std::cout << "[!] No pending approval record found for this document and account.\n";
        logAuditAction("APPROVAL_NOT_FOUND", targetDocId, admin.username);
        waitForEnter();
        return;
    }

    std::string targetPath = resolveDataPath(APPROVALS_FILE_PATH_PRIMARY, APPROVALS_FILE_PATH_FALLBACK);
    std::ofstream writer(targetPath);
    if (!writer.is_open()) {
        std::cout << "[!] Unable to update approvals file.\n";
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

    updateDocumentStatusBySystem(targetDocId, nextDocStatus);

    if (approve) {
        logAuditAction("APPROVE_DOC", targetDocId, admin.username);
        std::cout << "[+] Document approved successfully.\n";
    } else {
        logAuditAction("REJECT_DOC", targetDocId, admin.username);
        std::cout << "[+] Document rejected successfully.\n";
    }

    waitForEnter();
}
} // namespace

void ensureApprovalsDataFileExists() {
    std::ifstream checkFile;
    if (openInputFileWithFallback(checkFile, APPROVALS_FILE_PATH_PRIMARY, APPROVALS_FILE_PATH_FALLBACK)) {
        return;
    }

    std::ofstream createFile(APPROVALS_FILE_PATH_PRIMARY);
    if (!createFile.is_open()) {
        createFile.clear();
        createFile.open(APPROVALS_FILE_PATH_FALLBACK);
    }

    if (createFile.is_open()) {
        createFile << "docID|approverUsername|role|status\n";
    }
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
    std::cout << "\n==============================================================\n";
    std::cout << "  PENDING APPROVALS\n";
    std::cout << "==============================================================\n";

    std::ifstream file;
    if (!openInputFileWithFallback(file, APPROVALS_FILE_PATH_PRIMARY, APPROVALS_FILE_PATH_FALLBACK)) {
        std::cout << "[!] Unable to open approvals file.\n";
        logAuditAction("VIEW_PENDING_APPROVALS_FAILED", "N/A", admin.username);
        waitForEnter();
        return;
    }

    std::string line;
    std::getline(file, line); // Skip header.

    bool hasRows = false;
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
        std::cout << "Document ID : " << row.docId << '\n';
        std::cout << "Role        : " << row.role << '\n';
        std::cout << "Status      : " << row.status << '\n';
        std::cout << "--------------------------------------------------------------\n";
    }

    if (!hasRows) {
        std::cout << "[!] No pending approvals for your account.\n";
        logAuditAction("VIEW_PENDING_APPROVALS_EMPTY", "N/A", admin.username);
    } else {
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
