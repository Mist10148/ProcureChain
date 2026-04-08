#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include <string>

void ensureBlockchainNodeFilesExist();
void appendBlockchainAction(const std::string& action, const std::string& docId, const std::string& actor);
void validateBlockchainNodes(const std::string& actor);

#endif
