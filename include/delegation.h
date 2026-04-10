#ifndef DELEGATION_H
#define DELEGATION_H

#include <string>
#include <vector>
#include "auth.h"

struct Delegation {
    std::string delegatorUsername;
    std::string delegateeUsername;
    std::string startDate;
    std::string endDate;
    std::string status;
};

void ensureDelegationFileExists();
std::vector<Delegation> getActiveDelegationsFor(const std::string& delegateeUsername);
void runDelegationManagement(const Admin& admin);

#endif
