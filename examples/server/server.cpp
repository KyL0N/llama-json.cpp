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
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return -1;
    }
#endif
    return 0;
}

void socketCleanup()
{
#if defined(_WIN32)
    WSACleanup();
#endif
}

void handle_connection(SOCKET socket)
{
    // Receive data from the client
    const int bufferSize = 1024;
    char buffer[bufferSize];
    int  recvResult;
    bool connectionClosed = false;
    do {
        recvResult = recv(socket, buffer, sizeof(buffer), 0);
        if (recvResult == SOCKET_ERROR) {
            fprintf(stderr, "recv message failed\n");
            socketClose(socket);
            return;
        }

        // Push the message onto the queue
        std::string message(buffer, recvResult);
        messageQueue.push(message);

        if (recvResult == 0) {
            // Connection closed
            connectionClosed = true;
            break;
        }

    } while (recvResult > 0);

    if (connectionClosed) {
        socketClose(socket);
        return;
    }

    socketClose(socket);
}

void sendResponse(SOCKET clientSocket, const std::string& response) {
    // Send the response back to the client
    int sendResult = send(clientSocket, response.c_str(), response.length(), 0);
    if (sendResult == SOCKET_ERROR) {
        fprintf(stderr, "send message failed\n");
        socketClose(clientSocket);
        return;
    }
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
    if (clientSocket == INVALID_SOCKET) {
        fprintf(stderr, "accept socket failed\n");
        socketClose(listenSocket);
        return {};
    }

    // Handle the connection in a separate thread
    std::thread connectionThread(handle_connection, clientSocket);
    connectionThread.detach();

    std::thread messageThread([&]() {
        while (true) {
            // Wait for a response from the main thread
            std::string response;
            while (!responseQueue.tryPop(response)) {

                // Make sure we don't busy-wait too much
                std::this_thread::sleep_for(std::chrono::milliseconds(10));

                // Check if the connection thread has exited
                if (!__builtin_expect(connectionThread.joinable(), true)) {
                    return;
                }
            }

            // Send the response to the client
            sendResponse(clientSocket, response);
        }
    });



    std::vector<std::thread> threads;
    threads.push_back(std::move(connectionThread));
    threads.push_back(std::move(messageThread));
    return std::move(threads);
}

void deinit_server(std::vector<std::thread>& threads)
{
    if (listenSocket != INVALID_SOCKET) {
        socketClose(listenSocket);
        listenSocket = INVALID_SOCKET;
    }
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}