#include "../include/budget.h"

#include "../include/audit.h"
#include "../include/auth.h"
#include "../include/blockchain.h"
#include "../include/ui.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>

namespace {
const std::string BUDGETS_FILE_PATH_PRIMARY = "data/budgets.txt";
const std::string BUDGETS_FILE_PATH_FALLBACK = "../data/budgets.txt";

struct BudgetRow {
    std::string category;
    double amount;
};

bool openInputFileWithFallback(std::ifstream& file, const std::string& primaryPath, const std::string& fallbackPath) {
    file.open(primaryPath);
    if (file.is_open()) {
        return true;
    }

    file.clear();
    file.open(fallbackPath);
    return file.is_open();
}

std::string resolveDataPath(const std::string& primaryPath, const std::string& fallbackPath) {
    std::ifstream primary(primaryPath);
    if (primary.is_open()) {
        return primaryPath;
    }

    std::ifstream fallback(fallbackPath);
    if (fallback.is_open()) {
        return fallbackPath;
    }

    return primaryPath;
}

void clearInputBuffer() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

bool loadBudgetRows(std::vector<BudgetRow>& rows) {
    std::ifstream file;
    if (!openInputFileWithFallback(file, BUDGETS_FILE_PATH_PRIMARY, BUDGETS_FILE_PATH_FALLBACK)) {
        return false;
    }

    rows.clear();
    std::string line;
    std::getline(file, line); // Skip header.

    // Parse line-by-line and skip malformed entries so one bad row does not break the menu.
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        std::stringstream parser(line);
        BudgetRow row;
        std::string amountText;
        std::getline(parser, row.category, '|');
        std::getline(parser, amountText, '|');

        if (row.category.empty()) {
            continue;
        }

        row.amount = 0.0;
        std::stringstream amountParser(amountText);
        amountParser >> row.amount;
        rows.push_back(row);
    }

    return true;
}

bool saveBudgetRows(const std::vector<BudgetRow>& rows) {
    std::string targetPath = resolveDataPath(BUDGETS_FILE_PATH_PRIMARY, BUDGETS_FILE_PATH_FALLBACK);
    std::ofstream writer(targetPath);
    if (!writer.is_open()) {
        return false;
    }

    // The file is rewritten from memory to keep one consistent canonical format.
    writer << "category|amount\n";
    for (size_t i = 0; i < rows.size(); ++i) {
        writer << rows[i].category << '|' << std::fixed << std::setprecision(2) << rows[i].amount << '\n';
    }
    writer.flush();

    return true;
}

void printBudgetTable(const std::vector<BudgetRow>& rows) {
    const std::vector<std::string> headers = {"Category", "Amount (PHP)", "Share"};
    const std::vector<int> widths = {28, 14, 8};

    double total = 0.0;
    for (size_t i = 0; i < rows.size(); ++i) {
        total += rows[i].amount;
    }

    ui::printTableHeader(headers, widths);
    for (size_t i = 0; i < rows.size(); ++i) {
        std::ostringstream amountOut;
        amountOut << std::fixed << std::setprecision(2) << rows[i].amount;

        std::ostringstream shareOut;
        double share = total > 0.0 ? (rows[i].amount / total) * 100.0 : 0.0;
        shareOut << std::fixed << std::setprecision(1) << share << "%";

        ui::printTableRow({rows[i].category, amountOut.str(), shareOut.str()}, widths);
    }
    ui::printTableFooter(widths);
}

void printBudgetChart(const std::vector<BudgetRow>& rows) {
    if (rows.empty()) {
        return;
    }

    double maxAmount = 0.0;
    for (size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].amount > maxAmount) {
            maxAmount = rows[i].amount;
        }
    }

    std::cout << "\n" << ui::bold("Budget Allocation Chart") << "\n";
    for (size_t i = 0; i < rows.size(); ++i) {
        ui::printBar(rows[i].category, rows[i].amount, maxAmount, 24);
    }
}
} // namespace

void viewBudgetAllocations(const std::string& actor) {
    clearScreen();
    ui::printSectionTitle("PROCUREMENT BUDGET ALLOCATIONS");

    std::vector<BudgetRow> rows;
    if (!loadBudgetRows(rows)) {
        std::cout << ui::error("[!] Unable to open budgets file.") << "\n";
        logAuditAction("VIEW_BUDGETS_FAILED", "N/A", actor);
        waitForEnter();
        return;
    }

    if (rows.empty()) {
        std::cout << "\n" << ui::warning("[!] No budget entries available.") << "\n";
        logAuditAction("VIEW_BUDGETS_EMPTY", "N/A", actor);
        waitForEnter();
        return;
    }

    double totalBudget = 0.0;
    for (size_t i = 0; i < rows.size(); ++i) {
        totalBudget += rows[i].amount;
    }

    std::sort(rows.begin(), rows.end(), [](const BudgetRow& a, const BudgetRow& b) {
        return a.amount > b.amount;
    });

    printBudgetTable(rows);
    printBudgetChart(rows);

    std::cout << "\n" << ui::success("Total Budget: PHP ")
              << std::fixed << std::setprecision(2) << totalBudget << "\n";
    logAuditAction("VIEW_BUDGETS", "MULTI", actor);

    waitForEnter();
}

void manageBudgetsForAdmin(const Admin& admin) {
    int choice = -1;

    do {
        // This submenu intentionally loops so admins can perform multiple budget edits per session.
        clearScreen();
        ui::printSectionTitle("ADMIN BUDGET MANAGEMENT");
        std::cout << "  " << ui::info("[1]") << " View Budget Summary\n";
        std::cout << "  " << ui::info("[2]") << " Add Budget Category\n";
        std::cout << "  " << ui::info("[3]") << " Update Budget Amount\n";
        std::cout << "  " << ui::info("[0]") << " Back to Admin Dashboard\n";
        std::cout << ui::muted("--------------------------------------------------------------") << "\n";
        std::cout << "  Enter your choice: ";

        std::cin >> choice;
        if (std::cin.fail()) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "\n" << ui::warning("[!] Invalid input. Please enter a number from the menu.") << "\n";
            waitForEnter();
            continue;
        }

        if (choice == 1) {
            viewBudgetAllocations(admin.username);
            continue;
        }

        if (choice == 2) {
            clearScreen();
            ui::printSectionTitle("ADD BUDGET CATEGORY");

            clearInputBuffer();

            std::string category;
            std::cout << "Category Name: ";
            std::getline(std::cin, category);

            if (category.empty()) {
                std::cout << ui::warning("[!] Category name is required.") << "\n";
                logAuditAction("BUDGET_ADD_FAILED", "N/A", admin.username);
                waitForEnter();
                continue;
            }

            double amount = 0.0;
            std::cout << "Amount (PHP): ";
            std::cin >> amount;
            if (std::cin.fail() || amount < 0.0) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << ui::warning("[!] Invalid budget amount.") << "\n";
                logAuditAction("BUDGET_ADD_FAILED", category, admin.username);
                waitForEnter();
                continue;
            }

            std::vector<BudgetRow> rows;
            if (!loadBudgetRows(rows)) {
                std::cout << ui::error("[!] Unable to open budgets file.") << "\n";
                logAuditAction("BUDGET_ADD_FAILED", category, admin.username);
                waitForEnter();
                continue;
            }

            bool exists = false;
            for (size_t i = 0; i < rows.size(); ++i) {
                if (rows[i].category == category) {
                    exists = true;
                    break;
                }
            }

            if (exists) {
                std::cout << ui::warning("[!] Category already exists. Use update instead.") << "\n";
                logAuditAction("BUDGET_ADD_DUPLICATE", category, admin.username);
                waitForEnter();
                continue;
            }

            BudgetRow newRow;
            newRow.category = category;
            newRow.amount = amount;
            rows.push_back(newRow);

            if (!saveBudgetRows(rows)) {
                std::cout << ui::error("[!] Failed to save budget changes.") << "\n";
                logAuditAction("BUDGET_ADD_FAILED", category, admin.username);
                waitForEnter();
                continue;
            }

            appendBlockchainAction("BUDGET_ADD", category, admin.username);
            logAuditAction("BUDGET_ADD", category, admin.username);
            std::cout << ui::success("[+] Budget category added successfully.") << "\n";
            waitForEnter();
            continue;
        }

        if (choice == 3) {
            clearScreen();
            ui::printSectionTitle("UPDATE BUDGET AMOUNT");

            clearInputBuffer();

            std::string category;
            std::cout << "Category Name to Update: ";
            std::getline(std::cin, category);

            if (category.empty()) {
                std::cout << ui::warning("[!] Category name is required.") << "\n";
                logAuditAction("BUDGET_UPDATE_FAILED", "N/A", admin.username);
                waitForEnter();
                continue;
            }

            std::vector<BudgetRow> rows;
            if (!loadBudgetRows(rows)) {
                std::cout << ui::error("[!] Unable to open budgets file.") << "\n";
                logAuditAction("BUDGET_UPDATE_FAILED", category, admin.username);
                waitForEnter();
                continue;
            }

            bool found = false;
            size_t rowIndex = 0;
            for (size_t i = 0; i < rows.size(); ++i) {
                if (rows[i].category == category) {
                    found = true;
                    rowIndex = i;
                    break;
                }
            }

            if (!found) {
                std::cout << ui::warning("[!] Category not found.") << "\n";
                logAuditAction("BUDGET_UPDATE_NOT_FOUND", category, admin.username);
                waitForEnter();
                continue;
            }

            double newAmount = 0.0;
            std::cout << "New Amount (PHP): ";
            std::cin >> newAmount;
            if (std::cin.fail() || newAmount < 0.0) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << ui::warning("[!] Invalid budget amount.") << "\n";
                logAuditAction("BUDGET_UPDATE_FAILED", category, admin.username);
                waitForEnter();
                continue;
            }

            rows[rowIndex].amount = newAmount;

            if (!saveBudgetRows(rows)) {
                std::cout << ui::error("[!] Failed to save budget changes.") << "\n";
                logAuditAction("BUDGET_UPDATE_FAILED", category, admin.username);
                waitForEnter();
                continue;
            }

            appendBlockchainAction("BUDGET_UPDATE", category, admin.username);
            logAuditAction("BUDGET_UPDATE", category, admin.username);
            std::cout << ui::success("[+] Budget amount updated successfully.") << "\n";
            waitForEnter();
            continue;
        }

        if (choice == 0) {
            break;
        }

        std::cout << "\n" << ui::warning("[!] Invalid menu choice.") << "\n";
        waitForEnter();

    } while (choice != 0);
}
