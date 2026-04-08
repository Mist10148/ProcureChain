#include "../include/blockchain.h"

#include "../include/audit.h"
#include "../include/auth.h"
#include "../include/ui.h"
#include "../include/verification.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace {
const std::string NODE1_PATH = "data/blockchain/node1_chain.txt";
const std::string NODE2_PATH = "data/blockchain/node2_chain.txt";
const std::string NODE3_PATH = "data/blockchain/node3_chain.txt";
const std::string NODE1_FALLBACK = "../data/blockchain/node1_chain.txt";
const std::string NODE2_FALLBACK = "../data/blockchain/node2_chain.txt";
const std::string NODE3_FALLBACK = "../data/blockchain/node3_chain.txt";

bool ensureFileWithHeader(const std::string& primaryPath, const std::string& fallbackPath, const std::string& headerLine) {
    std::ifstream primary(primaryPath);
    if (primary.is_open()) {
        return true;
    }

    std::ifstream fallback(fallbackPath);
    if (fallback.is_open()) {
        return true;
    }

    std::ofstream createPrimary(primaryPath);
    if (createPrimary.is_open()) {
        createPrimary << headerLine << '\n';
        return true;
    }

    std::ofstream createFallback(fallbackPath);
    if (createFallback.is_open()) {
        createFallback << headerLine << '\n';
        return true;
    }

    return false;
}

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

bool parseBlockLine(
    const std::string& line,
    std::string& index,
    std::string& timestamp,
    std::string& action,
    std::string& documentId,
    std::string& actor,
    std::string& previousHash,
    std::string& currentHash
) {
    std::stringstream parser(line);
    std::getline(parser, index, '|');
    std::getline(parser, timestamp, '|');
    std::getline(parser, action, '|');
    std::getline(parser, documentId, '|');
    std::getline(parser, actor, '|');
    std::getline(parser, previousHash, '|');
    std::getline(parser, currentHash, '|');

    return !index.empty() && !timestamp.empty() && !action.empty() &&
           !documentId.empty() && !actor.empty() && !previousHash.empty() && !currentHash.empty();
}

bool validateSingleChain(const std::string& path, const std::string& fallback) {
    std::ifstream file;
    if (!openInputFileWithFallback(file, path, fallback)) {
        return false;
    }

    std::string line;
    std::getline(file, line); // Skip header.

    std::string expectedPrev = "0000";

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        std::string index;
        std::string timestamp;
        std::string action;
        std::string documentId;
        std::string actor;
        std::string previousHash;
        std::string currentHash;

        if (!parseBlockLine(line, index, timestamp, action, documentId, actor, previousHash, currentHash)) {
            return false;
        }

        if (previousHash != expectedPrev) {
            return false;
        }

        std::string source = index + "|" + timestamp + "|" + action + "|" + documentId + "|" + actor + "|" + previousHash;
        if (computeSimpleHash(source) != currentHash) {
            return false;
        }

        expectedPrev = currentHash;
    }

    return true;
}

std::string readFullFileWithFallback(const std::string& primaryPath, const std::string& fallbackPath) {
    std::ifstream file;
    if (!openInputFileWithFallback(file, primaryPath, fallbackPath)) {
        return "";
    }

    std::ostringstream out;
    out << file.rdbuf();
    return out.str();
}

bool hasDataRows(const std::string& primaryPath, const std::string& fallbackPath) {
    std::ifstream file;
    if (!openInputFileWithFallback(file, primaryPath, fallbackPath)) {
        return false;
    }

    std::string line;
    std::getline(file, line); // Skip header.
    while (std::getline(file, line)) {
        if (!line.empty()) {
            return true;
        }
    }

    return false;
}

void seedBlockchainIfAllEmpty() {
    if (hasDataRows(NODE1_PATH, NODE1_FALLBACK) ||
        hasDataRows(NODE2_PATH, NODE2_FALLBACK) ||
        hasDataRows(NODE3_PATH, NODE3_FALLBACK)) {
        return;
    }

    std::string prevHash = "0000";

    const std::string source1 = "1|2026-04-09 09:00:00|UPLOAD|CA003|proc_admin|" + prevHash;
    const std::string hash1 = computeSimpleHash(source1);
    const std::string row1 = source1 + "|" + hash1;

    prevHash = hash1;
    const std::string source2 = "2|2026-04-09 09:05:00|APPROVE|CA003|budget_admin|" + prevHash;
    const std::string hash2 = computeSimpleHash(source2);
    const std::string row2 = source2 + "|" + hash2;

    std::ofstream node1;
    std::ofstream node2;
    std::ofstream node3;

    if (!openAppendFileWithFallback(node1, NODE1_PATH, NODE1_FALLBACK)) {
        return;
    }
    if (!openAppendFileWithFallback(node2, NODE2_PATH, NODE2_FALLBACK)) {
        return;
    }
    if (!openAppendFileWithFallback(node3, NODE3_PATH, NODE3_FALLBACK)) {
        return;
    }

    node1 << row1 << '\n' << row2 << '\n';
    node2 << row1 << '\n' << row2 << '\n';
    node3 << row1 << '\n' << row2 << '\n';

    node1.flush();
    node2.flush();
    node3.flush();
}
} // namespace

void ensureBlockchainNodeFilesExist() {
    const std::string header = "index|timestamp|action|documentID|actor|previousHash|currentHash";
    ensureFileWithHeader(NODE1_PATH, NODE1_FALLBACK, header);
    ensureFileWithHeader(NODE2_PATH, NODE2_FALLBACK, header);
    ensureFileWithHeader(NODE3_PATH, NODE3_FALLBACK, header);
    seedBlockchainIfAllEmpty();
}

void appendBlockchainAction(const std::string& action, const std::string& docId, const std::string& actor) {
    ensureBlockchainNodeFilesExist();

    std::ifstream nodeReader;
    if (!openInputFileWithFallback(nodeReader, NODE1_PATH, NODE1_FALLBACK)) {
        return;
    }

    std::string line;
    std::getline(nodeReader, line); // Skip header.

    int nextIndex = 1;
    std::string previousHash = "0000";

    while (std::getline(nodeReader, line)) {
        if (line.empty()) {
            continue;
        }

        std::string index;
        std::string timestamp;
        std::string rowAction;
        std::string documentId;
        std::string rowActor;
        std::string rowPreviousHash;
        std::string currentHash;

        if (!parseBlockLine(line, index, timestamp, rowAction, documentId, rowActor, rowPreviousHash, currentHash)) {
            continue;
        }

        nextIndex = std::atoi(index.c_str()) + 1;
        previousHash = currentHash;
    }

    std::string indexText = std::to_string(nextIndex);
    std::string timestamp = getCurrentTimestamp();
    std::string source = indexText + "|" + timestamp + "|" + action + "|" + docId + "|" + actor + "|" + previousHash;
    std::string currentHash = computeSimpleHash(source);

    const std::string row = source + "|" + currentHash;

    std::ofstream node1;
    std::ofstream node2;
    std::ofstream node3;

    if (!openAppendFileWithFallback(node1, NODE1_PATH, NODE1_FALLBACK)) {
        return;
    }
    if (!openAppendFileWithFallback(node2, NODE2_PATH, NODE2_FALLBACK)) {
        return;
    }
    if (!openAppendFileWithFallback(node3, NODE3_PATH, NODE3_FALLBACK)) {
        return;
    }

    // Write the same block to all three simulated node files.
    node1 << row << '\n';
    node2 << row << '\n';
    node3 << row << '\n';
    node1.flush();
    node2.flush();
    node3.flush();
}

void validateBlockchainNodes(const std::string& actor) {
    clearScreen();
    ui::printSectionTitle("BLOCKCHAIN VALIDATION (SIMULATED)");

    ensureBlockchainNodeFilesExist();

    bool node1Valid = validateSingleChain(NODE1_PATH, NODE1_FALLBACK);
    bool node2Valid = validateSingleChain(NODE2_PATH, NODE2_FALLBACK);
    bool node3Valid = validateSingleChain(NODE3_PATH, NODE3_FALLBACK);

    std::string node1Data = readFullFileWithFallback(NODE1_PATH, NODE1_FALLBACK);
    std::string node2Data = readFullFileWithFallback(NODE2_PATH, NODE2_FALLBACK);
    std::string node3Data = readFullFileWithFallback(NODE3_PATH, NODE3_FALLBACK);

    bool sameContent = !node1Data.empty() && node1Data == node2Data && node2Data == node3Data;
    bool allValid = node1Valid && node2Valid && node3Valid && sameContent;

    const std::vector<std::string> headers = {"Validation Check", "Result"};
    const std::vector<int> widths = {30, 10};

    ui::printTableHeader(headers, widths);
    ui::printTableRow({"Node 1 chain integrity", node1Valid ? "PASS" : "FAIL"}, widths);
    ui::printTableRow({"Node 2 chain integrity", node2Valid ? "PASS" : "FAIL"}, widths);
    ui::printTableRow({"Node 3 chain integrity", node3Valid ? "PASS" : "FAIL"}, widths);
    ui::printTableRow({"Node content consistency", sameContent ? "PASS" : "FAIL"}, widths);
    ui::printTableFooter(widths);

    if (allValid) {
        std::cout << "\n" << ui::success("[+] Blockchain validation passed.") << "\n";
        logAuditAction("BLOCKCHAIN_VALIDATE_OK", "MULTI", actor);
    } else {
        std::cout << "\n" << ui::error("[!] Blockchain validation failed (inconsistency or tamper detected).") << "\n";
        logAuditAction("BLOCKCHAIN_VALIDATE_FAIL", "MULTI", actor);
    }

    waitForEnter();
}
