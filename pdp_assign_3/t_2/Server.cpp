#include <iostream>
#include <vector>
#include <chrono>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>


inline size_t idx(int i, int j, int N) {
    return static_cast<size_t>(i) * N + j;
}

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

    
    int header[2];
    size_t headerBytes = sizeof(header);
    size_t bytesRead = 0;
    char* headerPtr = reinterpret_cast<char*>(header);
    while (bytesRead < headerBytes) {
        ssize_t n = read(client_fd, headerPtr + bytesRead, headerBytes - bytesRead);
        if (n <= 0) {
            perror("read header");
            close(client_fd);
            close(server_fd);
            return 1;
        }
        bytesRead += n;
    }
    int N = header[0];
    int remoteRows = header[1];

   
    std::vector<int> B(static_cast<size_t>(N) * N);
    size_t bytesB = static_cast<size_t>(N) * N * sizeof(int);
    bytesRead = 0;
    char* BPtr = reinterpret_cast<char*>(B.data());
    while (bytesRead < bytesB) {
        ssize_t n = read(client_fd, BPtr + bytesRead, bytesB - bytesRead);
        if (n <= 0) {
            perror("read B");
            close(client_fd);
            close(server_fd);
            return 1;
        }
        bytesRead += n;
    }

  
    std::vector<int> A_remote(static_cast<size_t>(remoteRows) * N);
    size_t bytesA_remote = static_cast<size_t>(remoteRows) * N * sizeof(int);
    bytesRead = 0;
    char* AremotePtr = reinterpret_cast<char*>(A_remote.data());
    while (bytesRead < bytesA_remote) {
        ssize_t n = read(client_fd, AremotePtr + bytesRead, bytesA_remote - bytesRead);
        if (n <= 0) {
            perror("read A_remote");
            close(client_fd);
            close(server_fd);
            return 1;
        }
        bytesRead += n;
    }

    
    std::vector<int> C_remote(static_cast<size_t>(remoteRows) * N, 0);
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < remoteRows; ++i) {
        for (int j = 0; j < N; ++j) {
            int sum = 0;
            for (int k = 0; k < N; ++k) {
                sum += A_remote[idx(i, k, N)] * B[idx(k, j, N)];
            }
            C_remote[idx(i, j, N)] = sum;
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Server computed " << remoteRows << " rows (Time: " 
              << elapsed.count() << " sec)\n";

    size_t bytesToSend = static_cast<size_t>(remoteRows) * N * sizeof(int);
    size_t sent = 0;
    const char* CremotePtr = reinterpret_cast<const char*>(C_remote.data());
    while (sent < bytesToSend) {
        ssize_t n = send(client_fd, CremotePtr + sent, bytesToSend - sent, 0);
        if (n <= 0) {
            perror("send C_remote");
            break;
        }
        sent += n;
    }

    close(client_fd);
    close(server_fd);
    return 0;
}
