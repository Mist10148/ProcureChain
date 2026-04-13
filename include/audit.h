#ifndef AUDIT_H
#define AUDIT_H

#include <string>

struct Admin;

struct AuditLogMetadata {
    std::string targetType;
    std::string actorRole;
    std::string outcome;
    std::string visibility;
    int chainIndex;

    AuditLogMetadata() : chainIndex(-1) {}
};

std::string getCurrentTimestamp();
void ensureAuditTrailHashChain();
void logAuditAction(const std::string& action, const std::string& targetId, const std::string& actor, int chainIndex = -1);
void logAuditActionDetailed(const std::string& action,
                           const std::string& targetId,
                           const std::string& actor,
                           const AuditLogMetadata& metadata);
void viewAuditTrail(const std::string& actor);
void viewPublicAuditTrail(const std::string& actorUsername);
void viewInternalAuditLog(const Admin& admin);
void exportAuditTrailCsv(const std::string& actor);

#endif
