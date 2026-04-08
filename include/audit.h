#ifndef AUDIT_H
#define AUDIT_H

#include <string>

std::string getCurrentTimestamp();
void logAuditAction(const std::string& action, const std::string& targetId, const std::string& actor);
void viewAuditTrail(const std::string& actor);

#endif
