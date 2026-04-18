#ifndef VERIFICATION_H
#define VERIFICATION_H

#include <string>

std::string computeSimpleHash(const std::string& text);
std::string computeFileHashSha256(const std::string& filePath);
bool verifyDocumentHashAgainstBlockchain(const std::string& hashValue, const std::string& docId);
void verifyDocumentIntegrity(const std::string& actor);
void verifyPublishedDocumentAsCitizen(const std::string& actor);

#endif
