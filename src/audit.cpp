#include "../include/audit.h"

#include "../include/auth.h"

#include <fstream>
#include <iostream>
#include <ctime>

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
    std::cout << "\n==============================================================\n";
    std::cout << "  AUDIT TRAIL\n";
    std::cout << "==============================================================\n";

    std::ifstream file;
    if (!openInputFileWithFallback(file, AUDIT_FILE_PATH_PRIMARY, AUDIT_FILE_PATH_FALLBACK)) {
        std::cout << "[!] Unable to open audit log file.\n";
        waitForEnter();
        return;
    }

    std::string line;
    std::getline(file, line); // Skip header if present.

    bool hasRows = false;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        hasRows = true;
        std::cout << line << '\n';
    }

    if (!hasRows) {
        std::cout << "[!] No audit entries available yet.\n";
    }

    logAuditAction("VIEW_AUDIT_TRAIL", "MULTI", actor);
    waitForEnter();
}
