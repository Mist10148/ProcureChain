#include "../include/documents.h"

#include "../include/approvals.h"
#include "../include/audit.h"
#include "../include/auth.h"
#include "../include/blockchain.h"
#include "../include/notifications.h"
#include "../include/summarizer.h"
#include "../include/storage_utils.h"
#include "../include/ui.h"
#include "../include/verification.h"

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
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
const std::string STATUS_HISTORY_FILE_PATH_PRIMARY = "data/document_status_history.txt";
const std::string STATUS_HISTORY_FILE_PATH_FALLBACK = "../data/document_status_history.txt";
const std::string APPROVALS_FILE_PATH_PRIMARY = "data/approvals.txt";
const std::string APPROVALS_FILE_PATH_FALLBACK = "../data/approvals.txt";
const std::string BUDGETS_FILE_PATH_PRIMARY = "data/budgets.txt";
const std::string BUDGETS_FILE_PATH_FALLBACK = "../data/budgets.txt";
const std::string DOCUMENT_UPLOAD_DIR_PRIMARY = "data/documents";
const std::string DOCUMENT_UPLOAD_DIR_FALLBACK = "../data/documents";
const std::string DOCUMENT_SOURCE_UPLOAD_DIR_PRIMARY = "data/uploads";
const std::string DOCUMENT_SOURCE_UPLOAD_DIR_FALLBACK = "../data/uploads";

struct ApprovalChainRow {
    std::string approverUsername;
    std::string role;
    std::string status;
    std::string decidedAt;
    std::string note;
};

struct SearchMatch {
    Document doc;
    int score;
};

struct StatusHistoryRow {
    std::string docId;
    std::string timestamp;
    std::string actorUsername;
    std::string fromStatus;
    std::string toStatus;
    std::string note;
};

struct BudgetGuardrailCheck {
    bool hasBudget;
    bool warn;
    bool block;
    std::string category;
    double allocation;
    double projectedActual;
    double projectedUtilization;
};

enum DuplicateHashAction {
    DUPLICATE_HASH_CANCEL = 0,
    DUPLICATE_HASH_CONTINUE = 1,
    DUPLICATE_HASH_LINK = 2
};

struct DocumentFilter {
    // Optional fields used to build an in-memory filter pipeline.
    std::string status;
    std::string exactDate;
    std::string fromDate;
    std::string toDate;
    std::string category;
    std::string tags;
    std::string department;
    std::string uploader;
};

struct DocumentComparisonRow {
    std::string fieldName;
    std::string leftValue;
    std::string rightValue;
    bool changed;
};

void clearInputBuffer();
void printDocumentDetailPanel(const Document& doc, bool includeApprovalChain, const std::vector<Document>* allDocs);

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

std::vector<std::string> splitPipe(const std::string& line) {
    return storage::splitPipeRow(line);
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

std::string trimTagToken(const std::string& value) {
    const std::string whitespace = " \t\r\n";
    const size_t start = value.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return "";
    }

    const size_t end = value.find_last_not_of(whitespace);
    return value.substr(start, end - start + 1);
}

std::string sanitizeTagToken(const std::string& value) {
    const std::string input = toLowerCopy(trimTagToken(value));
    std::string cleaned;
    bool previousSpace = false;

    for (size_t i = 0; i < input.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(input[i]);
        const bool allowed = std::isalnum(ch) != 0 || input[i] == ' ' || input[i] == '-' || input[i] == '_' || input[i] == '/';
        if (!allowed) {
            continue;
        }

        if (input[i] == ' ') {
            if (previousSpace) {
                continue;
            }
            previousSpace = true;
        } else {
            previousSpace = false;
        }

        cleaned.push_back(input[i]);
    }

    return trimTagToken(cleaned);
}

std::vector<std::string> splitNormalizedTags(const std::string& csvTags) {
    std::vector<std::string> tags;
    std::stringstream parser(csvTags);
    std::string token;

    while (std::getline(parser, token, ',')) {
        const std::string normalized = sanitizeTagToken(token);
        if (normalized.empty()) {
            continue;
        }

        bool alreadyExists = false;
        for (size_t i = 0; i < tags.size(); ++i) {
            if (tags[i] == normalized) {
                alreadyExists = true;
                break;
            }
        }

        if (!alreadyExists) {
            tags.push_back(normalized);
        }
    }

    return tags;
}

std::string normalizeTagsForStorage(const std::string& csvTags) {
    const std::vector<std::string> tags = splitNormalizedTags(csvTags);
    if (tags.empty()) {
        return "";
    }

    std::ostringstream out;
    for (size_t i = 0; i < tags.size(); ++i) {
        if (i > 0) {
            out << ", ";
        }
        out << tags[i];
    }

    return out.str();
}

void appendTagCounts(const std::string& csvTags, std::map<std::string, int>& counts) {
    const std::vector<std::string> tags = splitNormalizedTags(csvTags);
    for (size_t i = 0; i < tags.size(); ++i) {
        counts[tags[i]] += 1;
    }
}

bool isDateTextValid(const std::string& value) {
    return storage::isDateTextValidStrict(value, true);
}

Document parseDocumentTokens(const std::vector<std::string>& tokens) {
    // Backward-compatible parser that defaults missing optional columns.
    Document doc;
    doc.docId = tokens.size() > 0 ? tokens[0] : "";
    doc.title = tokens.size() > 1 ? tokens[1] : "";
    doc.category = tokens.size() > 2 ? tokens[2] : "";
    doc.tags = "";
    doc.description = "";
    doc.department = "";
    doc.dateUploaded = "";
    doc.uploader = "";
    doc.status = "pending_approval";
    doc.hashValue = "";
    doc.fileName = "";
    doc.fileType = "";
    doc.filePath = "";
    doc.fileSizeBytes = 0;
    doc.budgetCategory = doc.category;
    doc.amount = 0.0;
    doc.versionNumber = 1;
    doc.previousDocId = "";

    if (tokens.size() >= 15) {
        doc.description = tokens[3];
        doc.department = tokens[4];
        doc.dateUploaded = tokens[5];
        doc.uploader = tokens[6];
        doc.status = tokens[7];
        doc.hashValue = tokens[8];
        doc.fileName = tokens[9];
        doc.fileType = tokens[10];
        doc.filePath = tokens[11];
        if (!storage::tryParseLongLongStrict(tokens[12], doc.fileSizeBytes)) {
            doc.fileSizeBytes = 0;
        }

        doc.budgetCategory = tokens[13];
        if (!storage::tryParseDoubleStrict(tokens[14], doc.amount)) {
            doc.amount = 0.0;
        }

        if (tokens.size() > 15) {
            if (!storage::tryParseIntStrict(tokens[15], doc.versionNumber) || doc.versionNumber <= 0) {
                doc.versionNumber = 1;
            }
        }

        doc.previousDocId = tokens.size() > 16 ? tokens[16] : "";
        doc.tags = tokens.size() > 17 ? normalizeTagsForStorage(tokens[17]) : "";
    } else {
        // Legacy schema fallback.
        doc.department = tokens.size() > 3 ? tokens[3] : "";
        doc.dateUploaded = tokens.size() > 4 ? tokens[4] : "";
        doc.uploader = tokens.size() > 5 ? tokens[5] : "";
        doc.status = tokens.size() > 6 ? tokens[6] : "pending_approval";
        doc.hashValue = tokens.size() > 7 ? tokens[7] : "";
        doc.budgetCategory = tokens.size() > 8 ? tokens[8] : doc.category;
        doc.fileName = doc.docId + ".legacy";
        doc.fileType = "legacy";
        doc.filePath = "";

        if (tokens.size() > 9 && !storage::tryParseDoubleStrict(tokens[9], doc.amount)) {
            doc.amount = 0.0;
        }

        doc.versionNumber = 1;
        doc.previousDocId = "";
    }

    if (doc.budgetCategory.empty()) {
        doc.budgetCategory = doc.category;
    }

    if (doc.department.empty()) {
        doc.department = "General";
    }

    return doc;
}

std::string computeDocumentRecordHash(const Document& doc) {
    if (!doc.filePath.empty() && std::filesystem::exists(doc.filePath)) {
        const std::string fileHash = computeFileHashSha256(doc.filePath);
        if (!fileHash.empty()) {
            return fileHash;
        }
    }

    const std::string metadataSource = doc.docId + "|" + doc.title + "|" + doc.category + "|" + doc.description + "|" + doc.department + "|" +
                                       doc.dateUploaded + "|" + doc.uploader + "|" + doc.tags;
    return computeSimpleHash(metadataSource);
}

std::string serializeDocument(const Document& doc) {
    const std::string metadataSource = doc.docId + "|" + doc.title + "|" + doc.category + "|" + doc.description + "|" + doc.department + "|" +
                                       doc.dateUploaded + "|" + doc.uploader + "|" + doc.tags;
    const std::string hashValue = !doc.hashValue.empty() ? doc.hashValue : computeSimpleHash(metadataSource);

    std::ostringstream fileSizeOut;
    fileSizeOut << doc.fileSizeBytes;
    std::ostringstream amountOut;
    amountOut << std::fixed << std::setprecision(2) << doc.amount;
    std::ostringstream versionOut;
    versionOut << doc.versionNumber;

    return storage::joinPipeRow({
        doc.docId,
        doc.title,
        doc.category,
        doc.description,
        doc.department,
        doc.dateUploaded,
        doc.uploader,
        doc.status,
        hashValue,
        doc.fileName,
        doc.fileType,
        doc.filePath,
        fileSizeOut.str(),
        doc.budgetCategory,
        amountOut.str(),
        versionOut.str(),
        doc.previousDocId,
        doc.tags
    });
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
    std::vector<std::string> rows;
    rows.reserve(docs.size());
    for (size_t i = 0; i < docs.size(); ++i) {
        rows.push_back(serializeDocument(docs[i]));
    }
    return storage::writePipeFileWithFallback(DOCUMENTS_FILE_PATH_PRIMARY,
                                              DOCUMENTS_FILE_PATH_FALLBACK,
                                              "docID|title|category|description|department|dateUploaded|uploader|status|hashValue|fileName|fileType|filePath|fileSizeBytes|budgetCategory|amount|versionNumber|previousDocId|tags",
                                              rows);
}

void ensureStatusHistoryFileExists() {
    std::ifstream check;
    if (openInputFileWithFallback(check, STATUS_HISTORY_FILE_PATH_PRIMARY, STATUS_HISTORY_FILE_PATH_FALLBACK)) {
        return;
    }

    storage::writeTextFileWithFallback(STATUS_HISTORY_FILE_PATH_PRIMARY,
                                       STATUS_HISTORY_FILE_PATH_FALLBACK,
                                       "docID|timestamp|actorUsername|fromStatus|toStatus|note\n");
}

StatusHistoryRow parseStatusHistoryTokens(const std::vector<std::string>& tokens) {
    StatusHistoryRow row;
    row.docId = tokens.size() > 0 ? tokens[0] : "";
    row.timestamp = tokens.size() > 1 ? tokens[1] : "";
    row.actorUsername = tokens.size() > 2 ? tokens[2] : "";
    row.fromStatus = tokens.size() > 3 ? tokens[3] : "";
    row.toStatus = tokens.size() > 4 ? tokens[4] : "";
    row.note = tokens.size() > 5 ? tokens[5] : "";
    return row;
}

std::string sanitizeHistoryNote(const std::string& noteText) {
    return storage::sanitizeSingleLineInput(noteText);
}

bool appendDocumentStatusHistory(const std::string& docId,
                                 const std::string& actorUsername,
                                 const std::string& fromStatus,
                                 const std::string& toStatus,
                                 const std::string& noteText) {
    if (docId.empty() || toStatus.empty()) {
        return false;
    }

    ensureStatusHistoryFileExists();
    return storage::appendLineToFileWithFallback(STATUS_HISTORY_FILE_PATH_PRIMARY,
                                                 STATUS_HISTORY_FILE_PATH_FALLBACK,
                                                 storage::joinPipeRow({
                                                     docId,
                                                     getCurrentTimestamp(),
                                                     actorUsername.empty() ? "SYSTEM" : actorUsername,
                                                     fromStatus,
                                                     toStatus,
                                                     sanitizeHistoryNote(noteText)
                                                 }));
}

bool removeDocumentStatusHistoryForDocument(const std::string& docId) {
    if (docId.empty()) {
        return false;
    }

    std::ifstream file;
    if (!openInputFileWithFallback(file, STATUS_HISTORY_FILE_PATH_PRIMARY, STATUS_HISTORY_FILE_PATH_FALLBACK)) {
        std::error_code ec;
        const bool missingPrimary = !std::filesystem::exists(STATUS_HISTORY_FILE_PATH_PRIMARY, ec);
        ec.clear();
        const bool missingFallback = !std::filesystem::exists(STATUS_HISTORY_FILE_PATH_FALLBACK, ec);
        return missingPrimary && missingFallback;
    }

    std::vector<std::string> serializedRows;
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

        const StatusHistoryRow row = parseStatusHistoryTokens(splitPipe(line));
        if (row.docId.empty() || row.docId == docId) {
            continue;
        }

        serializedRows.push_back(storage::joinPipeRow({
            row.docId,
            row.timestamp,
            row.actorUsername,
            row.fromStatus,
            row.toStatus,
            row.note
        }));
    }

    return storage::writePipeFileWithFallback(STATUS_HISTORY_FILE_PATH_PRIMARY,
                                              STATUS_HISTORY_FILE_PATH_FALLBACK,
                                              "docID|timestamp|actorUsername|fromStatus|toStatus|note",
                                              serializedRows);
}

bool removeDocumentRecordById(const std::string& docId) {
    if (docId.empty()) {
        return false;
    }

    std::vector<Document> docs;
    if (!loadDocuments(docs)) {
        return false;
    }

    std::vector<Document> filteredDocs;
    filteredDocs.reserve(docs.size());
    for (std::size_t i = 0; i < docs.size(); ++i) {
        if (docs[i].docId != docId) {
            filteredDocs.push_back(docs[i]);
        }
    }

    return saveDocuments(filteredDocs);
}

bool deleteStoredDocumentFile(const std::string& filePath) {
    if (filePath.empty()) {
        return true;
    }

    std::error_code ec;
    if (!std::filesystem::exists(filePath, ec)) {
        return !ec;
    }

    return std::filesystem::remove(filePath, ec) && !ec;
}

storage::OperationResult rollbackUploadedDocumentPersistenceDetailed(const Document& doc) {
    const bool removedHistory = removeDocumentStatusHistoryForDocument(doc.docId);
    const bool removedApprovals = removeApprovalRequestsForDocument(doc.docId);
    const bool removedDocument = removeDocumentRecordById(doc.docId);
    const bool removedFile = deleteStoredDocumentFile(doc.filePath);
    const bool rollbackOk = removedHistory && removedApprovals && removedDocument && removedFile;
    return storage::OperationResult(rollbackOk,
                                    rollbackOk ?
                                        "Upload rollback completed successfully." :
                                        "Upload rollback was incomplete; manual data repair is required.",
                                    true,
                                    rollbackOk);
}

std::vector<StatusHistoryRow> loadDocumentStatusHistoryRows(const std::string& docId) {
    std::vector<StatusHistoryRow> rows;
    std::ifstream file;

    if (!openInputFileWithFallback(file, STATUS_HISTORY_FILE_PATH_PRIMARY, STATUS_HISTORY_FILE_PATH_FALLBACK)) {
        return rows;
    }

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

        StatusHistoryRow row = parseStatusHistoryTokens(splitPipe(line));
        if (!docId.empty() && row.docId != docId) {
            continue;
        }

        if (!row.docId.empty()) {
            rows.push_back(row);
        }
    }

    std::sort(rows.begin(), rows.end(), [](const StatusHistoryRow& left, const StatusHistoryRow& right) {
        if (left.timestamp != right.timestamp) {
            return left.timestamp < right.timestamp;
        }

        if (left.fromStatus != right.fromStatus) {
            return left.fromStatus < right.fromStatus;
        }

        return left.toStatus < right.toStatus;
    });

    return rows;
}

void printDocumentStatusHistoryPanel(const std::string& docId) {
    const std::vector<StatusHistoryRow> rows = loadDocumentStatusHistoryRows(docId);
    if (rows.empty()) {
        return;
    }

    std::cout << "\n" << ui::bold("Status Timeline") << "\n";
    const std::vector<std::string> headers = {"When", "Actor", "From", "To", "Note"};
    const std::vector<int> widths = {19, 18, 16, 16, 26};
    ui::printTableHeader(headers, widths);

    for (std::size_t i = 0; i < rows.size(); ++i) {
        const std::string fromLabel = rows[i].fromStatus.empty() ? "(none)" : rows[i].fromStatus;
        const std::string noteLabel = rows[i].note.empty() ? "(none)" : rows[i].note;
        ui::printTableRow({rows[i].timestamp, rows[i].actorUsername, fromLabel, rows[i].toStatus, noteLabel}, widths);
    }

    ui::printTableFooter(widths);
}

std::vector<Document> findDocumentsByHash(const std::vector<Document>& docs, const std::string& hashValue) {
    std::vector<Document> matches;
    if (hashValue.empty()) {
        return matches;
    }

    for (std::size_t i = 0; i < docs.size(); ++i) {
        if (docs[i].hashValue == hashValue) {
            matches.push_back(docs[i]);
        }
    }

    return matches;
}

bool findDocumentByIdCaseInsensitive(const std::vector<Document>& docs, const std::string& docId, Document& found) {
    const std::string target = toLowerCopy(trimTagToken(docId));
    for (std::size_t i = 0; i < docs.size(); ++i) {
        if (toLowerCopy(docs[i].docId) == target) {
            found = docs[i];
            return true;
        }
    }
    return false;
}

void printDuplicateHashMatches(const std::vector<Document>& matches) {
    std::cout << "\n" << ui::warning("[!] Duplicate hash detected. Matching records:") << "\n";
    const std::vector<std::string> headers = {"Doc ID", "Ver", "Status", "Uploader", "Date", "Title"};
    const std::vector<int> widths = {8, 5, 16, 14, 10, 30};
    ui::printTableHeader(headers, widths);
    for (std::size_t i = 0; i < matches.size(); ++i) {
        ui::printTableRow({matches[i].docId,
                           std::to_string(matches[i].versionNumber),
                           matches[i].status,
                           matches[i].uploader,
                           matches[i].dateUploaded,
                           matches[i].title},
                          widths);
    }
    ui::printTableFooter(widths);
}

DuplicateHashAction promptDuplicateHashAction(const std::vector<Document>& matches, std::string& linkedDocId) {
    linkedDocId.clear();
    printDuplicateHashMatches(matches);

    std::cout << "  " << ui::info("[1]") << " Cancel upload\n";
    std::cout << "  " << ui::info("[2]") << " Continue with new document\n";
    std::cout << "  " << ui::info("[3]") << " Link as amendment\n";
    std::cout << ui::muted("--------------------------------------------------------------") << "\n";
    std::cout << "  Enter your choice: ";

    int choice = 0;
    std::cin >> choice;
    if (std::cin.fail()) {
        std::cin.clear();
        clearInputBuffer();
        return DUPLICATE_HASH_CANCEL;
    }

    clearInputBuffer();
    if (choice == 2) {
        return DUPLICATE_HASH_CONTINUE;
    }

    if (choice == 3) {
        std::cout << "Enter base Document ID from matches: ";
        std::getline(std::cin, linkedDocId);
        linkedDocId = trimTagToken(linkedDocId);
        return DUPLICATE_HASH_LINK;
    }

    return DUPLICATE_HASH_CANCEL;
}

std::map<std::string, double> loadPublishedBudgetAllocations() {
    std::map<std::string, double> budgets;
    std::ifstream file;

    if (!openInputFileWithFallback(file, BUDGETS_FILE_PATH_PRIMARY, BUDGETS_FILE_PATH_FALLBACK)) {
        return budgets;
    }

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

        const std::vector<std::string> tokens = splitPipe(line);
        if (tokens.size() < 2 || tokens[0].empty()) {
            continue;
        }

        double amount = 0.0;
        std::stringstream amountIn(tokens[1]);
        amountIn >> amount;
        budgets[tokens[0]] = amount;
    }

    return budgets;
}

double computeTrackedActualForCategory(const std::vector<Document>& docs,
                                       const std::string& category,
                                       const std::string& excludeDocId) {
    double total = 0.0;

    for (std::size_t i = 0; i < docs.size(); ++i) {
        if (!excludeDocId.empty() && docs[i].docId == excludeDocId) {
            continue;
        }

        const std::string normalizedStatus = toLowerCopy(docs[i].status);
        if (normalizedStatus != "approved" && normalizedStatus != "published") {
            continue;
        }

        const std::string docBudgetCategory = docs[i].budgetCategory.empty() ? docs[i].category : docs[i].budgetCategory;
        if (docBudgetCategory != category || docs[i].amount <= 0.0) {
            continue;
        }

        total += docs[i].amount;
    }

    return total;
}

BudgetGuardrailCheck buildDocumentPublishGuardrailCheck(const std::vector<Document>& docs,
                                                        const Document& targetDoc) {
    BudgetGuardrailCheck check;
    check.hasBudget = false;
    check.warn = false;
    check.block = false;
    check.category = targetDoc.budgetCategory.empty() ? targetDoc.category : targetDoc.budgetCategory;
    check.allocation = 0.0;
    check.projectedActual = 0.0;
    check.projectedUtilization = 0.0;

    if (check.category.empty() || targetDoc.amount <= 0.0) {
        return check;
    }

    const std::map<std::string, double> budgets = loadPublishedBudgetAllocations();
    std::map<std::string, double>::const_iterator found = budgets.find(check.category);
    if (found == budgets.end()) {
        return check;
    }

    check.hasBudget = true;
    check.allocation = found->second;

    const double actualWithoutTarget = computeTrackedActualForCategory(docs, check.category, targetDoc.docId);
    check.projectedActual = actualWithoutTarget + targetDoc.amount;

    if (check.allocation <= 0.0) {
        check.block = check.projectedActual > 0.0;
        return check;
    }

    check.projectedUtilization = (check.projectedActual / check.allocation) * 100.0;
    if (check.projectedUtilization > 100.0) {
        check.block = true;
    } else if (check.projectedUtilization >= 90.0) {
        check.warn = true;
    }

    return check;
}

bool confirmDocumentPublishGuardrail(const Document& doc,
                                     const std::vector<Document>& docs,
                                     const std::string& actorUsername) {
    const BudgetGuardrailCheck check = buildDocumentPublishGuardrailCheck(docs, doc);
    if (!check.hasBudget) {
        return true;
    }

    std::ostringstream allocationOut;
    allocationOut << std::fixed << std::setprecision(2) << check.allocation;
    std::ostringstream actualOut;
    actualOut << std::fixed << std::setprecision(2) << check.projectedActual;
    std::ostringstream utilizationOut;
    utilizationOut << std::fixed << std::setprecision(1) << check.projectedUtilization;

    if (check.block) {
        std::cout << ui::error("[!] Budget overrun guardrail blocked this publish transition.") << "\n";
        std::cout << "  Category            : " << check.category << "\n";
        std::cout << "  Published allocation: PHP " << allocationOut.str() << "\n";
        std::cout << "  Projected actual    : PHP " << actualOut.str() << "\n";
        if (check.allocation > 0.0) {
            std::cout << "  Projected utilization: " << utilizationOut.str() << "%\n";
        }
        logAuditAction("DOC_BUDGET_OVERRUN_BLOCKED", doc.docId, actorUsername.empty() ? "SYSTEM" : actorUsername);
        return false;
    }

    if (!check.warn) {
        return true;
    }

    std::cout << ui::warning("[!] Budget overrun warning (>=90% utilization) for this publish transition.") << "\n";
    std::cout << "  Category            : " << check.category << "\n";
    std::cout << "  Published allocation: PHP " << allocationOut.str() << "\n";
    std::cout << "  Projected actual    : PHP " << actualOut.str() << "\n";
    std::cout << "  Projected utilization: " << utilizationOut.str() << "%\n";

    logAuditAction("DOC_BUDGET_OVERRUN_WARNING", doc.docId, actorUsername.empty() ? "SYSTEM" : actorUsername);

    if (!ui::confirmAction("Proceed with publish despite high utilization warning?",
                           "Proceed",
                           "Cancel Publish")) {
        logAuditAction("DOC_BUDGET_OVERRUN_WARNING_CANCELLED", doc.docId, actorUsername.empty() ? "SYSTEM" : actorUsername);
        return false;
    }

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
    std::map<std::string, int> tags;
    std::map<std::string, int> departments;
    std::map<std::string, int> uploaders;
    std::map<std::string, int> dates;

    for (std::size_t i = 0; i < docs.size(); ++i) {
        statuses[docs[i].status] += 1;
        categories[docs[i].category] += 1;
        appendTagCounts(docs[i].tags, tags);
        departments[docs[i].department] += 1;
        uploaders[docs[i].uploader] += 1;
        dates[docs[i].dateUploaded] += 1;
    }

    std::cout << "\n" << ui::bold("Filter Suggestions") << "\n";
    std::cout << "  Status values    : " << joinTopKeys(statuses, 8) << "\n";
    std::cout << "  Category values  : " << joinTopKeys(categories, 8) << "\n";
    std::cout << "  Common tags      : " << joinTopKeys(tags, 8) << "\n";
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

std::vector<std::string> tokenizeSearchQuery(const std::string& query) {
    std::vector<std::string> terms;
    const std::string normalized = toLowerCopy(query);
    std::string current;

    for (size_t i = 0; i < normalized.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(normalized[i]);
        if (std::isalnum(ch) != 0 || normalized[i] == '_' || normalized[i] == '-') {
            current.push_back(normalized[i]);
            continue;
        }

        if (!current.empty()) {
            terms.push_back(current);
            current.clear();
        }
    }

    if (!current.empty()) {
        terms.push_back(current);
    }

    std::vector<std::string> uniqueTerms;
    for (size_t i = 0; i < terms.size(); ++i) {
        bool exists = false;
        for (size_t j = 0; j < uniqueTerms.size(); ++j) {
            if (uniqueTerms[j] == terms[i]) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            uniqueTerms.push_back(terms[i]);
        }
    }

    return uniqueTerms;
}

int scoreTermMatches(const std::string& text, const std::vector<std::string>& terms, int perTermWeight) {
    int score = 0;
    for (size_t i = 0; i < terms.size(); ++i) {
        if (!terms[i].empty() && text.find(terms[i]) != std::string::npos) {
            score += perTermWeight;
        }
    }
    return score;
}

int computeDocumentSearchScore(const Document& doc, const std::string& query) {
    const std::string normalizedQuery = toLowerCopy(trimTagToken(query));
    if (normalizedQuery.empty()) {
        return 0;
    }

    const std::vector<std::string> terms = tokenizeSearchQuery(normalizedQuery);

    const std::string id = toLowerCopy(doc.docId);
    const std::string title = toLowerCopy(doc.title);
    const std::string category = toLowerCopy(doc.category);
    const std::string description = toLowerCopy(doc.description);
    const std::string tags = toLowerCopy(doc.tags);

    int score = 0;

    if (id == normalizedQuery) {
        score += 1200;
    } else if (id.find(normalizedQuery) == 0) {
        score += 700;
    } else if (id.find(normalizedQuery) != std::string::npos) {
        score += 350;
    }

    if (title.find(normalizedQuery) != std::string::npos) {
        score += 240;
    }
    if (description.find(normalizedQuery) != std::string::npos) {
        score += 170;
    }
    if (tags.find(normalizedQuery) != std::string::npos) {
        score += 160;
    }
    if (category.find(normalizedQuery) != std::string::npos) {
        score += 130;
    }

    score += scoreTermMatches(id, terms, 120);
    score += scoreTermMatches(title, terms, 90);
    score += scoreTermMatches(tags, terms, 80);
    score += scoreTermMatches(description, terms, 60);
    score += scoreTermMatches(category, terms, 50);

    return score;
}

std::vector<SearchMatch> findRankedDocumentSuggestions(const std::vector<Document>& docs,
                                                       const std::string& token,
                                                       std::size_t maxResults) {
    std::vector<SearchMatch> suggestions;
    const std::string normalizedToken = toLowerCopy(trimTagToken(token));

    if (normalizedToken.empty()) {
        return suggestions;
    }

    for (std::size_t i = 0; i < docs.size(); ++i) {
        const int score = computeDocumentSearchScore(docs[i], normalizedToken);
        if (score > 0) {
            SearchMatch match;
            match.doc = docs[i];
            match.score = score;
            suggestions.push_back(match);
        }
    }

    std::sort(suggestions.begin(), suggestions.end(), [](const SearchMatch& a, const SearchMatch& b) {
        if (a.score != b.score) {
            return a.score > b.score;
        }

        if (a.doc.dateUploaded != b.doc.dateUploaded) {
            return a.doc.dateUploaded > b.doc.dateUploaded;
        }

        return a.doc.docId < b.doc.docId;
    });

    if (suggestions.size() > maxResults) {
        suggestions.resize(maxResults);
    }

    return suggestions;
}

void printRankedSearchMatches(const std::vector<SearchMatch>& matches) {
    const std::vector<std::string> headers = {"Score", "ID", "Ver", "Title", "Category", "Status", "Tags"};
    const std::vector<int> widths = {7, 6, 5, 18, 12, 14, 16};
    ui::printTableHeader(headers, widths);

    for (size_t i = 0; i < matches.size(); ++i) {
        ui::printTableRow({std::to_string(matches[i].score),
                           matches[i].doc.docId,
                           std::to_string(matches[i].doc.versionNumber),
                           matches[i].doc.title,
                           matches[i].doc.category,
                           matches[i].doc.status,
                           matches[i].doc.tags.empty() ? "(none)" : matches[i].doc.tags},
                          widths);
    }

    ui::printTableFooter(widths);
}

bool findDocumentInMatchesById(const std::vector<SearchMatch>& matches,
                               const std::string& targetDocId,
                               Document& foundDoc) {
    const std::string normalizedTarget = toLowerCopy(targetDocId);

    for (size_t i = 0; i < matches.size(); ++i) {
        if (toLowerCopy(matches[i].doc.docId) == normalizedTarget) {
            foundDoc = matches[i].doc;
            return true;
        }
    }

    return false;
}

std::string findRootDocumentId(const std::vector<Document>& docs, const Document& startDoc) {
    std::string currentId = startDoc.docId;
    std::string previousId = startDoc.previousDocId;
    int guard = 0;

    while (!previousId.empty() && guard < 1000) {
        Document previous;
        if (!findExactDocumentById(docs, previousId, previous)) {
            break;
        }

        currentId = previous.docId;
        previousId = previous.previousDocId;
        guard++;
    }

    return currentId;
}

bool compareDocumentLineageOrder(const Document& left, const Document& right) {
    if (left.versionNumber != right.versionNumber) {
        return left.versionNumber < right.versionNumber;
    }

    if (left.dateUploaded != right.dateUploaded) {
        return left.dateUploaded < right.dateUploaded;
    }

    return left.docId < right.docId;
}

std::vector<Document> collectDocumentLineage(const Document& doc, const std::vector<Document>& docs) {
    const std::string rootId = findRootDocumentId(docs, doc);
    std::vector<Document> lineage;

    for (std::size_t i = 0; i < docs.size(); ++i) {
        if (findRootDocumentId(docs, docs[i]) == rootId) {
            lineage.push_back(docs[i]);
        }
    }

    std::sort(lineage.begin(), lineage.end(), compareDocumentLineageOrder);
    return lineage;
}

std::string buildDocumentComparisonTarget(const Document& left, const Document& right) {
    return left.docId + "<->" + right.docId;
}

std::string formatDocumentComparisonPrevious(const Document& doc) {
    return doc.previousDocId.empty() ? "(root)" : doc.previousDocId;
}

std::string formatDocumentComparisonFileSize(long long fileSizeBytes) {
    std::ostringstream out;
    out << fileSizeBytes;
    return out.str();
}

void appendDocumentComparisonRow(std::vector<DocumentComparisonRow>& rows,
                                 const std::string& fieldName,
                                 const std::string& leftValue,
                                 const std::string& rightValue) {
    DocumentComparisonRow row;
    row.fieldName = fieldName;
    row.leftValue = leftValue.empty() ? "(none)" : leftValue;
    row.rightValue = rightValue.empty() ? "(none)" : rightValue;
    row.changed = (row.leftValue != row.rightValue);
    rows.push_back(row);
}

std::vector<DocumentComparisonRow> buildDocumentComparisonRows(const Document& left, const Document& right) {
    std::vector<DocumentComparisonRow> rows;
    appendDocumentComparisonRow(rows, "Document ID", left.docId, right.docId);
    appendDocumentComparisonRow(rows, "Version", std::to_string(left.versionNumber), std::to_string(right.versionNumber));
    appendDocumentComparisonRow(rows, "Previous Version", formatDocumentComparisonPrevious(left), formatDocumentComparisonPrevious(right));
    appendDocumentComparisonRow(rows, "Title", left.title, right.title);
    appendDocumentComparisonRow(rows, "Category", left.category, right.category);
    appendDocumentComparisonRow(rows, "Tags", left.tags, right.tags);
    appendDocumentComparisonRow(rows, "Description", left.description, right.description);
    appendDocumentComparisonRow(rows, "Department", left.department, right.department);
    appendDocumentComparisonRow(rows, "Date Uploaded", left.dateUploaded, right.dateUploaded);
    appendDocumentComparisonRow(rows, "Uploader", left.uploader, right.uploader);
    appendDocumentComparisonRow(rows, "Status", left.status, right.status);
    appendDocumentComparisonRow(rows, "Stored Hash", left.hashValue, right.hashValue);
    appendDocumentComparisonRow(rows, "File Name", left.fileName, right.fileName);
    appendDocumentComparisonRow(rows, "File Type", left.fileType, right.fileType);
    appendDocumentComparisonRow(rows, "File Size (bytes)",
                                formatDocumentComparisonFileSize(left.fileSizeBytes),
                                formatDocumentComparisonFileSize(right.fileSizeBytes));
    return rows;
}

bool findDocumentInLineageById(const std::vector<Document>& lineage,
                               const std::string& targetDocId,
                               Document& foundDoc,
                               std::size_t* indexOut) {
    const std::string normalizedTarget = toLowerCopy(storage::sanitizeSingleLineInput(targetDocId));

    for (std::size_t i = 0; i < lineage.size(); ++i) {
        if (toLowerCopy(lineage[i].docId) == normalizedTarget) {
            foundDoc = lineage[i];
            if (indexOut != NULL) {
                *indexOut = i;
            }
            return true;
        }
    }

    return false;
}

std::string buildLineageOrderText(const std::vector<Document>& lineage) {
    std::ostringstream out;

    for (std::size_t i = 0; i < lineage.size(); ++i) {
        if (i > 0) {
            out << " -> ";
        }

        out << lineage[i].docId << " (v" << lineage[i].versionNumber << ")";
    }

    return out.str();
}

void printLineageSelectionTable(const std::vector<Document>& lineage) {
    const std::vector<std::string> headers = {"Doc ID", "Version", "Status", "Previous", "Date"};
    const std::vector<int> widths = {10, 9, 16, 10, 10};
    ui::printTableHeader(headers, widths);

    for (std::size_t i = 0; i < lineage.size(); ++i) {
        ui::printTableRow({lineage[i].docId,
                           std::to_string(lineage[i].versionNumber),
                           lineage[i].status,
                           formatDocumentComparisonPrevious(lineage[i]),
                           lineage[i].dateUploaded},
                          widths);
    }

    ui::printTableFooter(widths);
}

void renderDocumentComparisonView(const Document& left,
                                  const Document& right,
                                  const std::vector<Document>& lineage,
                                  bool advancedMode) {
    std::size_t leftIndex = 0;
    std::size_t rightIndex = 0;
    Document ignored;
    findDocumentInLineageById(lineage, left.docId, ignored, &leftIndex);
    findDocumentInLineageById(lineage, right.docId, ignored, &rightIndex);

    const bool adjacent = (leftIndex > rightIndex ? leftIndex - rightIndex : rightIndex - leftIndex) == 1;
    const std::vector<DocumentComparisonRow> rows = buildDocumentComparisonRows(left, right);

    clearScreen();
    ui::printSectionTitle("DOCUMENT VERSION COMPARISON");
    std::cout << "  Mode        : " << (advancedMode ? "Advanced compare" : "Quick compare") << "\n";
    std::cout << "  Left Version : " << left.docId << " (v" << left.versionNumber << ")\n";
    std::cout << "  Right Version: " << right.docId << " (v" << right.versionNumber << ")\n";
    std::cout << "  Relationship : " << (adjacent ? "Adjacent versions" : "Non-adjacent versions") << "\n";

    const std::vector<std::string> headers = {"Field", "Left", "Right", "Status"};
    const std::vector<int> widths = {20, 24, 24, 10};
    ui::printTableHeader(headers, widths);

    for (std::size_t i = 0; i < rows.size(); ++i) {
        ui::printTableRow({rows[i].fieldName,
                           rows[i].leftValue,
                           rows[i].rightValue,
                           rows[i].changed ? "CHANGED" : "SAME"},
                          widths);
    }

    ui::printTableFooter(widths);

    std::cout << "\n" << ui::bold("Provenance") << "\n";
    std::cout << "  Lineage Order: " << buildLineageOrderText(lineage) << "\n";
    std::cout << "  Compared IDs : " << left.docId << " vs " << right.docId << "\n";
}

void runDocumentComparisonPanel(const Document& doc,
                                const std::vector<Document>* allDocs,
                                const std::string& actor) {
    if (allDocs == NULL) {
        return;
    }

    const std::vector<Document> lineage = collectDocumentLineage(doc, *allDocs);
    if (lineage.size() <= 1) {
        return;
    }

    int choice = -1;
    do {
        std::cout << "\n" << ui::bold("Document Comparison Actions") << "\n";
        std::cout << "  " << ui::info("[1]") << " Quick compare with previous version\n";
        std::cout << "  " << ui::info("[2]") << " Compare any two versions in lineage\n";
        std::cout << "  " << ui::info("[0]") << " Continue\n";
        std::cout << ui::muted("--------------------------------------------------------------") << "\n";
        std::cout << "  Enter your choice: ";

        std::cin >> choice;
        if (std::cin.fail()) {
            std::cin.clear();
            clearInputBuffer();
            std::cout << ui::warning("[!] Invalid choice.") << "\n";
            continue;
        }

        if (choice == 1) {
            if (doc.previousDocId.empty()) {
                std::cout << ui::warning("[!] Quick compare is unavailable for the root version.") << "\n";
                waitForEnter();
                continue;
            }

            Document previousVersion;
            if (!findDocumentInLineageById(lineage, doc.previousDocId, previousVersion, NULL)) {
                std::cout << ui::warning("[!] Previous version is not available in this visible lineage.") << "\n";
                waitForEnter();
                continue;
            }

            renderDocumentComparisonView(previousVersion, doc, lineage, false);
            logAuditAction("DOC_COMPARE_QUICK_VIEW", buildDocumentComparisonTarget(previousVersion, doc), actor);
            waitForEnter();
            continue;
        }

        if (choice == 2) {
            clearInputBuffer();
            clearScreen();
            ui::printSectionTitle("COMPARE ANY TWO VERSIONS");
            printLineageSelectionTable(lineage);

            std::string leftDocId;
            std::string rightDocId;
            std::cout << "\nLeft Document ID : ";
            std::getline(std::cin, leftDocId);
            std::cout << "Right Document ID: ";
            std::getline(std::cin, rightDocId);

            leftDocId = storage::sanitizeSingleLineInput(leftDocId);
            rightDocId = storage::sanitizeSingleLineInput(rightDocId);

            if (leftDocId.empty() || rightDocId.empty()) {
                std::cout << ui::warning("[!] Both Document IDs are required.") << "\n";
                waitForEnter();
                continue;
            }

            if (toLowerCopy(leftDocId) == toLowerCopy(rightDocId)) {
                std::cout << ui::warning("[!] Choose two different versions to compare.") << "\n";
                waitForEnter();
                continue;
            }

            Document leftVersion;
            Document rightVersion;
            if (!findDocumentInLineageById(lineage, leftDocId, leftVersion, NULL) ||
                !findDocumentInLineageById(lineage, rightDocId, rightVersion, NULL)) {
                std::cout << ui::warning("[!] One or both Document IDs are not part of the visible lineage.") << "\n";
                waitForEnter();
                continue;
            }

            if (compareDocumentLineageOrder(rightVersion, leftVersion)) {
                const Document temp = leftVersion;
                leftVersion = rightVersion;
                rightVersion = temp;
            }

            renderDocumentComparisonView(leftVersion, rightVersion, lineage, true);
            logAuditAction("DOC_COMPARE_ADVANCED_VIEW", buildDocumentComparisonTarget(leftVersion, rightVersion), actor);
            waitForEnter();
            continue;
        }

        if (choice != 0) {
            std::cout << ui::warning("[!] Invalid choice.") << "\n";
        }
    } while (choice != 0);
}

void printDocumentLineagePanel(const Document& doc, const std::vector<Document>& docs) {
    const std::vector<Document> lineage = collectDocumentLineage(doc, docs);

    if (lineage.size() <= 1) {
        return;
    }

    std::cout << "\n" << ui::bold("Version Lineage") << "\n";
    printLineageSelectionTable(lineage);
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

std::string trimCopy(const std::string& value) {
    const std::string whitespace = " \t\r\n";
    const size_t start = value.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return "";
    }

    const size_t end = value.find_last_not_of(whitespace);
    return value.substr(start, end - start + 1);
}

std::vector<std::string> buildDocumentCategoryChoices() {
    std::vector<std::string> choices;
    choices.push_back("Purchase Request");
    choices.push_back("Purchase Order");
    choices.push_back("Contract");
    choices.push_back("Canvass");
    choices.push_back("Notice of Award");
    choices.push_back("Inspection/Acceptance");
    return choices;
}

bool promptCategoryChoice(const std::vector<std::string>& choices, std::string& outCategory) {
    std::cout << "\n" << ui::bold("Choose Document Category") << "\n";
    for (std::size_t i = 0; i < choices.size(); ++i) {
        std::cout << "  [" << (i + 1) << "] " << choices[i] << "\n";
    }
    std::cout << "  [" << (choices.size() + 1) << "] Other (type custom category)\n";
    std::cout << "  Enter choice: ";

    int selected = 0;
    std::cin >> selected;
    if (std::cin.fail()) {
        std::cin.clear();
        clearInputBuffer();
        return false;
    }

    if (selected >= 1 && static_cast<std::size_t>(selected) <= choices.size()) {
        outCategory = choices[static_cast<std::size_t>(selected - 1)];
        clearInputBuffer();
        return true;
    }

    if (selected == static_cast<int>(choices.size()) + 1) {
        clearInputBuffer();
        std::cout << "  Custom category: ";
        std::getline(std::cin, outCategory);
        outCategory = trimCopy(outCategory);
        return !outCategory.empty();
    }

    clearInputBuffer();
    return false;
}

bool isSafeExportFileName(const std::string& fileName) {
    if (fileName.empty() || fileName.size() > 80) {
        return false;
    }

    if (fileName.find("..") != std::string::npos) {
        return false;
    }

    for (size_t i = 0; i < fileName.size(); ++i) {
        const char c = fileName[i];
        const bool safe = std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '-' || c == '.';
        if (!safe) {
            return false;
        }
    }

    return true;
}

std::string sanitizeFileToken(std::string token) {
    for (size_t i = 0; i < token.size(); ++i) {
        const char c = token[i];
        const bool safe = std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '-' || c == '.';
        if (!safe) {
            token[i] = '_';
        }
    }

    return token;
}

std::string defaultDocumentsExportFileName() {
    std::string stamp = getCurrentTimestamp();
    for (size_t i = 0; i < stamp.size(); ++i) {
        if (stamp[i] == ' ' || stamp[i] == ':') {
            stamp[i] = '_';
        }
    }

    return "documents_export_" + sanitizeFileToken(stamp) + ".txt";
}

bool isAllowedImportExtension(const std::string& extensionWithDot) {
    const std::string ext = toLowerCopy(extensionWithDot);
    return ext == ".pdf" || ext == ".docx" || ext == ".csv" || ext == ".txt";
}

std::string resolveUploadDirectory() {
    std::error_code ec;
    std::filesystem::create_directories(DOCUMENT_UPLOAD_DIR_PRIMARY, ec);
    if (!ec) {
        return DOCUMENT_UPLOAD_DIR_PRIMARY;
    }

    ec.clear();
    std::filesystem::create_directories(DOCUMENT_UPLOAD_DIR_FALLBACK, ec);
    if (!ec) {
        return DOCUMENT_UPLOAD_DIR_FALLBACK;
    }

    return "";
}

std::string resolveUploadSourceDirectory() {
    std::error_code ec;
    std::filesystem::create_directories(DOCUMENT_SOURCE_UPLOAD_DIR_PRIMARY, ec);
    if (!ec) {
        return DOCUMENT_SOURCE_UPLOAD_DIR_PRIMARY;
    }

    ec.clear();
    std::filesystem::create_directories(DOCUMENT_SOURCE_UPLOAD_DIR_FALLBACK, ec);
    if (!ec) {
        return DOCUMENT_SOURCE_UPLOAD_DIR_FALLBACK;
    }

    return "";
}

std::vector<std::filesystem::path> collectUploadSourceFiles(const std::string& uploadSourceDirectory) {
    std::vector<std::filesystem::path> files;
    std::error_code ec;

    for (std::filesystem::directory_iterator it(uploadSourceDirectory, ec); !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
        if (!it->is_regular_file()) {
            continue;
        }

        if (!isAllowedImportExtension(it->path().extension().string())) {
            continue;
        }

        files.push_back(it->path());
    }

    std::sort(files.begin(), files.end());
    return files;
}

bool copyImportedFileToStorage(const std::string& sourcePath,
                               const std::string& targetDirectory,
                               const std::string& documentId,
                               std::string& outStoredFilePath,
                               std::string& outStoredFileName,
                               std::string& outFileType,
                               long long& outFileSizeBytes) {
    std::error_code ec;
    std::filesystem::path source(sourcePath);

    if (!std::filesystem::exists(source, ec) || ec) {
        return false;
    }

    const std::string extension = toLowerCopy(source.extension().string());
    if (!isAllowedImportExtension(extension)) {
        return false;
    }

    const std::string outputName = documentId + extension;
    const std::filesystem::path target = std::filesystem::path(targetDirectory) / outputName;

    std::filesystem::copy_file(source, target, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        return false;
    }

    const std::uintmax_t sizeValue = std::filesystem::file_size(target, ec);
    if (ec) {
        return false;
    }

    outStoredFilePath = target.string();
    outStoredFileName = outputName;
    outFileType = extension.empty() ? "unknown" : extension.substr(1);
    outFileSizeBytes = static_cast<long long>(sizeValue);
    return true;
}

std::vector<ApprovalChainRow> loadApprovalChainRows(const std::string& docId) {
    std::vector<ApprovalChainRow> rows;
    std::ifstream file;
    if (!openInputFileWithFallback(file, APPROVALS_FILE_PATH_PRIMARY, APPROVALS_FILE_PATH_FALLBACK)) {
        return rows;
    }

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
        if (tokens.size() < 4 || tokens[0] != docId) {
            continue;
        }

        ApprovalChainRow row;
        row.approverUsername = tokens[1];
        row.role = tokens[2];
        row.status = tokens[3];
        row.decidedAt = tokens.size() > 5 ? tokens[5] : "";
        row.note = tokens.size() > 6 ? tokens[6] : "";
        rows.push_back(row);
    }

    return rows;
}

std::string buildSummaryFallbackText(const Document& doc) {
    std::ostringstream out;
    out << "Document ID: " << doc.docId << "\n";
    out << "Title: " << doc.title << "\n";
    out << "Category: " << doc.category << "\n";
    out << "Tags: " << (doc.tags.empty() ? "(none)" : doc.tags) << "\n";
    out << "Description: " << doc.description << "\n";
    out << "Department: " << doc.department << "\n";
    out << "Date Uploaded: " << doc.dateUploaded << "\n";
    out << "Uploader: " << doc.uploader << "\n";
    out << "Status: " << doc.status << "\n";
    out << "Hash: " << doc.hashValue << "\n";
    out << "Version: v" << doc.versionNumber << "\n";
    out << "Previous Version: " << (doc.previousDocId.empty() ? "(root)" : doc.previousDocId) << "\n";
    return out.str();
}

void printDocumentSummaryResult(const DocumentSummaryResult& result) {
    std::cout << "\n" << ui::bold("AI Document Summary") << "\n";
    std::cout << ui::muted("--------------------------------------------------------------") << "\n";
    std::cout << "  Document ID : " << result.docId << "\n";
    std::cout << "  Updated At  : " << (result.updatedAt.empty() ? "(not available)" : result.updatedAt) << "\n";
    std::cout << "  Model       : " << (result.model.empty() ? "(not available)" : result.model) << "\n";
    std::cout << "  Source      : " << (result.fromCache ? "cache" : "fresh run") << "\n";
    std::cout << "  Status      : " << (result.success ? "READY" : "UNAVAILABLE") << "\n";
    if (!result.message.empty()) {
        std::cout << "  Note        : " << result.message << "\n";
    }

    if (!result.summary.empty()) {
        std::cout << "\n" << result.summary << "\n";
    } else {
        std::cout << "\n" << ui::warning("[!] No summary text available yet.") << "\n";
    }
}

void runDocumentSummaryPanel(const Document& doc, const std::string& actor) {
    int choice = -1;

    do {
        std::cout << "\n" << ui::bold("AI Summary Actions") << "\n";
        std::cout << "  " << ui::info("[1]") << " View cached AI summary\n";
        std::cout << "  " << ui::info("[2]") << " Generate/Refresh AI summary\n";
        std::cout << "  " << ui::info("[0]") << " Back\n";
        std::cout << ui::muted("--------------------------------------------------------------") << "\n";
        std::cout << "  Enter your choice: ";

        std::cin >> choice;
        if (std::cin.fail()) {
            std::cin.clear();
            clearInputBuffer();
            std::cout << ui::warning("[!] Invalid choice.") << "\n";
            continue;
        }

        if (choice == 1) {
            const DocumentSummaryResult result = getDocumentSummary(doc.docId);
            printDocumentSummaryResult(result);
            logAuditAction("DOC_AI_SUMMARY_VIEW", doc.docId, actor);
            waitForEnter();
            continue;
        }

        if (choice == 2) {
            const DocumentSummaryResult result = generateDocumentSummary(doc.docId,
                                                                         doc.filePath,
                                                                         buildSummaryFallbackText(doc),
                                                                         true);
            printDocumentSummaryResult(result);
            logAuditAction(result.success ? "DOC_AI_SUMMARY_REFRESH_OK" : "DOC_AI_SUMMARY_REFRESH_FAIL",
                           doc.docId,
                           actor);
            waitForEnter();
            continue;
        }

        if (choice != 0) {
            std::cout << ui::warning("[!] Invalid choice.") << "\n";
        }
    } while (choice != 0);
}

void showDocumentDetailWithSummary(const Document& doc,
                                   bool includeApprovalChain,
                                   const std::vector<Document>* allDocs,
                                   const std::string& actor) {
    printDocumentDetailPanel(doc, includeApprovalChain, allDocs);
    runDocumentComparisonPanel(doc, allDocs, actor);
    runDocumentSummaryPanel(doc, actor);
}

void printDocumentDetailPanel(const Document& doc, bool includeApprovalChain, const std::vector<Document>* allDocs) {
    std::ostringstream sizeOut;
    sizeOut << doc.fileSizeBytes;

    const std::vector<std::string> headers = {"Field", "Value"};
    const std::vector<int> widths = {24, 66};
    ui::printTableHeader(headers, widths);
    ui::printTableRow({"Document ID", doc.docId}, widths);
    ui::printTableRow({"Title", doc.title}, widths);
    ui::printTableRow({"Category", doc.category}, widths);
    ui::printTableRow({"Tags", doc.tags.empty() ? "(none)" : doc.tags}, widths);
    ui::printTableRow({"Description", doc.description.empty() ? "(none)" : doc.description}, widths);
    ui::printTableRow({"Department", doc.department}, widths);
    ui::printTableRow({"Date Uploaded", doc.dateUploaded}, widths);
    ui::printTableRow({"Uploader", doc.uploader}, widths);
    ui::printTableRow({"Status", doc.status}, widths);
    ui::printTableRow({"Hash", doc.hashValue}, widths);
    ui::printTableRow({"File Name", doc.fileName.empty() ? "(legacy/no import)" : doc.fileName}, widths);
    ui::printTableRow({"File Type", doc.fileType.empty() ? "(legacy/no import)" : doc.fileType}, widths);
    ui::printTableRow({"File Path", doc.filePath.empty() ? "(legacy/no import)" : doc.filePath}, widths);
    ui::printTableRow({"File Size (bytes)", sizeOut.str()}, widths);
    ui::printTableRow({"Version", std::to_string(doc.versionNumber)}, widths);
    ui::printTableRow({"Previous Version", doc.previousDocId.empty() ? "(root)" : doc.previousDocId}, widths);
    ui::printTableRow({"Budget Link", "Managed in Budget Workspace (separate consensus flow)"}, widths);
    ui::printTableFooter(widths);

    printDocumentStatusHistoryPanel(doc.docId);

    if (allDocs != NULL) {
        printDocumentLineagePanel(doc, *allDocs);
    }

    if (!includeApprovalChain) {
        return;
    }

    const std::vector<ApprovalChainRow> chainRows = loadApprovalChainRows(doc.docId);
    if (chainRows.empty()) {
        std::cout << "\n" << ui::warning("[!] No approval-chain entries found for this document.") << "\n";
        return;
    }

    int approved = 0;
    int pending = 0;
    int rejected = 0;

    std::cout << "\n" << ui::bold("Approval Chain") << "\n";
    const std::vector<std::string> chainHeaders = {"Role", "Approver", "Status", "Decided At", "Note"};
    const std::vector<int> chainWidths = {22, 16, 12, 19, 24};
    ui::printTableHeader(chainHeaders, chainWidths);

    for (std::size_t i = 0; i < chainRows.size(); ++i) {
        ui::printTableRow({chainRows[i].role, chainRows[i].approverUsername, chainRows[i].status, chainRows[i].decidedAt, chainRows[i].note}, chainWidths);

        const std::string normalized = toLowerCopy(chainRows[i].status);
        if (normalized == "approved") {
            approved++;
        } else if (normalized == "pending") {
            pending++;
        } else if (normalized == "rejected") {
            rejected++;
        }
    }
    ui::printTableFooter(chainWidths);

    std::ostringstream summary;
    summary << "Approved: " << approved << " | Pending: " << pending << " | Rejected: " << rejected;
    std::cout << "  " << summary.str() << "\n";

    const int totalApprovers = static_cast<int>(chainRows.size());
    const int filled = std::max(0, std::min(approved, totalApprovers));
    const std::string progressBar = "[" + std::string(static_cast<std::size_t>(filled), '#') +
                                    std::string(static_cast<std::size_t>(totalApprovers - filled), '-') + "]";
    std::cout << "  Consensus Progress: " << progressBar
              << " " << approved << "/" << totalApprovers << " approvals\n";

    std::cout << "\n" << ui::bold("Approvals Checklist") << "\n";
    for (std::size_t i = 0; i < chainRows.size(); ++i) {
        const std::string normalized = toLowerCopy(chainRows[i].status);
        std::string marker = "[ ]";
        if (normalized == "approved") {
            marker = "[v]";
        } else if (normalized == "rejected") {
            marker = "[x]";
        }

        std::cout << "  " << marker << " " << chainRows[i].role << " - " << chainRows[i].approverUsername;
        if (normalized == "rejected" && !chainRows[i].note.empty()) {
            std::cout << " | Reason: " << chainRows[i].note;
        }
        std::cout << "\n";
    }

    if (rejected > 0) {
        std::cout << "  " << ui::consensusStatus("rejected") << " (publication blocked)\n";
    } else if (pending > 0) {
        std::cout << "  " << ui::consensusStatus("pending") << " (waiting unanimous approval)\n";
    } else {
        std::cout << "  " << ui::consensusStatus("published") << " (all required officials approved)\n";
    }
}

std::string chooseDocumentsExportPath(const std::string& fileName) {
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

void writeAppliedFilterLine(std::ofstream& out, const std::string& label, const std::string& value, bool& wroteAny) {
    if (value.empty()) {
        return;
    }

    out << "  - " << label << ": " << value << "\n";
    wroteAny = true;
}

bool writeDocumentsTxtReport(const std::string& outputPath,
                             const std::vector<Document>& rows,
                             const Admin& admin,
                             const std::string& modeLabel,
                             const DocumentFilter* filter) {
    std::ofstream out(outputPath);
    if (!out.is_open()) {
        return false;
    }

    out << "PROCURECHAIN DOCUMENTS EXPORT REPORT\n";
    out << "Generated at : " << getCurrentTimestamp() << "\n";
    out << "Generated by : " << admin.fullName << " (" << admin.username << ")\n";
    out << "Export mode  : " << modeLabel << "\n";
    out << "Total rows   : " << rows.size() << "\n";
    out << "==============================================================\n";

    if (filter != NULL) {
        out << "Applied Filters:\n";
        bool wroteAny = false;

        writeAppliedFilterLine(out, "Status", filter->status, wroteAny);
        writeAppliedFilterLine(out, "Exact Date", filter->exactDate, wroteAny);
        writeAppliedFilterLine(out, "From Date", filter->fromDate, wroteAny);
        writeAppliedFilterLine(out, "To Date", filter->toDate, wroteAny);
        writeAppliedFilterLine(out, "Category Contains", filter->category, wroteAny);
        writeAppliedFilterLine(out, "Tags Contains", filter->tags, wroteAny);
        writeAppliedFilterLine(out, "Department Contains", filter->department, wroteAny);
        writeAppliedFilterLine(out, "Uploader Contains", filter->uploader, wroteAny);

        if (!wroteAny) {
            out << "  - (none)\n";
        }

        out << "==============================================================\n";
    }

    for (size_t i = 0; i < rows.size(); ++i) {
        std::ostringstream amountOut;
        amountOut << std::fixed << std::setprecision(2) << rows[i].amount;

        out << "Record #" << (i + 1) << "\n";
        out << "  Document ID     : " << rows[i].docId << "\n";
        out << "  Title           : " << rows[i].title << "\n";
        out << "  Category        : " << rows[i].category << "\n";
        out << "  Tags            : " << (rows[i].tags.empty() ? "(none)" : rows[i].tags) << "\n";
        out << "  Department      : " << rows[i].department << "\n";
        out << "  Date Uploaded   : " << rows[i].dateUploaded << "\n";
        out << "  Uploader        : " << rows[i].uploader << "\n";
        out << "  Status          : " << rows[i].status << "\n";
        out << "  Hash Value      : " << rows[i].hashValue << "\n";
        out << "  File Metadata   : " << (rows[i].fileName.empty() ? "(none)" : rows[i].fileName) << "\n";
        out << "  Budget Link     : managed in budget workspace\n";
        out << "--------------------------------------------------------------\n";
    }

    out.flush();
    return out.good();
}

void printDocumentsTable(const std::vector<Document>& docs, bool includeHashValue) {
    // Tabular renderer for both full-list screens and single-record lookups.
    std::vector<int> widths;
    std::vector<std::string> headers;

    if (includeHashValue) {
        headers = {"ID", "Ver", "Title", "Category", "Department", "Date", "Uploader", "Status", "Tags", "Hash"};
        widths = {6, 5, 16, 12, 12, 10, 10, 12, 16, 10};
    } else {
        headers = {"ID", "Ver", "Title", "Category", "Department", "Date", "Uploader", "Status", "Tags"};
        widths = {6, 5, 18, 12, 12, 10, 10, 12, 16};
    }

    ui::printTableHeader(headers, widths);
    for (size_t i = 0; i < docs.size(); ++i) {
        if (includeHashValue) {
            ui::printTableRow({docs[i].docId,
                               std::to_string(docs[i].versionNumber),
                               docs[i].title,
                               docs[i].category,
                               docs[i].department,
                               docs[i].dateUploaded,
                               docs[i].uploader,
                               docs[i].status,
                               docs[i].tags,
                               docs[i].hashValue},
                              widths);
        } else {
            ui::printTableRow({docs[i].docId,
                               std::to_string(docs[i].versionNumber),
                               docs[i].title,
                               docs[i].category,
                               docs[i].department,
                               docs[i].dateUploaded,
                               docs[i].uploader,
                               docs[i].status,
                               docs[i].tags},
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

    const std::vector<std::string> requestedTags = splitNormalizedTags(filter.tags);
    for (size_t i = 0; i < requestedTags.size(); ++i) {
        if (!containsCaseInsensitive(doc.tags, requestedTags[i])) {
            return false;
        }
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

storage::OperationResult generateNextDocumentId(std::string& nextId) {
    // Scans existing IDs and increments max numeric suffix: O(n).
    nextId.clear();
    std::vector<Document> docs;
    if (!loadDocuments(docs)) {
        return storage::makeFailure("Unable to load documents data while generating the next document ID.");
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

        const std::string suffix = docs[i].docId.substr(numericStart);
        int parsedNumber = 0;
        if (!storage::tryParseIntStrict(suffix, parsedNumber)) {
            return storage::makeFailure("Malformed document ID encountered while generating the next document ID: " + docs[i].docId);
        }
        if (parsedNumber > maxIdNumber) {
            maxIdNumber = parsedNumber;
        }
    }

    std::ostringstream idBuilder;
    idBuilder << 'D' << std::setw(3) << std::setfill('0') << (maxIdNumber + 1);
    nextId = idBuilder.str();
    return storage::makeSuccess("Next document ID generated successfully.");
}

void ensureSampleDocumentsPresent() {
    // Showcase data is maintained in data/documents.txt, not hardcoded here.
    // This pass only backfills missing legacy hashes.
    // Existing hashes must remain immutable so verification can detect tampering reliably.
    ensureStatusHistoryFileExists();

    std::vector<Document> docs;
    if (!loadDocuments(docs)) {
        return;
    }

    if (!docs.empty()) {
        bool changed = false;
        for (std::size_t i = 0; i < docs.size(); ++i) {
            if (docs[i].hashValue.empty()) {
                docs[i].hashValue = computeDocumentRecordHash(docs[i]);
                changed = true;
            }
        }

        if (changed) {
            saveDocuments(docs);
        }
    }
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

void searchPublishedDocumentForCitizen(const std::string& actor) {
    clearScreen();
    ui::printSectionTitle("SEARCH PUBLISHED DOCUMENT (ID OR KEYWORD)");

    std::vector<Document> docs;
    if (!loadDocuments(docs)) {
        std::cout << ui::error("[!] Unable to open documents file.") << "\n";
        logAuditAction("SEARCH_PUBLISHED_DOC_FAILED", "N/A", actor);
        waitForEnter();
        return;
    }

    std::vector<Document> published;
    for (std::size_t i = 0; i < docs.size(); ++i) {
        if (toLowerCopy(docs[i].status) == "published") {
            published.push_back(docs[i]);
        }
    }

    if (published.empty()) {
        std::cout << ui::warning("[!] No published documents are available.") << "\n";
        logAuditAction("SEARCH_PUBLISHED_DOC_EMPTY", "N/A", actor);
        waitForEnter();
        return;
    }

    printRecentDocumentHints(published, 10);
    clearInputBuffer();

    std::cout << "\n" << ui::bold("Search Suggestions") << "\n";
    std::cout << "  - Exact ID (example: PR001)\n";
    std::cout << "  - Title keyword (example: contract)\n";
    std::cout << "  - Description keyword (example: renovation)\n";
    std::cout << "  - Tag keyword (example: urgent)\n";

    std::string query;
    std::cout << "Enter search value: ";
    std::getline(std::cin, query);

    if (query.empty()) {
        std::cout << ui::error("[!] Search value is required.") << "\n";
        logAuditAction("SEARCH_PUBLISHED_DOC_INPUT_ERROR", "N/A", actor);
        waitForEnter();
        return;
    }

    Document found;
    if (findExactDocumentById(published, query, found)) {
        showDocumentDetailWithSummary(found, true, &published, actor);
        logAuditAction("SEARCH_PUBLISHED_DOC", found.docId, actor);
        waitForEnter();
        return;
    }

    const std::vector<SearchMatch> matches = findRankedDocumentSuggestions(published, query, 12);
    if (matches.empty()) {
        std::cout << ui::warning("[!] No matching published documents found.") << "\n";
        logAuditAction("SEARCH_PUBLISHED_DOC_NOT_FOUND", query, actor);
        waitForEnter();
        return;
    }

    std::cout << "\n" << ui::info("[i] Showing ranked matches:") << "\n";
    printRankedSearchMatches(matches);

    std::string selectedDocId;
    std::cout << "\nEnter exact Document ID from results (blank to cancel): ";
    std::getline(std::cin, selectedDocId);

    if (selectedDocId.empty()) {
        logAuditAction("SEARCH_PUBLISHED_DOC_SUGGESTIONS_VIEW", query, actor);
        waitForEnter();
        return;
    }

    Document selected;
    if (findDocumentInMatchesById(matches, selectedDocId, selected)) {
        showDocumentDetailWithSummary(selected, true, &published, actor);
        logAuditAction("SEARCH_PUBLISHED_DOC", selected.docId, actor);
        waitForEnter();
        return;
    }

    std::cout << "\n" << ui::warning("[!] Selected Document ID is not in the ranked results.") << "\n";
    logAuditAction("SEARCH_PUBLISHED_DOC_NOT_FOUND", selectedDocId, actor);
    waitForEnter();
}

void viewPublishedDocumentDetailForCitizenById(const std::string& docId, const std::string& actor) {
    std::vector<Document> docs;
    if (!loadDocuments(docs)) {
        std::cout << ui::error("[!] Unable to open documents file.") << "\n";
        logAuditAction("AUDIT_DOC_DRILLDOWN_FAILED", docId, actor);
        waitForEnter();
        return;
    }

    Document found;
    if (!findExactDocumentById(docs, docId, found)) {
        std::cout << ui::warning("[!] Document not found for audit drill-down.") << "\n";
        logAuditAction("AUDIT_DOC_DRILLDOWN_NOT_FOUND", docId, actor);
        waitForEnter();
        return;
    }

    if (toLowerCopy(found.status) != "published") {
        std::cout << ui::warning("[i] This document is visible from public timeline events but is not currently published.") << "\n";
    }

    clearScreen();
    ui::printSectionTitle("DOCUMENT DETAIL (AUDIT DRILL-DOWN)");
    std::vector<Document> publishedDocs;
    for (std::size_t i = 0; i < docs.size(); ++i) {
        if (toLowerCopy(docs[i].status) == "published") {
            publishedDocs.push_back(docs[i]);
        }
    }

    const std::vector<Document>* visibleDocs = NULL;
    if (toLowerCopy(found.status) == "published") {
        visibleDocs = &publishedDocs;
    }

    showDocumentDetailWithSummary(found, true, visibleDocs, actor);
    logAuditAction("AUDIT_DOC_DRILLDOWN_VIEW", found.docId, actor);
    waitForEnter();
}

void uploadDocumentAsAdmin(const Admin& admin) {
    // Upload flow appends a new pending document, then creates approval rows,
    // then appends blockchain/audit events for traceability.
    clearScreen();
    ui::printSectionTitle("ADMIN DOCUMENT UPLOAD");

    std::cout << ui::bold("Mini Guide") << "\n";
    std::cout << "  1) Enter title/category/description.\n";
    std::cout << "  1.1) Optional: add comma-separated tags (e.g., urgent, legal, q2).\n";
    std::cout << "  2) Pick one source file from data/uploads (.pdf/.docx/.csv/.txt).\n";
    std::cout << "  3) The system copies the file into data/documents using the new document ID.\n";
    std::cout << "  4) It computes SHA-256 from the file (or metadata if no file) and stores it.\n";
    std::cout << "  5) It auto-creates pending approvals based on configured category rules (with safe defaults).\n\n";
    std::cout << "  6) Optional: link this upload as an amendment to a rejected document (v1 -> v2).\n\n";
    std::cout << "  7) If duplicate hash is found, choose cancel, continue, or link as amendment.\n\n";

    clearInputBuffer();

    Document doc;
    std::cout << "Title      : ";
    std::getline(std::cin, doc.title);
    doc.title = storage::sanitizeSingleLineInput(doc.title);
    if (!promptCategoryChoice(buildDocumentCategoryChoices(), doc.category)) {
        std::cout << ui::error("[!] Invalid category selection.") << "\n";
        logAuditAction("UPLOAD_DOC_FAILED", "N/A", admin.username);
        waitForEnter();
        return;
    }
    std::cout << "Description: ";
    std::getline(std::cin, doc.description);
    doc.description = storage::sanitizeSingleLineInput(doc.description);
    std::cout << "Tags (comma-separated, optional): ";
    std::string tagInput;
    std::getline(std::cin, tagInput);
    tagInput = storage::sanitizeSingleLineInput(tagInput);

    if (doc.title.empty() || doc.category.empty() || doc.description.empty()) {
        std::cout << ui::error("[!] Title, category, and description are required.") << "\n";
        logAuditAction("UPLOAD_DOC_FAILED", "N/A", admin.username);
        waitForEnter();
        return;
    }

    doc.tags = normalizeTagsForStorage(tagInput);

    std::vector<Document> docs;
    if (!loadDocuments(docs)) {
        std::cout << ui::error("[!] Unable to load document records. Upload is blocked until storage is healthy.") << "\n";
        logAuditAction("UPLOAD_DOC_FAILED", "N/A", admin.username);
        waitForEnter();
        return;
    }

    std::cout << "Amend rejected Document ID (optional): ";
    std::string amendmentDocId;
    std::getline(std::cin, amendmentDocId);
    amendmentDocId = trimCopy(amendmentDocId);

    doc.versionNumber = 1;
    doc.previousDocId = "";

    if (!amendmentDocId.empty()) {
        Document rejectedBase;
        if (!findExactDocumentById(docs, amendmentDocId, rejectedBase)) {
            std::cout << ui::error("[!] Rejected base document not found.") << "\n";
            logAuditAction("UPLOAD_DOC_FAILED", amendmentDocId, admin.username);
            waitForEnter();
            return;
        }

        if (toLowerCopy(rejectedBase.status) != "rejected") {
            std::cout << ui::error("[!] Amendment is allowed only for rejected documents.") << "\n";
            logAuditAction("UPLOAD_DOC_FAILED", amendmentDocId, admin.username);
            waitForEnter();
            return;
        }

        doc.versionNumber = rejectedBase.versionNumber + 1;
        doc.previousDocId = rejectedBase.docId;
    }

    // Budget allocation is handled in the dedicated budget workspace.
    doc.budgetCategory = "N/A";
    doc.amount = 0.0;

    storage::OperationResult nextIdResult = generateNextDocumentId(doc.docId);
    if (!nextIdResult.ok) {
        std::cout << ui::error("[!] ") << nextIdResult.message << "\n";
        logAuditAction("UPLOAD_DOC_FAILED", "N/A", admin.username);
        waitForEnter();
        return;
    }
    doc.dateUploaded = getCurrentTimestamp().substr(0, 10);
    doc.uploader = admin.username;
    doc.status = "pending_approval";
    doc.department = "General";

    std::string sourceFilePath;
    const std::string uploadSourceDirectory = resolveUploadSourceDirectory();
    if (uploadSourceDirectory.empty()) {
        std::cout << ui::error("[!] Unable to prepare uploads source folder.") << "\n";
        logAuditAction("UPLOAD_DOC_FAILED", doc.docId, admin.username);
        waitForEnter();
        return;
    }

    const std::vector<std::filesystem::path> uploadCandidates = collectUploadSourceFiles(uploadSourceDirectory);
    std::cout << "\n" << ui::bold("Uploads Folder Picker") << "\n";
    std::cout << "  Source folder: " << uploadSourceDirectory << "\n";

    if (!uploadCandidates.empty()) {
        const std::vector<std::string> uploadHeaders = {"#", "File Name", "Type", "Source Path"};
        const std::vector<int> uploadWidths = {4, 26, 8, 50};
        ui::printTableHeader(uploadHeaders, uploadWidths);

        for (std::size_t i = 0; i < uploadCandidates.size(); ++i) {
            const std::string ext = uploadCandidates[i].extension().string();
            const std::string type = ext.empty() ? "unknown" : toLowerCopy(ext.substr(1));
            ui::printTableRow({std::to_string(i + 1),
                               uploadCandidates[i].filename().string(),
                               type,
                               uploadCandidates[i].string()},
                              uploadWidths);
        }

        ui::printTableFooter(uploadWidths);
    } else {
        std::cout << "  " << ui::warning("[!] No eligible files found in uploads folder.") << "\n";
    }

    std::cout << "\n" << ui::info("[0]") << " Metadata-only upload\n";
    if (!uploadCandidates.empty()) {
        std::cout << "  " << ui::info("[1-" + std::to_string(uploadCandidates.size()) + "]") << " Pick source file\n";
    }
    std::cout << ui::muted("--------------------------------------------------------------") << "\n";
    std::cout << "  Select source option: ";

    int sourceChoice = -1;
    std::cin >> sourceChoice;
    if (std::cin.fail()) {
        std::cin.clear();
        clearInputBuffer();
        std::cout << ui::error("[!] Invalid upload source selection.") << "\n";
        logAuditAction("UPLOAD_DOC_FAILED", doc.docId, admin.username);
        waitForEnter();
        return;
    }

    if (sourceChoice < 0 || sourceChoice > static_cast<int>(uploadCandidates.size())) {
        clearInputBuffer();
        std::cout << ui::error("[!] Upload source selection out of range.") << "\n";
        logAuditAction("UPLOAD_DOC_FAILED", doc.docId, admin.username);
        waitForEnter();
        return;
    }

    clearInputBuffer();
    if (sourceChoice > 0) {
        sourceFilePath = uploadCandidates[static_cast<std::size_t>(sourceChoice - 1)].string();
    }

    if (!sourceFilePath.empty()) {
        const std::string uploadDirectory = resolveUploadDirectory();
        if (uploadDirectory.empty()) {
            std::cout << ui::error("[!] Unable to prepare document storage folder.") << "\n";
            logAuditAction("UPLOAD_DOC_FAILED", doc.docId, admin.username);
            waitForEnter();
            return;
        }

        if (!copyImportedFileToStorage(sourceFilePath,
                                       uploadDirectory,
                                       doc.docId,
                                       doc.filePath,
                                       doc.fileName,
                                       doc.fileType,
                                       doc.fileSizeBytes)) {
            std::cout << ui::error("[!] File import failed. Ensure the path exists and extension is one of: pdf, docx, csv, txt.") << "\n";
            logAuditAction("UPLOAD_DOC_FAILED", doc.docId, admin.username);
            waitForEnter();
            return;
        }

        doc.hashValue = computeFileHashSha256(doc.filePath);
        if (doc.hashValue.empty()) {
            deleteStoredDocumentFile(doc.filePath);
            std::cout << ui::error("[!] Unable to compute SHA-256 hash for the imported file.") << "\n";
            logAuditAction("UPLOAD_DOC_FAILED", doc.docId, admin.username);
            waitForEnter();
            return;
        }
    } else {
        doc.fileName = "";
        doc.fileType = "manual";
        doc.filePath = "";
        doc.fileSizeBytes = 0;
        doc.hashValue = computeDocumentRecordHash(doc);
    }

    const std::vector<Document> duplicateMatches = findDocumentsByHash(docs, doc.hashValue);
    if (!duplicateMatches.empty()) {
        logAuditAction("UPLOAD_DOC_DUPLICATE_HASH_DETECTED", doc.docId, admin.username);

        std::string linkedDocId;
        const DuplicateHashAction action = promptDuplicateHashAction(duplicateMatches, linkedDocId);

        if (action == DUPLICATE_HASH_CANCEL) {
            deleteStoredDocumentFile(doc.filePath);
            std::cout << ui::warning("[!] Upload cancelled due to duplicate hash.") << "\n";
            logAuditAction("UPLOAD_DOC_DUPLICATE_CANCELLED", doc.docId, admin.username);
            waitForEnter();
            return;
        }

        if (action == DUPLICATE_HASH_LINK) {
            Document linkedBase;
            if (!findDocumentByIdCaseInsensitive(duplicateMatches, linkedDocId, linkedBase)) {
                deleteStoredDocumentFile(doc.filePath);
                std::cout << ui::error("[!] Linked base Document ID is not part of duplicate matches.") << "\n";
                logAuditAction("UPLOAD_DOC_DUPLICATE_LINK_FAILED", doc.docId, admin.username);
                waitForEnter();
                return;
            }

            doc.previousDocId = linkedBase.docId;
            doc.versionNumber = linkedBase.versionNumber + 1;
            logAuditAction("UPLOAD_DOC_DUPLICATE_LINKED", doc.docId + "<-" + linkedBase.docId, admin.username);
        } else {
            logAuditAction("UPLOAD_DOC_DUPLICATE_CONTINUED", doc.docId, admin.username);
        }
    }

    docs.push_back(doc);
    if (!saveDocuments(docs)) {
        deleteStoredDocumentFile(doc.filePath);
        std::cout << ui::error("[!] Unable to save document record.") << "\n";
        logAuditAction("UPLOAD_DOC_FAILED", doc.docId, admin.username);
        waitForEnter();
        return;
    }

    const storage::OperationResult approvalResult = createApprovalRequestsForDocument(doc.docId, admin.username, doc.category);
    if (!approvalResult.ok) {
        const storage::OperationResult rollbackResult = rollbackUploadedDocumentPersistenceDetailed(doc);
        if (!approvalResult.rollbackSucceeded || !rollbackResult.rollbackSucceeded) {
            logTamperAlert("HIGH",
                           "UPLOAD_DOC_ROLLBACK",
                           doc.docId,
                           "Document upload failed during approval routing and at least one rollback step was incomplete. Manual repair is required.",
                           admin.username,
                           "PUBLIC");
        }

        std::cout << ui::error("[!] Unable to create approval routing for the uploaded document.") << "\n";
        if (!approvalResult.message.empty()) {
            std::cout << "  Reason: " << approvalResult.message << "\n";
        }
        if (!rollbackResult.rollbackSucceeded) {
            std::cout << "  " << ui::error("Rollback was incomplete. Inspect documents, approvals, history, and stored files manually.") << "\n";
        }
        logAuditAction("UPLOAD_DOC_FAILED", doc.docId, admin.username);
        waitForEnter();
        return;
    }

    const std::vector<ApprovalChainRow> generatedChain = loadApprovalChainRows(doc.docId);
    if (!generatedChain.empty()) {
        std::cout << "\n" << ui::bold("Approval Chain Created") << "\n";
        const std::vector<std::string> headers = {"Role", "Approver", "Status"};
        const std::vector<int> widths = {26, 18, 12};
        ui::printTableHeader(headers, widths);
        for (std::size_t i = 0; i < generatedChain.size(); ++i) {
            ui::printTableRow({generatedChain[i].role, generatedChain[i].approverUsername, generatedChain[i].status}, widths);
        }
        ui::printTableFooter(widths);
        std::cout << "  " << ui::muted("Active accounts matching the category approval rule are auto-included in this chain.") << "\n";
    }

    const int chainIndex = appendBlockchainAction("UPLOAD_HASH:" + doc.hashValue, doc.docId, admin.username);
    if (chainIndex < 0 ||
        !appendDocumentStatusHistory(doc.docId,
                                     admin.username,
                                     "",
                                     doc.status,
                                     "Document uploaded and routed for approvals.") ||
        !tryLogAuditAction("UPLOAD_DOC", doc.docId, admin.username, chainIndex)) {
        const storage::OperationResult rollbackResult = rollbackUploadedDocumentPersistenceDetailed(doc);
        if (!rollbackResult.rollbackSucceeded) {
            logTamperAlert("HIGH",
                           "UPLOAD_DOC_ROLLBACK",
                           doc.docId,
                           "Upload finalization failed and rollback was incomplete. Manual data repair is required.",
                           admin.username,
                           "PUBLIC");
            std::cout << ui::error("[!] Upload persistence failed and rollback was incomplete. Please inspect document, approval, and status-history data.") << "\n";
        } else {
            std::cout << ui::error("[!] Upload persistence failed during audit/blockchain finalization. The upload was rolled back.") << "\n";
        }
        logAuditAction("UPLOAD_DOC_FAILED", doc.docId, admin.username);
        waitForEnter();
        return;
    }
    if (!doc.previousDocId.empty()) {
        logAuditAction("DOC_AMENDMENT_CREATED", doc.docId + "<-" + doc.previousDocId, admin.username);
    }
    std::cout << "\n" << ui::success("[+] Document uploaded successfully.") << "\n";
    std::cout << "  Document ID : " << doc.docId << "\n";
    std::cout << "  Version     : v" << doc.versionNumber;
    if (!doc.previousDocId.empty()) {
        std::cout << " (amends " << doc.previousDocId << ")";
    }
    std::cout << "\n";
    std::cout << "  Tags        : " << (doc.tags.empty() ? "(none)" : doc.tags) << "\n";
    if (doc.fileType == "manual") {
        std::cout << "  File Import : skipped (metadata-only upload)\n";
    } else {
        std::cout << "  File Name   : " << doc.fileName << "\n";
        std::cout << "  File Type   : " << doc.fileType << "\n";
        std::cout << "  File Size   : " << doc.fileSizeBytes << " bytes\n";
    }
    std::cout << "  SHA-256 Hash: " << doc.hashValue << "\n";
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
    ui::printSectionTitle("SEARCH DOCUMENT (ID OR KEYWORD)");

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
    std::cout << "  - Description keyword (example: rehabilitation)\n";
    std::cout << "  - Tag keyword (example: urgent)\n";

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
        showDocumentDetailWithSummary(exact, true, &docs, admin.username);
        logAuditAction("SEARCH_DOC", exact.docId, admin.username);
        waitForEnter();
        return;
    }

    std::vector<SearchMatch> matches = findRankedDocumentSuggestions(docs, query, 15);
    if (matches.empty()) {
        std::cout << "\n" << ui::warning("[!] No matching documents found.") << "\n";
        logAuditAction("SEARCH_DOC_NOT_FOUND", query, admin.username);
        waitForEnter();
        return;
    }

    std::cout << "\n" << ui::info("[i] No exact ID match. Showing ranked results:") << "\n";
    printRankedSearchMatches(matches);

    std::string selectedDocId;
    std::cout << "\nEnter exact Document ID from results (blank to cancel): ";
    std::getline(std::cin, selectedDocId);

    if (selectedDocId.empty()) {
        logAuditAction("SEARCH_DOC_SUGGESTIONS_VIEW", query, admin.username);
        waitForEnter();
        return;
    }

    Document selected;
    if (findDocumentInMatchesById(matches, selectedDocId, selected)) {
        showDocumentDetailWithSummary(selected, true, &docs, admin.username);
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
    std::cout << "Tags contains (optional): ";
    std::getline(std::cin, filter.tags);
    std::cout << "Department contains (optional): ";
    std::getline(std::cin, filter.department);
    std::cout << "Uploader contains (optional): ";
    std::getline(std::cin, filter.uploader);

    filter.tags = trimCopy(filter.tags);

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

void exportDocumentsToTxt(const Admin& admin) {
    clearScreen();
    ui::printSectionTitle("EXPORT DOCUMENTS TO TXT");

    std::vector<Document> docs;
    if (!loadDocuments(docs)) {
        std::cout << ui::error("[!] Unable to open documents file.") << "\n";
        logAuditAction("EXPORT_DOCS_TXT_FAILED", "N/A", admin.username);
        waitForEnter();
        return;
    }

    if (docs.empty()) {
        std::cout << ui::warning("[!] No document rows available to export.") << "\n";
        logAuditAction("EXPORT_DOCS_TXT_EMPTY", "N/A", admin.username);
        waitForEnter();
        return;
    }

    std::cout << "  " << ui::info("[1]") << " Export all rows\n";
    std::cout << "  " << ui::info("[2]") << " Export filtered rows\n";
    std::cout << "  " << ui::info("[0]") << " Cancel\n";
    std::cout << ui::muted("--------------------------------------------------------------") << "\n";
    std::cout << "  Enter your choice: ";

    int choice = -1;
    std::cin >> choice;
    if (std::cin.fail()) {
        std::cin.clear();
        clearInputBuffer();
        std::cout << ui::warning("[!] Invalid choice.") << "\n";
        logAuditAction("EXPORT_DOCS_TXT_INPUT_ERROR", "CHOICE", admin.username);
        waitForEnter();
        return;
    }

    if (choice == 0) {
        return;
    }

    std::vector<Document> rows = docs;
    DocumentFilter filter;
    bool usedFilter = false;

    if (choice == 2) {
        usedFilter = true;
        clearInputBuffer();

        printRecentDocumentHints(docs, 6);
        printFilterSuggestions(docs);

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
        std::cout << "Tags contains (optional): ";
        std::getline(std::cin, filter.tags);
        std::cout << "Department contains (optional): ";
        std::getline(std::cin, filter.department);
        std::cout << "Uploader contains (optional): ";
        std::getline(std::cin, filter.uploader);

        filter.status = trimCopy(filter.status);
        filter.exactDate = trimCopy(filter.exactDate);
        filter.fromDate = trimCopy(filter.fromDate);
        filter.toDate = trimCopy(filter.toDate);
        filter.category = trimCopy(filter.category);
        filter.tags = normalizeTagsForStorage(filter.tags);
        filter.department = trimCopy(filter.department);
        filter.uploader = trimCopy(filter.uploader);

        if (!isDateTextValid(filter.exactDate) || !isDateTextValid(filter.fromDate) || !isDateTextValid(filter.toDate)) {
            std::cout << ui::error("[!] Invalid date format. Use YYYY-MM-DD.") << "\n";
            logAuditAction("EXPORT_DOCS_TXT_INPUT_ERROR", "DATE", admin.username);
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
            logAuditAction("EXPORT_DOCS_TXT_INPUT_ERROR", "RANGE", admin.username);
            waitForEnter();
            return;
        }

        std::vector<Document> result;
        for (size_t i = 0; i < docs.size(); ++i) {
            if (matchesFilter(docs[i], filter)) {
                result.push_back(docs[i]);
            }
        }

        rows = result;
    } else if (choice != 1) {
        std::cout << ui::warning("[!] Invalid choice.") << "\n";
        logAuditAction("EXPORT_DOCS_TXT_INPUT_ERROR", "CHOICE", admin.username);
        waitForEnter();
        return;
    } else {
        clearInputBuffer();
    }

    if (rows.empty()) {
        std::cout << ui::warning("[!] No rows matched the selected export criteria.") << "\n";
        logAuditAction("EXPORT_DOCS_TXT_EMPTY", "FILTERED", admin.username);
        waitForEnter();
        return;
    }

    std::string fileName;
    std::cout << "Output filename (.txt, blank for default): ";
    std::getline(std::cin, fileName);

    fileName = trimCopy(fileName);
    if (fileName.empty()) {
        fileName = defaultDocumentsExportFileName();
    } else if (!isSafeExportFileName(fileName)) {
        std::cout << ui::error("[!] Invalid filename. Use letters, numbers, '_', '-', and '.'.") << "\n";
        logAuditAction("EXPORT_DOCS_TXT_INPUT_ERROR", "FILENAME", admin.username);
        waitForEnter();
        return;
    }

    if (fileName.size() < 4 || toLowerCopy(fileName.substr(fileName.size() - 4)) != ".txt") {
        fileName += ".txt";
    }

    const std::string outputPath = chooseDocumentsExportPath(fileName);
    if (outputPath.empty()) {
        std::cout << ui::error("[!] Unable to create export path.") << "\n";
        logAuditAction("EXPORT_DOCS_TXT_FAILED", fileName, admin.username);
        waitForEnter();
        return;
    }

    const std::string modeLabel = usedFilter ? "filtered" : "all";
    const DocumentFilter* activeFilter = usedFilter ? &filter : NULL;

    if (!writeDocumentsTxtReport(outputPath, rows, admin, modeLabel, activeFilter)) {
        std::cout << ui::error("[!] Unable to write export file.") << "\n";
        logAuditAction("EXPORT_DOCS_TXT_FAILED", fileName, admin.username);
        waitForEnter();
        return;
    }

    logAuditAction("EXPORT_DOCS_TXT", fileName + "|" + modeLabel, admin.username);

    std::cout << ui::success("[+] Export completed: ") << outputPath << "\n";
    std::cout << ui::info("[i] Rows exported: ") << rows.size() << "\n";
    waitForEnter();
}

bool updateDocumentStatusBySystem(const std::string& targetDocId, const std::string& newStatus) {
    return updateDocumentStatusBySystem(targetDocId, newStatus, "SYSTEM", "", NULL, false);
}

bool updateDocumentStatusBySystem(const std::string& targetDocId,
                                  const std::string& newStatus,
                                  const std::string& actorUsername,
                                  const std::string& reasonNote) {
    return updateDocumentStatusBySystem(targetDocId, newStatus, actorUsername, reasonNote, NULL, true);
}

bool updateDocumentStatusBySystem(const std::string& targetDocId,
                                  const std::string& newStatus,
                                  const std::string& actorUsername,
                                  const std::string& reasonNote,
                                  std::string* previousStatusOut,
                                  bool allowInteractiveGuardrail) {
    // Used by approval workflows to synchronize consensus outcome to documents.
    std::vector<Document> docs;
    if (!loadDocuments(docs)) {
        return false;
    }

    const std::vector<Document> originalDocs = docs;
    bool found = false;
    std::string previousStatus;
    for (size_t i = 0; i < docs.size(); ++i) {
        if (docs[i].docId == targetDocId) {
            previousStatus = docs[i].status;
            if (previousStatusOut != NULL) {
                *previousStatusOut = previousStatus;
            }

            if (previousStatus == newStatus) {
                return true;
            }

            if (toLowerCopy(newStatus) == "published") {
                if (allowInteractiveGuardrail) {
                    if (!confirmDocumentPublishGuardrail(docs[i], docs, actorUsername)) {
                        return false;
                    }
                } else {
                    const BudgetGuardrailCheck check = buildDocumentPublishGuardrailCheck(docs, docs[i]);
                    if (check.hasBudget && check.block) {
                        return false;
                    }
                    if (check.hasBudget && check.warn) {
                        return false;
                    }
                }
            }

            docs[i].status = newStatus;
            found = true;
            break;
        }
    }

    if (!found) {
        return false;
    }

    if (!saveDocuments(docs)) {
        return false;
    }

    if (!appendDocumentStatusHistory(targetDocId,
                                     actorUsername,
                                     previousStatus,
                                     newStatus,
                                     reasonNote)) {
        saveDocuments(originalDocs);
        return false;
    }

    return true;
}

void updateDocumentStatusForAdmin(const Admin& admin) {
    // Manual status override reserved for admin workflows.
    clearScreen();
    ui::printSectionTitle("UPDATE DOCUMENT STATUS");

    clearInputBuffer();
    std::string targetDocId;
    std::cout << "Enter Document ID: ";
    std::getline(std::cin, targetDocId);
    targetDocId = storage::sanitizeSingleLineInput(targetDocId);

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

    if (!ui::confirmAction("Are you sure you want to apply this status change?",
                           "Confirm Status Update",
                           "Cancel")) {
        std::cout << ui::warning("[!] Status update cancelled.") << "\n";
        waitForEnter();
        return;
    }

    const std::string noteText = "Manual status override by " + admin.role;
    std::string previousStatus;
    if (!updateDocumentStatusBySystem(targetDocId, newStatus, admin.username, noteText, &previousStatus, true)) {
        if (newStatus == "published") {
            std::cout << "\n" << ui::warning("[!] Publication was blocked by budget overrun guardrail.") << "\n";
        } else {
            std::cout << "\n" << ui::error("[!] Document ID not found or update failed.") << "\n";
        }
        logAuditAction("UPDATE_STATUS_FAILED", targetDocId, admin.username);
        waitForEnter();
        return;
    }

    const int chainIndex = appendBlockchainAction("STATUS_UPDATE_" + newStatus, targetDocId, admin.username);
    AuditLogMetadata metadata;
    metadata.targetType = "DOCUMENT";
    metadata.actorRole = admin.role;
    metadata.visibility = "PUBLIC";
    metadata.chainIndex = chainIndex;
    if (newStatus == "published") {
        metadata.outcome = "PUBLISHED";
    } else if (newStatus == "rejected") {
        metadata.outcome = "REJECTED";
    } else {
        metadata.outcome = "PENDING";
    }

    if (chainIndex < 0 || !tryLogAuditActionDetailed("UPDATE_STATUS", targetDocId, admin.username, metadata)) {
        updateDocumentStatusBySystem(targetDocId,
                                     previousStatus,
                                     admin.username,
                                     "Rollback after status finalization failure.",
                                     NULL,
                                     false);
        std::cout << "\n" << ui::error("[!] Status change was rolled back because blockchain/audit finalization failed.") << "\n";
        logAuditAction("UPDATE_STATUS_FAILED", targetDocId, admin.username);
        waitForEnter();
        return;
    }

    std::cout << ui::success("[+] Document status updated successfully.") << "\n";
    waitForEnter();
}




