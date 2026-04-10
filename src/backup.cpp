#include "../include/backup.h"
#include "../include/ui.h"
#include "../include/audit.h"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>

namespace {

const std::string DATA_DIR_PRIMARY = "data";
const std::string DATA_DIR_FALLBACK = "../data";
const std::string BACKUP_DIR_NAME = "backups";

const std::vector<std::string> DATA_FILES = {
    "users.txt", "admins.txt", "documents.txt", "approvals.txt",
    "approval_rules.txt", "budgets.txt", "budget_entries.txt",
    "budget_approvals.txt", "audit_log.txt", "password_flags.txt",
    "delegations.txt"
};

const std::vector<std::string> BLOCKCHAIN_FILES = {
    "blockchain/node1_chain.txt", "blockchain/node2_chain.txt",
    "blockchain/node3_chain.txt", "blockchain/node4_chain.txt",
    "blockchain/node5_chain.txt"
};

std::string resolveDataDir() {
    if (std::filesystem::exists(DATA_DIR_PRIMARY)) {
        return DATA_DIR_PRIMARY;
    }
    if (std::filesystem::exists(DATA_DIR_FALLBACK)) {
        return DATA_DIR_FALLBACK;
    }
    return DATA_DIR_PRIMARY;
}

std::string generateBackupTimestamp() {
    std::time_t now = std::time(NULL);
    struct std::tm* local = std::localtime(&now);
    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H%M%S", local);
    return std::string(buffer);
}

bool copyFile(const std::string& source, const std::string& dest) {
    std::ifstream in(source, std::ios::binary);
    if (!in.is_open()) { return false; }
    std::ofstream out(dest, std::ios::binary);
    if (!out.is_open()) { return false; }
    out << in.rdbuf();
    out.flush();
    return true;
}

void clearInputBuffer() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

} // namespace

void runBackupWorkspace(const Admin& admin) {
    if (admin.role != "Super Admin") {
        std::cout << ui::error("[!] Access denied. Super Admin only.") << "\n";
        waitForEnter();
        return;
    }

    int choice = -1;
    do {
        clearScreen();
        ui::printSectionTitle("DATA BACKUP AND RESTORE");
        ui::printBreadcrumb({"ADMIN", "ACCOUNT ADMIN", "BACKUP & RESTORE"});
        std::cout << "  " << ui::info("[1]") << " Create Backup\n";
        std::cout << "  " << ui::info("[2]") << " Restore from Backup\n";
        std::cout << "  " << ui::info("[0]") << " Back\n";
        std::cout << ui::muted("--------------------------------------------------------------") << "\n";
        std::cout << "  Enter your choice: ";

        std::cin >> choice;
        if (std::cin.fail()) {
            std::cin.clear();
            clearInputBuffer();
            std::cout << "\n" << ui::error("[!] Invalid input.") << "\n";
            waitForEnter();
            continue;
        }

        if (choice == 1) {
            // Create backup
            const std::string dataDir = resolveDataDir();
            const std::string backupTs = generateBackupTimestamp();
            const std::string backupPath = dataDir + "/" + BACKUP_DIR_NAME + "/" + backupTs;

            std::filesystem::create_directories(backupPath + "/blockchain");

            int copied = 0;
            for (size_t i = 0; i < DATA_FILES.size(); ++i) {
                const std::string src = dataDir + "/" + DATA_FILES[i];
                const std::string dst = backupPath + "/" + DATA_FILES[i];
                if (std::filesystem::exists(src)) {
                    if (copyFile(src, dst)) { copied++; }
                }
            }
            for (size_t i = 0; i < BLOCKCHAIN_FILES.size(); ++i) {
                const std::string src = dataDir + "/" + BLOCKCHAIN_FILES[i];
                const std::string dst = backupPath + "/" + BLOCKCHAIN_FILES[i];
                if (std::filesystem::exists(src)) {
                    if (copyFile(src, dst)) { copied++; }
                }
            }

            std::cout << "\n" << ui::success("[+] Backup created successfully.") << "\n";
            std::cout << "  Location: " << backupPath << "\n";
            std::cout << "  Files copied: " << copied << "\n";
            logAuditAction("DATA_BACKUP_CREATED", backupPath, admin.username);
            waitForEnter();

        } else if (choice == 2) {
            // Restore from backup
            const std::string dataDir = resolveDataDir();
            const std::string backupsBase = dataDir + "/" + BACKUP_DIR_NAME;

            if (!std::filesystem::exists(backupsBase)) {
                std::cout << "\n" << ui::warning("[!] No backups directory found.") << "\n";
                waitForEnter();
                continue;
            }

            std::vector<std::string> backupDirs;
            for (const auto& entry : std::filesystem::directory_iterator(backupsBase)) {
                if (entry.is_directory()) {
                    backupDirs.push_back(entry.path().filename().string());
                }
            }

            if (backupDirs.empty()) {
                std::cout << "\n" << ui::warning("[!] No backups available.") << "\n";
                waitForEnter();
                continue;
            }

            std::sort(backupDirs.begin(), backupDirs.end());

            std::cout << "\n" << ui::bold("Available Backups") << "\n";
            for (size_t i = 0; i < backupDirs.size(); ++i) {
                std::cout << "  " << ui::info("[" + std::to_string(i + 1) + "]") << " " << backupDirs[i] << "\n";
            }
            std::cout << "  " << ui::info("[0]") << " Cancel\n";
            std::cout << "\n  Select backup to restore: ";

            int restoreChoice = -1;
            std::cin >> restoreChoice;
            if (std::cin.fail() || restoreChoice < 0 || restoreChoice > static_cast<int>(backupDirs.size())) {
                std::cin.clear();
                clearInputBuffer();
                std::cout << ui::error("[!] Invalid selection.") << "\n";
                waitForEnter();
                continue;
            }

            if (restoreChoice == 0) { continue; }

            const std::string selectedBackup = backupsBase + "/" + backupDirs[static_cast<size_t>(restoreChoice - 1)];

            clearInputBuffer();
            std::cout << "\n" << ui::warning("[!] WARNING: This will overwrite all current data files.") << "\n";
            std::cout << "  Are you sure? (y/n): ";
            std::string confirm;
            std::getline(std::cin, confirm);

            if (confirm != "y" && confirm != "Y") {
                std::cout << ui::muted("  Restore cancelled.") << "\n";
                waitForEnter();
                continue;
            }

            int restored = 0;
            for (size_t i = 0; i < DATA_FILES.size(); ++i) {
                const std::string src = selectedBackup + "/" + DATA_FILES[i];
                const std::string dst = dataDir + "/" + DATA_FILES[i];
                if (std::filesystem::exists(src)) {
                    if (copyFile(src, dst)) { restored++; }
                }
            }
            for (size_t i = 0; i < BLOCKCHAIN_FILES.size(); ++i) {
                const std::string src = selectedBackup + "/" + BLOCKCHAIN_FILES[i];
                const std::string dst = dataDir + "/" + BLOCKCHAIN_FILES[i];
                if (std::filesystem::exists(src)) {
                    if (copyFile(src, dst)) { restored++; }
                }
            }

            std::cout << "\n" << ui::success("[+] Restore completed successfully.") << "\n";
            std::cout << "  Files restored: " << restored << "\n";
            logAuditAction("DATA_RESTORE_COMPLETED", selectedBackup, admin.username);
            waitForEnter();

        } else if (choice != 0) {
            std::cout << "\n" << ui::error("[!] Invalid choice.") << "\n";
            waitForEnter();
        }

    } while (choice != 0);
}
