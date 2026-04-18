#include "../include/backup.h"
#include "../include/blockchain.h"
#include "../include/ui.h"
#include "../include/audit.h"
#include "../include/storage_utils.h"

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
    "approval_rules.txt", "approval_comments.txt", "budgets.txt", "budget_entries.txt",
    "budget_approvals.txt", "audit_log.txt", "password_flags.txt",
    "delegations.txt"
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
    std::filesystem::create_directories(std::filesystem::path(dest).parent_path());
    std::ofstream out(dest, std::ios::binary);
    if (!out.is_open()) { return false; }
    out << in.rdbuf();
    out.flush();
    return out.good();
}

void clearInputBuffer() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

std::vector<std::string> buildRestoreFileList(const std::vector<std::string>& blockchainFiles) {
    std::vector<std::string> required = DATA_FILES;
    for (size_t i = 0; i < blockchainFiles.size(); ++i) {
        required.push_back(blockchainFiles[i]);
    }
    return required;
}

storage::OperationResult preflightBackupSnapshot(const std::string& selectedBackup,
                                                 const std::vector<std::string>& requiredFiles,
                                                 std::vector<std::string>& missingFiles) {
    missingFiles.clear();
    for (size_t i = 0; i < requiredFiles.size(); ++i) {
        if (!std::filesystem::exists(selectedBackup + "/" + requiredFiles[i])) {
            missingFiles.push_back(requiredFiles[i]);
        }
    }

    if (!missingFiles.empty()) {
        return storage::makeFailure("Selected backup is incomplete and cannot be restored safely.");
    }

    return storage::makeSuccess("Backup snapshot passed preflight.");
}

storage::OperationResult copyRelativeFiles(const std::string& sourceBase,
                                           const std::string& destBase,
                                           const std::vector<std::string>& relativeFiles,
                                           int& copiedCount,
                                           std::vector<std::string>& failedFiles) {
    copiedCount = 0;
    failedFiles.clear();

    for (size_t i = 0; i < relativeFiles.size(); ++i) {
        const std::string src = sourceBase + "/" + relativeFiles[i];
        const std::string dst = destBase + "/" + relativeFiles[i];
        if (!std::filesystem::exists(src)) {
            failedFiles.push_back(relativeFiles[i]);
            continue;
        }

        if (!copyFile(src, dst)) {
            failedFiles.push_back(relativeFiles[i]);
            continue;
        }

        if (!std::filesystem::exists(dst)) {
            failedFiles.push_back(relativeFiles[i]);
            continue;
        }

        copiedCount++;
    }

    if (!failedFiles.empty()) {
        return storage::makeFailure("One or more backup files could not be copied safely.");
    }

    return storage::makeSuccess("All files copied successfully.");
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
            const std::vector<std::string> blockchainFiles = getBlockchainNodeRelativeFiles();
            std::filesystem::create_directories(backupPath);

            std::vector<std::string> filesToCopy;
            for (size_t i = 0; i < DATA_FILES.size(); ++i) {
                if (std::filesystem::exists(dataDir + "/" + DATA_FILES[i])) {
                    filesToCopy.push_back(DATA_FILES[i]);
                }
            }
            for (size_t i = 0; i < blockchainFiles.size(); ++i) {
                if (std::filesystem::exists(dataDir + "/" + blockchainFiles[i])) {
                    filesToCopy.push_back(blockchainFiles[i]);
                }
            }

            int copied = 0;
            std::vector<std::string> failedFiles;
            const storage::OperationResult backupResult = copyRelativeFiles(dataDir,
                                                                            backupPath,
                                                                            filesToCopy,
                                                                            copied,
                                                                            failedFiles);

            if (!backupResult.ok) {
                std::cout << "\n" << ui::error("[!] Backup failed. Some files could not be copied safely.") << "\n";
                if (!failedFiles.empty()) {
                    std::cout << "  First failed file: " << failedFiles[0] << "\n";
                }
                logAuditAction("DATA_BACKUP_FAILED", backupPath, admin.username);
                waitForEnter();
                continue;
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
            const std::vector<std::string> blockchainFiles = getBlockchainNodeRelativeFiles();
            const std::vector<std::string> requiredFiles = buildRestoreFileList(blockchainFiles);

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

            std::vector<std::string> missingFiles;
            const storage::OperationResult preflightResult = preflightBackupSnapshot(selectedBackup, requiredFiles, missingFiles);
            if (!preflightResult.ok) {
                std::cout << "\n" << ui::error("[!] Restore blocked. The selected backup is missing required files.") << "\n";
                if (!missingFiles.empty()) {
                    std::cout << "  First missing file: " << missingFiles[0] << "\n";
                }
                logAuditAction("DATA_RESTORE_FAILED", selectedBackup, admin.username);
                waitForEnter();
                continue;
            }

            const std::string restoreStage = backupsBase + "/.restore_stage_" + generateBackupTimestamp();
            std::filesystem::remove_all(restoreStage);
            std::filesystem::create_directories(restoreStage);

            int staged = 0;
            std::vector<std::string> failedStageFiles;
            const storage::OperationResult stageResult = copyRelativeFiles(selectedBackup,
                                                                           restoreStage,
                                                                           requiredFiles,
                                                                           staged,
                                                                           failedStageFiles);
            if (!stageResult.ok || staged != static_cast<int>(requiredFiles.size())) {
                std::filesystem::remove_all(restoreStage);
                std::cout << "\n" << ui::error("[!] Restore blocked. Backup staging failed verification.") << "\n";
                if (!failedStageFiles.empty()) {
                    std::cout << "  First failed file: " << failedStageFiles[0] << "\n";
                }
                logAuditAction("DATA_RESTORE_FAILED", selectedBackup, admin.username);
                waitForEnter();
                continue;
            }

            int restored = 0;
            std::vector<std::string> failedRestoreFiles;
            const storage::OperationResult restoreResult = copyRelativeFiles(restoreStage,
                                                                             dataDir,
                                                                             requiredFiles,
                                                                             restored,
                                                                             failedRestoreFiles);
            std::filesystem::remove_all(restoreStage);

            if (!restoreResult.ok || restored != static_cast<int>(requiredFiles.size())) {
                std::cout << "\n" << ui::error("[!] Restore did not complete safely. Current data may be partially updated.") << "\n";
                if (!failedRestoreFiles.empty()) {
                    std::cout << "  First failed file: " << failedRestoreFiles[0] << "\n";
                }
                logAuditAction("DATA_RESTORE_FAILED", selectedBackup, admin.username);
                waitForEnter();
                continue;
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
