#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include <algorithm>  // For std::max

constexpr size_t MAXDATASIZE = 1024;
constexpr int PORT_OFFSET = 1000;

void* get_in_addr(struct sockaddr* sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

std::string toupper(const std::string &input) {
    std::string output;
    bool capitalize = true;

    for (char c : input) {
        if (std::isalpha(c)) {
            output += capitalize ? std::toupper(c) : std::tolower(c);
            capitalize = !capitalize;
        } else {
            output += c;
        }
    }
    return output;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "usage: client client.conf\n";
        return 1;
    }

    std::string serverIP, serverPort;
    std::ifstream configFile(argv[1]);
    if (!configFile.is_open()) {
        std::cerr << "Error opening config file: " << argv[1] << std::endl;
        return 1;
    }

    std::string line;
    while (std::getline(configFile, line)) {
        if (line.find("SERVER_IP=") == 0) {
            serverIP = line.substr(10);
        } else if (line.find("SERVER_PORT=") == 0) {
            serverPort = line.substr(12);
        }
    }
    configFile.close();

    if (serverIP.empty() || serverPort.empty()) {
        std::cerr << "Invalid config file format.\n";
        return 1;
    }

    addrinfo hints{}, *servinfo, *p;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int rv = getaddrinfo(serverIP.c_str(), serverPort.c_str(), &hints, &servinfo);
    if (rv != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(rv) << std::endl;
        return 1;
    }

    int sockfd;
    for (p = servinfo; p != nullptr; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("client: connect");
            close(sockfd);
            continue;
        }
        break;
    }

    if (p == nullptr) {
        std::cerr << "client: failed to connect\n";
        return 2;
    }

    char s[INET6_ADDRSTRLEN];
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr*)p->ai_addr), s, sizeof s);
    std::cout << "Connected to server at " << s << std::endl;

    // Get the client's dynamically assigned TCP port
    sockaddr_in client_addr{};
    socklen_t client_addr_len = sizeof(client_addr);
    if (getsockname(sockfd, (struct sockaddr*)&client_addr, &client_addr_len) == -1) {
        perror("getsockname");
        close(sockfd);
        return 1;
    }

    int tcpPort = ntohs(client_addr.sin_port);  // Client's dynamically assigned TCP port
    int udpPort = tcpPort + PORT_OFFSET;        // Calculate UDP listening port

    std::cout << "Client's TCP port: " << tcpPort << "\n";
    std::cout << "Client's UDP listening port: " << udpPort << "\n";

    freeaddrinfo(servinfo);

    // Set up UDP socket for listening
    int udpSockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSockfd == -1) {
        perror("client: udp socket");
        close(sockfd);
        return 1;
    }

    sockaddr_in udpAddr{};
    udpAddr.sin_family = AF_INET;
    udpAddr.sin_port = htons(udpPort);
    udpAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(udpSockfd, (struct sockaddr*)&udpAddr, sizeof(udpAddr)) == -1) {
        perror("client: bind");
        close(sockfd);
        close(udpSockfd);
        return 1;
    }

    std::cout << "UDP port " << udpPort << " is set up and listening.\n";
    std::cout << "> ";
    std::cout.flush();

    fd_set read_fds;
    char buf[MAXDATASIZE];
    std::string userInput;

    while (true) {
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(udpSockfd, &read_fds);  // Add UDP socket to the set

        int max_fd = std::max(sockfd, std::max(STDIN_FILENO, udpSockfd));

        int activity = select(max_fd + 1, &read_fds, nullptr, nullptr, nullptr);
        if (activity == -1) {
            perror("select");
            break;
        }

        if (FD_ISSET(sockfd, &read_fds)) {
            int numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0);
            if (numbytes <= 0) {
                if (numbytes == 0) {
                    std::cout << "\nServer closed the connection.\n";
                } else {
                    perror("recv");
                }
                break;
            }
            buf[numbytes] = '\0';
            std::cout << "\rServer (TCP): " << buf << "\n> ";
            std::cout.flush();
        }

        if (FD_ISSET(udpSockfd, &read_fds)) {
            sockaddr_in srcAddr;
            socklen_t srcAddrLen = sizeof(srcAddr);
            int numbytes = recvfrom(udpSockfd, buf, MAXDATASIZE - 1, 0, (struct sockaddr*)&srcAddr, &srcAddrLen);
            if (numbytes > 0) {
                buf[numbytes] = '\0';
                std::cout << "\rServer (UDP): " << buf << "\n> ";
                if (std::string(buf) == "HEARTBEAT") {
                    std::string udpMessage = "HEARTBEAT RECEIVED";
                    if (send(sockfd, udpMessage.c_str(), udpMessage.size(), 0) == -1) {
                        perror("send");
                    }
                }
                std::cout.flush();
            } else if (numbytes == -1) {
                perror("recvfrom");
            }
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            std::getline(std::cin, userInput);
            std::string input = toupper(userInput);
            if (userInput == "exit" || userInput == "QUIT" || userInput.substr(0, 6) == "QUIT :") {
                if (send(sockfd, userInput.c_str(), userInput.size(), 0) == -1) {
                    perror("send");
                }
                break;
            }
            if (send(sockfd, userInput.c_str(), userInput.size(), 0) == -1) {
                perror("send");
                break;
            }
            std::cout << "> ";
            std::cout.flush();
        }
    }

    close(sockfd);
    close(udpSockfd);
    return 0;
}
