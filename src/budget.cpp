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
#include <map>
#include <sstream>
#include <vector>

namespace {
const std::string BUDGETS_FILE_PATH_PRIMARY = "data/budgets.txt";
const std::string BUDGETS_FILE_PATH_FALLBACK = "../data/budgets.txt";
const std::string DOCUMENTS_FILE_PATH_PRIMARY = "data/documents.txt";
const std::string DOCUMENTS_FILE_PATH_FALLBACK = "../data/documents.txt";

struct BudgetRow {
    std::string category;
    double amount;
};

struct VarianceRow {
    std::string category;
    double allocated;
    double actual;
    double variance;
    double utilization;
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

std::vector<std::string> splitPipe(const std::string& line) {
    std::vector<std::string> tokens;
    std::stringstream parser(line);
    std::string token;
    while (std::getline(parser, token, '|')) {
        tokens.push_back(token);
    }
    return tokens;
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
    bool firstLine = true;

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        if (firstLine) {
            firstLine = false;
            if (line.find("category|") == 0) {
                continue;
            }
        }

        const std::vector<std::string> tokens = splitPipe(line);
        if (tokens.empty()) {
            continue;
        }

        BudgetRow row;
        row.category = tokens[0];
        row.amount = 0.0;

        if (tokens.size() > 1) {
            std::stringstream amountParser(tokens[1]);
            amountParser >> row.amount;
        }

        if (!row.category.empty()) {
            rows.push_back(row);
        }
    }

    return true;
}

bool saveBudgetRows(const std::vector<BudgetRow>& rows) {
    std::string targetPath = resolveDataPath(BUDGETS_FILE_PATH_PRIMARY, BUDGETS_FILE_PATH_FALLBACK);
    std::ofstream writer(targetPath);
    if (!writer.is_open()) {
        return false;
    }

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
        const double share = total > 0.0 ? (rows[i].amount / total) * 100.0 : 0.0;
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

std::map<std::string, double> loadActualSpendByCategory() {
    std::map<std::string, double> totals;

    std::ifstream file;
    if (!openInputFileWithFallback(file, DOCUMENTS_FILE_PATH_PRIMARY, DOCUMENTS_FILE_PATH_FALLBACK)) {
        return totals;
    }

    std::string line;
    bool firstLine = true;

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        if (firstLine) {
            firstLine = false;
            if (line.find("docID|") == 0) {
                continue;
            }
        }

        const std::vector<std::string> tokens = splitPipe(line);
        if (tokens.size() < 8) {
            continue;
        }

        const std::string status = tokens[6];
        if (status != "approved" && status != "published") {
            continue;
        }

        const std::string fallbackCategory = tokens[2];
        const std::string budgetCategory = tokens.size() > 8 && !tokens[8].empty() ? tokens[8] : fallbackCategory;

        double amount = 0.0;
        if (tokens.size() > 9) {
            std::stringstream amountIn(tokens[9]);
            amountIn >> amount;
        }

        if (!budgetCategory.empty() && amount > 0.0) {
            totals[budgetCategory] += amount;
        }
    }

    return totals;
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

    std::cout << "\n" << ui::success("Total Budget: PHP ") << std::fixed << std::setprecision(2) << totalBudget << "\n";
    logAuditAction("VIEW_BUDGETS", "MULTI", actor);

    waitForEnter();
}

void viewBudgetVarianceReport(const std::string& actor) {
    clearScreen();
    ui::printSectionTitle("BUDGET VARIANCE REPORT");

    std::vector<BudgetRow> budgets;
    if (!loadBudgetRows(budgets)) {
        std::cout << ui::error("[!] Unable to open budgets file.") << "\n";
        logAuditAction("BUDGET_VARIANCE_FAILED", "N/A", actor);
        waitForEnter();
        return;
    }

    if (budgets.empty()) {
        std::cout << ui::warning("[!] No budget rows available.") << "\n";
        waitForEnter();
        return;
    }

    const std::map<std::string, double> actuals = loadActualSpendByCategory();
    std::vector<VarianceRow> report;

    for (size_t i = 0; i < budgets.size(); ++i) {
        VarianceRow row;
        row.category = budgets[i].category;
        row.allocated = budgets[i].amount;

        std::map<std::string, double>::const_iterator found = actuals.find(row.category);
        row.actual = (found != actuals.end()) ? found->second : 0.0;
        row.variance = row.allocated - row.actual;
        row.utilization = row.allocated > 0.0 ? (row.actual / row.allocated) * 100.0 : 0.0;
        report.push_back(row);
    }

    const std::vector<std::string> headers = {"Category", "Allocated", "Actual", "Variance", "Utilization"};
    const std::vector<int> widths = {24, 12, 12, 12, 12};
    ui::printTableHeader(headers, widths);

    for (size_t i = 0; i < report.size(); ++i) {
        std::ostringstream allocated;
        std::ostringstream actual;
        std::ostringstream variance;
        std::ostringstream utilization;

        allocated << std::fixed << std::setprecision(2) << report[i].allocated;
        actual << std::fixed << std::setprecision(2) << report[i].actual;
        variance << std::fixed << std::setprecision(2) << report[i].variance;
        utilization << std::fixed << std::setprecision(1) << report[i].utilization << "%";

        ui::printTableRow({report[i].category, allocated.str(), actual.str(), variance.str(), utilization.str()}, widths);
    }
    ui::printTableFooter(widths);

    double maxActual = 1.0;
    for (size_t i = 0; i < report.size(); ++i) {
        if (report[i].actual > maxActual) {
            maxActual = report[i].actual;
        }
    }

    std::cout << "\n" << ui::bold("Actual Spend By Category") << "\n";
    for (size_t i = 0; i < report.size(); ++i) {
        ui::printBar(report[i].category, report[i].actual, maxActual, 24);
    }

    logAuditAction("BUDGET_VARIANCE_VIEW", "MULTI", actor);
    waitForEnter();
}

void manageBudgetsForAdmin(const Admin& admin) {
    int choice = -1;

    do {
        clearScreen();
        ui::printSectionTitle("ADMIN BUDGET MANAGEMENT");
        std::cout << "  " << ui::info("[1]") << " View Budget Summary\n";
        std::cout << "  " << ui::info("[2]") << " Add Budget Category\n";
        std::cout << "  " << ui::info("[3]") << " Update Budget Amount\n";
        std::cout << "  " << ui::info("[4]") << " View Budget Variance Report\n";
        std::cout << "  " << ui::info("[0]") << " Back to Admin Dashboard\n";
        std::cout << ui::muted("--------------------------------------------------------------") << "\n";
        std::cout << "  Enter your choice: ";

        std::cin >> choice;
        if (std::cin.fail()) {
            std::cin.clear();
            clearInputBuffer();
            std::cout << "\n" << ui::warning("[!] Invalid input. Please enter a number from the menu.") << "\n";
            waitForEnter();
            continue;
        }

        if (choice == 1) {
            viewBudgetAllocations(admin.username);
            continue;
        }

        if (choice == 4) {
            viewBudgetVarianceReport(admin.username);
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
                clearInputBuffer();
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

            const int chainIndex = appendBlockchainAction("BUDGET_ADD", category, admin.username);
            logAuditAction("BUDGET_ADD", category, admin.username, chainIndex);
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
                clearInputBuffer();
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

            const int chainIndex = appendBlockchainAction("BUDGET_UPDATE", category, admin.username);
            logAuditAction("BUDGET_UPDATE", category, admin.username, chainIndex);
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
