#include "server.h"

ThreadSafeQueue<std::string> messageQueue;
ThreadSafeQueue<std::string> responseQueue;
SOCKET                       listenSocket;
SOCKET                       clientSocket;

int socketClose(SOCKET socket)
{
#if defined(_WIN32)
    return closesocket(socket);
#else
    return close(socket);
#endif
}

int socketInit()
{
#if defined(_WIN32)
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData);
#else
    return 0;
#endif
}

void socketCleanup()
{
#if defined(_WIN32)
    WSACleanup();
#endif
}

void handle_connection(SOCKET clientSocket)
{
    char buffer[1024];
    int  recvResult;
    do {
        // Receive data from the client
        recvResult = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (recvResult == SOCKET_ERROR) {
            fprintf(stderr, "recv message failed\n");
            socketClose(clientSocket);
            return;
        }

        std::string message(buffer, recvResult);

        // Push the message onto the queue
        messageQueue.push(message);

    } while (recvResult > 0);

    socketClose(clientSocket);
}

std::vector<std::thread> init_server()
{
    int iResult;
    iResult = socketInit();
    if (iResult != 0) {
        fprintf(stderr, "socketInit failed\n");
        return {};
    }

    // Create a socket to listen for incoming connections
    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        fprintf(stderr, "listenSocket is invalid\n");
        socketCleanup();
        return {};
    }

    // Bind the socket to a local address and port
    sockaddr_in localAddr;
    localAddr.sin_family      = AF_INET;
    localAddr.sin_addr.s_addr = INADDR_ANY;
    localAddr.sin_port        = htons(1145);  // Replace with desired port number
    iResult                   = bind(listenSocket, reinterpret_cast<sockaddr*>(&localAddr), sizeof(localAddr));
    if (iResult == SOCKET_ERROR) {
        fprintf(stderr, "bind port %d failed\n", ntohs(localAddr.sin_port));
        socketClose(listenSocket);
        socketCleanup();
        return {};
    }

    // Set the socket to listen for incoming connections
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        fprintf(stderr, "listen on port %d failed\n", ntohs(localAddr.sin_port));
        socketClose(listenSocket);
        socketCleanup();
        return {};
    }

    printf("Waiting for client to connect 127.0.0.1:%d\n", ntohs(localAddr.sin_port));
    // block until a client connects
    clientSocket = accept(listenSocket, nullptr, nullptr);
    std::thread acceptThread([&]() {
        if (clientSocket == INVALID_SOCKET) {
            fprintf(stderr, "accept socket failed\n");
            socketClose(listenSocket);
            return;
        }

        // Handle the connection in a separate thread
        std::thread connectionThread(handle_connection, clientSocket);
        connectionThread.detach();
    });

    std::thread messageThread([&]() {
        while (true) {
            // Wait for a response from the main thread
            std::string response;
            // std::string responseStr = "";

            while (!responseQueue.tryPop(response)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            // to string
            // for (int i = 0; i < response.size(); i++) {
            //     responseStr += std::to_string(response[i]) + ",";
            // }
            // Send the response back to the client
            int sendResult = send(clientSocket, response.c_str(), response.length(), 0);
            if (sendResult == SOCKET_ERROR) {
                fprintf(stderr, "send message failed\n");
                socketClose(clientSocket);
                return;
            }
        }
    });

    std::vector<std::thread> threads;
    threads.push_back(std::move(acceptThread));
    threads.push_back(std::move(messageThread));
    return threads;
}

void deinit_server(std::vector<std::thread>& threads)
{
    socketClose(listenSocket);
    for (auto& thread : threads) {
        thread.join();
    }
}
