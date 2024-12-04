//
// Created by iliut on 12/4/24.
//

#ifndef COMMON_H
#define COMMON_H

#include <algorithm>
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

std::vector<std::string> splitCommand(const std::string& command);

#endif // COMMON_H
