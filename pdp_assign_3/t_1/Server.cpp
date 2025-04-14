#include <iostream>
#include <vector>
#include <chrono>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: ./Server <PORT>\n";
        return 1;
    }

    int port = std::stoi(argv[1]);
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 1) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    int client_fd = accept(server_fd, nullptr, nullptr);
    if (client_fd < 0) {
        perror("accept");
        close(server_fd);
        return 1;
    }

    uint64_t arraySize;
    size_t bytesRead = 0;
    char* buffer = reinterpret_cast<char*>(&arraySize);
    while (bytesRead < sizeof(arraySize)) {
        ssize_t n = read(client_fd, buffer + bytesRead, sizeof(arraySize) - bytesRead);
        if (n <= 0) {
            perror("read arraySize");
            close(client_fd);
            close(server_fd);
            return 1;
        }
        bytesRead += n;
    }

    std::vector<int> data(arraySize);
    size_t bytesExpected = arraySize * sizeof(int);
    size_t bytesReceived = 0;
    char* ptr = reinterpret_cast<char*>(data.data());
    while (bytesReceived < bytesExpected) {
        ssize_t n = read(client_fd, ptr + bytesReceived, bytesExpected - bytesReceived);
        if (n <= 0) {
            perror("read data");
            close(client_fd);
            close(server_fd);
            return 1;
        }
        bytesReceived += n;
    }

    auto start = std::chrono::high_resolution_clock::now();
    uint64_t sum = 0;
    for (int val : data) sum += val;
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Server Partial Sum: " << sum << " (Time: " << elapsed.count() << " sec)\n";

    // Send sum back
    size_t sent = 0;
    const char* sumPtr = reinterpret_cast<const char*>(&sum);
    while (sent < sizeof(sum)) {
        ssize_t n = send(client_fd, sumPtr + sent, sizeof(sum) - sent, 0);
        if (n <= 0) {
            perror("send sum");
            break;
        }
        sent += n;
    }

    close(client_fd);
    close(server_fd);
    return 0;
}