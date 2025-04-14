#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <omp.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

#define MATRIX_DIM 1024

inline size_t idx(int i, int j, int N) {
    return static_cast<size_t>(i) * N + j;
}

void computeSerial(const std::vector<int>& A, const std::vector<int>& B, std::vector<int>& C, int N) {
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            int sum = 0;
            for (int k = 0; k < N; ++k) {
                sum += A[idx(i, k, N)] * B[idx(k, j, N)];
            }
            C[idx(i, j, N)] = sum;
        }
    }
}

// Parallel matrix multiplication using OpenMP.
void computeParallel(const std::vector<int>& A, const std::vector<int>& B, std::vector<int>& C, int N) {
    #pragma omp parallel for collapse(2)
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            int sum = 0;
            for (int k = 0; k < N; ++k) {
                sum += A[idx(i, k, N)] * B[idx(k, j, N)];
            }
            C[idx(i, j, N)] = sum;
        }
    }
}

// Distributed matrix multiplication:
// Partition rows of A into two halves. The client computes the first half,
// sends the second half of A along with full B to the server, and receives back
// the computed block of the result matrix.
void computeDistributed(const std::vector<int>& A, const std::vector<int>& B, std::vector<int>& C, 
                        const char* ip, int port, int N) {
    int mid = N / 2;
    int remoteRows = N - mid;
    
    std::vector<int> A_remote(remoteRows * N);
    std::copy(A.begin() + mid * N, A.end(), A_remote.begin());
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return;
    }

    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return;
    }

    if (connect(sock, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        close(sock);
        return;
    }

    int header[2] = { N, remoteRows };
    size_t headerBytes = sizeof(header);
    size_t sent = 0;
    const char* headerPtr = reinterpret_cast<const char*>(header);
    while (sent < headerBytes) {
        ssize_t n = send(sock, headerPtr + sent, headerBytes - sent, 0);
        if (n <= 0) {
            perror("send header");
            close(sock);
            return;
        }
        sent += n;
    }

    size_t bytesB = static_cast<size_t>(N) * N * sizeof(int);
    sent = 0;
    const char* BPtr = reinterpret_cast<const char*>(B.data());
    while (sent < bytesB) {
        ssize_t n = send(sock, BPtr + sent, bytesB - sent, 0);
        if (n <= 0) {
            perror("send B");
            close(sock);
            return;
        }
        sent += n;
    }

    size_t bytesA_remote = static_cast<size_t>(remoteRows) * N * sizeof(int);
    sent = 0;
    const char* AremotePtr = reinterpret_cast<const char*>(A_remote.data());
    while (sent < bytesA_remote) {
        ssize_t n = send(sock, AremotePtr + sent, bytesA_remote - sent, 0);
        if (n <= 0) {
            perror("send A_remote");
            close(sock);
            return;
        }
        sent += n;
    }

    std::vector<int> C_local(mid * N, 0);
    computeParallel(A, B, C_local, mid);

    std::vector<int> C_remote(remoteRows * N);
    size_t bytesToReceive = static_cast<size_t>(remoteRows) * N * sizeof(int);
    size_t bytesReceived = 0;
    char* recvPtr = reinterpret_cast<char*>(C_remote.data());
    while (bytesReceived < bytesToReceive) {
        ssize_t n = read(sock, recvPtr + bytesReceived, bytesToReceive - bytesReceived);
        if (n <= 0) {
            perror("read C_remote");
            close(sock);
            return;
        }
        bytesReceived += n;
    }

    close(sock);

    
    std::copy(C_local.begin(), C_local.end(), C.begin());
    std::copy(C_remote.begin(), C_remote.end(), C.begin() + mid * N);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Usage: ./Client <SERVER_IP> <PORT>\n";
        return 1;
    }

    const char* ip = argv[1];
    int port = std::stoi(argv[2]);
    int N = MATRIX_DIM;

    std::vector<int> A(N * N, 1);  
    std::vector<int> B(N * N, 1);  
    std::vector<int> C(N * N, 0);

    std::ofstream csvFile("timings.csv");
    csvFile << "method,time_seconds\n";

   
    {
        std::vector<int> C_serial(N * N, 0);
        auto start = std::chrono::high_resolution_clock::now();
        computeSerial(A, B, C_serial, N);
        auto end = std::chrono::high_resolution_clock::now();
        csvFile << "serial," << std::chrono::duration<double>(end - start).count() << "\n";
    }

  
    {
        std::vector<int> C_parallel(N * N, 0);
        auto start = std::chrono::high_resolution_clock::now();
        computeParallel(A, B, C_parallel, N);
        auto end = std::chrono::high_resolution_clock::now();
        csvFile << "openmp," << std::chrono::duration<double>(end - start).count() << "\n";
    }

    
    {
        auto start = std::chrono::high_resolution_clock::now();
        computeDistributed(A, B, C, ip, port, N);
        auto end = std::chrono::high_resolution_clock::now();
        csvFile << "distributed," << std::chrono::duration<double>(end - start).count() << "\n";
    }

    csvFile.close();
    std::cout << "All results written to timings.csv\n";
    return 0;
}
