#include "../include/blockchain.h"

#include "../include/audit.h"
#include "../include/auth.h"
#include "../include/notifications.h"
#include "../include/storage_utils.h"
#include "../include/ui.h"
#include "../include/verification.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
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

struct BlockchainIncident {
    std::string incidentId;
    std::string detectedAt;
    std::string source;
    std::string targetId;
    std::string severity;
    std::string status;
    std::string affectedSummary;
    std::string latestDetail;
    std::string resolvedAt;
    std::string resolvedBy;
    std::string resolutionNote;
};

struct BlockchainLiveStatus {
    bool sameContent;
    bool allValid;
    int affectedNodes;
    int totalNodes;
    std::string affectedSummary;
    std::vector<std::string> nodeSummaries;
};

const std::string ADMINS_FILE_PATH_PRIMARY = "data/admins.txt";
const std::string ADMINS_FILE_PATH_FALLBACK = "../data/admins.txt";
const std::string BLOCKCHAIN_INCIDENTS_FILE_PATH_PRIMARY = "data/blockchain_incidents.txt";
const std::string BLOCKCHAIN_INCIDENTS_FILE_PATH_FALLBACK = "../data/blockchain_incidents.txt";

void clearInputBuffer();
std::string serializeBlockWithoutCurrentHash(const BlockRow& row);
NodeValidationResult validateSingleChainDetailed(const std::string& path, const std::string& fallback);
std::string readFullFileWithFallback(const std::string& primaryPath, const std::string& fallbackPath);

std::string sanitizeAdminToken(std::string value) {
    for (std::size_t i = 0; i < value.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(value[i]);
        if (std::isalnum(c) == 0) {
            value[i] = '_';
        } else if (value[i] >= 'A' && value[i] <= 'Z') {
            value[i] = static_cast<char>(value[i] + ('a' - 'A'));
        }
    }

    if (value.empty()) {
        value = "admin";
    }

    return value;
}

std::string toLowerCopy(std::string value) {
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] >= 'A' && value[i] <= 'Z') {
            value[i] = static_cast<char>(value[i] + ('a' - 'A'));
        }
    }

    return value;
}

std::string trimCopy(const std::string& value) {
    const std::string whitespace = " \t\r\n";
    const std::size_t start = value.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return "";
    }

    const std::size_t end = value.find_last_not_of(whitespace);
    return value.substr(start, end - start + 1);
}

std::string shortenText(const std::string& value, std::size_t maxLength) {
    if (value.size() <= maxLength) {
        return value;
    }

    if (maxLength <= 3) {
        return value.substr(0, maxLength);
    }

    return value.substr(0, maxLength - 3) + "...";
}

std::string sanitizeFileToken(std::string token) {
    for (std::size_t i = 0; i < token.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(token[i]);
        const bool safe = std::isalnum(c) != 0 || token[i] == '_' || token[i] == '-' || token[i] == '.';
        if (!safe) {
            token[i] = '_';
        }
    }
    return token;
}

std::string buildTimestampToken(const std::string& timestamp) {
    std::string token = timestamp;
    for (std::size_t i = 0; i < token.size(); ++i) {
        if (token[i] == ' ' || token[i] == ':') {
            token[i] = '_';
        }
    }
    return sanitizeFileToken(token);
}

bool isSafeExportFileName(const std::string& fileName) {
    if (fileName.empty() || fileName.size() > 80) {
        return false;
    }

    if (fileName.find("..") != std::string::npos) {
        return false;
    }

    for (std::size_t i = 0; i < fileName.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(fileName[i]);
        const bool safe = std::isalnum(c) != 0 || fileName[i] == '_' || fileName[i] == '-' || fileName[i] == '.';
        if (!safe) {
            return false;
        }
    }

    return true;
}

std::string chooseExportPath(const std::string& fileName) {
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

bool loadAdminUsernames(std::vector<std::string>& usernames) {
    std::ifstream file;
    if (!openInputFileWithFallback(file, ADMINS_FILE_PATH_PRIMARY, ADMINS_FILE_PATH_FALLBACK)) {
        return false;
    }

    usernames.clear();
    std::set<std::string> unique;

    std::string line;
    bool firstLine = true;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        if (firstLine) {
            firstLine = false;
            if (line.find("adminID|") == 0) {
                continue;
            }
        }

        const std::vector<std::string> tokens = storage::splitPipeRow(line);

        if (tokens.size() < 3 || tokens[2].empty()) {
            continue;
        }

        unique.insert(tokens[2]);
    }

    usernames.assign(unique.begin(), unique.end());
    std::sort(usernames.begin(), usernames.end());
    return true;
}

NodePath buildAdminNodePath(const std::string& username) {
    const std::string safe = sanitizeAdminToken(username);
    NodePath path;
    path.primaryPath = "data/blockchain/node_admin_" + safe + "_chain.txt";
    path.fallbackPath = "../data/blockchain/node_admin_" + safe + "_chain.txt";
    return path;
}

std::vector<NodePath> buildNodePaths() {
    std::vector<NodePath> paths;
    paths.push_back({"data/blockchain/node1_chain.txt", "../data/blockchain/node1_chain.txt"});
    paths.push_back({"data/blockchain/node2_chain.txt", "../data/blockchain/node2_chain.txt"});
    paths.push_back({"data/blockchain/node3_chain.txt", "../data/blockchain/node3_chain.txt"});
    paths.push_back({"data/blockchain/node4_chain.txt", "../data/blockchain/node4_chain.txt"});
    paths.push_back({"data/blockchain/node5_chain.txt", "../data/blockchain/node5_chain.txt"});

    std::vector<std::string> usernames;
    if (loadAdminUsernames(usernames)) {
        for (std::size_t i = 0; i < usernames.size(); ++i) {
            paths.push_back(buildAdminNodePath(usernames[i]));
        }
    }

    return paths;
}

std::vector<std::string> buildNodeRelativeFiles() {
    const std::vector<NodePath> paths = buildNodePaths();
    std::vector<std::string> out;
    out.reserve(paths.size());

    for (std::size_t i = 0; i < paths.size(); ++i) {
        const std::string prefix = "data/";
        if (paths[i].primaryPath.find(prefix) == 0) {
            out.push_back(paths[i].primaryPath.substr(prefix.size()));
        } else {
            out.push_back(paths[i].primaryPath);
        }
    }

    return out;
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

void clearInputBuffer() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

bool ensureBlockchainIncidentDataFileExists() {
    return ensureFileWithHeader(BLOCKCHAIN_INCIDENTS_FILE_PATH_PRIMARY,
                                BLOCKCHAIN_INCIDENTS_FILE_PATH_FALLBACK,
                                "incidentID|detectedAt|source|targetID|severity|status|affectedSummary|latestDetail|resolvedAt|resolvedBy|resolutionNote");
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

BlockchainIncident parseBlockchainIncidentTokens(const std::vector<std::string>& tokens) {
    BlockchainIncident incident;
    incident.incidentId = tokens.size() > 0 ? tokens[0] : "";
    incident.detectedAt = tokens.size() > 1 ? tokens[1] : "";
    incident.source = tokens.size() > 2 ? tokens[2] : "";
    incident.targetId = tokens.size() > 3 ? tokens[3] : "";
    incident.severity = tokens.size() > 4 ? tokens[4] : "HIGH";
    incident.status = tokens.size() > 5 ? tokens[5] : "OPEN";
    incident.affectedSummary = tokens.size() > 6 ? tokens[6] : "";
    incident.latestDetail = tokens.size() > 7 ? tokens[7] : "";
    incident.resolvedAt = tokens.size() > 8 ? tokens[8] : "";
    incident.resolvedBy = tokens.size() > 9 ? tokens[9] : "";
    incident.resolutionNote = tokens.size() > 10 ? tokens[10] : "";
    return incident;
}

std::string serializeBlockchainIncident(const BlockchainIncident& incident) {
    return storage::joinPipeRow({
        incident.incidentId,
        incident.detectedAt,
        incident.source,
        incident.targetId,
        incident.severity,
        incident.status,
        incident.affectedSummary,
        incident.latestDetail,
        incident.resolvedAt,
        incident.resolvedBy,
        incident.resolutionNote
    });
}

bool loadBlockchainIncidents(std::vector<BlockchainIncident>& incidents) {
    if (!ensureBlockchainIncidentDataFileExists()) {
        return false;
    }

    std::ifstream file;
    if (!openInputFileWithFallback(file, BLOCKCHAIN_INCIDENTS_FILE_PATH_PRIMARY, BLOCKCHAIN_INCIDENTS_FILE_PATH_FALLBACK)) {
        return false;
    }

    incidents.clear();
    std::string line;
    bool firstLine = true;

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        if (firstLine) {
            firstLine = false;
            if (line.find("incidentID|") == 0) {
                continue;
            }
        }

        const BlockchainIncident incident = parseBlockchainIncidentTokens(storage::splitPipeRow(line));
        if (!incident.incidentId.empty()) {
            incidents.push_back(incident);
        }
    }

    return true;
}

bool saveBlockchainIncidents(const std::vector<BlockchainIncident>& incidents) {
    std::vector<std::string> rows;
    rows.reserve(incidents.size());

    for (std::size_t i = 0; i < incidents.size(); ++i) {
        rows.push_back(serializeBlockchainIncident(incidents[i]));
    }

    return storage::writePipeFileWithFallback(BLOCKCHAIN_INCIDENTS_FILE_PATH_PRIMARY,
                                              BLOCKCHAIN_INCIDENTS_FILE_PATH_FALLBACK,
                                              "incidentID|detectedAt|source|targetID|severity|status|affectedSummary|latestDetail|resolvedAt|resolvedBy|resolutionNote",
                                              rows);
}

std::string generateNextIncidentId(const std::vector<BlockchainIncident>& incidents) {
    int maxId = 0;

    for (std::size_t i = 0; i < incidents.size(); ++i) {
        const std::string id = incidents[i].incidentId;
        if (id.size() <= 3 || id.substr(0, 3) != "BCI") {
            continue;
        }

        const int parsed = std::atoi(id.substr(3).c_str());
        if (parsed > maxId) {
            maxId = parsed;
        }
    }

    std::ostringstream out;
    out << "BCI" << std::setw(3) << std::setfill('0') << (maxId + 1);
    return out.str();
}

bool computeNodeConsensusMismatch(const std::vector<BlockRow>& referenceRows,
                                  const std::vector<BlockRow>& candidateRows,
                                  std::string& outMismatch) {
    outMismatch = "";
    const std::size_t shared = std::min(referenceRows.size(), candidateRows.size());

    for (std::size_t idx = 0; idx < shared; ++idx) {
        const std::string left = serializeBlockWithoutCurrentHash(referenceRows[idx]) + "|" + referenceRows[idx].currentHash;
        const std::string right = serializeBlockWithoutCurrentHash(candidateRows[idx]) + "|" + candidateRows[idx].currentHash;
        if (left != right) {
            outMismatch = referenceRows[idx].index;
            return true;
        }
    }

    if (referenceRows.size() != candidateRows.size()) {
        outMismatch = std::to_string(shared + 1);
        return true;
    }

    return false;
}

BlockchainLiveStatus buildBlockchainLiveStatus(const std::vector<NodeValidationResult>& nodeResults,
                                               const std::vector<std::string>& nodeData) {
    BlockchainLiveStatus status;
    status.sameContent = !nodeData.empty() && !nodeData[0].empty();
    status.allValid = status.sameContent;
    status.affectedNodes = 0;
    status.totalNodes = static_cast<int>(nodeResults.size());

    for (std::size_t i = 1; i < nodeData.size() && status.sameContent; ++i) {
        if (nodeData[i] != nodeData[0]) {
            status.sameContent = false;
        }
    }

    const std::vector<BlockRow> emptyReference;
    const std::vector<BlockRow>& referenceRows = nodeResults.empty() ? emptyReference : nodeResults[0].rows;

    std::ostringstream summary;
    for (std::size_t i = 0; i < nodeResults.size(); ++i) {
        std::ostringstream nodeSummary;
        nodeSummary << "Node " << (i + 1) << ": ";

        bool affected = false;
        if (!nodeResults[i].integrityOk) {
            affected = true;
            nodeSummary << nodeResults[i].reason;
            if (!nodeResults[i].failedIndex.empty()) {
                nodeSummary << " @index " << nodeResults[i].failedIndex;
            }
        } else {
            std::string mismatchIndex;
            if (computeNodeConsensusMismatch(referenceRows, nodeResults[i].rows, mismatchIndex)) {
                affected = true;
                nodeSummary << "CONSENSUS_MISMATCH";
                if (!mismatchIndex.empty()) {
                    nodeSummary << " @index " << mismatchIndex;
                }
            } else {
                nodeSummary << "OK";
            }
        }

        if (affected) {
            status.affectedNodes++;
        }

        status.allValid = status.allValid && nodeResults[i].integrityOk;
        status.nodeSummaries.push_back(nodeSummary.str());
    }

    summary << status.affectedNodes << "/" << status.totalNodes << " nodes affected";
    if (!status.sameContent) {
        summary << "; divergence detected";
    }

    bool wroteAnyNode = false;
    for (std::size_t i = 0; i < status.nodeSummaries.size(); ++i) {
        if (status.nodeSummaries[i].find(": OK") != std::string::npos) {
            continue;
        }
        summary << (wroteAnyNode ? " | " : "; ");
        summary << status.nodeSummaries[i];
        wroteAnyNode = true;
    }

    if (!wroteAnyNode && status.sameContent && status.affectedNodes == 0) {
        summary << "; all synchronized";
    }

    status.affectedSummary = summary.str();
    status.allValid = status.allValid && status.sameContent;
    return status;
}

BlockchainLiveStatus inspectBlockchainState() {
    ensureBlockchainNodeFilesExist();

    const std::vector<NodePath> nodePaths = buildNodePaths();
    std::vector<NodeValidationResult> nodeResults;
    std::vector<std::string> nodeData;
    nodeResults.reserve(nodePaths.size());
    nodeData.reserve(nodePaths.size());

    for (std::size_t i = 0; i < nodePaths.size(); ++i) {
        nodeResults.push_back(validateSingleChainDetailed(nodePaths[i].primaryPath, nodePaths[i].fallbackPath));
        nodeData.push_back(readFullFileWithFallback(nodePaths[i].primaryPath, nodePaths[i].fallbackPath));
    }

    return buildBlockchainLiveStatus(nodeResults, nodeData);
}

std::string promptIncidentNote(const std::string& label) {
    std::string note;
    std::cout << label;
    std::getline(std::cin, note);
    note = storage::sanitizeSingleLineInput(note);
    if (note.size() > 220) {
        note = note.substr(0, 220);
    }
    return note;
}

bool findIncidentById(const std::vector<BlockchainIncident>& incidents,
                      const std::string& incidentId,
                      BlockchainIncident& outIncident,
                      std::size_t* indexOut) {
    const std::string target = toLowerCopy(trimCopy(incidentId));
    for (std::size_t i = 0; i < incidents.size(); ++i) {
        if (toLowerCopy(incidents[i].incidentId) == target) {
            outIncident = incidents[i];
            if (indexOut != NULL) {
                *indexOut = i;
            }
            return true;
        }
    }
    return false;
}

bool upsertBlockchainIncident(const std::string& source,
                              const std::string& targetId,
                              const std::string& severity,
                              const std::string& affectedSummary,
                              const std::string& detail,
                              const std::string& actor) {
    std::vector<BlockchainIncident> incidents;
    if (!loadBlockchainIncidents(incidents)) {
        return false;
    }

    std::size_t matchIndex = incidents.size();
    for (std::size_t i = 0; i < incidents.size(); ++i) {
        if (toLowerCopy(incidents[i].status) == "open" &&
            incidents[i].source == source &&
            incidents[i].targetId == targetId) {
            matchIndex = i;
        }
    }

    bool created = false;
    if (matchIndex == incidents.size()) {
        BlockchainIncident incident;
        incident.incidentId = generateNextIncidentId(incidents);
        incident.detectedAt = getCurrentTimestamp();
        incident.source = source;
        incident.targetId = targetId;
        incident.severity = severity;
        incident.status = "OPEN";
        incident.affectedSummary = storage::sanitizeSingleLineInput(affectedSummary);
        incident.latestDetail = storage::sanitizeSingleLineInput(detail);
        incidents.push_back(incident);
        created = true;
    } else {
        incidents[matchIndex].severity = severity;
        incidents[matchIndex].affectedSummary = storage::sanitizeSingleLineInput(affectedSummary);
        incidents[matchIndex].latestDetail = storage::sanitizeSingleLineInput(detail);
        incidents[matchIndex].status = "OPEN";
        incidents[matchIndex].resolvedAt.clear();
        incidents[matchIndex].resolvedBy.clear();
        incidents[matchIndex].resolutionNote.clear();
    }

    if (!saveBlockchainIncidents(incidents)) {
        return false;
    }

    if (created) {
        logAuditAction("BLOCKCHAIN_INCIDENT_CREATED", incidents.back().incidentId + "|" + source, actor);
    } else {
        logAuditAction("BLOCKCHAIN_INCIDENT_UPDATED", incidents[matchIndex].incidentId + "|" + source, actor);
    }

    return true;
}

int resolveOpenBlockchainIncidents(const std::string& actor,
                                   const std::string& note,
                                   const std::string* specificIncidentId) {
    std::vector<BlockchainIncident> incidents;
    if (!loadBlockchainIncidents(incidents)) {
        return -1;
    }

    int resolvedCount = 0;
    const std::string trimmedNote = storage::sanitizeSingleLineInput(note);

    for (std::size_t i = 0; i < incidents.size(); ++i) {
        if (toLowerCopy(incidents[i].status) != "open") {
            continue;
        }

        if (specificIncidentId != NULL &&
            toLowerCopy(incidents[i].incidentId) != toLowerCopy(*specificIncidentId)) {
            continue;
        }

        incidents[i].status = "RESOLVED";
        incidents[i].resolvedAt = getCurrentTimestamp();
        incidents[i].resolvedBy = actor;
        incidents[i].resolutionNote = trimmedNote;
        resolvedCount++;
    }

    if (resolvedCount <= 0) {
        return 0;
    }

    if (!saveBlockchainIncidents(incidents)) {
        return -1;
    }

    return resolvedCount;
}

std::string buildIncidentExportBaseName(const BlockchainIncident& incident) {
    return "blockchain_incident_" + sanitizeFileToken(incident.incidentId) + "_" + buildTimestampToken(getCurrentTimestamp());
}

std::string buildIncidentTxtReport(const BlockchainIncident& incident,
                                   const BlockchainLiveStatus& liveStatus,
                                   const std::string& actor,
                                   const std::string& exportNote) {
    std::ostringstream out;
    out << "PROCURECHAIN BLOCKCHAIN INCIDENT REPORT\n";
    out << "Exported At    : " << getCurrentTimestamp() << "\n";
    out << "Exported By    : " << actor << "\n";
    out << "Export Note    : " << (exportNote.empty() ? "(none)" : exportNote) << "\n";
    out << "\n";
    out << "Incident ID    : " << incident.incidentId << "\n";
    out << "Detected At    : " << incident.detectedAt << "\n";
    out << "Source         : " << incident.source << "\n";
    out << "Target ID      : " << incident.targetId << "\n";
    out << "Severity       : " << incident.severity << "\n";
    out << "Status         : " << incident.status << "\n";
    out << "Affected Nodes : " << incident.affectedSummary << "\n";
    out << "Latest Detail  : " << incident.latestDetail << "\n";
    out << "Resolved At    : " << (incident.resolvedAt.empty() ? "(not resolved)" : incident.resolvedAt) << "\n";
    out << "Resolved By    : " << (incident.resolvedBy.empty() ? "(not resolved)" : incident.resolvedBy) << "\n";
    out << "Resolution Note: " << (incident.resolutionNote.empty() ? "(none)" : incident.resolutionNote) << "\n";
    out << "\n";
    out << "Live Status\n";
    out << "Reproducible   : " << (liveStatus.allValid ? "NO" : "YES") << "\n";
    out << "Current Summary: " << liveStatus.affectedSummary << "\n";
    for (std::size_t i = 0; i < liveStatus.nodeSummaries.size(); ++i) {
        out << "  - " << liveStatus.nodeSummaries[i] << "\n";
    }
    return out.str();
}

std::string buildIncidentCsvReport(const BlockchainIncident& incident,
                                   const BlockchainLiveStatus& liveStatus,
                                   const std::string& actor,
                                   const std::string& exportNote) {
    const std::vector<std::string> headers = {
        "incidentID", "detectedAt", "source", "targetID", "severity", "status",
        "affectedSummary", "latestDetail", "resolvedAt", "resolvedBy", "resolutionNote",
        "exportedAt", "exportedBy", "exportNote", "liveReproducible", "liveSummary"
    };
    const std::vector<std::string> values = {
        incident.incidentId,
        incident.detectedAt,
        incident.source,
        incident.targetId,
        incident.severity,
        incident.status,
        incident.affectedSummary,
        incident.latestDetail,
        incident.resolvedAt,
        incident.resolvedBy,
        incident.resolutionNote,
        getCurrentTimestamp(),
        actor,
        exportNote,
        liveStatus.allValid ? "NO" : "YES",
        liveStatus.affectedSummary
    };

    std::ostringstream out;
    for (std::size_t i = 0; i < headers.size(); ++i) {
        if (i > 0) {
            out << ",";
        }
        out << headers[i];
    }
    out << "\n";

    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            out << ",";
        }
        out << "\"" << values[i] << "\"";
    }
    out << "\n";
    return out.str();
}

void printIncidentListTable(const std::vector<BlockchainIncident>& incidents) {
    const std::vector<std::string> headers = {"ID", "Status", "Severity", "Detected", "Source", "Affected", "Summary"};
    const std::vector<int> widths = {8, 10, 10, 19, 20, 16, 28};
    ui::printTableHeader(headers, widths);

    for (std::size_t i = 0; i < incidents.size(); ++i) {
        ui::printTableRow({incidents[i].incidentId,
                           incidents[i].status,
                           incidents[i].severity,
                           incidents[i].detectedAt,
                           incidents[i].source,
                           shortenText(incidents[i].affectedSummary, 16),
                           shortenText(incidents[i].latestDetail, 28)},
                          widths);
    }

    ui::printTableFooter(widths);
}

void printIncidentDetailView(const BlockchainIncident& incident, const BlockchainLiveStatus& liveStatus) {
    clearScreen();
    ui::printSectionTitle("BLOCKCHAIN INCIDENT DETAIL");

    const std::vector<std::string> headers = {"Field", "Value"};
    const std::vector<int> widths = {20, 72};
    ui::printTableHeader(headers, widths);
    ui::printTableRow({"Incident ID", incident.incidentId}, widths);
    ui::printTableRow({"Detected At", incident.detectedAt}, widths);
    ui::printTableRow({"Source", incident.source}, widths);
    ui::printTableRow({"Target ID", incident.targetId}, widths);
    ui::printTableRow({"Severity", incident.severity}, widths);
    ui::printTableRow({"Status", incident.status}, widths);
    ui::printTableRow({"Affected Nodes", incident.affectedSummary}, widths);
    ui::printTableRow({"Latest Detail", incident.latestDetail}, widths);
    ui::printTableRow({"Resolved At", incident.resolvedAt.empty() ? "(not resolved)" : incident.resolvedAt}, widths);
    ui::printTableRow({"Resolved By", incident.resolvedBy.empty() ? "(not resolved)" : incident.resolvedBy}, widths);
    ui::printTableRow({"Resolution Note", incident.resolutionNote.empty() ? "(none)" : incident.resolutionNote}, widths);
    ui::printTableFooter(widths);

    std::cout << "\n" << ui::bold("Live Blockchain Status") << "\n";
    std::cout << "  Reproducible : " << (liveStatus.allValid ? "NO" : "YES") << "\n";
    std::cout << "  Summary      : " << liveStatus.affectedSummary << "\n";
    for (std::size_t i = 0; i < liveStatus.nodeSummaries.size(); ++i) {
        std::cout << "  - " << liveStatus.nodeSummaries[i] << "\n";
    }
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
    // Blockchain showcase rows are maintained in data/blockchain/node*_chain.txt.
}

void synchronizeEmptyNodesFromReference() {
    // Copies the first non-empty node into any empty node so replication starts
    // from a consistent baseline before live writes happen.
    std::string referenceData;

    const std::vector<NodePath> nodePaths = buildNodePaths();

    for (std::size_t i = 0; i < nodePaths.size(); ++i) {
        if (!hasDataRows(nodePaths[i].primaryPath, nodePaths[i].fallbackPath)) {
            continue;
        }

        referenceData = readFullFileWithFallback(nodePaths[i].primaryPath, nodePaths[i].fallbackPath);
        if (!referenceData.empty()) {
            break;
        }
    }

    if (referenceData.empty()) {
        return;
    }

    for (std::size_t i = 0; i < nodePaths.size(); ++i) {
        if (hasDataRows(nodePaths[i].primaryPath, nodePaths[i].fallbackPath)) {
            continue;
        }

        std::ofstream nodeWriter;
        if (!openWriteFileWithFallback(nodeWriter, nodePaths[i].primaryPath, nodePaths[i].fallbackPath)) {
            continue;
        }

        nodeWriter << referenceData;
        nodeWriter.flush();
    }
}

bool saveChainRows(const std::string& primaryPath, const std::string& fallbackPath, const std::vector<BlockRow>& rows) {
    std::vector<std::string> serializedRows;
    serializedRows.reserve(rows.size());
    for (std::size_t i = 0; i < rows.size(); ++i) {
        serializedRows.push_back(storage::joinPipeRow({
            rows[i].index,
            rows[i].timestamp,
            rows[i].action,
            rows[i].documentId,
            rows[i].actor,
            rows[i].previousHash,
            rows[i].currentHash
        }));
    }
    return storage::writePipeFileWithFallback(primaryPath,
                                              fallbackPath,
                                              "index|timestamp|action|documentID|actor|previousHash|currentHash",
                                              serializedRows);
}

void rebuildChainHashes(std::vector<BlockRow>& rows) {
    std::string previousHash = "0000";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        rows[i].previousHash = previousHash;
        rows[i].currentHash = computeSimpleHash(serializeBlockWithoutCurrentHash(rows[i]));
        previousHash = rows[i].currentHash;
    }
}

bool rewriteNodeFilesFromCanonical(const std::vector<NodePath>& nodePaths,
                                   const std::vector<BlockRow>& rows,
                                   const std::vector<std::string>* originalContents,
                                   bool& rollbackSucceeded) {
    rollbackSucceeded = true;
    const std::string expectedContents =
        "index|timestamp|action|documentID|actor|previousHash|currentHash\n" +
        [&rows]() {
            std::ostringstream out;
            for (std::size_t i = 0; i < rows.size(); ++i) {
                out << storage::joinPipeRow({
                    rows[i].index,
                    rows[i].timestamp,
                    rows[i].action,
                    rows[i].documentId,
                    rows[i].actor,
                    rows[i].previousHash,
                    rows[i].currentHash
                }) << '\n';
            }
            return out.str();
        }();

    std::vector<std::string> targetPaths;
    targetPaths.reserve(nodePaths.size());
    for (std::size_t i = 0; i < nodePaths.size(); ++i) {
        targetPaths.push_back(storage::resolveWritePath(nodePaths[i].primaryPath, nodePaths[i].fallbackPath));
    }

    std::size_t writtenCount = 0;
    for (std::size_t i = 0; i < targetPaths.size(); ++i) {
        if (!storage::writeTextFileAtomically(targetPaths[i], expectedContents)) {
            if (originalContents != NULL) {
                for (std::size_t rollbackIndex = 0; rollbackIndex < writtenCount; ++rollbackIndex) {
                    if (!storage::writeTextFileAtomically(targetPaths[rollbackIndex], (*originalContents)[rollbackIndex])) {
                        rollbackSucceeded = false;
                    }
                }
            }
            return false;
        }
        writtenCount++;
    }

    for (std::size_t i = 0; i < nodePaths.size(); ++i) {
        if (readFullFileWithFallback(nodePaths[i].primaryPath, nodePaths[i].fallbackPath) != expectedContents) {
            if (originalContents != NULL) {
                for (std::size_t rollbackIndex = 0; rollbackIndex < targetPaths.size(); ++rollbackIndex) {
                    if (!storage::writeTextFileAtomically(targetPaths[rollbackIndex], (*originalContents)[rollbackIndex])) {
                        rollbackSucceeded = false;
                    }
                }
            }
            return false;
        }
    }

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
    const std::vector<NodePath> nodePaths = buildNodePaths();
    for (std::size_t i = 0; i < nodePaths.size(); ++i) {
        migrateNodeChainToSha256(nodePaths[i]);
    }
}

void removeOrphanAdminNodeFiles(const std::vector<NodePath>& activePaths) {
    std::set<std::string> activePrimary;
    std::set<std::string> activeFallback;
    for (std::size_t i = 0; i < activePaths.size(); ++i) {
        activePrimary.insert(activePaths[i].primaryPath);
        activeFallback.insert(activePaths[i].fallbackPath);
    }

    const std::string primaryDir = "data/blockchain";
    const std::string fallbackDir = "../data/blockchain";
    std::error_code ec;

    if (std::filesystem::exists(primaryDir, ec)) {
        for (std::filesystem::directory_iterator it(primaryDir, ec); !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
            if (!it->is_regular_file()) {
                continue;
            }
            const std::string name = it->path().filename().string();
            if (name.find("node_admin_") != 0 || name.rfind("_chain.txt") == std::string::npos) {
                continue;
            }
            const std::string full = it->path().string();
            if (activePrimary.find(full) != activePrimary.end()) {
                continue;
            }
            std::filesystem::remove(it->path(), ec);
            ec.clear();
        }
    }

    ec.clear();
    if (std::filesystem::exists(fallbackDir, ec)) {
        for (std::filesystem::directory_iterator it(fallbackDir, ec); !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
            if (!it->is_regular_file()) {
                continue;
            }
            const std::string name = it->path().filename().string();
            if (name.find("node_admin_") != 0 || name.rfind("_chain.txt") == std::string::npos) {
                continue;
            }
            const std::string full = it->path().string();
            if (activeFallback.find(full) != activeFallback.end()) {
                continue;
            }
            std::filesystem::remove(it->path(), ec);
            ec.clear();
        }
    }
}
} // namespace

void ensureBlockchainNodeFilesExist() {
    // Startup bootstrap only ensures the expected files exist.
    const std::string header = "index|timestamp|action|documentID|actor|previousHash|currentHash";
    const std::vector<NodePath> nodePaths = buildNodePaths();

    for (std::size_t i = 0; i < nodePaths.size(); ++i) {
        ensureFileWithHeader(nodePaths[i].primaryPath, nodePaths[i].fallbackPath, header);
    }
}

int appendBlockchainAction(const std::string& action, const std::string& docId, const std::string& actor) {
    ensureBlockchainNodeFilesExist();

    const std::vector<NodePath> nodePaths = buildNodePaths();
    if (nodePaths.empty()) {
        return -1;
    }

    std::vector<NodeValidationResult> nodeResults;
    std::vector<std::string> nodeContents;
    nodeResults.reserve(nodePaths.size());
    nodeContents.reserve(nodePaths.size());

    for (std::size_t i = 0; i < nodePaths.size(); ++i) {
        nodeResults.push_back(validateSingleChainDetailed(nodePaths[i].primaryPath, nodePaths[i].fallbackPath));
        nodeContents.push_back(readFullFileWithFallback(nodePaths[i].primaryPath, nodePaths[i].fallbackPath));
        if (!nodeResults[i].integrityOk) {
            logTamperAlert("HIGH",
                           "BLOCKCHAIN_APPEND",
                           docId,
                           "Append blocked because at least one blockchain replica failed validation. Run blockchain repair first.",
                           actor,
                           "PUBLIC");
            return -1;
        }
    }

    bool sameContent = !nodeContents.empty();
    for (std::size_t i = 1; i < nodeContents.size() && sameContent; ++i) {
        if (nodeContents[i] != nodeContents[0]) {
            sameContent = false;
        }
    }

    if (!sameContent) {
        logTamperAlert("HIGH",
                       "BLOCKCHAIN_APPEND",
                       docId,
                       "Append blocked because blockchain replicas are diverged. Run blockchain repair first.",
                       actor,
                       "PUBLIC");
        return -1;
    }

    const std::vector<BlockRow>& referenceRows = nodeResults[0].rows;
    int nextIndex = 1;
    std::string previousHash = "0000";
    if (!referenceRows.empty()) {
        nextIndex = std::atoi(referenceRows.back().index.c_str()) + 1;
        previousHash = referenceRows.back().currentHash;
    }

    std::string indexText = std::to_string(nextIndex);
    std::string timestamp = getCurrentTimestamp();
    BlockRow newRow;
    newRow.index = indexText;
    newRow.timestamp = timestamp;
    newRow.action = action;
    newRow.documentId = docId;
    newRow.actor = actor;
    newRow.previousHash = previousHash;
    newRow.currentHash = computeSimpleHash(serializeBlockWithoutCurrentHash(newRow));

    std::vector<BlockRow> candidateRows = referenceRows;
    candidateRows.push_back(newRow);

    bool rollbackSucceeded = true;
    if (!rewriteNodeFilesFromCanonical(nodePaths, candidateRows, &nodeContents, rollbackSucceeded)) {
        logTamperAlert("HIGH",
                       "BLOCKCHAIN_APPEND",
                       docId,
                       rollbackSucceeded ?
                           "Append failed and was rolled back because replica rewrite verification failed." :
                           "Append failed and rollback also encountered errors. Manual blockchain repair is required.",
                       actor,
                       "PUBLIC");
        return -1;
    }

    return nextIndex;
}

void validateBlockchainNodes(const std::string& actor) {
    // Performs per-node validation plus cross-node byte-level consistency check.
    // Overall complexity: O(k * n), k=node count, n=rows per node.
    clearScreen();
    ui::printSectionTitle("BLOCKCHAIN VALIDATION (SIMULATED)");

    ensureBlockchainNodeFilesExist();

    const std::vector<NodePath> nodePaths = buildNodePaths();

    std::vector<NodeValidationResult> nodeResults;
    nodeResults.reserve(nodePaths.size());

    std::vector<std::string> nodeData;
    nodeData.reserve(nodePaths.size());

    for (std::size_t i = 0; i < nodePaths.size(); ++i) {
        nodeResults.push_back(validateSingleChainDetailed(nodePaths[i].primaryPath, nodePaths[i].fallbackPath));
        nodeData.push_back(readFullFileWithFallback(nodePaths[i].primaryPath, nodePaths[i].fallbackPath));
    }

    const BlockchainLiveStatus liveStatus = buildBlockchainLiveStatus(nodeResults, nodeData);
    const bool sameContent = liveStatus.sameContent;
    const bool allValid = liveStatus.allValid;

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
        logTamperAlert("HIGH",
                       "BLOCKCHAIN_VALIDATE",
                       "MULTI",
                       "Validation failed due to chain inconsistency or tamper indicators.",
                       actor,
                       "PUBLIC");
        upsertBlockchainIncident("BLOCKCHAIN_VALIDATE",
                                 "MULTI",
                                 "HIGH",
                                 liveStatus.affectedSummary,
                                 "Validation failed due to chain inconsistency or tamper indicators.",
                                 actor);
        logAuditAction("BLOCKCHAIN_VALIDATE_FAIL", "MULTI", actor);
    }

    waitForEnter();
}

void viewBlockchainExplorer(const std::string& actor) {
    clearScreen();
    ui::printSectionTitle("BLOCKCHAIN EXPLORER (DYNAMIC NODE CONSENSUS)");

    ensureBlockchainNodeFilesExist();

    const std::vector<NodePath> nodePaths = buildNodePaths();

    std::vector<NodeValidationResult> nodeResults;
    nodeResults.reserve(nodePaths.size());
    std::vector<std::string> nodeData;
    nodeData.reserve(nodePaths.size());

    for (std::size_t i = 0; i < nodePaths.size(); ++i) {
        nodeResults.push_back(validateSingleChainDetailed(nodePaths[i].primaryPath, nodePaths[i].fallbackPath));
        nodeData.push_back(readFullFileWithFallback(nodePaths[i].primaryPath, nodePaths[i].fallbackPath));
    }

    const BlockchainLiveStatus liveStatus = buildBlockchainLiveStatus(nodeResults, nodeData);

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
        logTamperAlert("HIGH",
                       "BLOCKCHAIN_EXPLORER",
                       std::to_string(tamperedNodes) + "_nodes",
                       "Explorer detected diverging blockchain node replicas.",
                       actor,
                       "PUBLIC");
        upsertBlockchainIncident("BLOCKCHAIN_EXPLORER",
                                 "MULTI",
                                 "HIGH",
                                 liveStatus.affectedSummary,
                                 "Explorer detected diverging blockchain node replicas.",
                                 actor);
        logAuditAction("BLOCKCHAIN_EXPLORER_TAMPER_ALERT", std::to_string(tamperedNodes), actor);
    } else {
        std::cout << "\n" << ui::success("[+] Explorer confirms all ") << nodeResults.size()
                  << ui::success(" nodes are synchronized and valid.") << "\n";
        logAuditAction("BLOCKCHAIN_EXPLORER_OK", std::to_string(nodeResults.size()) + "_nodes", actor);
    }

    waitForEnter();
}

void repairBlockchainNodes(const std::string& actor) {
    clearScreen();
    ui::printSectionTitle("BLOCKCHAIN REPAIR (SIMULATED)");

    if (!ui::confirmAction("Repair blockchain replicas using the best available canonical chain?",
                           "Run Repair",
                           "Cancel")) {
        std::cout << ui::warning("[!] Blockchain repair cancelled.") << "\n";
        waitForEnter();
        return;
    }

    const std::vector<NodePath> nodePaths = buildNodePaths();
    if (nodePaths.empty()) {
        std::cout << ui::error("[!] No blockchain nodes are configured.") << "\n";
        waitForEnter();
        return;
    }

    removeOrphanAdminNodeFiles(nodePaths);
    ensureBlockchainNodeFilesExist();

    std::vector<NodeValidationResult> nodeResults;
    std::vector<std::string> originalContents;
    nodeResults.reserve(nodePaths.size());
    originalContents.reserve(nodePaths.size());

    for (std::size_t i = 0; i < nodePaths.size(); ++i) {
        nodeResults.push_back(validateSingleChainDetailed(nodePaths[i].primaryPath, nodePaths[i].fallbackPath));
        originalContents.push_back(readFullFileWithFallback(nodePaths[i].primaryPath, nodePaths[i].fallbackPath));
    }

    const BlockchainLiveStatus originalStatus = buildBlockchainLiveStatus(nodeResults, originalContents);

    std::vector<BlockRow> canonicalRows;
    std::size_t bestValidIndex = nodeResults.size();
    std::size_t bestValidLength = 0;
    for (std::size_t i = 0; i < nodeResults.size(); ++i) {
        if (!nodeResults[i].integrityOk) {
            continue;
        }
        if (bestValidIndex == nodeResults.size() || nodeResults[i].rows.size() > bestValidLength) {
            bestValidIndex = i;
            bestValidLength = nodeResults[i].rows.size();
        }
    }

    if (bestValidIndex != nodeResults.size()) {
        canonicalRows = nodeResults[bestValidIndex].rows;
    } else {
        for (std::size_t i = 0; i < nodeResults.size(); ++i) {
            if (!nodeResults[i].rows.empty()) {
                canonicalRows = nodeResults[i].rows;
                rebuildChainHashes(canonicalRows);
                break;
            }
        }
    }

    bool rollbackSucceeded = true;
    if (!rewriteNodeFilesFromCanonical(nodePaths, canonicalRows, &originalContents, rollbackSucceeded)) {
        logTamperAlert("HIGH",
                       "BLOCKCHAIN_REPAIR",
                       "MULTI",
                       rollbackSucceeded ?
                           "Blockchain repair failed and was rolled back." :
                           "Blockchain repair failed and rollback also encountered errors. Manual intervention is required.",
                       actor,
                       "PUBLIC");
        upsertBlockchainIncident("BLOCKCHAIN_REPAIR",
                                 "MULTI",
                                 "HIGH",
                                 originalStatus.affectedSummary,
                                 rollbackSucceeded ?
                                     "Blockchain repair failed and was rolled back." :
                                     "Blockchain repair failed and rollback also encountered errors. Manual intervention is required.",
                                 actor);
        std::cout << ui::error("[!] Blockchain repair failed.") << "\n";
        waitForEnter();
        return;
    }

    clearInputBuffer();
    const std::string resolutionNote = promptIncidentNote("Resolution note (optional): ");
    const int resolvedCount = resolveOpenBlockchainIncidents(actor, resolutionNote, NULL);

    tryLogAuditAction("BLOCKCHAIN_REPAIR_OK",
                      "MULTI[" + std::to_string(canonicalRows.size()) + "_blocks]",
                      actor);
    std::cout << ui::success("[+] Blockchain replicas repaired successfully.") << "\n";
    std::cout << "  Canonical blocks : " << canonicalRows.size() << "\n";
    if (resolvedCount > 0) {
        logAuditAction("BLOCKCHAIN_INCIDENT_RESOLVED", std::to_string(resolvedCount) + "_incidents", actor);
        std::cout << "  Resolved incidents: " << resolvedCount << "\n";
    }
    waitForEnter();
}

void viewBlockchainIncidentReports(const std::string& actor) {
    int filterMode = 0; // 0=open, 1=resolved, 2=all
    int choice = -1;

    do {
        std::vector<BlockchainIncident> incidents;
        if (!loadBlockchainIncidents(incidents)) {
            clearScreen();
            ui::printSectionTitle("BLOCKCHAIN INCIDENT REPORTS");
            std::cout << ui::error("[!] Unable to load blockchain incident records.") << "\n";
            waitForEnter();
            return;
        }

        std::sort(incidents.begin(), incidents.end(), [](const BlockchainIncident& left, const BlockchainIncident& right) {
            if (left.detectedAt != right.detectedAt) {
                return left.detectedAt > right.detectedAt;
            }
            return left.incidentId > right.incidentId;
        });

        std::vector<BlockchainIncident> filtered;
        int openCount = 0;
        int resolvedCount = 0;
        for (std::size_t i = 0; i < incidents.size(); ++i) {
            const bool isOpen = toLowerCopy(incidents[i].status) == "open";
            if (isOpen) {
                openCount++;
            } else {
                resolvedCount++;
            }

            if (filterMode == 0 && !isOpen) {
                continue;
            }
            if (filterMode == 1 && isOpen) {
                continue;
            }

            filtered.push_back(incidents[i]);
        }

        clearScreen();
        ui::printSectionTitle("BLOCKCHAIN INCIDENT REPORTS");
        std::cout << "  Open incidents    : " << openCount << "\n";
        std::cout << "  Resolved incidents: " << resolvedCount << "\n";
        std::cout << "  Active filter     : "
                  << (filterMode == 0 ? "OPEN" : (filterMode == 1 ? "RESOLVED" : "ALL")) << "\n\n";

        if (filtered.empty()) {
            std::cout << ui::warning("[!] No incidents match the current filter.") << "\n";
        } else {
            printIncidentListTable(filtered);
        }

        std::cout << "\n";
        std::cout << "  [1] Show Open Incidents\n";
        std::cout << "  [2] Show Resolved Incidents\n";
        std::cout << "  [3] Show All Incidents\n";
        std::cout << "  [4] View Incident Detail\n";
        std::cout << "  [5] Resolve Incident\n";
        std::cout << "  [6] Export Incident Report (TXT + CSV)\n";
        std::cout << "  [0] Back\n";
        std::cout << "  Enter your choice: ";

        std::cin >> choice;
        if (std::cin.fail()) {
            std::cin.clear();
            clearInputBuffer();
            std::cout << ui::warning("[!] Invalid choice.") << "\n";
            waitForEnter();
            continue;
        }

        if (choice == 1) {
            filterMode = 0;
            continue;
        }

        if (choice == 2) {
            filterMode = 1;
            continue;
        }

        if (choice == 3) {
            filterMode = 2;
            continue;
        }

        if (choice == 4) {
            clearInputBuffer();
            std::string incidentId;
            std::cout << "Incident ID: ";
            std::getline(std::cin, incidentId);

            BlockchainIncident incident;
            if (!findIncidentById(incidents, incidentId, incident, NULL)) {
                std::cout << ui::warning("[!] Incident ID not found.") << "\n";
                waitForEnter();
                continue;
            }

            const BlockchainLiveStatus liveStatus = inspectBlockchainState();
            printIncidentDetailView(incident, liveStatus);
            logAuditAction("BLOCKCHAIN_INCIDENT_VIEW", incident.incidentId, actor);
            waitForEnter();
            continue;
        }

        if (choice == 5) {
            clearInputBuffer();
            std::string incidentId;
            std::cout << "Incident ID to resolve: ";
            std::getline(std::cin, incidentId);

            BlockchainIncident incident;
            if (!findIncidentById(incidents, incidentId, incident, NULL)) {
                std::cout << ui::warning("[!] Incident ID not found.") << "\n";
                waitForEnter();
                continue;
            }

            if (toLowerCopy(incident.status) != "open") {
                std::cout << ui::warning("[!] Incident is already resolved.") << "\n";
                waitForEnter();
                continue;
            }

            const std::string note = promptIncidentNote("Resolution note (optional): ");
            const int resolveResult = resolveOpenBlockchainIncidents(actor, note, &incident.incidentId);
            if (resolveResult <= 0) {
                std::cout << ui::error("[!] Incident resolution failed.") << "\n";
                waitForEnter();
                continue;
            }

            logAuditAction("BLOCKCHAIN_INCIDENT_RESOLVED", incident.incidentId, actor);
            std::cout << ui::success("[+] Incident marked resolved.") << "\n";
            waitForEnter();
            continue;
        }

        if (choice == 6) {
            clearInputBuffer();
            std::string incidentId;
            std::cout << "Incident ID to export: ";
            std::getline(std::cin, incidentId);

            BlockchainIncident incident;
            if (!findIncidentById(incidents, incidentId, incident, NULL)) {
                std::cout << ui::warning("[!] Incident ID not found.") << "\n";
                waitForEnter();
                continue;
            }

            const std::string exportNote = promptIncidentNote("Export note (optional): ");
            const BlockchainLiveStatus liveStatus = inspectBlockchainState();
            const std::string baseName = buildIncidentExportBaseName(incident);
            const std::string txtName = baseName + ".txt";
            const std::string csvName = baseName + ".csv";

            if (!isSafeExportFileName(txtName) || !isSafeExportFileName(csvName)) {
                std::cout << ui::error("[!] Unable to build safe export filenames.") << "\n";
                waitForEnter();
                continue;
            }

            const std::string txtPath = chooseExportPath(txtName);
            const std::string csvPath = chooseExportPath(csvName);
            if (txtPath.empty() || csvPath.empty()) {
                std::cout << ui::error("[!] Unable to resolve incident export paths.") << "\n";
                waitForEnter();
                continue;
            }

            if (!storage::writeTextFileAtomically(txtPath, buildIncidentTxtReport(incident, liveStatus, actor, exportNote)) ||
                !storage::writeTextFileAtomically(csvPath, buildIncidentCsvReport(incident, liveStatus, actor, exportNote))) {
                std::cout << ui::error("[!] Unable to write incident export files.") << "\n";
                waitForEnter();
                continue;
            }

            logAuditAction("BLOCKCHAIN_INCIDENT_EXPORTED", incident.incidentId, actor);
            std::cout << ui::success("[+] Incident reports exported.") << "\n";
            std::cout << "  TXT: " << txtPath << "\n";
            std::cout << "  CSV: " << csvPath << "\n";
            waitForEnter();
            continue;
        }

        if (choice != 0) {
            std::cout << ui::warning("[!] Invalid choice.") << "\n";
            waitForEnter();
        }
    } while (choice != 0);
}

std::vector<BlockchainNodePath> getBlockchainNodePaths() {
    const std::vector<NodePath> internal = buildNodePaths();
    std::vector<BlockchainNodePath> out;
    out.reserve(internal.size());

    for (std::size_t i = 0; i < internal.size(); ++i) {
        BlockchainNodePath row;
        row.primaryPath = internal[i].primaryPath;
        row.fallbackPath = internal[i].fallbackPath;
        out.push_back(row);
    }

    return out;
}

std::vector<std::string> getBlockchainNodeRelativeFiles() {
    return buildNodeRelativeFiles();
}

bool removeBlockchainNodeForAdmin(const std::string& adminUsername) {
    const NodePath path = buildAdminNodePath(adminUsername);

    std::error_code ec;
    if (std::filesystem::exists(path.primaryPath, ec)) {
        std::filesystem::remove(path.primaryPath, ec);
        if (ec) {
            return false;
        }
    }

    ec.clear();
    if (std::filesystem::exists(path.fallbackPath, ec)) {
        std::filesystem::remove(path.fallbackPath, ec);
        if (ec) {
            return false;
        }
    }

    return true;
}
