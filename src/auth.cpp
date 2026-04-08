#include "../include/auth.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

const std::string USERS_FILE_PATH = "data/users.txt";

void printAuthPageHeader(const std::string& title) {
    std::cout << "\n==============================================================\n";
    std::cout << "  " << title << "\n";
    std::cout << "==============================================================\n";
}

void clearInputBuffer() {
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

void ensureUserDataFileExists() {
    std::ifstream checkFile(USERS_FILE_PATH);
    if (checkFile.good()) {
        return;
    }

    std::ofstream createFile(USERS_FILE_PATH);
    createFile << "userID|fullName|username|password\n";
}

int countExistingUsers() {
    std::ifstream file(USERS_FILE_PATH);
    if (!file.is_open()) {
        return 0;
    }

    std::string line;
    int count = 0;

    std::getline(file, line); // Skip header.

    while (std::getline(file, line)) {
        if (!line.empty()) {
            count++;
        }
    }

    return count;
}

std::string generateNextUserId() {
    int nextNumber = countExistingUsers() + 1;

    std::ostringstream idBuilder;
    idBuilder << 'U' << std::setw(3) << std::setfill('0') << nextNumber;
    return idBuilder.str();
}

bool isUsernameTaken(const std::string& username) {
    std::ifstream file(USERS_FILE_PATH);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    std::getline(file, line); // Skip header.

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        std::stringstream parser(line);
        std::string userId;
        std::string fullName;
        std::string currentUsername;
        std::string password;

        std::getline(parser, userId, '|');
        std::getline(parser, fullName, '|');
        std::getline(parser, currentUsername, '|');
        std::getline(parser, password, '|');

        if (currentUsername == username) {
            return true;
        }
    }

    return false;
}

bool signUpCitizen() {
    clearInputBuffer();

    User newUser;
    printAuthPageHeader("CITIZEN SIGN UP");
    std::cout << "Full Name: ";
    std::getline(std::cin, newUser.fullName);

    std::cout << "Username : ";
    std::getline(std::cin, newUser.username);

    if (newUser.fullName.empty() || newUser.username.empty()) {
        std::cout << "[!] Full name and username are required.\n";
        return false;
    }

    if (isUsernameTaken(newUser.username)) {
        std::cout << "[!] Username is already taken.\n";
        return false;
    }

    std::cout << "Password : ";
    std::getline(std::cin, newUser.password);

    if (newUser.password.empty()) {
        std::cout << "[!] Password is required.\n";
        return false;
    }

    newUser.userId = generateNextUserId();

    std::ofstream file(USERS_FILE_PATH, std::ios::app);
    if (!file.is_open()) {
        std::cout << "[!] Failed to save user account.\n";
        return false;
    }

    file << newUser.userId << '|'
         << newUser.fullName << '|'
         << newUser.username << '|'
         << newUser.password << '\n';

    // Flush user data immediately so signup is persisted before returning.
    file.flush();

    std::cout << "[+] Account created successfully. Your User ID is " << newUser.userId << ".\n";
    return true;
}

bool loginCitizen() {
    clearInputBuffer();

    std::string username;
    std::string password;

    printAuthPageHeader("CITIZEN LOGIN");
    std::cout << "Username: ";
    std::getline(std::cin, username);
    std::cout << "Password: ";
    std::getline(std::cin, password);

    std::ifstream file(USERS_FILE_PATH);
    if (!file.is_open()) {
        std::cout << "[!] Unable to open user account records.\n";
        return false;
    }

    std::string line;
    std::getline(file, line); // Skip header.

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        std::stringstream parser(line);
        std::string userId;
        std::string fullName;
        std::string currentUsername;
        std::string currentPassword;

        std::getline(parser, userId, '|');
        std::getline(parser, fullName, '|');
        std::getline(parser, currentUsername, '|');
        std::getline(parser, currentPassword, '|');

        if (currentUsername == username && currentPassword == password) {
            std::cout << "[+] Welcome, " << fullName << " (" << userId << ").\n";
            return true;
        }
    }

    std::cout << "[!] Invalid username or password.\n";
    return false;
}
