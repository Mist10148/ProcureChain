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

struct DocumentFilter {
    std::string status;
    std::string exactDate;
    std::string fromDate;
    std::string toDate;
    std::string category;
    std::string department;
    std::string uploader;
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

std::vector<std::string> splitPipe(const std::string& line) {
    std::vector<std::string> tokens;
    std::stringstream parser(line);
    std::string token;
    while (std::getline(parser, token, '|')) {
        tokens.push_back(token);
    }
    return tokens;
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
    return toLowerCopy(text).find(toLowerCopy(token)) != std::string::npos;
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

Document parseDocumentTokens(const std::vector<std::string>& tokens) {
    Document doc;
    doc.docId = tokens.size() > 0 ? tokens[0] : "";
    doc.title = tokens.size() > 1 ? tokens[1] : "";
    doc.category = tokens.size() > 2 ? tokens[2] : "";
    doc.department = tokens.size() > 3 ? tokens[3] : "";
    doc.dateUploaded = tokens.size() > 4 ? tokens[4] : "";
    doc.uploader = tokens.size() > 5 ? tokens[5] : "";
    doc.status = tokens.size() > 6 ? tokens[6] : "pending_approval";
    doc.hashValue = tokens.size() > 7 ? tokens[7] : "";
    doc.budgetCategory = tokens.size() > 8 ? tokens[8] : doc.category;
    doc.amount = 0.0;

    if (tokens.size() > 9) {
        std::stringstream amountIn(tokens[9]);
        amountIn >> doc.amount;
    }

    if (doc.budgetCategory.empty()) {
        doc.budgetCategory = doc.category;
    }

    return doc;
}

std::string serializeDocument(const Document& doc) {
    // Hash source intentionally keeps original 7 fields to preserve compatibility with existing verification logic.
    const std::string hashSource = doc.docId + "|" + doc.title + "|" + doc.category + "|" + doc.department + "|" +
                                   doc.dateUploaded + "|" + doc.uploader + "|" + doc.status;

    std::ostringstream out;
    out << hashSource << '|'
        << computeSimpleHash(hashSource) << '|'
        << doc.budgetCategory << '|'
        << std::fixed << std::setprecision(2) << doc.amount;
    return out.str();
}

bool loadDocuments(std::vector<Document>& docs) {
    std::ifstream file;
    if (!openInputFileWithFallback(file, DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK)) {
        return false;
    }

    docs.clear();
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

        Document parsed = parseDocumentTokens(splitPipe(line));
        if (parsed.docId.empty()) {
            continue;
        }

        docs.push_back(parsed);
    }

    return true;
}

bool saveDocuments(const std::vector<Document>& docs) {
    std::ofstream writer(resolveDataPath(DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK));
    if (!writer.is_open()) {
        return false;
    }

    writer << "docID|title|category|department|dateUploaded|uploader|status|hashValue|budgetCategory|amount\n";
    for (size_t i = 0; i < docs.size(); ++i) {
        writer << serializeDocument(docs[i]) << '\n';
    }
    writer.flush();
    return true;
}

std::string statusDisplayLabel(const std::string& normalizedStatus) {
    if (normalizedStatus == "approved" || normalizedStatus == "published") {
        return "approved";
    }

    if (normalizedStatus == "denied" || normalizedStatus == "rejected") {
        return "denied";
    }

    if (normalizedStatus == "pending" || normalizedStatus == "pending_approval") {
        return "pending";
    }

    return normalizedStatus;
}

void printConsensusLegend() {
    std::cout << "  Consensus Legend: "
              << ui::consensusStatus("approved") << " | "
              << ui::consensusStatus("denied") << " | "
              << ui::consensusStatus("pending") << "\n";
}

void clearInputBuffer() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

void printDocumentsTable(const std::vector<Document>& docs, bool includeHashValue) {
    std::vector<int> widths;
    std::vector<std::string> headers;

    if (includeHashValue) {
        headers = {"ID", "Title", "Category", "Department", "Date", "Uploader", "Status", "Hash", "BudgetCat", "Amount"};
        widths = {6, 18, 14, 14, 10, 12, 14, 9, 12, 8};
    } else {
        headers = {"ID", "Title", "Category", "Department", "Date", "Uploader", "Status", "BudgetCat", "Amount"};
        widths = {6, 20, 14, 14, 10, 12, 14, 12, 8};
    }

    ui::printTableHeader(headers, widths);
    for (size_t i = 0; i < docs.size(); ++i) {
        std::ostringstream amountOut;
        amountOut << std::fixed << std::setprecision(2) << docs[i].amount;

        if (includeHashValue) {
            ui::printTableRow({docs[i].docId,
                               docs[i].title,
                               docs[i].category,
                               docs[i].department,
                               docs[i].dateUploaded,
                               docs[i].uploader,
                               docs[i].status,
                               docs[i].hashValue,
                               docs[i].budgetCategory,
                               amountOut.str()},
                              widths);
        } else {
            ui::printTableRow({docs[i].docId,
                               docs[i].title,
                               docs[i].category,
                               docs[i].department,
                               docs[i].dateUploaded,
                               docs[i].uploader,
                               docs[i].status,
                               docs[i].budgetCategory,
                               amountOut.str()},
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
    printConsensusLegend();
    double maxCount = 0.0;
    for (std::map<std::string, double>::const_iterator it = counts.begin(); it != counts.end(); ++it) {
        if (it->second > maxCount) {
            maxCount = it->second;
        }
    }

    for (std::map<std::string, double>::const_iterator it = counts.begin(); it != counts.end(); ++it) {
        ui::printBar(statusDisplayLabel(it->first), it->second, maxCount, 24, ui::consensusColorCode(it->first));
    }
}

bool matchesFilter(const Document& doc, const DocumentFilter& filter) {
    if (!filter.status.empty() && toLowerCopy(doc.status) != toLowerCopy(filter.status)) {
        return false;
    }

    if (!filter.exactDate.empty() && doc.dateUploaded != filter.exactDate) {
        return false;
    }

    // Date values use YYYY-MM-DD so lexical comparison is safe for inclusive range checks.
    if (!filter.fromDate.empty() && doc.dateUploaded < filter.fromDate) {
        return false;
    }

    if (!filter.toDate.empty() && doc.dateUploaded > filter.toDate) {
        return false;
    }

    if (!containsCaseInsensitive(doc.category, filter.category)) {
        return false;
    }

    if (!containsCaseInsensitive(doc.department, filter.department)) {
        return false;
    }

    if (!containsCaseInsensitive(doc.uploader, filter.uploader)) {
        return false;
    }

    return true;
}
} // namespace

std::string generateNextDocumentId() {
    std::vector<Document> docs;
    if (!loadDocuments(docs)) {
        return "D001";
    }

    int maxIdNumber = 0;
    for (size_t i = 0; i < docs.size(); ++i) {
        if (docs[i].docId.size() < 2) {
            continue;
        }

        size_t numericStart = 1;
        while (numericStart < docs[i].docId.size() && !std::isdigit(static_cast<unsigned char>(docs[i].docId[numericStart]))) {
            numericStart++;
        }

        if (numericStart >= docs[i].docId.size()) {
            continue;
        }

        const int parsedNumber = std::atoi(docs[i].docId.substr(numericStart).c_str());
        if (parsedNumber > maxIdNumber) {
            maxIdNumber = parsedNumber;
        }
    }

    std::ostringstream idBuilder;
    idBuilder << 'D' << std::setw(3) << std::setfill('0') << (maxIdNumber + 1);
    return idBuilder.str();
}

void ensureSampleDocumentsPresent() {
    std::vector<Document> docs;
    if (!loadDocuments(docs)) {
        return;
    }

    if (!docs.empty()) {
        saveDocuments(docs);
        return;
    }

    Document row1;
    row1.docId = "PR001";
    row1.title = "Office Supplies Purchase";
    row1.category = "Purchase Request";
    row1.department = "Procurement Office";
    row1.dateUploaded = "2026-04-09";
    row1.uploader = "proc_admin";
    row1.status = "published";
    row1.budgetCategory = "Office Supplies";
    row1.amount = 5000.0;

    Document row2;
    row2.docId = "PO002";
    row2.title = "Road Repair Contract";
    row2.category = "Purchase Order";
    row2.department = "Engineering Department";
    row2.dateUploaded = "2026-04-09";
    row2.uploader = "proc_admin";
    row2.status = "published";
    row2.budgetCategory = "Infrastructure Procurement";
    row2.amount = 45000.0;

    Document row3;
    row3.docId = "CA003";
    row3.title = "Clinic Equipment Acquisition";
    row3.category = "Contract";
    row3.department = "Health Office";
    row3.dateUploaded = "2026-04-09";
    row3.uploader = "proc_admin";
    row3.status = "pending_approval";
    row3.budgetCategory = "Health Supplies";
    row3.amount = 25000.0;

    std::vector<Document> seedRows;
    seedRows.push_back(row1);
    seedRows.push_back(row2);
    seedRows.push_back(row3);
    saveDocuments(seedRows);
}

void showPublishedDocuments(const std::string& actor) {
    clearScreen();
    ui::printSectionTitle("PUBLISHED PROCUREMENT DOCUMENTS");

    std::vector<Document> docs;
    if (!loadDocuments(docs)) {
        std::cout << ui::error("[!] Unable to open documents file.") << "\n";
        logAuditAction("VIEW_PUBLISHED_DOCS_FAILED", "N/A", actor);
        waitForEnter();
        return;
    }

    std::vector<Document> publishedDocs;
    for (size_t i = 0; i < docs.size(); ++i) {
        if (toLowerCopy(docs[i].status) == "published") {
            publishedDocs.push_back(docs[i]);
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

    Document doc;
    std::cout << "Title      : ";
    std::getline(std::cin, doc.title);
    std::cout << "Category   : ";
    std::getline(std::cin, doc.category);
    std::cout << "Department : ";
    std::getline(std::cin, doc.department);
    std::cout << "Budget Category (blank = same as Category): ";
    std::getline(std::cin, doc.budgetCategory);

    if (doc.title.empty() || doc.category.empty() || doc.department.empty()) {
        std::cout << ui::error("[!] Title, category, and department are required.") << "\n";
        logAuditAction("UPLOAD_DOC_FAILED", "N/A", admin.username);
        waitForEnter();
        return;
    }

    if (doc.budgetCategory.empty()) {
        doc.budgetCategory = doc.category;
    }

    std::cout << "Document Amount (PHP): ";
    std::cin >> doc.amount;
    if (std::cin.fail() || doc.amount < 0.0) {
        std::cin.clear();
        clearInputBuffer();
        std::cout << ui::error("[!] Invalid document amount.") << "\n";
        logAuditAction("UPLOAD_DOC_FAILED", "N/A", admin.username);
        waitForEnter();
        return;
    }

    doc.docId = generateNextDocumentId();
    doc.dateUploaded = getCurrentTimestamp().substr(0, 10);
    doc.uploader = admin.username;
    doc.status = "pending_approval";

    std::ofstream file;
    if (!openAppendFileWithFallback(file, DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK)) {
        std::cout << ui::error("[!] Unable to save document record.") << "\n";
        logAuditAction("UPLOAD_DOC_FAILED", doc.docId, admin.username);
        waitForEnter();
        return;
    }

    file << serializeDocument(doc) << '\n';
    file.flush();

    createApprovalRequestsForDocument(doc.docId, admin.username);
    const int chainIndex = appendBlockchainAction("UPLOAD", doc.docId, admin.username);

    logAuditAction("UPLOAD_DOC", doc.docId, admin.username, chainIndex);
    std::cout << ui::success("[+] Document uploaded successfully with ID ") << doc.docId << ".\n";
    waitForEnter();
}

void viewAllDocumentsForAdmin(const Admin& admin) {
    clearScreen();
    ui::printSectionTitle("ALL DOCUMENT RECORDS");

    std::vector<Document> docs;
    if (!loadDocuments(docs)) {
        std::cout << ui::error("[!] Unable to open documents file.") << "\n";
        logAuditAction("VIEW_ALL_DOCS_FAILED", "N/A", admin.username);
        waitForEnter();
        return;
    }

    if (docs.empty()) {
        std::cout << "\n" << ui::warning("[!] No documents available.") << "\n";
        logAuditAction("VIEW_ALL_DOCS_EMPTY", "N/A", admin.username);
    } else {
        printDocumentsTable(docs, false);
        printStatusChart(docs);
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
        std::cout << ui::error("[!] Document ID is required.") << "\n";
        logAuditAction("SEARCH_DOC_INPUT_ERROR", "N/A", admin.username);
        waitForEnter();
        return;
    }

    std::vector<Document> docs;
    if (!loadDocuments(docs)) {
        std::cout << ui::error("[!] Unable to open documents file.") << "\n";
        logAuditAction("SEARCH_DOC_FAILED", targetDocId, admin.username);
        waitForEnter();
        return;
    }

    for (size_t i = 0; i < docs.size(); ++i) {
        if (docs[i].docId != targetDocId) {
            continue;
        }

        printDocumentsTable(std::vector<Document>(1, docs[i]), true);
        logAuditAction("SEARCH_DOC", targetDocId, admin.username);
        waitForEnter();
        return;
    }

    std::cout << "\n" << ui::warning("[!] Document ID not found.") << "\n";
    logAuditAction("SEARCH_DOC_NOT_FOUND", targetDocId, admin.username);
    waitForEnter();
}

void filterDocumentsForAdmin(const Admin& admin) {
    clearScreen();
    ui::printSectionTitle("ADVANCED DOCUMENT FILTERS");

    std::vector<Document> docs;
    if (!loadDocuments(docs)) {
        std::cout << ui::error("[!] Unable to open documents file.") << "\n";
        logAuditAction("FILTER_DOCS_FAILED", "N/A", admin.username);
        waitForEnter();
        return;
    }

    if (docs.empty()) {
        std::cout << ui::warning("[!] No documents available for filtering.") << "\n";
        waitForEnter();
        return;
    }

    clearInputBuffer();
    DocumentFilter filter;
    std::cout << "Status (optional, e.g. pending_approval/published/rejected): ";
    std::getline(std::cin, filter.status);
    std::cout << "Exact upload date (YYYY-MM-DD, optional): ";
    std::getline(std::cin, filter.exactDate);
    std::cout << "From date (YYYY-MM-DD, optional): ";
    std::getline(std::cin, filter.fromDate);
    std::cout << "To date (YYYY-MM-DD, optional): ";
    std::getline(std::cin, filter.toDate);
    std::cout << "Category contains (optional): ";
    std::getline(std::cin, filter.category);
    std::cout << "Department contains (optional): ";
    std::getline(std::cin, filter.department);
    std::cout << "Uploader contains (optional): ";
    std::getline(std::cin, filter.uploader);

    if (!isDateTextValid(filter.exactDate) || !isDateTextValid(filter.fromDate) || !isDateTextValid(filter.toDate)) {
        std::cout << ui::error("[!] Invalid date format. Use YYYY-MM-DD.") << "\n";
        logAuditAction("FILTER_DOCS_INPUT_ERROR", "DATE", admin.username);
        waitForEnter();
        return;
    }

    if (!filter.exactDate.empty() && (!filter.fromDate.empty() || !filter.toDate.empty())) {
        std::cout << ui::warning("[!] Exact date was provided; date range fields will be ignored.") << "\n";
        filter.fromDate.clear();
        filter.toDate.clear();
    }

    if (!filter.fromDate.empty() && !filter.toDate.empty() && filter.fromDate > filter.toDate) {
        std::cout << ui::error("[!] Date range is invalid: fromDate is after toDate.") << "\n";
        logAuditAction("FILTER_DOCS_INPUT_ERROR", "RANGE", admin.username);
        waitForEnter();
        return;
    }

    std::vector<Document> filtered;
    for (size_t i = 0; i < docs.size(); ++i) {
        if (matchesFilter(docs[i], filter)) {
            filtered.push_back(docs[i]);
        }
    }

    if (filtered.empty()) {
        std::cout << "\n" << ui::warning("[!] No records matched the selected filters.") << "\n";
        logAuditAction("FILTER_DOCS_EMPTY", "MULTI", admin.username);
    } else {
        printDocumentsTable(filtered, false);
        printStatusChart(filtered);
        logAuditAction("FILTER_DOCS", "MULTI", admin.username);
    }

    waitForEnter();
}

bool updateDocumentStatusBySystem(const std::string& targetDocId, const std::string& newStatus) {
    std::vector<Document> docs;
    if (!loadDocuments(docs)) {
        return false;
    }

    bool found = false;
    for (size_t i = 0; i < docs.size(); ++i) {
        if (docs[i].docId == targetDocId) {
            docs[i].status = newStatus;
            found = true;
            break;
        }
    }

    if (!found) {
        return false;
    }

    return saveDocuments(docs);
}

void updateDocumentStatusForAdmin(const Admin& admin) {
    clearScreen();
    ui::printSectionTitle("UPDATE DOCUMENT STATUS");

    clearInputBuffer();
    std::string targetDocId;
    std::cout << "Enter Document ID: ";
    std::getline(std::cin, targetDocId);

    if (targetDocId.empty()) {
        std::cout << ui::error("[!] Document ID is required.") << "\n";
        logAuditAction("UPDATE_STATUS_INPUT_ERROR", "N/A", admin.username);
        waitForEnter();
        return;
    }

    std::cout << "\nSelect new status:\n";
    std::cout << "  [1] " << ui::consensusStatus("pending") << "\n";
    std::cout << "  [2] " << ui::consensusStatus("approved") << "\n";
    std::cout << "  [3] " << ui::consensusStatus("denied") << "\n";
    std::cout << "  Enter choice: ";

    int statusChoice = 0;
    std::cin >> statusChoice;
    if (std::cin.fail()) {
        std::cin.clear();
        clearInputBuffer();
        std::cout << ui::error("[!] Invalid status input.") << "\n";
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
        std::cout << ui::error("[!] Invalid status option.") << "\n";
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

    const int chainIndex = appendBlockchainAction("STATUS_UPDATE_" + newStatus, targetDocId, admin.username);
    logAuditAction("UPDATE_STATUS", targetDocId, admin.username, chainIndex);
    std::cout << ui::success("[+] Document status updated successfully.") << "\n";
    waitForEnter();
}
