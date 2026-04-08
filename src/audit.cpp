#include "../include/audit.h"

#include "../include/auth.h"
#include "../include/ui.h"

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <ctime>
#include <vector>

namespace {
const std::string AUDIT_FILE_PATH_PRIMARY = "data/audit_log.txt";
const std::string AUDIT_FILE_PATH_FALLBACK = "../data/audit_log.txt";

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
} // namespace

std::string getCurrentTimestamp() {
    std::time_t now = std::time(NULL);
    std::tm localTime;
    localtime_s(&localTime, &now);

    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &localTime);
    return std::string(buffer);
}

void logAuditAction(const std::string& action, const std::string& targetId, const std::string& actor) {
    std::ofstream file;
    if (!openAppendFileWithFallback(file, AUDIT_FILE_PATH_PRIMARY, AUDIT_FILE_PATH_FALLBACK)) {
        return;
    }

    // Each row is append-only to keep a simple historical trace.
    file << getCurrentTimestamp() << '|' << action << '|' << targetId << '|' << actor << '\n';
    file.flush();
}

void viewAuditTrail(const std::string& actor) {
    clearScreen();
    ui::printSectionTitle("AUDIT TRAIL");

    std::ifstream file;
    if (!openInputFileWithFallback(file, AUDIT_FILE_PATH_PRIMARY, AUDIT_FILE_PATH_FALLBACK)) {
        std::cout << ui::error("[!] Unable to open audit log file.") << "\n";
        waitForEnter();
        return;
    }

    std::string line;
    std::getline(file, line); // Skip header if present.

    bool hasRows = false;
    std::vector<std::vector<std::string> > rows;
    std::map<std::string, double> actionCounts;

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        hasRows = true;
        std::stringstream parser(line);
        std::string timestamp;
        std::string action;
        std::string target;
        std::string actorName;

        std::getline(parser, timestamp, '|');
        std::getline(parser, action, '|');
        std::getline(parser, target, '|');
        std::getline(parser, actorName, '|');

        rows.push_back(std::vector<std::string>());
        rows.back().push_back(timestamp);
        rows.back().push_back(action);
        rows.back().push_back(target);
        rows.back().push_back(actorName);
        actionCounts[action] += 1.0;
    }

    if (!hasRows) {
        std::cout << ui::warning("[!] No audit entries available yet.") << "\n";
    } else {
        const std::vector<std::string> headers = {"Timestamp", "Action", "Target", "Actor"};
        const std::vector<int> widths = {19, 26, 10, 14};

        ui::printTableHeader(headers, widths);
        for (size_t i = 0; i < rows.size(); ++i) {
            ui::printTableRow(rows[i], widths);
        }
        ui::printTableFooter(widths);

        std::cout << "\n" << ui::bold("Action Frequency Chart") << "\n";
        double maxCount = 0.0;
        for (std::map<std::string, double>::const_iterator it = actionCounts.begin(); it != actionCounts.end(); ++it) {
            if (it->second > maxCount) {
                maxCount = it->second;
            }
        }
        for (std::map<std::string, double>::const_iterator it = actionCounts.begin(); it != actionCounts.end(); ++it) {
            ui::printBar(it->first, it->second, maxCount, 24);
        }
    }

    logAuditAction("VIEW_AUDIT_TRAIL", "MULTI", actor);
    waitForEnter();
}
