//
// Created by iliut on 12/4/24.
//

#include "common.h"

std::vector<std::string> splitCommand(const std::string& command) {
    std::string trimmedCommand = command;

    if (trimmedCommand.size() >= 2 &&
        trimmedCommand[trimmedCommand.size() - 2] == '\r' &&
        trimmedCommand[trimmedCommand.size() - 1] == '\n') {
        trimmedCommand.erase(trimmedCommand.size() - 2);
        }

    std::istringstream stream(trimmedCommand);
    std::vector<std::string> tokens;
    std::string token;
    while (std::getline(stream, token, ' ')) {
        tokens.push_back(token);
    }
    return tokens;
}

