#ifndef APPROVALS_H
#define APPROVALS_H

#include <string>
#include "auth.h"

struct Approval {
    std::string docId;
    std::string approverUsername;
    std::string role;
    std::string status;
    std::string createdAt;
    std::string decidedAt;
};

void ensureApprovalsDataFileExists();
void createApprovalRequestsForDocument(const std::string& docId, const std::string& uploader);
void viewPendingApprovalsForAdmin(const Admin& admin);
void viewApprovalAnalyticsDashboard(const Admin& admin);
void approveDocumentAsAdmin(const Admin& admin);
void rejectDocumentAsAdmin(const Admin& admin);

#endif
