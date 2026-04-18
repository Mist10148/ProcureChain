#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include <string>
#include <vector>

struct BlockchainNodePath {
	std::string primaryPath;
	std::string fallbackPath;
};

void ensureBlockchainNodeFilesExist();
int appendBlockchainAction(const std::string& action, const std::string& docId, const std::string& actor);
void validateBlockchainNodes(const std::string& actor);
void viewBlockchainExplorer(const std::string& actor);
void repairBlockchainNodes(const std::string& actor);
std::vector<BlockchainNodePath> getBlockchainNodePaths();
std::vector<std::string> getBlockchainNodeRelativeFiles();
bool removeBlockchainNodeForAdmin(const std::string& adminUsername);

#endif
