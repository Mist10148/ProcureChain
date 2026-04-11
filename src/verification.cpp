#include "../include/verification.h"

#include "../include/audit.h"
#include "../include/auth.h"
#include "../include/ui.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>

namespace {
// Verification reads stored document rows and recomputes the same hash source
// fields used during serialization to detect content drift.
const std::string DOCUMENTS_FILE_PATH_PRIMARY = "data/documents.txt";
const std::string DOCUMENTS_FILE_PATH_FALLBACK = "../data/documents.txt";
const std::string VERIFY_FOLDER_PATH_PRIMARY = "data/verify";
const std::string VERIFY_FOLDER_PATH_FALLBACK = "../data/verify";
const std::string BLOCKCHAIN_NODE_PRIMARY_PREFIX = "data/blockchain/node";
const std::string BLOCKCHAIN_NODE_FALLBACK_PREFIX = "../data/blockchain/node";
const int BLOCKCHAIN_NODE_COUNT = 5;

struct VerificationHint {
    std::string docId;
    std::string dateUploaded;
    std::string status;
    std::string title;
};

struct VerificationDocument {
    std::string docId;
    std::string title;
    std::string category;
    std::string description;
    std::string department;
    std::string dateUploaded;
    std::string uploader;
    std::string status;
    std::string hashValue;
    std::string fileName;
    std::string fileType;
    std::string filePath;
    std::string tags;
};

std::vector<std::string> splitPipe(const std::string& line) {
    std::vector<std::string> tokens;
    std::stringstream parser(line);
    std::string token;
    while (std::getline(parser, token, '|')) {
        tokens.push_back(token);
    }
    return tokens;
}

bool openInputFileWithFallback(std::ifstream& file, const std::string& primaryPath, const std::string& fallbackPath) {
    // Constant-time path fallback for documents file reads.
    file.open(primaryPath);
    if (file.is_open()) {
        return true;
    }

    file.clear();
    file.open(fallbackPath);
    return file.is_open();
}

void clearInputBuffer() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

bool openBlockchainNodeInput(std::ifstream& file, int nodeIndex) {
    const std::string primary = BLOCKCHAIN_NODE_PRIMARY_PREFIX + std::to_string(nodeIndex) + "_chain.txt";
    const std::string fallback = BLOCKCHAIN_NODE_FALLBACK_PREFIX + std::to_string(nodeIndex) + "_chain.txt";
    return openInputFileWithFallback(file, primary, fallback);
}

std::string resolveDirectoryPathWithFallback(const std::string& primaryPath, const std::string& fallbackPath) {
    std::error_code ec;
    std::filesystem::create_directories(primaryPath, ec);
    if (!ec) {
        return primaryPath;
    }

    ec.clear();
    std::filesystem::create_directories(fallbackPath, ec);
    if (!ec) {
        return fallbackPath;
    }

    return "";
}

std::string toLowerCopy(std::string value) {
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] >= 'A' && value[i] <= 'Z') {
            value[i] = static_cast<char>(value[i] + ('a' - 'A'));
        }
    }
    return value;
}

VerificationDocument parseVerificationDocument(const std::vector<std::string>& tokens) {
    VerificationDocument doc;
    doc.docId = tokens.size() > 0 ? tokens[0] : "";
    doc.title = tokens.size() > 1 ? tokens[1] : "";
    doc.category = tokens.size() > 2 ? tokens[2] : "";

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
        doc.tags = tokens.size() > 17 ? tokens[17] : "";
    } else {
        doc.description = "";
        doc.department = tokens.size() > 3 ? tokens[3] : "";
        doc.dateUploaded = tokens.size() > 4 ? tokens[4] : "";
        doc.uploader = tokens.size() > 5 ? tokens[5] : "";
        doc.status = tokens.size() > 6 ? tokens[6] : "";
        doc.hashValue = tokens.size() > 7 ? tokens[7] : "";
        doc.fileName = "";
        doc.fileType = "";
        doc.filePath = "";
        doc.tags = "";
    }

    return doc;
}

std::string buildDocumentMetadataHashSource(const VerificationDocument& doc) {
    return doc.docId + "|" + doc.title + "|" + doc.category + "|" + doc.description + "|" + doc.department + "|" +
           doc.dateUploaded + "|" + doc.uploader + "|" + doc.tags;
}

bool isAllowedVerificationExtension(const std::string& extensionWithDot) {
    const std::string ext = toLowerCopy(extensionWithDot);
    return ext == ".pdf" || ext == ".docx" || ext == ".csv" || ext == ".txt";
}

bool resolveCandidateVerifyFilePath(const std::string& docId, std::string& outFilePath, std::string& outReason) {
    outFilePath.clear();
    outReason.clear();

    const std::string verifyBasePath = resolveDirectoryPathWithFallback(VERIFY_FOLDER_PATH_PRIMARY, VERIFY_FOLDER_PATH_FALLBACK);
    if (verifyBasePath.empty()) {
        outReason = "Unable to prepare verify folder path.";
        return false;
    }

    const std::filesystem::path verifyDocFolder = std::filesystem::path(verifyBasePath) / docId;
    std::error_code ec;
    if (!std::filesystem::exists(verifyDocFolder, ec) || ec || !std::filesystem::is_directory(verifyDocFolder, ec)) {
        outReason = "Missing folder: " + verifyDocFolder.string();
        return false;
    }

    std::vector<std::filesystem::path> eligibleFiles;
    for (std::filesystem::directory_iterator it(verifyDocFolder, ec); !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
        if (!it->is_regular_file()) {
            continue;
        }

        if (!isAllowedVerificationExtension(it->path().extension().string())) {
            continue;
        }

        eligibleFiles.push_back(it->path());
    }

    if (ec) {
        outReason = "Unable to read folder: " + verifyDocFolder.string();
        return false;
    }

    std::sort(eligibleFiles.begin(), eligibleFiles.end());

    if (eligibleFiles.empty()) {
        outReason = "No eligible file found in " + verifyDocFolder.string() + " (.pdf/.docx/.csv/.txt).";
        return false;
    }

    if (eligibleFiles.size() > 1) {
        outReason = "Multiple files found in " + verifyDocFolder.string() + ". Keep exactly one file.";
        return false;
    }

    outFilePath = eligibleFiles[0].string();
    return true;
}

std::string shortHash(const std::string& value) {
    if (value.size() <= 10) {
        return value;
    }

    return value.substr(0, 10) + "...";
}

bool loadDocumentsForVerification(std::vector<VerificationDocument>& docs) {
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

        const VerificationDocument doc = parseVerificationDocument(splitPipe(line));
        if (!doc.docId.empty()) {
            docs.push_back(doc);
        }
    }

    return true;
}

bool findDocumentById(const std::vector<VerificationDocument>& docs, const std::string& docId, VerificationDocument& out) {
    const std::string needle = toLowerCopy(docId);
    for (std::size_t i = 0; i < docs.size(); ++i) {
        if (toLowerCopy(docs[i].docId) == needle) {
            out = docs[i];
            return true;
        }
    }
    return false;
}

std::string computeVerificationHash(const VerificationDocument& doc) {
    if (!doc.filePath.empty() && std::filesystem::exists(doc.filePath)) {
        const std::string fileHash = computeFileHashSha256(doc.filePath);
        if (!fileHash.empty()) {
            return fileHash;
        }
    }

    return computeSimpleHash(buildDocumentMetadataHashSource(doc));
}

bool actionContainsHash(const std::string& action, const std::string& hashValue) {
    return toLowerCopy(action).find(toLowerCopy(hashValue)) != std::string::npos;
}

bool loadVerificationHints(std::vector<VerificationHint>& hints) {
    std::vector<VerificationDocument> docs;
    if (!loadDocumentsForVerification(docs)) {
        return false;
    }

    hints.clear();
    for (std::size_t i = 0; i < docs.size(); ++i) {
        VerificationHint hint;
        hint.docId = docs[i].docId;
        hint.title = docs[i].title;
        hint.dateUploaded = docs[i].dateUploaded;
        hint.status = docs[i].status;
        hints.push_back(hint);
    }

    return true;
}

void printVerificationHints(const std::vector<VerificationHint>& hints, std::size_t maxRows) {
    if (hints.empty()) {
        return;
    }

    std::vector<VerificationHint> recent = hints;
    std::sort(recent.begin(), recent.end(), [](const VerificationHint& a, const VerificationHint& b) {
        if (a.dateUploaded != b.dateUploaded) {
            return a.dateUploaded > b.dateUploaded;
        }
        return a.docId > b.docId;
    });

    if (recent.size() > maxRows) {
        recent.resize(maxRows);
    }

    std::cout << "\n" << ui::bold("Recent/Available Documents") << "\n";
    const std::vector<std::string> headers = {"Document ID", "Date", "Status", "Title"};
    const std::vector<int> widths = {12, 10, 16, 24};
    ui::printTableHeader(headers, widths);

    for (std::size_t i = 0; i < recent.size(); ++i) {
        ui::printTableRow({recent[i].docId, recent[i].dateUploaded, recent[i].status, recent[i].title}, widths);
    }

    ui::printTableFooter(widths);
}
} // namespace

std::string computeSimpleHash(const std::string& text) {
    // SHA-256 implementation in procedural style for deterministic integrity checks.
    const std::array<std::uint32_t, 64> k = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    auto rotr32 = [](std::uint32_t value, std::uint32_t bits) -> std::uint32_t {
        return (value >> bits) | (value << (32 - bits));
    };

    std::vector<std::uint8_t> data(text.begin(), text.end());
    const std::uint64_t bitLen = static_cast<std::uint64_t>(data.size()) * 8ULL;

    data.push_back(0x80);
    while ((data.size() % 64) != 56) {
        data.push_back(0x00);
    }

    for (int i = 7; i >= 0; --i) {
        data.push_back(static_cast<std::uint8_t>((bitLen >> (i * 8)) & 0xFF));
    }

    std::uint32_t h0 = 0x6a09e667;
    std::uint32_t h1 = 0xbb67ae85;
    std::uint32_t h2 = 0x3c6ef372;
    std::uint32_t h3 = 0xa54ff53a;
    std::uint32_t h4 = 0x510e527f;
    std::uint32_t h5 = 0x9b05688c;
    std::uint32_t h6 = 0x1f83d9ab;
    std::uint32_t h7 = 0x5be0cd19;

    for (std::size_t offset = 0; offset < data.size(); offset += 64) {
        std::uint32_t w[64] = {0};

        for (int i = 0; i < 16; ++i) {
            const std::size_t base = offset + static_cast<std::size_t>(i) * 4;
            w[i] = (static_cast<std::uint32_t>(data[base]) << 24) |
                   (static_cast<std::uint32_t>(data[base + 1]) << 16) |
                   (static_cast<std::uint32_t>(data[base + 2]) << 8) |
                   static_cast<std::uint32_t>(data[base + 3]);
        }

        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        std::uint32_t a = h0;
        std::uint32_t b = h1;
        std::uint32_t c = h2;
        std::uint32_t d = h3;
        std::uint32_t e = h4;
        std::uint32_t f = h5;
        std::uint32_t g = h6;
        std::uint32_t h = h7;

        for (int i = 0; i < 64; ++i) {
            const std::uint32_t S1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
            const std::uint32_t ch = (e & f) ^ ((~e) & g);
            const std::uint32_t temp1 = h + S1 + ch + k[i] + w[i];
            const std::uint32_t S0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = S0 + maj;

            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
        h5 += f;
        h6 += g;
        h7 += h;
    }

    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::nouppercase
        << std::setw(8) << h0
        << std::setw(8) << h1
        << std::setw(8) << h2
        << std::setw(8) << h3
        << std::setw(8) << h4
        << std::setw(8) << h5
        << std::setw(8) << h6
        << std::setw(8) << h7;

    return out.str();
}

std::string computeFileHashSha256(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return computeSimpleHash(buffer.str());
}

bool verifyDocumentHashAgainstBlockchain(const std::string& hashValue, const std::string& docId) {
    for (int node = 1; node <= BLOCKCHAIN_NODE_COUNT; ++node) {
        std::ifstream file;
        if (!openBlockchainNodeInput(file, node)) {
            continue;
        }

        std::string line;
        std::getline(file, line); // header

        while (std::getline(file, line)) {
            if (line.empty()) {
                continue;
            }

            const std::vector<std::string> tokens = splitPipe(line);
            if (tokens.size() < 7) {
                continue;
            }

            const std::string action = tokens[2];
            const std::string blockTarget = tokens[3];

            if (actionContainsHash(action, hashValue)) {
                return true;
            }

            if (blockTarget == docId && toLowerCopy(action).find("upload") != std::string::npos) {
                return true;
            }
        }
    }

    return false;
}

void verifyDocumentIntegrity(const std::string& actor) {
    // Verification flow scans documents linearly until target ID is found,
    // then compares stored hash vs recomputed hash and logs the outcome.
    // Worst-case complexity is O(n) over document rows.
    clearScreen();
    ui::printSectionTitle("DOCUMENT INTEGRITY VERIFICATION (ADMIN)");

    std::vector<VerificationHint> hints;
    if (loadVerificationHints(hints)) {
        printVerificationHints(hints, 8);
        std::cout << "  " << ui::muted("Tip: use one of the listed Document IDs for quick verification.") << "\n";
    }

    clearInputBuffer();
    std::string targetDocId;
    std::cout << "Enter Document ID to verify: ";
    std::getline(std::cin, targetDocId);

    if (targetDocId.empty()) {
        std::cout << ui::error("[!] Document ID is required.") << "\n";
        logAuditAction("VERIFY_DOC_INPUT_ERROR", "N/A", actor);
        waitForEnter();
        return;
    }

    std::vector<VerificationDocument> docs;
    if (!loadDocumentsForVerification(docs)) {
        std::cout << ui::error("[!] Unable to open documents file.") << "\n";
        logAuditAction("VERIFY_DOC_FAILED", targetDocId, actor);
        waitForEnter();
        return;
    }

    VerificationDocument doc;
    const bool found = findDocumentById(docs, targetDocId, doc);

    if (found) {
        std::cout << "\n" << ui::bold("Verification Pipeline") << "\n";
        std::cout << "  Checking official document record...\n";
        const std::string computedHash = computeVerificationHash(doc);
        std::cout << "  Generating SHA-256 hash...\n";
        const bool blockchainMatch = verifyDocumentHashAgainstBlockchain(computedHash, doc.docId);
        std::cout << "  Validating blockchain registration...\n";

        const std::vector<std::string> headers = {"Field", "Value"};
        const std::vector<int> widths = {18, 60};
        ui::printTableHeader(headers, widths);
        ui::printTableRow({"Document ID", doc.docId}, widths);
        ui::printTableRow({"Stored Hash", doc.hashValue}, widths);
        ui::printTableRow({"Computed Hash", computedHash}, widths);
        ui::printTableRow({"Blockchain Match", blockchainMatch ? "FOUND" : "NOT FOUND"}, widths);
        ui::printTableRow({"Algorithm", "SHA-256"}, widths);
        ui::printTableFooter(widths);

        std::cout << "\n" << ui::bold("====================================") << "\n";
        std::cout << ui::bold("RESULT") << ": ";
        if (doc.hashValue == computedHash && blockchainMatch) {
            std::cout << ui::success("[VERIFIED]") << "\n";
            std::cout << ui::success("This document matches its official stored hash and on-chain record.") << "\n";
            logAuditAction("VERIFY_DOC_VALID", doc.docId, actor);
        } else {
            std::cout << ui::error("[TAMPERED]") << "\n";
            std::cout << ui::error("Hash mismatch or chain mismatch detected. Investigate possible alteration.") << "\n";
            logAuditAction("VERIFY_DOC_TAMPERED", doc.docId, actor);
        }
        std::cout << ui::bold("====================================") << "\n";
    }

    if (!found) {
        std::cout << "\n" << ui::error("[!] Document ID not found.") << "\n";
        logAuditAction("VERIFY_DOC_NOT_FOUND", targetDocId, actor);
    }

    waitForEnter();
}

void verifyPublishedDocumentAsCitizen(const std::string& actor) {
    clearScreen();
    ui::printSectionTitle("VERIFY PUBLISHED DOCUMENT (CITIZEN)");

    std::vector<VerificationDocument> docs;
    if (!loadDocumentsForVerification(docs)) {
        std::cout << ui::error("[!] Unable to open documents file.") << "\n";
        logAuditAction("VERIFY_DOC_FAILED", "N/A", actor);
        waitForEnter();
        return;
    }

    std::vector<VerificationHint> hints;
    for (std::size_t i = 0; i < docs.size(); ++i) {
        if (toLowerCopy(docs[i].status) != "published") {
            continue;
        }

        VerificationHint hint;
        hint.docId = docs[i].docId;
        hint.dateUploaded = docs[i].dateUploaded;
        hint.status = docs[i].status;
        hint.title = docs[i].title;
        hints.push_back(hint);
    }

    if (hints.empty()) {
        std::cout << ui::warning("[!] No published documents are available for verification.") << "\n";
        logAuditAction("VERIFY_DOC_NOT_FOUND", "NO_PUBLISHED", actor);
        waitForEnter();
        return;
    }

    printVerificationHints(hints, 10);
    std::cout << "\n" << ui::bold("Verification Input Requirement") << "\n";
    std::cout << "  1) Enter the Published Document ID below.\n";
    std::cout << "  2) Place exactly one candidate file in: "
              << ui::muted("data/verify/<DocumentID>/") << "\n";
    std::cout << "  3) Allowed formats: .pdf .docx .csv .txt\n";
    clearInputBuffer();

    std::string targetDocId;
    std::cout << "Enter Published Document ID: ";
    std::getline(std::cin, targetDocId);

    if (targetDocId.empty()) {
        std::cout << ui::error("[!] Document ID is required.") << "\n";
        logAuditAction("VERIFY_DOC_INPUT_ERROR", "N/A", actor);
        waitForEnter();
        return;
    }

    VerificationDocument doc;
    if (!findDocumentById(docs, targetDocId, doc) || toLowerCopy(doc.status) != "published") {
        std::cout << ui::error("[!] Published document not found.") << "\n";
        logAuditAction("VERIFY_DOC_NOT_FOUND", targetDocId, actor);
        waitForEnter();
        return;
    }

    std::string verifyCandidatePath;
    std::string verifyLookupReason;
    const bool hasVerifyCandidate = resolveCandidateVerifyFilePath(doc.docId, verifyCandidatePath, verifyLookupReason);

    std::cout << "\n" << ui::bold("Verification Pipeline") << "\n";
    std::cout << "  Checking verify folder...\n";
    if (hasVerifyCandidate) {
        std::cout << "  " << ui::success("[OK]") << " Candidate file found: " << verifyCandidatePath << "\n";
    } else {
        std::cout << "  " << ui::warning("[WARN]") << " " << verifyLookupReason << "\n";
    }

    std::string computedHash;
    std::string hashSource;
    if (hasVerifyCandidate) {
        std::cout << "  Generating SHA-256 hash from verify file...\n";
        computedHash = computeFileHashSha256(verifyCandidatePath);
        hashSource = "VERIFY_FOLDER_FILE";
    } else {
        std::cout << "  Generating SHA-256 hash from stored record fallback...\n";
        computedHash = computeVerificationHash(doc);
        hashSource = "STORED_RECORD_FALLBACK";
    }

    if (computedHash.empty()) {
        std::cout << ui::error("[!] Unable to compute hash for verification.") << "\n";
        logAuditAction("VERIFY_DOC_FAILED", doc.docId, actor);
        waitForEnter();
        return;
    }

    std::cout << "  Comparing hash with official document record...\n";
    const bool hashMatch = (computedHash == doc.hashValue);
    std::cout << "  Validating blockchain registration...\n";
    const bool blockchainMatch = verifyDocumentHashAgainstBlockchain(doc.hashValue, doc.docId);

    const std::vector<std::string> headers = {"Field", "Value"};
    const std::vector<int> widths = {18, 60};
    ui::printTableHeader(headers, widths);
    ui::printTableRow({"Document ID", doc.docId}, widths);
    ui::printTableRow({"Title", doc.title}, widths);
    ui::printTableRow({"Stored Hash", doc.hashValue}, widths);
    ui::printTableRow({"Computed Hash", computedHash}, widths);
    ui::printTableRow({"Hash Source", hashSource}, widths);
    ui::printTableRow({"Hash Match", hashMatch ? "YES" : "NO"}, widths);
    ui::printTableRow({"Found on Blockchain", blockchainMatch ? "YES" : "NO"}, widths);
    ui::printTableRow({"Verify Folder File", hasVerifyCandidate ? verifyCandidatePath : "(not found)"}, widths);
    ui::printTableFooter(widths);

    std::cout << "\n" << ui::bold("====================================") << "\n";
    std::cout << ui::bold("RESULT") << ": ";
    if (hashMatch && blockchainMatch && hasVerifyCandidate) {
        std::cout << ui::success("[VERIFIED]") << "\n";
        std::cout << ui::success("This document matches the official record and blockchain registration.") << "\n";
        logAuditAction("VERIFY_DOC_VALID", doc.docId, actor);
    } else if (!hashMatch) {
        std::cout << ui::error("[TAMPERED]") << "\n";
        std::cout << ui::error("Hash mismatch detected. The submitted file may have been altered.") << "\n";
        logAuditAction("VERIFY_DOC_TAMPERED", doc.docId, actor);
    } else if (!hasVerifyCandidate) {
        std::cout << ui::warning("[INCOMPLETE]") << "\n";
        std::cout << ui::warning("No valid candidate file was found in verify folder. Record-only check was shown.") << "\n";
        logAuditAction("VERIFY_DOC_FILE_MISSING", doc.docId, actor);
    } else {
        std::cout << ui::warning("[PARTIAL]") << "\n";
        std::cout << ui::warning("Hash matches official record, but blockchain registration was not found.") << "\n";
        logAuditAction("VERIFY_DOC_CHAIN_MISSING", doc.docId, actor);
    }
    std::cout << ui::bold("====================================") << "\n";

    std::cout << "\n" << ui::muted("Trace: Stored=") << shortHash(doc.hashValue)
              << ui::muted(" | Computed=") << shortHash(computedHash) << "\n";

    waitForEnter();
}
