#ifndef AUTH_H
#define AUTH_H

#include <string>

struct User {
    std::string userId;
    std::string fullName;
    std::string username;
    std::string password;
    std::string status;
    std::string updatedAt;
};

struct Admin {
    std::string adminId;
    std::string fullName;
    std::string username;
    std::string password;
    std::string role;
    std::string status;
    std::string updatedAt;
};

void clearScreen();
void waitForEnter();
void ensureUserDataFileExists();
int countExistingUsers();
std::string generateNextUserId();
int countExistingAdmins();
std::string generateNextAdminId();
bool isUsernameTaken(const std::string& username);
bool signUpCitizen();
bool signUpAdmin();
void signUpAccount();
bool loginCitizen(User& loggedInUser);
bool loginCitizen();
bool loginAdmin(Admin& loggedInAdmin);
bool loginAdmin();
void manageAccountLifecycleForAdmin(const Admin& admin);
bool checkAndForcePasswordChange(const std::string& username, bool isAdmin);

#endif
