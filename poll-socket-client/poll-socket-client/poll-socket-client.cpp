#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 80
#define BUFFER_SIZE 4096
#define POLL_TIMEOUT -1  // 5秒超时

// 客户端状态枚举
typedef enum {
    CLIENT_CONNECTING,
    CLIENT_SENDING,
    CLIENT_RECEIVING,
    CLIENT_COMPLETE,
    CLIENT_ERROR
} ClientState;

// 客户端结构
typedef struct {
    SOCKET sock;
    char send_buf[BUFFER_SIZE];
    int send_len;
    int send_pos;
    char recv_buf[BUFFER_SIZE];
    int recv_len;
    ClientState state;
    time_t connect_start;
} Client;

// 初始化客户端
int client_init(Client* client, const char* ip, int port) {
    memset(client, 0, sizeof(Client));

    // 创建套接字
    client->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client->sock == INVALID_SOCKET) {
        fprintf(stderr, "socket() failed: %d\n", WSAGetLastError());
        return -1;
    }

    // 设置非阻塞模式
    u_long mode = 1;
    if (ioctlsocket(client->sock, FIONBIO, &mode) == SOCKET_ERROR) {
        fprintf(stderr, "ioctlsocket() failed: %d\n", WSAGetLastError());
        closesocket(client->sock);
        return -1;
    }

    // 准备服务器地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    // 转换IP地址
    if (InetPton(AF_INET, "127.0.0.1", &server_addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid IP address: %s\n", ip);
        closesocket(client->sock);
        return -1;
    }

    // 开始非阻塞连接
    int result = connect(client->sock, (SOCKADDR*)&server_addr, sizeof(server_addr));
    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK) {
            fprintf(stderr, "connect() failed: %d\n", error);
            closesocket(client->sock);
            return -1;
        }
        // 连接正在进行中
        client->state = CLIENT_CONNECTING;
        client->connect_start = time(NULL);
    }
    else {
        // 连接立即完成
        client->state = CLIENT_SENDING;
    }

    return 0;
}

// 准备要发送的数据
void client_prepare_data(Client* client, const char* data) {
    strncpy_s(client->send_buf, data, BUFFER_SIZE - 1);
    client->send_len = (int)strlen(client->send_buf);
    client->send_pos = 0;
}

// 处理客户端事件
void client_handle_events(WSAPOLLFD* poll_fd, Client* client) {
    switch (client->state) {
    case CLIENT_CONNECTING: {
        // 检查连接超时
        if (time(NULL) - client->connect_start > 10) {
            fprintf(stderr, "Connection timeout\n");
            client->state = CLIENT_ERROR;
            break;
        }

        // 检查连接是否完成
        if (poll_fd->revents & POLLWRNORM) {
            // 验证连接是否成功
            int error = 0;
            int error_len = sizeof(error);
            if (getsockopt(client->sock, SOL_SOCKET, SO_ERROR, (char*)&error, &error_len) == 0 && error == 0) {
                printf("Connected to server\n");
                client->state = CLIENT_SENDING;
            }
            else {
                fprintf(stderr, "Connection failed: %d\n", error);
                client->state = CLIENT_ERROR;
            }
        }
        break;
    }

    case CLIENT_SENDING: {
        // 发送数据
        if (poll_fd->revents & POLLWRNORM) {
            int bytes_sent = send(client->sock,
                client->send_buf + client->send_pos,
                client->send_len - client->send_pos,
                0);

            if (bytes_sent > 0) {
                client->send_pos += bytes_sent;
                printf("Sent %d bytes, total %d/%d\n", bytes_sent, client->send_pos, client->send_len);

                // 检查是否发送完成
                if (client->send_pos >= client->send_len) {
                   // printf("All data sent. Waiting for response...\n");
                    client->state = CLIENT_RECEIVING;
                    // 现在我们需要监视可读事件
                    poll_fd->events = POLLRDNORM;
                }
            }
            else if (bytes_sent == SOCKET_ERROR) {
                int error = WSAGetLastError();
                if (error != WSAEWOULDBLOCK) {
                    fprintf(stderr, "send() failed: %d\n", error);
                    client->state = CLIENT_ERROR;
                }
            }
        }
        break;
    }

    case CLIENT_RECEIVING: {
        // 接收数据
        if (poll_fd->revents & POLLRDNORM) {
            int bytes_recv = recv(client->sock, client->recv_buf ,128,0);

            if (bytes_recv > 0) {
                client->recv_len = bytes_recv;
              //  printf("Received %d bytes, total %d\n", bytes_recv, client->recv_len);

                // 确保字符串以空字符结尾
                client->recv_buf[client->recv_len] = '\0';

                // 检查是否收到完整响应（根据协议确定）
                // 这里简单假设收到任何数据就算完成
                if (client->recv_len > 0) {
                    printf("client:%.*s", client->recv_len, client->recv_buf);
                    
                }
            }
            else if (bytes_recv == 0) {
                printf("Server closed connection\n");
                client->state = CLIENT_COMPLETE;
            }
            else {
                int error = WSAGetLastError();
                if (error != WSAEWOULDBLOCK) {
                    fprintf(stderr, "recv() failed: %d\n", error);
                    client->state = CLIENT_ERROR;
                }
            }
        }
        break;
    }

    case CLIENT_COMPLETE:
    case CLIENT_ERROR:
        // 无需处理，主循环会处理
        break;
    }
}

int main() {
    // 1. 初始化 Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }

    // 2. 初始化客户端
    Client client;
    if (client_init(&client, SERVER_IP, SERVER_PORT) != 0) {
        WSACleanup();
        return 1;
    }

    // 准备要发送的数据
    client_prepare_data(&client, "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");

    // 3. 设置 WSAPoll
    WSAPOLLFD poll_fd;
    poll_fd.fd = client.sock;
    poll_fd.events = POLLWRNORM; // 初始监视可写事件（用于连接和发送）

    if (client.state == CLIENT_CONNECTING) {
        printf("Connecting to %s:%d...\n", SERVER_IP, SERVER_PORT);
    }
    else {
        printf("Connected immediately. Sending data...\n");
    }

    // 4. 主事件循环
    while (client.state != CLIENT_COMPLETE && client.state != CLIENT_ERROR) {
        // 调用 WSAPoll
        int ready = WSAPoll(&poll_fd, 1, POLL_TIMEOUT);

        if (ready == SOCKET_ERROR) {
            fprintf(stderr, "WSAPoll failed: %d\n", WSAGetLastError());
            client.state = CLIENT_ERROR;
            break;
        }
        else if (ready == 0) {
            printf("Poll timeout. Retrying...\n");
            continue;
        }
        else if(poll_fd.revents== POLLRDNORM){
            client.state = CLIENT_RECEIVING;
        }

        // 处理事件
        client_handle_events(&poll_fd, &client);
    }

    // 5. 清理资源
    closesocket(client.sock);
    WSACleanup();

    if (client.state == CLIENT_ERROR) {
        printf("Client exited with error\n");
        return 1;
    }

    printf("Client completed successfully\n");
    return 0;
}