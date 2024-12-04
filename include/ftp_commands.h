//
// Created by iliut on 12/4/24.
//

#ifndef FTP_COMMANDS_H
#define FTP_COMMANDS_H

#include "common.h"

void handlePortCommand(const std::vector<std::string>& tokens, sockaddr_in& dataAddr, int& dataSocket, int clientSocket);
int startPassiveDataConnection(sockaddr_in& dataAddr, int& dataSocket, int clientSocket);
void handleRetrCommand(const std::string& filename, int dataClientSocket, int clientSocket, const std::string& transferType);
void handleStorCommand(const std::string& filename, int dataClientSocket, int clientSocket, const std::string& transferType);
void handlePwdCommand(int clientSocket);
void handleCwdCommand(const std::vector<std::string>& tokens, int clientSocket);
void handleMkdCommand(const std::vector<std::string>& tokens, int clientSocket);
void handleMdtmCommand(const std::vector<std::string>& tokens, int clientSocket);
void handleTypeCommand(const std::vector<std::string>& tokens, int clientSocket);
void handleSizeCommand(const std::vector<std::string>& tokens, int clientSocket);
void handleListCommand(int dataClientSocket, int clientSocket);
void handleClient(int clientSocket);

#endif // FTP_COMMANDS_H

