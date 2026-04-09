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
// This module simulates a replicated blockchain by writing identical rows into
// multiple node files and validating hash/previous-hash continuity per node.
struct NodePath {
    std::string primaryPath;
    std::string fallbackPath;
};

const std::vector<NodePath> NODE_PATHS = {
    {"data/blockchain/node1_chain.txt", "../data/blockchain/node1_chain.txt"},
    {"data/blockchain/node2_chain.txt", "../data/blockchain/node2_chain.txt"},
    {"data/blockchain/node3_chain.txt", "../data/blockchain/node3_chain.txt"},
    {"data/blockchain/node4_chain.txt", "../data/blockchain/node4_chain.txt"},
    {"data/blockchain/node5_chain.txt", "../data/blockchain/node5_chain.txt"}
};

bool ensureFileWithHeader(const std::string& primaryPath, const std::string& fallbackPath, const std::string& headerLine) {
    // Idempotent file bootstrap for each blockchain node file.
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
    // Path fallback keeps node access robust across launch directories.
    file.open(primaryPath);
    if (file.is_open()) {
        return true;
    }

    file.clear();
    file.open(fallbackPath);
    return file.is_open();
}

bool openAppendFileWithFallback(std::ofstream& file, const std::string& primaryPath, const std::string& fallbackPath) {
    // Append mode preserves existing chain rows in each node file.
    file.open(primaryPath, std::ios::app);
    if (file.is_open()) {
        return true;
    }

    file.clear();
    file.open(fallbackPath, std::ios::app);
    return file.is_open();
}

bool openWriteFileWithFallback(std::ofstream& file, const std::string& primaryPath, const std::string& fallbackPath) {
    // Truncate mode is used only when synchronizing empty nodes from reference.
    file.open(primaryPath, std::ios::out | std::ios::trunc);
    if (file.is_open()) {
        return true;
    }

    file.clear();
    file.open(fallbackPath, std::ios::out | std::ios::trunc);
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
    // Parses one block row according to fixed schema order.
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
    // Validates one node by scanning all rows once: O(n).
    // Checks both linkage (previousHash) and content hash recomputation.
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
    // Returns full node content for cross-node consistency comparison.
    std::ifstream file;
    if (!openInputFileWithFallback(file, primaryPath, fallbackPath)) {
        return "";
    }

    std::ostringstream out;
    out << file.rdbuf();
    return out.str();
}

bool hasDataRows(const std::string& primaryPath, const std::string& fallbackPath) {
    // True when at least one non-header row exists in the node file.
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
    // Writes deterministic sample blocks only when every node is empty.
    for (std::size_t i = 0; i < NODE_PATHS.size(); ++i) {
        if (hasDataRows(NODE_PATHS[i].primaryPath, NODE_PATHS[i].fallbackPath)) {
            return;
        }
    }

    std::string prevHash = "0000";

    const std::string source1 = "1|2026-04-09 09:00:00|UPLOAD|CA003|proc_admin|" + prevHash;
    const std::string hash1 = computeSimpleHash(source1);
    const std::string row1 = source1 + "|" + hash1;

    prevHash = hash1;
    const std::string source2 = "2|2026-04-09 09:05:00|APPROVE|CA003|budget_admin|" + prevHash;
    const std::string hash2 = computeSimpleHash(source2);
    const std::string row2 = source2 + "|" + hash2;

    for (std::size_t i = 0; i < NODE_PATHS.size(); ++i) {
        std::ofstream nodeWriter;
        if (!openAppendFileWithFallback(nodeWriter, NODE_PATHS[i].primaryPath, NODE_PATHS[i].fallbackPath)) {
            return;
        }

        nodeWriter << row1 << '\n' << row2 << '\n';
        nodeWriter.flush();
    }
}

void synchronizeEmptyNodesFromReference() {
    // Copies the first non-empty node into any empty node so replication starts
    // from a consistent baseline before live writes happen.
    std::string referenceData;

    for (std::size_t i = 0; i < NODE_PATHS.size(); ++i) {
        if (!hasDataRows(NODE_PATHS[i].primaryPath, NODE_PATHS[i].fallbackPath)) {
            continue;
        }

        referenceData = readFullFileWithFallback(NODE_PATHS[i].primaryPath, NODE_PATHS[i].fallbackPath);
        if (!referenceData.empty()) {
            break;
        }
    }

    if (referenceData.empty()) {
        return;
    }

    for (std::size_t i = 0; i < NODE_PATHS.size(); ++i) {
        if (hasDataRows(NODE_PATHS[i].primaryPath, NODE_PATHS[i].fallbackPath)) {
            continue;
        }

        std::ofstream nodeWriter;
        if (!openWriteFileWithFallback(nodeWriter, NODE_PATHS[i].primaryPath, NODE_PATHS[i].fallbackPath)) {
            continue;
        }

        nodeWriter << referenceData;
        nodeWriter.flush();
    }
}
} // namespace

void ensureBlockchainNodeFilesExist() {
    // Startup integrity bootstrap for all simulated nodes.
    const std::string header = "index|timestamp|action|documentID|actor|previousHash|currentHash";
    for (std::size_t i = 0; i < NODE_PATHS.size(); ++i) {
        ensureFileWithHeader(NODE_PATHS[i].primaryPath, NODE_PATHS[i].fallbackPath, header);
    }
    seedBlockchainIfAllEmpty();
    synchronizeEmptyNodesFromReference();
}

int appendBlockchainAction(const std::string& action, const std::string& docId, const std::string& actor) {
    // Append pipeline:
    // 1) read latest index/hash from node 1,
    // 2) build next block,
    // 3) append identical block to all nodes.
    // Complexity is O(n + k), n=rows in chain, k=node count.
    ensureBlockchainNodeFilesExist();

    std::ifstream nodeReader;
    if (!openInputFileWithFallback(nodeReader, NODE_PATHS[0].primaryPath, NODE_PATHS[0].fallbackPath)) {
        return -1;
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

    std::vector<std::ofstream> nodeWriters;
    nodeWriters.reserve(NODE_PATHS.size());

    for (std::size_t i = 0; i < NODE_PATHS.size(); ++i) {
        std::ofstream nodeWriter;
        if (!openAppendFileWithFallback(nodeWriter, NODE_PATHS[i].primaryPath, NODE_PATHS[i].fallbackPath)) {
            return -1;
        }

        nodeWriters.push_back(std::move(nodeWriter));
    }

    // Write the same block to all simulated node files.
    for (std::size_t i = 0; i < nodeWriters.size(); ++i) {
        nodeWriters[i] << row << '\n';
        nodeWriters[i].flush();
    }

    return nextIndex;
}

void validateBlockchainNodes(const std::string& actor) {
    // Performs per-node validation plus cross-node byte-level consistency check.
    // Overall complexity: O(k * n), k=node count, n=rows per node.
    clearScreen();
    ui::printSectionTitle("BLOCKCHAIN VALIDATION (SIMULATED)");

    ensureBlockchainNodeFilesExist();

    std::vector<bool> nodeValidFlags;
    nodeValidFlags.reserve(NODE_PATHS.size());

    std::vector<std::string> nodeData;
    nodeData.reserve(NODE_PATHS.size());

    for (std::size_t i = 0; i < NODE_PATHS.size(); ++i) {
        nodeValidFlags.push_back(validateSingleChain(NODE_PATHS[i].primaryPath, NODE_PATHS[i].fallbackPath));
        nodeData.push_back(readFullFileWithFallback(NODE_PATHS[i].primaryPath, NODE_PATHS[i].fallbackPath));
    }

    bool sameContent = !nodeData.empty() && !nodeData[0].empty();
    for (std::size_t i = 1; i < nodeData.size() && sameContent; ++i) {
        if (nodeData[i] != nodeData[0]) {
            sameContent = false;
        }
    }

    bool allValid = sameContent;
    for (std::size_t i = 0; i < nodeValidFlags.size(); ++i) {
        allValid = allValid && nodeValidFlags[i];
    }

    const std::vector<std::string> headers = {"Validation Check", "Result"};
    const std::vector<int> widths = {30, 10};

    ui::printTableHeader(headers, widths);
    for (std::size_t i = 0; i < nodeValidFlags.size(); ++i) {
        const std::string label = "Node " + std::to_string(i + 1) + " chain integrity";
        ui::printTableRow({label, nodeValidFlags[i] ? "PASS" : "FAIL"}, widths);
    }
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
