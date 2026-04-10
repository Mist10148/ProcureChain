#ifndef APPROVALS_H
#define APPROVALS_H

#include <string>
#include <vector>
#include "auth.h"

struct Approval {
    std::string docId;
    std::string approverUsername;
    std::string role;
    std::string status;
    std::string createdAt;
    std::string decidedAt;
    std::string note;
};

struct ApprovalRule {
    std::string category;
    std::vector<std::string> requiredRoles;
    int maxDecisionDays;
};

void ensureApprovalsDataFileExists();
void ensureApprovalRulesDataFileExists();
void createApprovalRequestsForDocument(const std::string& docId, const std::string& uploader, const std::string& category);
void viewPendingApprovalsForAdmin(const Admin& admin);
void viewApprovalAnalyticsDashboard(const Admin& admin);
void viewEscalationQueueForAdmin(const Admin& admin);
void approveDocumentAsAdmin(const Admin& admin);
void rejectDocumentAsAdmin(const Admin& admin);
void manageApprovalRulesForAdmin(const Admin& admin);

#endif
