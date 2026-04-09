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
// Document management stores records in a flat file and supports listing,
// searching, filtering, status transitions, and approval-request creation.
const std::string DOCUMENTS_FILE_PATH_PRIMARY = "data/documents.txt";
const std::string DOCUMENTS_FILE_PATH_FALLBACK = "../data/documents.txt";

struct DocumentFilter {
    // Optional fields used to build an in-memory filter pipeline.
    std::string status;
    std::string exactDate;
    std::string fromDate;
    std::string toDate;
    std::string category;
    std::string department;
    std::string uploader;
};

bool openInputFileWithFallback(std::ifstream& file, const std::string& primaryPath, const std::string& fallbackPath) {
    // Constant-time path fallback for read operations.
    file.open(primaryPath);
    if (file.is_open()) {
        return true;
    }

    file.clear();
    file.open(fallbackPath);
    return file.is_open();
}

bool openAppendFileWithFallback(std::ofstream& file, const std::string& primaryPath, const std::string& fallbackPath) {
    // Append mode is used for document creation without rewriting full file.
    file.open(primaryPath, std::ios::app);
    if (file.is_open()) {
        return true;
    }

    file.clear();
    file.open(fallbackPath, std::ios::app);
    return file.is_open();
}

std::string resolveDataPath(const std::string& primaryPath, const std::string& fallbackPath) {
    // Selects the currently valid location for rewrite operations.
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
    // One-pass tokenizer: O(m), m = line length.
    std::vector<std::string> tokens;
    std::stringstream parser(line);
    std::string token;
    while (std::getline(parser, token, '|')) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string toLowerCopy(std::string value) {
    // ASCII lowercase utility for case-insensitive compare operations.
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] >= 'A' && value[i] <= 'Z') {
            value[i] = static_cast<char>(value[i] + ('a' - 'A'));
        }
    }
    return value;
}

bool containsCaseInsensitive(const std::string& text, const std::string& token) {
    // O(n) substring check after normalization.
    if (token.empty()) {
        return true;
    }
    return toLowerCopy(text).find(toLowerCopy(token)) != std::string::npos;
}

bool isDateTextValid(const std::string& value) {
    // Validates strict YYYY-MM-DD text shape used by date filters.
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
    // Backward-compatible parser that defaults missing optional columns.
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
    // Full dataset load used by search/filter/update flows: O(n).
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
    // Canonical rewrite ensures hash/category/amount columns stay aligned.
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
    // Normalizes equivalent storage states for human-friendly chart labels.
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

std::vector<Document> buildRecentDocuments(const std::vector<Document>& docs, std::size_t limit) {
    std::vector<Document> recent = docs;
    std::sort(recent.begin(), recent.end(), [](const Document& a, const Document& b) {
        if (a.dateUploaded != b.dateUploaded) {
            return a.dateUploaded > b.dateUploaded;
        }
        return a.docId > b.docId;
    });

    if (recent.size() > limit) {
        recent.resize(limit);
    }

    return recent;
}

std::string joinTopKeys(const std::map<std::string, int>& values, std::size_t maxItems) {
    std::string output;
    std::size_t count = 0;

    for (std::map<std::string, int>::const_iterator it = values.begin(); it != values.end(); ++it) {
        if (it->first.empty()) {
            continue;
        }

        if (!output.empty()) {
            output += ", ";
        }

        output += it->first;
        count++;

        if (count >= maxItems) {
            break;
        }
    }

    return output.empty() ? "(none)" : output;
}

void printRecentDocumentHints(const std::vector<Document>& docs, std::size_t limit) {
    const std::vector<Document> recent = buildRecentDocuments(docs, limit);
    if (recent.empty()) {
        return;
    }

    std::cout << "\n" << ui::bold("Recent/Available Documents") << "\n";
    const std::vector<std::string> headers = {"ID", "Date", "Status", "Title"};
    const std::vector<int> widths = {8, 10, 16, 26};
    ui::printTableHeader(headers, widths);

    for (std::size_t i = 0; i < recent.size(); ++i) {
        ui::printTableRow({recent[i].docId, recent[i].dateUploaded, recent[i].status, recent[i].title}, widths);
    }

    ui::printTableFooter(widths);
}

void printFilterSuggestions(const std::vector<Document>& docs) {
    std::map<std::string, int> statuses;
    std::map<std::string, int> categories;
    std::map<std::string, int> departments;
    std::map<std::string, int> uploaders;
    std::map<std::string, int> dates;

    for (std::size_t i = 0; i < docs.size(); ++i) {
        statuses[docs[i].status] += 1;
        categories[docs[i].category] += 1;
        departments[docs[i].department] += 1;
        uploaders[docs[i].uploader] += 1;
        dates[docs[i].dateUploaded] += 1;
    }

    std::cout << "\n" << ui::bold("Filter Suggestions") << "\n";
    std::cout << "  Status values    : " << joinTopKeys(statuses, 8) << "\n";
    std::cout << "  Category values  : " << joinTopKeys(categories, 8) << "\n";
    std::cout << "  Department values: " << joinTopKeys(departments, 8) << "\n";
    std::cout << "  Uploader values  : " << joinTopKeys(uploaders, 8) << "\n";

    std::vector<Document> recent = buildRecentDocuments(docs, 6);
    std::map<std::string, int> recentDates;
    for (std::size_t i = 0; i < recent.size(); ++i) {
        recentDates[recent[i].dateUploaded] += 1;
    }
    std::cout << "  Recent dates     : " << joinTopKeys(recentDates, 6) << "\n";
    std::cout << "  " << ui::muted("Tip: leave any field blank to ignore it.") << "\n";
}

bool findExactDocumentById(const std::vector<Document>& docs, const std::string& targetDocId, Document& foundDoc) {
    const std::string normalizedTarget = toLowerCopy(targetDocId);

    for (std::size_t i = 0; i < docs.size(); ++i) {
        if (toLowerCopy(docs[i].docId) == normalizedTarget) {
            foundDoc = docs[i];
            return true;
        }
    }

    return false;
}

std::vector<Document> findDocumentSuggestions(const std::vector<Document>& docs, const std::string& token) {
    std::vector<Document> suggestions;
    const std::string normalizedToken = toLowerCopy(token);

    for (std::size_t i = 0; i < docs.size(); ++i) {
        const std::string id = toLowerCopy(docs[i].docId);
        const std::string title = toLowerCopy(docs[i].title);
        const std::string category = toLowerCopy(docs[i].category);

        const bool idStartsWith = id.find(normalizedToken) == 0;
        const bool idContains = id.find(normalizedToken) != std::string::npos;
        const bool titleContains = title.find(normalizedToken) != std::string::npos;
        const bool categoryContains = category.find(normalizedToken) != std::string::npos;

        if (idStartsWith || idContains || titleContains || categoryContains) {
            suggestions.push_back(docs[i]);
        }
    }

    std::sort(suggestions.begin(), suggestions.end(), [](const Document& a, const Document& b) {
        if (a.dateUploaded != b.dateUploaded) {
            return a.dateUploaded > b.dateUploaded;
        }
        return a.docId < b.docId;
    });

    return suggestions;
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
    // Tabular renderer for both full-list screens and single-record lookups.
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
    // Aggregates status frequencies in O(n), then renders scaled bars.
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
    // Applies all active criteria as an AND filter.
    // Per-document cost is O(1) plus substring checks for text fields.
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
    // Scans existing IDs and increments max numeric suffix: O(n).
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
    // Seeds representative rows only when the document file is empty.
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
    // Citizen-facing screen that filters to published records only.
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
    // Upload flow appends a new pending document, then creates approval rows,
    // then appends blockchain/audit events for traceability.
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
    // Admin-wide visibility over all document rows in current storage.
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
    // Linear search over loaded rows: O(n) worst-case.
    clearScreen();
    ui::printSectionTitle("SEARCH DOCUMENT BY ID");

    std::vector<Document> docs;
    if (!loadDocuments(docs)) {
        std::cout << ui::error("[!] Unable to open documents file.") << "\n";
        logAuditAction("SEARCH_DOC_FAILED", "N/A", admin.username);
        waitForEnter();
        return;
    }

    if (docs.empty()) {
        std::cout << "\n" << ui::warning("[!] No documents are available for search.") << "\n";
        logAuditAction("SEARCH_DOC_EMPTY", "N/A", admin.username);
        waitForEnter();
        return;
    }

    printRecentDocumentHints(docs, 8);
    std::cout << "\n" << ui::bold("Search Suggestions") << "\n";
    std::cout << "  - Exact ID (example: D006)\n";
    std::cout << "  - ID prefix (example: D00)\n";
    std::cout << "  - Title keyword (example: Contract)\n";

    clearInputBuffer();
    std::string query;
    std::cout << "Enter search value: ";
    std::getline(std::cin, query);

    if (query.empty()) {
        std::cout << ui::error("[!] Search value is required.") << "\n";
        logAuditAction("SEARCH_DOC_INPUT_ERROR", "N/A", admin.username);
        waitForEnter();
        return;
    }

    Document exact;
    if (findExactDocumentById(docs, query, exact)) {
        printDocumentsTable(std::vector<Document>(1, exact), true);
        logAuditAction("SEARCH_DOC", exact.docId, admin.username);
        waitForEnter();
        return;
    }

    std::vector<Document> suggestions = findDocumentSuggestions(docs, query);
    if (suggestions.empty()) {
        std::cout << "\n" << ui::warning("[!] No matching documents found.") << "\n";
        logAuditAction("SEARCH_DOC_NOT_FOUND", query, admin.username);
        waitForEnter();
        return;
    }

    std::cout << "\n" << ui::info("[i] No exact ID match. Showing related results:") << "\n";
    printDocumentsTable(suggestions, false);

    std::string selectedDocId;
    std::cout << "\nEnter exact Document ID from results (blank to cancel): ";
    std::getline(std::cin, selectedDocId);

    if (selectedDocId.empty()) {
        logAuditAction("SEARCH_DOC_SUGGESTIONS_VIEW", query, admin.username);
        waitForEnter();
        return;
    }

    Document selected;
    if (findExactDocumentById(suggestions, selectedDocId, selected)) {
        printDocumentsTable(std::vector<Document>(1, selected), true);
        logAuditAction("SEARCH_DOC", selected.docId, admin.username);
        waitForEnter();
        return;
    }

    std::cout << "\n" << ui::warning("[!] Selected Document ID is not in the suggestions list.") << "\n";
    logAuditAction("SEARCH_DOC_NOT_FOUND", selectedDocId, admin.username);
    waitForEnter();
}

void filterDocumentsForAdmin(const Admin& admin) {
    // Interactive multi-criteria filtering in one in-memory pass: O(n).
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

    printRecentDocumentHints(docs, 6);
    printFilterSuggestions(docs);

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
    // Used by approval workflows to synchronize consensus outcome to documents.
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
    // Manual status override reserved for admin workflows.
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
