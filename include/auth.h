#ifndef AUTH_H
#define AUTH_H

#include <string>

struct User {
    std::string userId;
    std::string fullName;
    std::string username;
    std::string password;
};

void ensureUserDataFileExists();
int countExistingUsers();
std::string generateNextUserId();
bool isUsernameTaken(const std::string& username);
bool signUpCitizen();
bool loginCitizen();

#endif
