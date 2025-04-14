#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <omp.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

#define ARRAY_SIZE 100000000

uint64_t computeSerial(const std::vector<int>& arr) {
    uint64_t sum = 0;
    for (int v : arr) sum += v;
    return sum;
}

uint64_t computeParallel(const std::vector<int>& arr) {
    uint64_t sum = 0;
    #pragma omp parallel for reduction(+:sum)
    for (size_t i = 0; i < arr.size(); ++i) sum += arr[i];
    return sum;
}

uint64_t computeDistributed(const std::vector<int>& arr, const char* ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 0;
    }

    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return 0;
    }

    if (connect(sock, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        close(sock);
        return 0;
    }

    size_t mid = arr.size() / 2;
    std::vector<int> remote(arr.begin() + mid, arr.end());
    uint64_t remoteSize = remote.size();

    // Send remoteSize
    ssize_t sent = 0;
    const char* remoteSizePtr = reinterpret_cast<const char*>(&remoteSize);
    while (sent < sizeof(remoteSize)) {
        ssize_t n = send(sock, remoteSizePtr + sent, sizeof(remoteSize) - sent, 0);
        if (n <= 0) {
            perror("send remoteSize");
            close(sock);
            return 0;
        }
        sent += n;
    }

    // Send remote data
    const char* dataPtr = reinterpret_cast<const char*>(remote.data());
    size_t totalDataBytes = remoteSize * sizeof(int);
    sent = 0;
    while (sent < totalDataBytes) {
        ssize_t n = send(sock, dataPtr + sent, totalDataBytes - sent, 0);
        if (n <= 0) {
            perror("send data");
            close(sock);
            return 0;
        }
        sent += n;
    }

    // Receive remoteSum
    uint64_t remoteSum;
    size_t bytesRead = 0;
    char* sumPtr = reinterpret_cast<char*>(&remoteSum);
    while (bytesRead < sizeof(remoteSum)) {
        ssize_t n = read(sock, sumPtr + bytesRead, sizeof(remoteSum) - bytesRead);
        if (n <= 0) {
            perror("read remoteSum");
            close(sock);
            return 0;
        }
        bytesRead += n;
    }

    close(sock);

    uint64_t localSum = computeParallel({arr.begin(), arr.begin() + mid});
    return localSum + remoteSum;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Usage: ./Client <SERVER_IP> <PORT>\n";
        return 1;
    }

    const char* ip = argv[1];
    int port = std::stoi(argv[2]);
    std::vector<int> arr(ARRAY_SIZE, 1);

    std::ofstream csvFile("timings.csv");
    csvFile << "method,time_seconds\n";

    auto start = std::chrono::high_resolution_clock::now();
    uint64_t serialSum = computeSerial(arr);
    auto end = std::chrono::high_resolution_clock::now();
    csvFile << "serial," << std::chrono::duration<double>(end - start).count() << "\n";

    start = std::chrono::high_resolution_clock::now();
    uint64_t parallelSum = computeParallel(arr);
    end = std::chrono::high_resolution_clock::now();
    csvFile << "openmp," << std::chrono::duration<double>(end - start).count() << "\n";

    start = std::chrono::high_resolution_clock::now();
    uint64_t distSum = computeDistributed(arr, ip, port);
    end = std::chrono::high_resolution_clock::now();
    csvFile << "distributed," << std::chrono::duration<double>(end - start).count() << "\n";

    csvFile.close();

    std::cout << "All results written to timings.csv\n";
    return 0;
}