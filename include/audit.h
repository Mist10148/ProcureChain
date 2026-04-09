#ifndef AUDIT_H
#define AUDIT_H

#include <string>

std::string getCurrentTimestamp();
void logAuditAction(const std::string& action, const std::string& targetId, const std::string& actor, int chainIndex = -1);
void viewAuditTrail(const std::string& actor);
void exportAuditTrailCsv(const std::string& actor);

#endif
