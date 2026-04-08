#ifndef BUDGET_H
#define BUDGET_H

#include <string>
#include "auth.h"

void viewBudgetAllocations(const std::string& actor);
void manageBudgetsForAdmin(const Admin& admin);

#endif
