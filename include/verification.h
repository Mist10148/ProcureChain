#ifndef VERIFICATION_H
#define VERIFICATION_H

#include <string>

std::string computeSimpleHash(const std::string& text);
void verifyDocumentIntegrity(const std::string& actor);

#endif
