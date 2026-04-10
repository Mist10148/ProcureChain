#include "../include/documents.h"

#include "../include/approvals.h"
#include "../include/audit.h"
#include "../include/auth.h"
#include "../include/blockchain.h"
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
const std::string APPROVALS_FILE_PATH_PRIMARY = "data/approvals.txt";
const std::string APPROVALS_FILE_PATH_FALLBACK = "../data/approvals.txt";
const std::string DOCUMENT_UPLOAD_DIR_PRIMARY = "data/documents";
const std::string DOCUMENT_UPLOAD_DIR_FALLBACK = "../data/documents";

struct ApprovalChainRow {
    std::string approverUsername;
    std::string role;
    std::string status;
    std::string decidedAt;
};

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

        std::stringstream sizeIn(tokens[12]);
        sizeIn >> doc.fileSizeBytes;

        doc.budgetCategory = tokens[13];

        std::stringstream amountIn(tokens[14]);
        amountIn >> doc.amount;

        if (tokens.size() > 15) {
            std::stringstream versionIn(tokens[15]);
            versionIn >> doc.versionNumber;
            if (versionIn.fail() || doc.versionNumber <= 0) {
                doc.versionNumber = 1;
            }
        }

        doc.previousDocId = tokens.size() > 16 ? tokens[16] : "";
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

        if (tokens.size() > 9) {
            std::stringstream amountIn(tokens[9]);
            amountIn >> doc.amount;
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
                                       doc.dateUploaded + "|" + doc.uploader;
    return computeSimpleHash(metadataSource);
}

std::string serializeDocument(const Document& doc) {
    const std::string metadataSource = doc.docId + "|" + doc.title + "|" + doc.category + "|" + doc.description + "|" + doc.department + "|" +
                                       doc.dateUploaded + "|" + doc.uploader;
    const std::string hashValue = !doc.hashValue.empty() ? doc.hashValue : computeSimpleHash(metadataSource);

    std::ostringstream out;
    out << doc.docId << '|'
        << doc.title << '|'
        << doc.category << '|'
        << doc.description << '|'
        << doc.department << '|'
        << doc.dateUploaded << '|'
        << doc.uploader << '|'
        << doc.status << '|'
        << hashValue << '|'
        << doc.fileName << '|'
        << doc.fileType << '|'
        << doc.filePath << '|'
        << doc.fileSizeBytes << '|'
        << doc.budgetCategory << '|'
        << std::fixed << std::setprecision(2) << doc.amount << '|'
        << doc.versionNumber << '|'
        << doc.previousDocId;

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

    writer << "docID|title|category|description|department|dateUploaded|uploader|status|hashValue|fileName|fileType|filePath|fileSizeBytes|budgetCategory|amount|versionNumber|previousDocId\n";
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

void printDocumentLineagePanel(const Document& doc, const std::vector<Document>& docs) {
    const std::string rootId = findRootDocumentId(docs, doc);
    std::vector<Document> lineage;

    for (std::size_t i = 0; i < docs.size(); ++i) {
        if (findRootDocumentId(docs, docs[i]) == rootId) {
            lineage.push_back(docs[i]);
        }
    }

    if (lineage.size() <= 1) {
        return;
    }

    std::sort(lineage.begin(), lineage.end(), [](const Document& left, const Document& right) {
        if (left.versionNumber != right.versionNumber) {
            return left.versionNumber < right.versionNumber;
        }

        if (left.dateUploaded != right.dateUploaded) {
            return left.dateUploaded < right.dateUploaded;
        }

        return left.docId < right.docId;
    });

    std::cout << "\n" << ui::bold("Version Lineage") << "\n";
    const std::vector<std::string> headers = {"Doc ID", "Version", "Status", "Previous", "Date"};
    const std::vector<int> widths = {10, 9, 16, 10, 10};
    ui::printTableHeader(headers, widths);

    for (std::size_t i = 0; i < lineage.size(); ++i) {
        std::string previous = lineage[i].previousDocId.empty() ? "(root)" : lineage[i].previousDocId;
        ui::printTableRow({lineage[i].docId,
                           std::to_string(lineage[i].versionNumber),
                           lineage[i].status,
                           previous,
                           lineage[i].dateUploaded},
                          widths);
    }

    ui::printTableFooter(widths);
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
        rows.push_back(row);
    }

    return rows;
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
    const std::vector<std::string> chainHeaders = {"Role", "Approver", "Status", "Decided At"};
    const std::vector<int> chainWidths = {26, 18, 14, 19};
    ui::printTableHeader(chainHeaders, chainWidths);

    for (std::size_t i = 0; i < chainRows.size(); ++i) {
        ui::printTableRow({chainRows[i].role, chainRows[i].approverUsername, chainRows[i].status, chainRows[i].decidedAt}, chainWidths);

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
    return true;
}

void printDocumentsTable(const std::vector<Document>& docs, bool includeHashValue) {
    // Tabular renderer for both full-list screens and single-record lookups.
    std::vector<int> widths;
    std::vector<std::string> headers;

    if (includeHashValue) {
        headers = {"ID", "Ver", "Title", "Category", "Department", "Date", "Uploader", "Status", "Hash"};
        widths = {6, 5, 18, 14, 14, 10, 10, 14, 10};
    } else {
        headers = {"ID", "Ver", "Title", "Category", "Department", "Date", "Uploader", "Status"};
        widths = {6, 5, 20, 14, 14, 10, 10, 14};
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
                               docs[i].status},
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
    // Showcase data is maintained in data/documents.txt, not hardcoded here.
    // This pass only normalizes hashes/serialization for existing rows.
    std::vector<Document> docs;
    if (!loadDocuments(docs)) {
        return;
    }

    if (!docs.empty()) {
        for (std::size_t i = 0; i < docs.size(); ++i) {
            docs[i].hashValue = computeDocumentRecordHash(docs[i]);
        }
        saveDocuments(docs);
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
    ui::printSectionTitle("SEARCH PUBLISHED DOCUMENT");

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

    std::string docId;
    std::cout << "Enter published Document ID: ";
    std::getline(std::cin, docId);

    if (docId.empty()) {
        std::cout << ui::error("[!] Document ID is required.") << "\n";
        logAuditAction("SEARCH_PUBLISHED_DOC_INPUT_ERROR", "N/A", actor);
        waitForEnter();
        return;
    }

    Document found;
    if (!findExactDocumentById(published, docId, found)) {
        std::cout << ui::warning("[!] Published document not found.") << "\n";
        logAuditAction("SEARCH_PUBLISHED_DOC_NOT_FOUND", docId, actor);
        waitForEnter();
        return;
    }

    printDocumentDetailPanel(found, true, &published);
    logAuditAction("SEARCH_PUBLISHED_DOC", found.docId, actor);
    waitForEnter();
}

void uploadDocumentAsAdmin(const Admin& admin) {
    // Upload flow appends a new pending document, then creates approval rows,
    // then appends blockchain/audit events for traceability.
    clearScreen();
    ui::printSectionTitle("ADMIN DOCUMENT UPLOAD");

    std::cout << ui::bold("Mini Guide") << "\n";
    std::cout << "  1) Enter title/category/description.\n";
    std::cout << "  2) Optionally provide a source file path (.pdf/.docx/.csv/.txt).\n";
    std::cout << "  3) The system copies the file into data/documents using the new document ID.\n";
    std::cout << "  4) It computes SHA-256 from the file (or metadata if no file) and stores it.\n";
    std::cout << "  5) It auto-creates pending approvals based on configured category rules (with safe defaults).\n\n";
    std::cout << "  6) Optional: link this upload as an amendment to a rejected document (v1 -> v2).\n\n";

    clearInputBuffer();

    Document doc;
    std::cout << "Title      : ";
    std::getline(std::cin, doc.title);
    if (!promptCategoryChoice(buildDocumentCategoryChoices(), doc.category)) {
        std::cout << ui::error("[!] Invalid category selection.") << "\n";
        logAuditAction("UPLOAD_DOC_FAILED", "N/A", admin.username);
        waitForEnter();
        return;
    }
    std::cout << "Description: ";
    std::getline(std::cin, doc.description);
    std::cout << "Source file path (optional): ";
    std::string sourceFilePath;
    std::getline(std::cin, sourceFilePath);

    // Keep upload guidance visible so users know exact accepted formats.
    std::cout << "  " << ui::muted("Tip: leave blank for metadata-only upload, or paste a full path to .pdf/.docx/.csv/.txt") << "\n";

    if (doc.title.empty() || doc.category.empty() || doc.description.empty()) {
        std::cout << ui::error("[!] Title, category, and description are required.") << "\n";
        logAuditAction("UPLOAD_DOC_FAILED", "N/A", admin.username);
        waitForEnter();
        return;
    }

    std::vector<Document> docs;
    if (!loadDocuments(docs)) {
        docs.clear();
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

    doc.docId = generateNextDocumentId();
    doc.dateUploaded = getCurrentTimestamp().substr(0, 10);
    doc.uploader = admin.username;
    doc.status = "pending_approval";
    doc.department = "General";

    sourceFilePath = trimCopy(sourceFilePath);
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

    docs.push_back(doc);
    if (!saveDocuments(docs)) {
        std::cout << ui::error("[!] Unable to save document record.") << "\n";
        logAuditAction("UPLOAD_DOC_FAILED", doc.docId, admin.username);
        waitForEnter();
        return;
    }

    createApprovalRequestsForDocument(doc.docId, admin.username, doc.category);

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

    logAuditAction("UPLOAD_DOC", doc.docId, admin.username, chainIndex);
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
        printDocumentDetailPanel(exact, true, &docs);
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
        printDocumentDetailPanel(selected, true, &docs);
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
        std::cout << "Department contains (optional): ";
        std::getline(std::cin, filter.department);
        std::cout << "Uploader contains (optional): ";
        std::getline(std::cin, filter.uploader);

        filter.status = trimCopy(filter.status);
        filter.exactDate = trimCopy(filter.exactDate);
        filter.fromDate = trimCopy(filter.fromDate);
        filter.toDate = trimCopy(filter.toDate);
        filter.category = trimCopy(filter.category);
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
