#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <sstream>
#include <netinet/in.h>
#include <unistd.h>

#define CONTROL_PORT 2123
#define BUFFER_SIZE 1024

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

void handlePortCommand(const std::vector<std::string>& tokens, sockaddr_in& dataAddr, int& dataSocket, int clientSocket) {
    if (tokens.size() < 2) {
        send(clientSocket, "501 Syntax error in parameters or arguments.\r\n", 46, 0);
        return;
    }

    std::string hostPort = tokens[1];
    std::replace(hostPort.begin(), hostPort.end(), ',', ' ');

    std::istringstream stream(hostPort);
    int p1, p2;
    std::vector<int> parts(6);
    for (int i = 0; i < 6; ++i) {
        stream >> parts[i];
    }

    dataAddr.sin_family = AF_INET;
    dataAddr.sin_port = htons((parts[4] * 256) + parts[5]);

    dataSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (dataSocket < 0) {
        perror("Data socket creation failed");
        send(clientSocket, "425 Can't open data connection.\r\n", 34, 0);
        return;
    }

    if (connect(dataSocket, (struct sockaddr*)&dataAddr, sizeof(dataAddr)) < 0) {
        perror("Data connection failed");
        send(clientSocket, "425 Can't open data connection.\r\n", 34, 0);
        close(dataSocket);
        dataSocket = -1;
        return;
    }

    send(clientSocket, "200 PORT command successful.\r\n", 30, 0);
}

int startPassiveDataConnection(sockaddr_in& dataAddr, int& dataSocket, int clientSocket) {
    dataSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (dataSocket < 0) {
        perror("Data socket creation failed");
        send(clientSocket, "425 Can't open data connection.\r\n", 34, 0);
        return -1;
    }

    dataAddr.sin_family = AF_INET;
    dataAddr.sin_addr.s_addr = INADDR_ANY;
    dataAddr.sin_port = htons(0); // Let OS choose an available port

    if (bind(dataSocket, (struct sockaddr*)&dataAddr, sizeof(dataAddr)) < 0) {
        perror("Data socket bind failed");
        send(clientSocket, "425 Can't open data connection.\r\n", 34, 0);
        close(dataSocket);
        return -1;
    }

    if (listen(dataSocket, 1) < 0) {
        perror("Data socket listen failed");
        send(clientSocket, "425 Can't open data connection.\r\n", 34, 0);
        close(dataSocket);
        return -1;
    }

    socklen_t len = sizeof(dataAddr);
    getsockname(dataSocket, (struct sockaddr*)&dataAddr, &len);

    int p1 = ntohs(dataAddr.sin_port) / 256;
    int p2 = ntohs(dataAddr.sin_port) % 256;

    char response[BUFFER_SIZE];
    snprintf(response, BUFFER_SIZE, "227 Entering Passive Mode (127,0,0,1,%d,%d).\r\n", p1, p2);
    send(clientSocket, response, strlen(response), 0);

    return dataSocket;
}

void handleRetrCommand(const std::string& filename, int dataSocket, int clientSocket) {
    FILE* file = fopen(filename.c_str(), "rb");
    if (!file) {
        perror("File open failed");
        send(clientSocket, "550 File not found or access denied.\r\n", 39, 0);
        return;
    }

    send(clientSocket, "150 Opening data connection.\r\n", 30, 0);

    char buffer[BUFFER_SIZE];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        send(dataSocket, buffer, bytesRead, 0);
    }

    fclose(file);
    close(dataSocket);

    send(clientSocket, "226 Transfer complete.\r\n", 24, 0);
}

void handleStorCommand(const std::string& filename, int dataSocket, int clientSocket) {
    FILE* file = fopen(filename.c_str(), "wb");
    if (!file) {
        perror("File open failed");
        send(clientSocket, "550 Could not create file.\r\n", 28, 0);
        return;
    }

    send(clientSocket, "150 Opening data connection.\r\n", 30, 0);

    char buffer[BUFFER_SIZE];
    ssize_t bytesRead;
    while ((bytesRead = recv(dataSocket, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytesRead, file);
    }

    fclose(file);
    close(dataSocket);

    send(clientSocket, "226 Transfer complete.\r\n", 24, 0);
}

void handleClient(int clientSocket) {
    char buffer[BUFFER_SIZE];
    bool isAuthenticated = false;
    std::string username;

    int dataSocket = -1;
    sockaddr_in dataAddr{};

    send(clientSocket, "220 Welcome to FTP Server\r\n", 27, 0);

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytesRead = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
        if (bytesRead <= 0) {
            std::cout << "Connection closed or error occurred.\n";
            break;
        }
        std::string command(buffer);
        auto tokens = splitCommand(command);

        if (tokens.empty()) continue;

        std::string cmd = tokens[0];
        if (cmd == "USER") {
            if (tokens.size() < 2) {
                send(clientSocket, "501 Syntax error in parameters or arguments.\r\n", 46, 0);
                continue;
            }
            username = tokens[1];
            send(clientSocket, "331 Username okay, need password.\r\n", 34, 0);
        } else if (cmd == "PASS") {
            if (tokens.size() < 2) {
                send(clientSocket, "501 Syntax error in parameters or arguments.\r\n", 46, 0);
                continue;
            }

            if (!username.empty()) {
                isAuthenticated = true;
                send(clientSocket, "230 User logged in, proceed.\r\n", 30, 0);
            } else {
                send(clientSocket, "503 Bad sequence of commands.\r\n", 32, 0);
            }
        } else if (cmd == "QUIT") {
            send(clientSocket, "221 Goodbye.\r\n", 15, 0);
            break;
        } else if (cmd == "PORT") {
            handlePortCommand(tokens, dataAddr, dataSocket, clientSocket);
        } else if (cmd == "PASV") {
            dataSocket = startPassiveDataConnection(dataAddr, dataSocket, clientSocket);
        } else if (cmd == "RETR") {
            if (tokens.size() < 2) {
                send(clientSocket, "501 Syntax error in parameters or arguments.\r\n", 46, 0);
            } else {
                handleRetrCommand(tokens[1], dataSocket, clientSocket);
            }
        } else if (cmd == "STOR") {
            if (tokens.size() < 2) {
                send(clientSocket, "501 Syntax error in parameters or arguments.\r\n", 46, 0);
            } else {
                handleStorCommand(tokens[1], dataSocket, clientSocket);
            }
        } else if (cmd == "NOOP") {
            send(clientSocket, "200 Command okay.\r\n", 19, 0);
        } else {
            send(clientSocket, "502 Command not implemented.\r\n", 31, 0);
        }
    }

    close(clientSocket);
    std::cout << "Client disconnected\n";
}

int main() {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        perror("Socket creation failed");
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(CONTROL_PORT);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    if (listen(serverSocket, 5) < 0) {
        perror("Listen failed");
        return 1;
    }

    std::cout << "FTP Server listening on port " << CONTROL_PORT << "...\n";

    while (true) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientSocket < 0) {
            perror("Accept failed");
            continue;
        }

        std::cout << "Client connected\n";

        std::thread clientThread(handleClient, clientSocket);
        clientThread.detach();
    }

    close(serverSocket);
    return 0;
}
