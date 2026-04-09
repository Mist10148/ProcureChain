#include "../include/blockchain.h"

#include "../include/audit.h"
#include "../include/auth.h"
#include "../include/ui.h"
#include "../include/verification.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

namespace {
// This module simulates a replicated blockchain by writing identical rows into
// multiple node files and validating hash/previous-hash continuity per node.
struct NodePath {
    std::string primaryPath;
    std::string fallbackPath;
};

struct BlockRow {
    std::string index;
    std::string timestamp;
    std::string action;
    std::string documentId;
    std::string actor;
    std::string previousHash;
    std::string currentHash;
};

struct NodeValidationResult {
    bool integrityOk;
    std::string reason;
    std::string failedIndex;
    std::vector<BlockRow> rows;
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

bool parseBlockLine(const std::string& line, BlockRow& row) {
    // Parses one block row according to fixed schema order.
    std::stringstream parser(line);
    std::getline(parser, row.index, '|');
    std::getline(parser, row.timestamp, '|');
    std::getline(parser, row.action, '|');
    std::getline(parser, row.documentId, '|');
    std::getline(parser, row.actor, '|');
    std::getline(parser, row.previousHash, '|');
    std::getline(parser, row.currentHash, '|');

    return !row.index.empty() && !row.timestamp.empty() && !row.action.empty() &&
           !row.documentId.empty() && !row.actor.empty() && !row.previousHash.empty() && !row.currentHash.empty();
}

bool loadChainRows(const std::string& path, const std::string& fallback, std::vector<BlockRow>& rows) {
    std::ifstream file;
    if (!openInputFileWithFallback(file, path, fallback)) {
        return false;
    }

    rows.clear();

    std::string line;
    std::getline(file, line); // Skip header.

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        BlockRow row;
        if (!parseBlockLine(line, row)) {
            continue;
        }

        rows.push_back(row);
    }

    return true;
}

std::string serializeBlockWithoutCurrentHash(const BlockRow& row) {
    return row.index + "|" + row.timestamp + "|" + row.action + "|" + row.documentId + "|" + row.actor + "|" + row.previousHash;
}

NodeValidationResult validateSingleChainDetailed(const std::string& path, const std::string& fallback) {
    NodeValidationResult result;
    result.integrityOk = true;
    result.reason = "OK";

    if (!loadChainRows(path, fallback, result.rows)) {
        result.integrityOk = false;
        result.reason = "FILE_OPEN_FAILED";
        return result;
    }

    std::string expectedPrev = "0000";

    for (std::size_t i = 0; i < result.rows.size(); ++i) {
        if (result.rows[i].previousHash != expectedPrev) {
            result.integrityOk = false;
            result.reason = "PREVIOUS_HASH_MISMATCH";
            result.failedIndex = result.rows[i].index;
            return result;
        }

        const std::string source = serializeBlockWithoutCurrentHash(result.rows[i]);
        if (computeSimpleHash(source) != result.rows[i].currentHash) {
            result.integrityOk = false;
            result.reason = "CURRENT_HASH_MISMATCH";
            result.failedIndex = result.rows[i].index;
            return result;
        }

        expectedPrev = result.rows[i].currentHash;
    }

    return result;
}

bool validateSingleChain(const std::string& path, const std::string& fallback) {
    return validateSingleChainDetailed(path, fallback).integrityOk;
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

bool saveChainRows(const std::string& primaryPath, const std::string& fallbackPath, const std::vector<BlockRow>& rows) {
    std::ofstream writer;
    if (!openWriteFileWithFallback(writer, primaryPath, fallbackPath)) {
        return false;
    }

    writer << "index|timestamp|action|documentID|actor|previousHash|currentHash\n";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        writer << rows[i].index << '|'
               << rows[i].timestamp << '|'
               << rows[i].action << '|'
               << rows[i].documentId << '|'
               << rows[i].actor << '|'
               << rows[i].previousHash << '|'
               << rows[i].currentHash << '\n';
    }

    writer.flush();
    return true;
}

void migrateNodeChainToSha256(const NodePath& nodePath) {
    std::vector<BlockRow> rows;
    if (!loadChainRows(nodePath.primaryPath, nodePath.fallbackPath, rows)) {
        return;
    }

    std::string previousHash = "0000";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        rows[i].previousHash = previousHash;
        rows[i].currentHash = computeSimpleHash(serializeBlockWithoutCurrentHash(rows[i]));
        previousHash = rows[i].currentHash;
    }

    saveChainRows(nodePath.primaryPath, nodePath.fallbackPath, rows);
}

void migrateAllNodesToSha256() {
    for (std::size_t i = 0; i < NODE_PATHS.size(); ++i) {
        migrateNodeChainToSha256(NODE_PATHS[i]);
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
    migrateAllNodesToSha256();
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

        BlockRow row;
        if (!parseBlockLine(line, row)) {
            continue;
        }

        nextIndex = std::atoi(row.index.c_str()) + 1;
        previousHash = row.currentHash;
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

    std::vector<NodeValidationResult> nodeResults;
    nodeResults.reserve(NODE_PATHS.size());

    std::vector<std::string> nodeData;
    nodeData.reserve(NODE_PATHS.size());

    for (std::size_t i = 0; i < NODE_PATHS.size(); ++i) {
        nodeResults.push_back(validateSingleChainDetailed(NODE_PATHS[i].primaryPath, NODE_PATHS[i].fallbackPath));
        nodeData.push_back(readFullFileWithFallback(NODE_PATHS[i].primaryPath, NODE_PATHS[i].fallbackPath));
    }

    bool sameContent = !nodeData.empty() && !nodeData[0].empty();
    for (std::size_t i = 1; i < nodeData.size() && sameContent; ++i) {
        if (nodeData[i] != nodeData[0]) {
            sameContent = false;
        }
    }

    bool allValid = sameContent;
    for (std::size_t i = 0; i < nodeResults.size(); ++i) {
        allValid = allValid && nodeResults[i].integrityOk;
    }

    const std::vector<std::string> headers = {"Validation Check", "Result", "Detail"};
    const std::vector<int> widths = {28, 10, 26};

    ui::printTableHeader(headers, widths);
    for (std::size_t i = 0; i < nodeResults.size(); ++i) {
        const std::string label = "Node " + std::to_string(i + 1) + " chain integrity";
        std::string detail = nodeResults[i].integrityOk ? "OK" : nodeResults[i].reason;
        if (!nodeResults[i].failedIndex.empty()) {
            detail += " @index " + nodeResults[i].failedIndex;
        }
        ui::printTableRow({label, nodeResults[i].integrityOk ? "PASS" : "FAIL", detail}, widths);
    }
    ui::printTableRow({"Node content consistency", sameContent ? "PASS" : "FAIL", sameContent ? "all synchronized" : "divergence detected"}, widths);
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

void viewBlockchainExplorer(const std::string& actor) {
    clearScreen();
    ui::printSectionTitle("BLOCKCHAIN EXPLORER (5 NODE CONSENSUS)");

    ensureBlockchainNodeFilesExist();

    std::vector<NodeValidationResult> nodeResults;
    nodeResults.reserve(NODE_PATHS.size());

    for (std::size_t i = 0; i < NODE_PATHS.size(); ++i) {
        nodeResults.push_back(validateSingleChainDetailed(NODE_PATHS[i].primaryPath, NODE_PATHS[i].fallbackPath));
    }

    std::vector<BlockRow> emptyReference;
    const std::vector<BlockRow>& reference = nodeResults.empty() ? emptyReference : nodeResults[0].rows;
    std::size_t maxRows = reference.size();
    for (std::size_t i = 1; i < nodeResults.size(); ++i) {
        if (nodeResults[i].rows.size() > maxRows) {
            maxRows = nodeResults[i].rows.size();
        }
    }

    const std::vector<std::string> nodeHeaders = {"Node", "Blocks", "Integrity", "Consensus", "First Mismatch"};
    const std::vector<int> nodeWidths = {8, 8, 10, 10, 18};
    ui::printTableHeader(nodeHeaders, nodeWidths);

    int tamperedNodes = 0;

    for (std::size_t i = 0; i < nodeResults.size(); ++i) {
        std::string mismatch = "-";
        bool consensus = true;
        const std::size_t shared = std::min(reference.size(), nodeResults[i].rows.size());

        for (std::size_t idx = 0; idx < shared; ++idx) {
            const std::string left = serializeBlockWithoutCurrentHash(reference[idx]) + "|" + reference[idx].currentHash;
            const std::string right = serializeBlockWithoutCurrentHash(nodeResults[i].rows[idx]) + "|" + nodeResults[i].rows[idx].currentHash;
            if (left != right) {
                mismatch = reference[idx].index;
                consensus = false;
                break;
            }
        }

        if (consensus && reference.size() != nodeResults[i].rows.size()) {
            consensus = false;
            mismatch = std::to_string(shared + 1);
        }

        if (!nodeResults[i].integrityOk || !consensus) {
            tamperedNodes++;
        }

        ui::printTableRow({"Node " + std::to_string(i + 1),
                           std::to_string(nodeResults[i].rows.size()),
                           nodeResults[i].integrityOk ? "PASS" : "FAIL",
                           consensus ? "PASS" : "FAIL",
                           mismatch},
                          nodeWidths);
    }
    ui::printTableFooter(nodeWidths);

    if (!reference.empty()) {
        std::cout << "\n" << ui::bold("Reference Chain (Node 1)") << "\n";
        const std::vector<std::string> blockHeaders = {"Idx", "Timestamp", "Action", "Target", "Actor", "PrevHash", "CurrHash"};
        const std::vector<int> blockWidths = {5, 19, 24, 10, 12, 12, 12};
        ui::printTableHeader(blockHeaders, blockWidths);

        for (std::size_t i = 0; i < reference.size(); ++i) {
            const std::string prevShort = reference[i].previousHash.size() > 12 ? reference[i].previousHash.substr(0, 12) : reference[i].previousHash;
            const std::string currShort = reference[i].currentHash.size() > 12 ? reference[i].currentHash.substr(0, 12) : reference[i].currentHash;
            ui::printTableRow({reference[i].index,
                               reference[i].timestamp,
                               reference[i].action,
                               reference[i].documentId,
                               reference[i].actor,
                               prevShort,
                               currShort},
                              blockWidths);
        }
        ui::printTableFooter(blockWidths);

        std::cout << "\n" << ui::bold("Per-Block Consensus") << "\n";
        const std::vector<std::string> consensusHeaders = {"Block Index", "Nodes in Agreement", "Status"};
        const std::vector<int> consensusWidths = {12, 18, 12};
        ui::printTableHeader(consensusHeaders, consensusWidths);

        for (std::size_t idx = 0; idx < maxRows; ++idx) {
            std::map<std::string, int> signatures;
            for (std::size_t node = 0; node < nodeResults.size(); ++node) {
                if (idx >= nodeResults[node].rows.size()) {
                    signatures["<missing>"] += 1;
                    continue;
                }

                const std::string signature = serializeBlockWithoutCurrentHash(nodeResults[node].rows[idx]) + "|" + nodeResults[node].rows[idx].currentHash;
                signatures[signature] += 1;
            }

            int maxAgree = 0;
            for (std::map<std::string, int>::const_iterator it = signatures.begin(); it != signatures.end(); ++it) {
                if (it->second > maxAgree) {
                    maxAgree = it->second;
                }
            }

            const std::string blockIndex = idx < reference.size() ? reference[idx].index : std::to_string(idx + 1);
            ui::printTableRow({blockIndex,
                               std::to_string(maxAgree) + "/" + std::to_string(nodeResults.size()),
                               maxAgree == static_cast<int>(nodeResults.size()) ? "PASS" : "FAIL"},
                              consensusWidths);
        }

        ui::printTableFooter(consensusWidths);
    }

    if (tamperedNodes > 0) {
        std::cout << "\n" << ui::error("[!] Explorer detected tampering or divergence across node files.") << "\n";
        logAuditAction("BLOCKCHAIN_EXPLORER_TAMPER_ALERT", std::to_string(tamperedNodes), actor);
    } else {
        std::cout << "\n" << ui::success("[+] Explorer confirms all 5 nodes are synchronized and valid.") << "\n";
        logAuditAction("BLOCKCHAIN_EXPLORER_OK", "5_nodes", actor);
    }

    waitForEnter();
}
