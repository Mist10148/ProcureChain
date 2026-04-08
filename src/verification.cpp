#include "../include/verification.h"

#include "../include/audit.h"
#include "../include/auth.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

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

void clearInputBuffer() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}
} // namespace

std::string computeSimpleHash(const std::string& text) {
    // This is intentionally non-cryptographic and used only for classroom simulation.
    unsigned long long hash = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        hash = (hash * 131ULL + static_cast<unsigned long long>(text[i])) % 0xFFFFFFFFULL;
    }

    std::ostringstream out;
    out << std::uppercase << std::hex << hash;
    return out.str();
}

void verifyDocumentIntegrity(const std::string& actor) {
    clearScreen();
    std::cout << "\n==============================================================\n";
    std::cout << "  DOCUMENT INTEGRITY VERIFICATION\n";
    std::cout << "==============================================================\n";

    clearInputBuffer();
    std::string targetDocId;
    std::cout << "Enter Document ID to verify: ";
    std::getline(std::cin, targetDocId);

    if (targetDocId.empty()) {
        std::cout << "[!] Document ID is required.\n";
        logAuditAction("VERIFY_DOC_INPUT_ERROR", "N/A", actor);
        waitForEnter();
        return;
    }

    std::ifstream file;
    if (!openInputFileWithFallback(file, DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK)) {
        std::cout << "[!] Unable to open documents file.\n";
        logAuditAction("VERIFY_DOC_FAILED", targetDocId, actor);
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

        if (docId != targetDocId) {
            continue;
        }

        found = true;

        // Rebuild the same source fields used when the row hash was generated.
        std::string source = docId + "|" + title + "|" + category + "|" + department + "|" +
                             dateUploaded + "|" + uploader + "|" + status;
        std::string computedHash = computeSimpleHash(source);

        std::cout << "\nDocument ID  : " << docId << '\n';
        std::cout << "Stored Hash  : " << hashValue << '\n';
        std::cout << "Computed Hash: " << computedHash << '\n';
        std::cout << "Note         : Simple classroom hash (not cryptographic).\n";

        if (hashValue == computedHash) {
            std::cout << "[+] Verification Result: VALID\n";
            logAuditAction("VERIFY_DOC_VALID", docId, actor);
        } else {
            std::cout << "[!] Verification Result: POTENTIALLY TAMPERED\n";
            logAuditAction("VERIFY_DOC_TAMPERED", docId, actor);
        }
        break;
    }

    if (!found) {
        std::cout << "\n[!] Document ID not found.\n";
        logAuditAction("VERIFY_DOC_NOT_FOUND", targetDocId, actor);
    }

    waitForEnter();
}
