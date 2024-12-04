#include <algorithm>
#include <argon2.h>
#include <csignal>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/types.h>

#define CONTROL_PORT 2121
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

bool verifyPassword(const std::string& username, const std::string& password) {
    std::ifstream credentialsFile("credentials.txt");
    if (!credentialsFile.is_open()) {
        std::cerr << "Could not open credentials file.\n";
        return false;
    }

    std::string line;
    while (std::getline(credentialsFile, line)) {
        // format: user:<hashedpassword>
        size_t delimiterPos = line.find(':');
        if (delimiterPos == std::string::npos) {
            continue; // skip malformed lines
        }

        std::string storedUsername = line.substr(0, delimiterPos);
        std::string storedHashedPassword = line.substr(delimiterPos + 1);

        if (storedUsername == username) {
            // verify password
            int result = argon2id_verify(
                storedHashedPassword.c_str(), // stored hash
                password.c_str(),             // password to verify
                password.length()             // password length
            );

            return result == ARGON2_OK;
        }
    }

    return false; // user not found
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
    dataAddr.sin_port = htons(0);

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

void handleRetrCommand(const std::string& filename, int dataClientSocket, int clientSocket) {
    const std::string fullPath = std::string("storage") + "/" + filename;

    // Open the file for reading
    FILE* file = fopen(fullPath.c_str(), "rb");
    if (!file) {
        perror("File open failed");
        send(clientSocket, "550 File not found or access denied.\r\n", 39, 0);
        close(dataClientSocket);
        return;
    }

    send(clientSocket, "150 Opening data connection.\r\n", 30, 0);

    // Read the file and send data to the client
    char buffer[BUFFER_SIZE];
    size_t bytesRead;
    bool transferFailed = false;
    while ((bytesRead = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        if (send(dataClientSocket, buffer, bytesRead, 0) < 0) {
            perror("Data send failed");
            transferFailed = true;
            break;
        }
    }

    fclose(file);
    close(dataClientSocket); // Close the accepted data connection

    if (transferFailed) {
        send(clientSocket, "426 Connection closed; transfer aborted.\r\n", 43, 0);
    } else {
        send(clientSocket, "226 Transfer complete.\r\n", 24, 0);
    }
}

void handleStorCommand(const std::string& filename, int dataClientSocket, int clientSocket) {
    const std::string storageDir = "storage";

    // ensure the "storage" directory exists
    struct stat st = {0};
    if (stat(storageDir.c_str(), &st) == -1) {
        if (mkdir(storageDir.c_str(), 0755) < 0) {
            perror("Failed to create 'storage' directory");
            send(clientSocket, "550 Could not create directory.\r\n", 32, 0);
            close(dataClientSocket);
            return;
        }
    }

    // Construct the full path to save the file
    const std::string fullPath = storageDir + "/" + filename;

    // Open the file for writing
    FILE* file = fopen(fullPath.c_str(), "wb");
    if (!file) {
        perror("File open failed");
        send(clientSocket, "550 Could not create file.\r\n", 28, 0);
        close(dataClientSocket);
        return;
    }

    send(clientSocket, "150 Opening data connection.\r\n", 30, 0);

    // Receive data and write to the file
    char buffer[BUFFER_SIZE];
    ssize_t bytesRead;
    bool transferFailed = false;
    while ((bytesRead = recv(dataClientSocket, buffer, BUFFER_SIZE, 0)) > 0) {
        if (fwrite(buffer, 1, bytesRead, file) < bytesRead) {
            perror("File write failed");
            transferFailed = true;
            break;
        }
    }

    fclose(file);
    close(dataClientSocket); // Close the accepted data connection

    if (transferFailed || bytesRead < 0) {
        send(clientSocket, "426 Connection closed; transfer aborted.\r\n", 43, 0);
    } else {
        send(clientSocket, "226 Transfer complete.\r\n", 24, 0);
    }
}

void handlePwdCommand(int clientSocket) {
    const std::string storageDir = "/storage";

    // send the "storage" directory as the current working directory
    std::string response = "257 \"" + storageDir + "\" is the current directory.\r\n";
    send(clientSocket, response.c_str(), response.size(), 0);
}

void handleCwdCommand(const std::vector<std::string>& tokens, int clientSocket) {
    if (tokens.size() < 2) {
        send(clientSocket, "501 Syntax error in parameters or arguments.\r\n", 46, 0);
        return;
    }

    const std::string directory = tokens[1];
    if (directory == "/storage") {
        send(clientSocket, "250 Directory successfully changed.\r\n", 36, 0);
    } else {
        send(clientSocket, "550 Failed to change directory.\r\n", 34, 0);
    }
}

void handleMkdCommand(const std::vector<std::string>& tokens, int clientSocket) {
    if (tokens.size() < 2) {
        send(clientSocket, "501 Syntax error in parameters or arguments.\r\n", 46, 0);
        return;
    }

    const std::string directory = tokens[1];
    if (directory == "/storage") {
        send(clientSocket, "257 \"/storage\" directory created.\r\n", 34, 0);
    } else {
        send(clientSocket, "550 Failed to create directory.\r\n", 34, 0);
    }
}

void handleMdtmCommand(const std::vector<std::string>& tokens, int clientSocket) {
    if (tokens.size() < 2) {
        send(clientSocket, "501 Syntax error in parameters or arguments.\r\n", 46, 0);
        return;
    }

    const std::string filePath = "storage/" + tokens[1];
    struct stat st;
    if (stat(filePath.c_str(), &st) == 0) {
        struct tm* tm = gmtime(&st.st_mtime);
        char timeBuf[BUFFER_SIZE];
        strftime(timeBuf, sizeof(timeBuf), "%Y%m%d%H%M%S", tm);

        std::string response = "213 " + std::string(timeBuf) + "\r\n";
        send(clientSocket, response.c_str(), response.size(), 0);
    } else {
        send(clientSocket, "550 File not found.\r\n", 22, 0);
    }
}

void handleTypeCommand(const std::vector<std::string>& tokens, int clientSocket) {
    if (tokens.size() < 2) {
        send(clientSocket, "501 Syntax error in parameters or arguments.\r\n", 46, 0);
        return;
    }

    const std::string type = tokens[1];
    if (type == "A" || type == "I") {
        send(clientSocket, "200 Type set successfully.\r\n", 29, 0);
    } else {
        send(clientSocket, "504 Command not implemented for that parameter.\r\n", 49, 0);
    }
}

void handleSizeCommand(const std::vector<std::string>& tokens, int clientSocket) {
    if (tokens.size() < 2) {
        send(clientSocket, "501 Syntax error in parameters or arguments.\r\n", 46, 0);
        return;
    }

    const std::string filePath = "storage/" + tokens[1];
    struct stat st;
    if (stat(filePath.c_str(), &st) == 0) {
        std::string response = "213 " + std::to_string(st.st_size) + "\r\n";
        send(clientSocket, response.c_str(), response.size(), 0);
    } else {
        send(clientSocket, "550 File not found.\r\n", 22, 0);
    }
}

void handleListCommand(int dataClientSocket, int clientSocket) {
    const std::string storageDir = "storage";

    DIR* dir = opendir(storageDir.c_str());
    if (!dir) {
        perror("Failed to open directory");
        send(clientSocket, "450 Requested file action not taken. Directory unavailable.\r\n", 61, 0);
        close(dataClientSocket);
        return;
    }

    send(clientSocket, "150 Opening data connection for directory listing.\r\n", 52, 0);

    struct dirent* entry;
    std::string listData;

    while ((entry = readdir(dir)) != nullptr) {
        if (std::string(entry->d_name) == "." || std::string(entry->d_name) == "..") {
            continue; // Skip current and parent directory entries
        }
        listData += entry->d_name;
        listData += "\r\n";
    }

    closedir(dir);

    if (!listData.empty()) {
        size_t totalSent = 0;
        while (totalSent < listData.size()) {
            ssize_t bytesSent = send(dataClientSocket, listData.c_str() + totalSent, listData.size() - totalSent, 0);
            if (bytesSent <= 0) {
                perror("Data send failed");
                send(clientSocket, "426 Connection closed; transfer aborted.\r\n", 43, 0);
                close(dataClientSocket);
                return;
            }
            totalSent += bytesSent;
        }
    }

    shutdown(dataClientSocket, SHUT_WR); // Ensure client reads all data
    close(dataClientSocket); // Close the accepted data connection
    send(clientSocket, "226 Directory send OK.\r\n", 24, 0);
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
                if (verifyPassword(username, tokens[1])) {
                    isAuthenticated = true;
                    send(clientSocket, "230 User logged in, proceed.\r\n", 30, 0);
                } else {
                    send(clientSocket, "530 Invalid username or password.\r\n", 36, 0);
                }
            } else {
                send(clientSocket, "503 Bad sequence of commands.\r\n", 32, 0);
            }
        } else if (!isAuthenticated) {
            send(clientSocket, "530 Please log in first.\r\n", 27, 0);
        } else if (cmd == "QUIT") {
            send(clientSocket, "221 Goodbye.\r\n", 15, 0);
            break;
        } else if (cmd == "PWD") {
            handlePwdCommand(clientSocket);
        } else if (cmd == "CWD") {
            handleCwdCommand(tokens, clientSocket);
        } else if (cmd == "MKD") {
            handleMkdCommand(tokens, clientSocket);
        } else if (cmd == "SIZE") {
            handleSizeCommand(tokens, clientSocket);
        } else if (cmd == "MDTM") {
            handleMdtmCommand(tokens, clientSocket);
        } else if (cmd == "TYPE") {
            handleTypeCommand(tokens, clientSocket);
        } else if (cmd == "PORT") {
            handlePortCommand(tokens, dataAddr, dataSocket, clientSocket);
        } else if (cmd == "PASV") {
            dataSocket = startPassiveDataConnection(dataAddr, dataSocket, clientSocket);
        } else if (cmd == "STOR") {
            if (dataSocket < 0) {
                send(clientSocket, "425 Use PASV first.\r\n", 21, 0);
            } else if (tokens.size() < 2) {
                send(clientSocket, "501 Syntax error in parameters or arguments.\r\n", 46, 0);
            } else {
                sockaddr_in dataClientAddr{};
                socklen_t addrLen = sizeof(dataClientAddr);

                int dataClientSocket = accept(dataSocket, (struct sockaddr*)&dataClientAddr, &addrLen);
                if (dataClientSocket < 0) {
                    perror("Data connection accept failed");
                    send(clientSocket, "425 Can't open data connection.\r\n", 34, 0);
                    continue;
                }

                handleStorCommand(tokens[1], dataClientSocket, clientSocket);
            }
        } else if (cmd == "RETR") {
            if (dataSocket < 0) {
                send(clientSocket, "425 Use PASV first.\r\n", 21, 0);
            } else if (tokens.size() < 2) {
                send(clientSocket, "501 Syntax error in parameters or arguments.\r\n", 46, 0);
            } else {
                sockaddr_in dataClientAddr{};
                socklen_t addrLen = sizeof(dataClientAddr);

                int dataClientSocket = accept(dataSocket, (struct sockaddr*)&dataClientAddr, &addrLen);
                if (dataClientSocket < 0) {
                    perror("Data connection accept failed");
                    send(clientSocket, "425 Can't open data connection.\r\n", 34, 0);
                    continue;
                }

                handleRetrCommand(tokens[1], dataClientSocket, clientSocket);
            }
        } else if (cmd == "LIST") {
            if (dataSocket < 0) {
                send(clientSocket, "425 Use PASV first.\r\n", 21, 0);
            } else {
                sockaddr_in dataClientAddr{};
                socklen_t addrLen = sizeof(dataClientAddr);

                int dataClientSocket = accept(dataSocket, (struct sockaddr*)&dataClientAddr, &addrLen);
                if (dataClientSocket < 0) {
                    perror("Data connection accept failed");
                    send(clientSocket, "425 Can't open data connection.\r\n", 34, 0);
                    continue;
                }

                handleListCommand(dataClientSocket, clientSocket);
            }
        }

        else if (cmd == "NOOP") {
            send(clientSocket, "200 Command okay.\r\n", 19, 0);
        } else {
            send(clientSocket, "502 Command not implemented.\r\n", 31, 0);
        }
    }

    close(clientSocket);
    std::cout << "Client disconnected\n";
}

int main() {
    // Ignore SIGPIPE
    signal(SIGPIPE, SIG_IGN);

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

