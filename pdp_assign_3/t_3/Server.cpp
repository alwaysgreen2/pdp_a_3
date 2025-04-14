#include <vector>
#include <cmath>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

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

void recvAll(int fd, char* buf, size_t bytes) {
    size_t r = 0;
    while (r < bytes) {
        ssize_t n = read(fd, buf + r, bytes - r);
        if (n <= 0) { perror("recv"); exit(1); }
        r += n;
    }
}

void sendAll(int fd, const char* buf, size_t bytes) {
    size_t s = 0;
    while (s < bytes) {
        ssize_t n = send(fd, buf + s, bytes - s, 0);
        if (n <= 0) { perror("send"); exit(1); }
        s += n;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: ./Server <PORT>\n";
        return 1;
    }
    int port = std::stoi(argv[1]);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_addr.s_addr = INADDR_ANY; addr.sin_port = htons(port);
    bind(listen_fd, (sockaddr*)&addr, sizeof(addr));
    listen(listen_fd, 1);

    int client_fd = accept(listen_fd, nullptr, nullptr);

    int mid = (N-2)/2 + 1;
    int remoteRows = (N-1) - mid;
    size_t bytes = remoteRows * N * sizeof(double);

    std::vector<double> V_remote(remoteRows * N);
    recvAll(client_fd, reinterpret_cast<char*>(V_remote.data()), bytes);

    std::vector<double> V(N*N), Vnew(N*N);
    initialize(V);
    for (int i = 0; i < remoteRows; ++i)
        for (int j = 0; j < N; ++j)
            V[idx(mid + i, j)] = V_remote[i*N + j];

    // Jacobi on rows mid..N-2
    for (int iter = 0; iter < MAX_ITERS; ++iter) {
        double maxdiff = 0.0;
        for (int i = mid; i < N-1; ++i) {
            for (int j = 1; j < N-1; ++j) {
                double up    = V[idx(i-1,j)];
                double down  = V[idx(i+1,j)];
                double left  = V[idx(i,j-1)];
                double right = V[idx(i,j+1)];
                double vnew  = 0.25*(up + down + left + right);
                maxdiff = std::max(maxdiff, fabs(vnew - V[idx(i,j)]));
                Vnew[idx(i,j)] = vnew;
            }
        }
        for (int i = mid; i < N-1; ++i)
            for (int j = 1; j < N-1; ++j)
                V[idx(i,j)] = Vnew[idx(i,j)];
        if (maxdiff < EPS) break;
    }

    std::vector<double> Cres(remoteRows * N);
    for (int i = 0; i < remoteRows; ++i)
        for (int j = 0; j < N; ++j)
            Cres[i*N + j] = V[idx(mid + i, j)];

    sendAll(client_fd, reinterpret_cast<char*>(Cres.data()), bytes);

    close(client_fd);
    close(listen_fd);
    return 0;
}
