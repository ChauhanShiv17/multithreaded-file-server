#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

constexpr int SERVER_PORT = 8080;
constexpr int BACKLOG = 10;
constexpr size_t BUFFER_SIZE = 4096;

std::mutex log_mutex;

void write_log(const std::string &entry) {
    std::lock_guard<std::mutex> guard(log_mutex);
    std::ofstream log_file("logs.txt", std::ios::app);
    if (!log_file) {
        std::cerr << "Failed to open logs.txt for writing" << std::endl;
        return;
    }
    log_file << entry << std::endl;
}

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

std::string sanitize_filename(const std::string &request) {
    if (request.empty()) {
        return {};
    }
    if (request.find("..") != std::string::npos) {
        return {};
    }
    if (request.find('/') != std::string::npos || request.find('\\') != std::string::npos) {
        return {};
    }
    std::filesystem::path candidate(request);
    if (candidate.filename().string() != request) {
        return {};
    }
    return request;
}

void handle_client(int client_socket, const sockaddr_in &client_addr) {
    char client_ip[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));

    std::string request;
    if (!recv_line(client_socket, request)) {
        write_log("[ERROR] Connection lost while reading request from " + std::string(client_ip));
        close(client_socket);
        return;
    }

    std::string filename = sanitize_filename(request);
    if (filename.empty()) {
        std::string error_message = "ERROR: Invalid file request or path traversal attempt.";
        send_all(client_socket, error_message.c_str(), error_message.size());
        write_log("[SECURITY] Invalid request from " + std::string(client_ip) + ": " + request);
        close(client_socket);
        return;
    }

    std::filesystem::path base_dir = std::filesystem::current_path() / "files";
    std::filesystem::path file_path = base_dir / filename;

    if (!std::filesystem::exists(file_path) || !std::filesystem::is_regular_file(file_path)) {
        std::string error_message = "ERROR: File not found: " + filename;
        send_all(client_socket, error_message.c_str(), error_message.size());
        write_log("[NOT_FOUND] " + std::string(client_ip) + " requested " + filename);
        close(client_socket);
        return;
    }

    std::ifstream file_stream(file_path, std::ios::binary);
    if (!file_stream) {
        std::string error_message = "ERROR: Failed to open file.";
        send_all(client_socket, error_message.c_str(), error_message.size());
        write_log("[ERROR] Failed opening file for " + std::string(client_ip) + ": " + filename);
        close(client_socket);
        return;
    }

    file_stream.seekg(0, std::ios::end);
    size_t file_size = static_cast<size_t>(file_stream.tellg());
    file_stream.seekg(0, std::ios::beg);

    std::ostringstream header;
    header << "OK\n" << file_size << "\n";
    std::string header_str = header.str();

    if (!send_all(client_socket, header_str.c_str(), header_str.size())) {
        write_log("[ERROR] Failed to send header to " + std::string(client_ip));
        close(client_socket);
        return;
    }

    char buffer[BUFFER_SIZE];
    while (file_stream.good()) {
        file_stream.read(buffer, sizeof(buffer));
        std::streamsize bytes_read = file_stream.gcount();
        if (bytes_read > 0) {
            if (!send_all(client_socket, buffer, static_cast<size_t>(bytes_read))) {
                write_log("[ERROR] Connection lost during transfer to " + std::string(client_ip));
                close(client_socket);
                return;
            }
        }
    }

    write_log("[SENT] " + std::string(client_ip) + " -> " + filename + " (" + std::to_string(file_size) + " bytes)");
    close(client_socket);
}

int main() {
    std::filesystem::path files_path = std::filesystem::current_path() / "files";
    if (!std::filesystem::exists(files_path)) {
        std::filesystem::create_directories(files_path);
    }

    int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket < 0) {
        std::cerr << "Socket creation failed: " << strerror(errno) << std::endl;
        return EXIT_FAILURE;
    }

    int reuse = 1;
    if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        std::cerr << "setsockopt failed: " << strerror(errno) << std::endl;
        close(listen_socket);
        return EXIT_FAILURE;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    if (bind(listen_socket, reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr)) < 0) {
        std::cerr << "Bind failed: " << strerror(errno) << std::endl;
        close(listen_socket);
        return EXIT_FAILURE;
    }

    if (listen(listen_socket, BACKLOG) < 0) {
        std::cerr << "Listen failed: " << strerror(errno) << std::endl;
        close(listen_socket);
        return EXIT_FAILURE;
    }

    std::cout << "Multithreaded File Server started on port " << SERVER_PORT << "." << std::endl;
    std::cout << "Waiting for clients..." << std::endl;

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(listen_socket, reinterpret_cast<sockaddr *>(&client_addr), &client_len);
        if (client_socket < 0) {
            std::cerr << "Accept failed: " << strerror(errno) << std::endl;
            continue;
        }
        std::thread client_thread(handle_client, client_socket, client_addr);
        client_thread.detach();
    }

    close(listen_socket);
    return EXIT_SUCCESS;
}
