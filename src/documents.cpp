#include "../include/documents.h"

#include "../include/approvals.h"
#include "../include/audit.h"
#include "../include/auth.h"
#include "../include/blockchain.h"
#include "../include/ui.h"
#include "../include/verification.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
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

void printDocumentsTable(const std::vector<Document>& docs, bool includeHashValue) {
    std::vector<int> widths;
    std::vector<std::string> headers;

    if (includeHashValue) {
        headers = {"ID", "Title", "Category", "Department", "Date", "Uploader", "Status", "Hash"};
        widths = {6, 20, 16, 16, 10, 12, 16, 10};
    } else {
        headers = {"ID", "Title", "Category", "Department", "Date", "Uploader", "Status"};
        widths = {6, 24, 16, 16, 10, 12, 16};
    }

    ui::printTableHeader(headers, widths);
    for (size_t i = 0; i < docs.size(); ++i) {
        if (includeHashValue) {
            ui::printTableRow({docs[i].docId,
                               docs[i].title,
                               docs[i].category,
                               docs[i].department,
                               docs[i].dateUploaded,
                               docs[i].uploader,
                               docs[i].status,
                               docs[i].hashValue},
                              widths);
        } else {
            ui::printTableRow({docs[i].docId,
                               docs[i].title,
                               docs[i].category,
                               docs[i].department,
                               docs[i].dateUploaded,
                               docs[i].uploader,
                               docs[i].status},
                              widths);
        }
    }
    ui::printTableFooter(widths);
}

void printStatusChart(const std::vector<Document>& docs) {
    std::map<std::string, double> counts;
    for (size_t i = 0; i < docs.size(); ++i) {
        counts[toLowerCopy(docs[i].status)] += 1.0;
    }

    if (counts.empty()) {
        return;
    }

    std::cout << "\n" << ui::bold("Status Distribution") << "\n";
    double maxCount = 0.0;
    for (std::map<std::string, double>::const_iterator it = counts.begin(); it != counts.end(); ++it) {
        if (it->second > maxCount) {
            maxCount = it->second;
        }
    }

    for (std::map<std::string, double>::const_iterator it = counts.begin(); it != counts.end(); ++it) {
        ui::printBar(it->first, it->second, maxCount, 24);
    }
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
    ui::printSectionTitle("PUBLISHED PROCUREMENT DOCUMENTS");

    std::ifstream file;
    if (!openInputFileWithFallback(file, DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK)) {
        std::cout << ui::error("[!] Unable to open documents file.") << "\n";
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
        std::cout << "\n" << ui::warning("[!] No published documents available yet.") << "\n";
        logAuditAction("VIEW_PUBLISHED_DOCS_EMPTY", "N/A", actor);
    } else {
        printDocumentsTable(publishedDocs, false);
        printStatusChart(publishedDocs);
        logAuditAction("VIEW_PUBLISHED_DOCS", "MULTI", actor);
    }

    waitForEnter();
}

void uploadDocumentAsAdmin(const Admin& admin) {
    clearScreen();
    ui::printSectionTitle("ADMIN DOCUMENT UPLOAD");

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
        std::cout << ui::warning("[!] Title, category, and department are required.") << "\n";
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
        std::cout << ui::error("[!] Unable to save document record.") << "\n";
        logAuditAction("UPLOAD_DOC_FAILED", docId, admin.username);
        waitForEnter();
        return;
    }

    file << hashSource << '|' << hashValue << '\n';
    file.flush();

    // Approval requests are generated immediately after upload for consensus workflow.
    createApprovalRequestsForDocument(docId, admin.username);
    appendBlockchainAction("UPLOAD", docId, admin.username);

    logAuditAction("UPLOAD_DOC", docId, admin.username);
    std::cout << ui::success("[+] Document uploaded successfully with ID ") << docId << ".\n";
    waitForEnter();
}

void viewAllDocumentsForAdmin(const Admin& admin) {
    clearScreen();
    ui::printSectionTitle("ALL DOCUMENT RECORDS");

    std::ifstream file;
    if (!openInputFileWithFallback(file, DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK)) {
        std::cout << ui::error("[!] Unable to open documents file.") << "\n";
        logAuditAction("VIEW_ALL_DOCS_FAILED", "N/A", admin.username);
        waitForEnter();
        return;
    }

    std::string line;
    std::getline(file, line); // Skip header.

    bool hasRows = false;
    std::vector<Document> allDocs;
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

        allDocs.push_back(doc);
    }

    if (!hasRows) {
        std::cout << "\n" << ui::warning("[!] No documents available.") << "\n";
        logAuditAction("VIEW_ALL_DOCS_EMPTY", "N/A", admin.username);
    } else {
        printDocumentsTable(allDocs, false);
        printStatusChart(allDocs);
        logAuditAction("VIEW_ALL_DOCS", "MULTI", admin.username);
    }

    waitForEnter();
}

void searchDocumentByIdForAdmin(const Admin& admin) {
    clearScreen();
    ui::printSectionTitle("SEARCH DOCUMENT BY ID");

    clearInputBuffer();
    std::string targetDocId;
    std::cout << "Enter Document ID: ";
    std::getline(std::cin, targetDocId);

    if (targetDocId.empty()) {
        std::cout << ui::warning("[!] Document ID is required.") << "\n";
        logAuditAction("SEARCH_DOC_INPUT_ERROR", "N/A", admin.username);
        waitForEnter();
        return;
    }

    std::ifstream file;
    if (!openInputFileWithFallback(file, DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK)) {
        std::cout << ui::error("[!] Unable to open documents file.") << "\n";
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
        printDocumentsTable(std::vector<Document>(1, doc), true);
        break;
    }

    if (!found) {
        std::cout << "\n" << ui::warning("[!] Document ID not found.") << "\n";
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
    ui::printSectionTitle("UPDATE DOCUMENT STATUS");

    clearInputBuffer();
    std::string targetDocId;
    std::cout << "Enter Document ID: ";
    std::getline(std::cin, targetDocId);

    if (targetDocId.empty()) {
        std::cout << ui::warning("[!] Document ID is required.") << "\n";
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
        std::cout << ui::warning("[!] Invalid status input.") << "\n";
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
        std::cout << ui::warning("[!] Invalid status option.") << "\n";
        logAuditAction("UPDATE_STATUS_INPUT_ERROR", targetDocId, admin.username);
        waitForEnter();
        return;
    }

    if (!updateDocumentStatusBySystem(targetDocId, newStatus)) {
        std::cout << "\n" << ui::error("[!] Document ID not found or update failed.") << "\n";
        logAuditAction("UPDATE_STATUS_FAILED", targetDocId, admin.username);
        waitForEnter();
        return;
    }

    appendBlockchainAction("STATUS_UPDATE_" + newStatus, targetDocId, admin.username);
    logAuditAction("UPDATE_STATUS", targetDocId, admin.username);
    std::cout << ui::success("[+] Document status updated successfully.") << "\n";
    waitForEnter();
}
