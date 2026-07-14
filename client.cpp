#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <string>

constexpr const char *SERVER_ADDRESS = "127.0.0.1";
constexpr int SERVER_PORT = 8080;
constexpr size_t BUFFER_SIZE = 4096;

bool send_all(int sockfd, const void *data, size_t length) {
    const char *buffer = static_cast<const char *>(data);
    size_t total_sent = 0;
    while (total_sent < length) {
        ssize_t sent = send(sockfd, buffer + total_sent, length - total_sent, 0);
        if (sent <= 0) {
            return false;
        }
        total_sent += static_cast<size_t>(sent);
    }
    return true;
}

bool recv_line(int sockfd, std::string &line) {
    line.clear();
    char ch;
    while (true) {
        ssize_t received = recv(sockfd, &ch, 1, 0);
        if (received <= 0) {
            return false;
        }
        if (ch == '\n') {
            break;
        }
        if (ch != '\r') {
            line.push_back(ch);
        }
    }
    return true;
}

bool recv_exact(int sockfd, char *buffer, size_t length) {
    size_t total_received = 0;
    while (total_received < length) {
        ssize_t received = recv(sockfd, buffer + total_received, length - total_received, 0);
        if (received <= 0) {
            return false;
        }
        total_received += static_cast<size_t>(received);
    }
    return true;
}

int main(int argc, char *argv[]) {
    std::string filename;
    if (argc >= 2) {
        filename = argv[1];
    } else {
        std::cout << "Enter filename to request: ";
        std::getline(std::cin, filename);
    }

    if (filename.empty()) {
        std::cerr << "Filename cannot be empty." << std::endl;
        return EXIT_FAILURE;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cerr << "Socket creation failed: " << strerror(errno) << std::endl;
        return EXIT_FAILURE;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_ADDRESS, &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid server address." << std::endl;
        close(sockfd);
        return EXIT_FAILURE;
    }

    if (connect(sockfd, reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr)) < 0) {
        std::cerr << "Connection failed: " << strerror(errno) << std::endl;
        close(sockfd);
        return EXIT_FAILURE;
    }

    std::string request = filename + "\n";
    if (!send_all(sockfd, request.c_str(), request.size())) {
        std::cerr << "Failed to send file request." << std::endl;
        close(sockfd);
        return EXIT_FAILURE;
    }

    std::string status_line;
    if (!recv_line(sockfd, status_line)) {
        std::cerr << "No response from server." << std::endl;
        close(sockfd);
        return EXIT_FAILURE;
    }

    if (status_line.rfind("ERROR:", 0) == 0) {
        std::cerr << status_line << std::endl;
        close(sockfd);
        return EXIT_FAILURE;
    }

    if (status_line != "OK") {
        std::cerr << "Unexpected server response: " << status_line << std::endl;
        close(sockfd);
        return EXIT_FAILURE;
    }

    std::string size_line;
    if (!recv_line(sockfd, size_line)) {
        std::cerr << "Failed to read file size from server." << std::endl;
        close(sockfd);
        return EXIT_FAILURE;
    }

    size_t file_size = 0;
    try {
        file_size = std::stoul(size_line);
    } catch (...) {
        std::cerr << "Invalid file size received." << std::endl;
        close(sockfd);
        return EXIT_FAILURE;
    }

    std::filesystem::path output_path = std::filesystem::current_path() / ("downloaded_" + filename);
    std::ofstream output_file(output_path, std::ios::binary);
    if (!output_file) {
        std::cerr << "Failed to open output file: " << output_path << std::endl;
        close(sockfd);
        return EXIT_FAILURE;
    }

    size_t remaining = file_size;
    std::vector<char> buffer(BUFFER_SIZE);
    while (remaining > 0) {
        size_t chunk = std::min(remaining, BUFFER_SIZE);
        if (!recv_exact(sockfd, buffer.data(), chunk)) {
            std::cerr << "Connection lost before file transfer completed." << std::endl;
            close(sockfd);
            return EXIT_FAILURE;
        }
        output_file.write(buffer.data(), static_cast<std::streamsize>(chunk));
        remaining -= chunk;
    }

    std::cout << "File received successfully: " << output_path << " (" << file_size << " bytes)" << std::endl;
    close(sockfd);
    return EXIT_SUCCESS;
}