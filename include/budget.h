#ifndef BUDGET_H
#define BUDGET_H

#include <string>
#include <vector>
#include "auth.h"

void ensureBudgetConsensusFilesExist();
void viewBudgetAllocations(const std::string& actor);
void viewBudgetVarianceReport(const std::string& actor);
void manageBudgetsForAdmin(const Admin& admin);
bool getPendingBudgetApprovalIdsForApprover(const std::string& approverUsername,
											std::vector<std::string>& outEntryIds);

#endif
