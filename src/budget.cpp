#include "../include/budget.h"

#include "../include/audit.h"
#include "../include/auth.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {
const std::string BUDGETS_FILE_PATH_PRIMARY = "data/budgets.txt";
const std::string BUDGETS_FILE_PATH_FALLBACK = "../data/budgets.txt";

bool openInputFileWithFallback(std::ifstream& file, const std::string& primaryPath, const std::string& fallbackPath) {
    file.open(primaryPath);
    if (file.is_open()) {
        return true;
    }

    file.clear();
    file.open(fallbackPath);
    return file.is_open();
}
} // namespace

void viewBudgetAllocations(const std::string& actor) {
    clearScreen();
    std::cout << "\n==============================================================\n";
    std::cout << "  PROCUREMENT BUDGET ALLOCATIONS\n";
    std::cout << "==============================================================\n";

    std::ifstream file;
    if (!openInputFileWithFallback(file, BUDGETS_FILE_PATH_PRIMARY, BUDGETS_FILE_PATH_FALLBACK)) {
        std::cout << "[!] Unable to open budgets file.\n";
        logAuditAction("VIEW_BUDGETS_FAILED", "N/A", actor);
        waitForEnter();
        return;
    }

    std::string line;
    std::getline(file, line); // Skip header.

    bool hasBudgetRows = false;
    double totalBudget = 0.0;

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        std::stringstream parser(line);
        std::string category;
        std::string amount;
        std::getline(parser, category, '|');
        std::getline(parser, amount, '|');

        hasBudgetRows = true;
        double parsedAmount = 0.0;
        std::stringstream amountParser(amount);
        amountParser >> parsedAmount;
        totalBudget += parsedAmount;

        std::cout << "Category: " << category << '\n';
        std::cout << "Amount  : PHP " << std::fixed << std::setprecision(2) << parsedAmount << "\n";
        std::cout << "--------------------------------------------------------------\n";
    }

    if (!hasBudgetRows) {
        std::cout << "\n[!] No budget entries available.\n";
        logAuditAction("VIEW_BUDGETS_EMPTY", "N/A", actor);
    } else {
        std::cout << "Total   : PHP " << std::fixed << std::setprecision(2) << totalBudget << "\n";
        logAuditAction("VIEW_BUDGETS", "MULTI", actor);
    }

    waitForEnter();
}
