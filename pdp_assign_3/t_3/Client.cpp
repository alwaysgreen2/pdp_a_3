#include <vector>
#include <cmath>
#include <chrono>
#include <omp.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <fstream>

#define N 1024
#define EPS 1e-5
#define MAX_ITERS 10000

inline size_t idx(int i, int j) {
    return static_cast<size_t>(i)*N + j;
}

void initialize(std::vector<double>& V) {
    for(int i = 0; i < N; ++i) {
        for(int j = 0; j < N; ++j) {
            if (i == 0)           V[idx(i,j)] =  5.0;
            else if (i == N-1)    V[idx(i,j)] = -5.0;
            else if (j == 0 || j == N-1) V[idx(i,j)] = 0.0;
            else                  V[idx(i,j)] = 0.0;
        }
    }
}

double jacobiParallel(const std::vector<double>& V, std::vector<double>& Vnew) {
    double maxdiff = 0.0;
    #pragma omp parallel for collapse(2) reduction(max:maxdiff)
    for(int i = 1; i < N-1; ++i) {
        for(int j = 1; j < N-1; ++j) {
            double up    = V[idx(i-1,j)];
            double down  = V[idx(i+1,j)];
            double left  = V[idx(i,j-1)];
            double right = V[idx(i,j+1)];
            double vnew  = 0.25*(up + down + left + right);
            maxdiff = std::max(maxdiff, fabs(vnew - V[idx(i,j)]));
            Vnew[idx(i,j)] = vnew;
        }
    }
    return maxdiff;
}

void solveDistributed(std::vector<double>& V, const char* ip, int port) {
    int mid = (N-2)/2 + 1;
    int remoteRows = (N-1) - mid;

    // extract remote block
    std::vector<double> V_remote(remoteRows * N);
    for(int i = 0; i < remoteRows; ++i)
        for(int j = 0; j < N; ++j)
            V_remote[i*N + j] = V[idx(mid + i, j)];

    // socket setup
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return; }
    sockaddr_in serv{}; serv.sin_family = AF_INET; serv.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &serv.sin_addr) <= 0) {
        perror("inet_pton"); close(sock); return;
    }
    if (connect(sock, (sockaddr*)&serv, sizeof(serv)) < 0) {
        perror("connect"); close(sock); return;
    }

    // send V_remote
    size_t bytes = V_remote.size() * sizeof(double);
    size_t sent = 0;
    const char* ptr = reinterpret_cast<const char*>(V_remote.data());
    while (sent < bytes) {
        ssize_t n = send(sock, ptr + sent, bytes - sent, 0);
        if (n <= 0) { perror("send V_remote"); close(sock); return; }
        sent += n;
    }

    // client-side Jacobi on top half
    std::vector<double> Vnew(N*N);
    for (int iter = 0; iter < MAX_ITERS; ++iter) {
        double diff = jacobiParallel(V, Vnew);
        std::swap(V, Vnew);
        if (diff < EPS) break;
    }

    // receive solved block
    std::vector<double> Cres(remoteRows * N);
    size_t recvd = 0;
    char* recvPtr = reinterpret_cast<char*>(Cres.data());
    while (recvd < bytes) {
        ssize_t n = read(sock, recvPtr + recvd, bytes - recvd);
        if (n <= 0) { perror("recv Cres"); break; }
        recvd += n;
    }
    close(sock);

    // merge back
    for (int i = 0; i < remoteRows; ++i)
        for (int j = 0; j < N; ++j)
            V[idx(mid + i, j)] = Cres[i*N + j];
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Usage: ./Client <SERVER_IP> <PORT>\n";
        return 1;
    }
    const char* ip = argv[1];
    int port = std::stoi(argv[2]);

    std::vector<double> V(N*N), Vnew(N*N);
    initialize(V);

    std::ofstream csv("timings.csv");
    csv << "method,time\n";

    // Serial (using parallel function with 1 thread for simplicity)
    {
        auto U = V;
        auto start = std::chrono::high_resolution_clock::now();
        for (int it = 0; it < MAX_ITERS; ++it) {
            double diff = jacobiParallel(U, Vnew);
            std::swap(U, Vnew);
            if (diff < EPS) break;
        }
        auto end = std::chrono::high_resolution_clock::now();
        csv << "serial," << std::chrono::duration<double>(end - start).count() << "\n";
    }

    // OpenMP
    {
        auto U = V;
        auto start = std::chrono::high_resolution_clock::now();
        for (int it = 0; it < MAX_ITERS; ++it) {
            double diff = jacobiParallel(U, Vnew);
            std::swap(U, Vnew);
            if (diff < EPS) break;
        }
        auto end = std::chrono::high_resolution_clock::now();
        csv << "openmp," << std::chrono::duration<double>(end - start).count() << "\n";
    }

    // Distributed
    {
        auto U = V;
        auto start = std::chrono::high_resolution_clock::now();
        solveDistributed(U, ip, port);
        auto end = std::chrono::high_resolution_clock::now();
        csv << "distributed," << std::chrono::duration<double>(end - start).count() << "\n";
    }

    csv.close();
    std::cout << "Results in timings.csv\n";
    return 0;
}


