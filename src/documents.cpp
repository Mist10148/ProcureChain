#include "../include/documents.h"

#include "../include/approvals.h"
#include "../include/audit.h"
#include "../include/auth.h"
#include "../include/verification.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>
#include <algorithm>

namespace {
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

std::string toLowerCopy(std::string value) {
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] >= 'A' && value[i] <= 'Z') {
            value[i] = static_cast<char>(value[i] + ('a' - 'A'));
        }
    }
    return value;
}

void clearInputBuffer() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}
} // namespace

std::string generateNextDocumentId() {
    std::ifstream file;
    if (!openInputFileWithFallback(file, DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK)) {
        return "D001";
    }

    std::string line;
    std::getline(file, line); // Skip header.

    int maxIdNumber = 0;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        std::stringstream parser(line);
        std::string docId;
        std::getline(parser, docId, '|');

        if (docId.size() < 2) {
            continue;
        }

        size_t numericStart = 1;
        while (numericStart < docId.size() && !std::isdigit(static_cast<unsigned char>(docId[numericStart]))) {
            numericStart++;
        }

        if (numericStart >= docId.size()) {
            continue;
        }

        int parsedNumber = std::atoi(docId.substr(numericStart).c_str());
        if (parsedNumber > maxIdNumber) {
            maxIdNumber = parsedNumber;
        }
    }

    std::ostringstream idBuilder;
    idBuilder << 'D' << std::setw(3) << std::setfill('0') << (maxIdNumber + 1);
    return idBuilder.str();
}

void ensureSampleDocumentsPresent() {
    std::ifstream reader;
    if (!openInputFileWithFallback(reader, DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK)) {
        return;
    }

    std::string line;
    int nonEmptyLines = 0;
    while (std::getline(reader, line)) {
        if (!line.empty()) {
            nonEmptyLines++;
        }
    }

    if (nonEmptyLines > 1) {
        return;
    }

    reader.close();

    std::ofstream writer;
    if (!openAppendFileWithFallback(writer, DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK)) {
        return;
    }

    std::string row1Source = "PR001|Office Supplies Purchase|Purchase Request|Procurement Office|2026-04-09|proc_admin|published";
    std::string row2Source = "PO002|Road Repair Contract|Purchase Order|Engineering Department|2026-04-09|proc_admin|published";
    std::string row3Source = "CA003|Clinic Equipment Acquisition|Contract|Health Office|2026-04-09|proc_admin|pending_approval";

    writer << row1Source << '|' << computeSimpleHash(row1Source) << '\n';
    writer << row2Source << '|' << computeSimpleHash(row2Source) << '\n';
    writer << row3Source << '|' << computeSimpleHash(row3Source) << '\n';
    writer.flush();
}

void showPublishedDocuments(const std::string& actor) {
    clearScreen();
    std::cout << "\n==============================================================\n";
    std::cout << "  PUBLISHED PROCUREMENT DOCUMENTS\n";
    std::cout << "==============================================================\n";

    std::ifstream file;
    if (!openInputFileWithFallback(file, DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK)) {
        std::cout << "[!] Unable to open documents file.\n";
        logAuditAction("VIEW_PUBLISHED_DOCS_FAILED", "N/A", actor);
        waitForEnter();
        return;
    }

    std::string line;
    std::getline(file, line); // Skip header.

    std::vector<Document> publishedDocs;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        std::stringstream parser(line);
        Document doc;
        std::getline(parser, doc.docId, '|');
        std::getline(parser, doc.title, '|');
        std::getline(parser, doc.category, '|');
        std::getline(parser, doc.department, '|');
        std::getline(parser, doc.dateUploaded, '|');
        std::getline(parser, doc.uploader, '|');
        std::getline(parser, doc.status, '|');
        std::getline(parser, doc.hashValue, '|');

        if (toLowerCopy(doc.status) == "published") {
            publishedDocs.push_back(doc);
        }
    }

    std::sort(publishedDocs.begin(), publishedDocs.end(), [](const Document& a, const Document& b) {
        return a.docId < b.docId;
    });

    if (publishedDocs.empty()) {
        std::cout << "\n[!] No published documents available yet.\n";
        logAuditAction("VIEW_PUBLISHED_DOCS_EMPTY", "N/A", actor);
    } else {
        for (size_t i = 0; i < publishedDocs.size(); ++i) {
            std::cout << "\nDocument ID : " << publishedDocs[i].docId << '\n';
            std::cout << "Title       : " << publishedDocs[i].title << '\n';
            std::cout << "Category    : " << publishedDocs[i].category << '\n';
            std::cout << "Department  : " << publishedDocs[i].department << '\n';
            std::cout << "Uploaded On : " << publishedDocs[i].dateUploaded << '\n';
            std::cout << "Uploader    : " << publishedDocs[i].uploader << '\n';
            std::cout << "Status      : " << publishedDocs[i].status << '\n';
            std::cout << "--------------------------------------------------------------\n";
        }
        logAuditAction("VIEW_PUBLISHED_DOCS", "MULTI", actor);
    }

    waitForEnter();
}

void uploadDocumentAsAdmin(const Admin& admin) {
    clearScreen();
    std::cout << "\n==============================================================\n";
    std::cout << "  ADMIN DOCUMENT UPLOAD\n";
    std::cout << "==============================================================\n";

    clearInputBuffer();

    std::string title;
    std::string category;
    std::string department;

    std::cout << "Title      : ";
    std::getline(std::cin, title);
    std::cout << "Category   : ";
    std::getline(std::cin, category);
    std::cout << "Department : ";
    std::getline(std::cin, department);

    if (title.empty() || category.empty() || department.empty()) {
        std::cout << "[!] Title, category, and department are required.\n";
        logAuditAction("UPLOAD_DOC_FAILED", "N/A", admin.username);
        waitForEnter();
        return;
    }

    std::string docId = generateNextDocumentId();
    std::string dateUploaded = getCurrentTimestamp().substr(0, 10);
    std::string status = "pending_approval";

    // Hash source excludes hash column itself for stable recomputation.
    std::string hashSource = docId + "|" + title + "|" + category + "|" + department + "|" +
                             dateUploaded + "|" + admin.username + "|" + status;
    std::string hashValue = computeSimpleHash(hashSource);

    std::ofstream file;
    if (!openAppendFileWithFallback(file, DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK)) {
        std::cout << "[!] Unable to save document record.\n";
        logAuditAction("UPLOAD_DOC_FAILED", docId, admin.username);
        waitForEnter();
        return;
    }

    file << hashSource << '|' << hashValue << '\n';
    file.flush();

    // Approval requests are generated immediately after upload for consensus workflow.
    createApprovalRequestsForDocument(docId, admin.username);

    logAuditAction("UPLOAD_DOC", docId, admin.username);
    std::cout << "[+] Document uploaded successfully with ID " << docId << ".\n";
    waitForEnter();
}

void viewAllDocumentsForAdmin(const Admin& admin) {
    clearScreen();
    std::cout << "\n==============================================================\n";
    std::cout << "  ALL DOCUMENT RECORDS\n";
    std::cout << "==============================================================\n";

    std::ifstream file;
    if (!openInputFileWithFallback(file, DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK)) {
        std::cout << "[!] Unable to open documents file.\n";
        logAuditAction("VIEW_ALL_DOCS_FAILED", "N/A", admin.username);
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

        hasRows = true;
        std::stringstream parser(line);
        Document doc;
        std::getline(parser, doc.docId, '|');
        std::getline(parser, doc.title, '|');
        std::getline(parser, doc.category, '|');
        std::getline(parser, doc.department, '|');
        std::getline(parser, doc.dateUploaded, '|');
        std::getline(parser, doc.uploader, '|');
        std::getline(parser, doc.status, '|');
        std::getline(parser, doc.hashValue, '|');

        std::cout << "\nDocument ID : " << doc.docId << '\n';
        std::cout << "Title       : " << doc.title << '\n';
        std::cout << "Category    : " << doc.category << '\n';
        std::cout << "Department  : " << doc.department << '\n';
        std::cout << "Uploaded On : " << doc.dateUploaded << '\n';
        std::cout << "Uploader    : " << doc.uploader << '\n';
        std::cout << "Status      : " << doc.status << '\n';
        std::cout << "--------------------------------------------------------------\n";
    }

    if (!hasRows) {
        std::cout << "\n[!] No documents available.\n";
        logAuditAction("VIEW_ALL_DOCS_EMPTY", "N/A", admin.username);
    } else {
        logAuditAction("VIEW_ALL_DOCS", "MULTI", admin.username);
    }

    waitForEnter();
}

void searchDocumentByIdForAdmin(const Admin& admin) {
    clearScreen();
    std::cout << "\n==============================================================\n";
    std::cout << "  SEARCH DOCUMENT BY ID\n";
    std::cout << "==============================================================\n";

    clearInputBuffer();
    std::string targetDocId;
    std::cout << "Enter Document ID: ";
    std::getline(std::cin, targetDocId);

    if (targetDocId.empty()) {
        std::cout << "[!] Document ID is required.\n";
        logAuditAction("SEARCH_DOC_INPUT_ERROR", "N/A", admin.username);
        waitForEnter();
        return;
    }

    std::ifstream file;
    if (!openInputFileWithFallback(file, DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK)) {
        std::cout << "[!] Unable to open documents file.\n";
        logAuditAction("SEARCH_DOC_FAILED", targetDocId, admin.username);
        waitForEnter();
        return;
    }

    std::string line;
    std::getline(file, line); // Skip header.

    bool found = false;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        std::stringstream parser(line);
        Document doc;
        std::getline(parser, doc.docId, '|');
        std::getline(parser, doc.title, '|');
        std::getline(parser, doc.category, '|');
        std::getline(parser, doc.department, '|');
        std::getline(parser, doc.dateUploaded, '|');
        std::getline(parser, doc.uploader, '|');
        std::getline(parser, doc.status, '|');
        std::getline(parser, doc.hashValue, '|');

        if (doc.docId != targetDocId) {
            continue;
        }

        found = true;
        std::cout << "\nDocument ID : " << doc.docId << '\n';
        std::cout << "Title       : " << doc.title << '\n';
        std::cout << "Category    : " << doc.category << '\n';
        std::cout << "Department  : " << doc.department << '\n';
        std::cout << "Uploaded On : " << doc.dateUploaded << '\n';
        std::cout << "Uploader    : " << doc.uploader << '\n';
        std::cout << "Status      : " << doc.status << '\n';
        std::cout << "Hash Value  : " << doc.hashValue << '\n';
        break;
    }

    if (!found) {
        std::cout << "\n[!] Document ID not found.\n";
        logAuditAction("SEARCH_DOC_NOT_FOUND", targetDocId, admin.username);
    } else {
        logAuditAction("SEARCH_DOC", targetDocId, admin.username);
    }

    waitForEnter();
}

bool updateDocumentStatusBySystem(const std::string& targetDocId, const std::string& newStatus) {
    std::ifstream file;
    if (!openInputFileWithFallback(file, DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK)) {
        return false;
    }

    std::vector<std::string> updatedRows;
    std::string header;
    std::getline(file, header);

    bool found = false;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        std::stringstream parser(line);
        Document doc;
        std::getline(parser, doc.docId, '|');
        std::getline(parser, doc.title, '|');
        std::getline(parser, doc.category, '|');
        std::getline(parser, doc.department, '|');
        std::getline(parser, doc.dateUploaded, '|');
        std::getline(parser, doc.uploader, '|');
        std::getline(parser, doc.status, '|');
        std::getline(parser, doc.hashValue, '|');

        if (doc.docId == targetDocId) {
            found = true;
            doc.status = newStatus;

            // Recompute hash whenever status changes to keep verify flow consistent.
            std::string hashSource = doc.docId + "|" + doc.title + "|" + doc.category + "|" + doc.department + "|" +
                                     doc.dateUploaded + "|" + doc.uploader + "|" + doc.status;
            doc.hashValue = computeSimpleHash(hashSource);
            updatedRows.push_back(hashSource + "|" + doc.hashValue);
        } else {
            updatedRows.push_back(line);
        }
    }

    if (!found) {
        return false;
    }

    std::string targetPath = resolveDataPath(DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK);
    std::ofstream writer(targetPath);
    if (!writer.is_open()) {
        return false;
    }

    writer << header << '\n';
    for (size_t i = 0; i < updatedRows.size(); ++i) {
        writer << updatedRows[i] << '\n';
    }
    writer.flush();

    return true;
}

void updateDocumentStatusForAdmin(const Admin& admin) {
    clearScreen();
    std::cout << "\n==============================================================\n";
    std::cout << "  UPDATE DOCUMENT STATUS\n";
    std::cout << "==============================================================\n";

    clearInputBuffer();
    std::string targetDocId;
    std::cout << "Enter Document ID: ";
    std::getline(std::cin, targetDocId);

    if (targetDocId.empty()) {
        std::cout << "[!] Document ID is required.\n";
        logAuditAction("UPDATE_STATUS_INPUT_ERROR", "N/A", admin.username);
        waitForEnter();
        return;
    }

    std::cout << "\nSelect new status:\n";
    std::cout << "  [1] pending_approval\n";
    std::cout << "  [2] published\n";
    std::cout << "  [3] rejected\n";
    std::cout << "  Enter choice: ";

    int statusChoice = 0;
    std::cin >> statusChoice;
    if (std::cin.fail()) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "[!] Invalid status input.\n";
        logAuditAction("UPDATE_STATUS_INPUT_ERROR", targetDocId, admin.username);
        waitForEnter();
        return;
    }

    std::string newStatus;
    if (statusChoice == 1) {
        newStatus = "pending_approval";
    } else if (statusChoice == 2) {
        newStatus = "published";
    } else if (statusChoice == 3) {
        newStatus = "rejected";
    } else {
        std::cout << "[!] Invalid status option.\n";
        logAuditAction("UPDATE_STATUS_INPUT_ERROR", targetDocId, admin.username);
        waitForEnter();
        return;
    }

    if (!updateDocumentStatusBySystem(targetDocId, newStatus)) {
        std::cout << "\n[!] Document ID not found or update failed.\n";
        logAuditAction("UPDATE_STATUS_FAILED", targetDocId, admin.username);
        waitForEnter();
        return;
    }

    logAuditAction("UPDATE_STATUS", targetDocId, admin.username);
    std::cout << "[+] Document status updated successfully.\n";
    waitForEnter();
}
