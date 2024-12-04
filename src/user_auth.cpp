//
// Created by iliut on 12/4/24.
//

#include "user_auth.h"

#include <iostream>

bool verifyPassword(const std::string& username, const std::string& password) {
    std::ifstream credentialsFile("credentials.txt");
    if (!credentialsFile.is_open()) {
        std::cerr << "Could not open credentials file.\n";
        return false;
    }

    std::string line;
    while (std::getline(credentialsFile, line)) {
        size_t delimiterPos = line.find(':');
        if (delimiterPos == std::string::npos) {
            continue; // skip malformed lines
        }

        std::string storedUsername = line.substr(0, delimiterPos);
        std::string storedHashedPassword = line.substr(delimiterPos + 1);

        if (storedUsername == username) {
            int result = argon2id_verify(
                storedHashedPassword.c_str(),
                password.c_str(),
                password.length()
            );

            return result == ARGON2_OK;
        }
    }

    return false; // User not found
}

