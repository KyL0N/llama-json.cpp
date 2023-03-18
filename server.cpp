#include "server.h"

void handle_connection(SOCKET clientSocket, ThreadSafeQueue<std::string>& messageQueue)
{
    char buffer[1024];
    int  recvResult;
    do {
        // Receive data from the client
        recvResult = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (recvResult == SOCKET_ERROR) {
            std::cerr << "recv failed with error: " << WSAGetLastError() << std::endl;
            closesocket(clientSocket);
            return;
        }
        // if (recvResult > 0) {
        // Process the received data
        std::string message(buffer, recvResult);

        // Push the message onto the queue
        messageQueue.push(message);

    } while (recvResult > 0);

    closesocket(clientSocket);
}

std::vector<std::thread> init_server(ThreadSafeQueue<std::string>& messageQueue, ThreadSafeQueue<std::vector<int>>& responseQueue)
{
    WSADATA wsaData;
    int     iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cerr << "WSAStartup failed with error: " << iResult << std::endl;
        return {};
    }

    // Create a socket to listen for incoming connections
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "socket failed with error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return {};
    }

    // Bind the socket to a local address and port
    sockaddr_in localAddr;
    localAddr.sin_family      = AF_INET;
    localAddr.sin_addr.s_addr = INADDR_ANY;
    localAddr.sin_port        = htons(1145);  // Replace with desired port number
    iResult                   = bind(listenSocket, reinterpret_cast<sockaddr*>(&localAddr), sizeof(localAddr));
    if (iResult == SOCKET_ERROR) {
        std::cerr << "bind failed with error: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return {};
    }

    // Set the socket to listen for incoming connections
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen failed with error: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return {};
    }

    SOCKET clientSocket;
    printf("Waiting for client to connect 127.0.0.1:1145 ...");
    // block until a client connects
    clientSocket = accept(listenSocket, nullptr, nullptr);

    std::thread acceptThread([&]() {
        while (true) {
            // clientSocket = accept(listenSocket, nullptr, nullptr);
            if (clientSocket == INVALID_SOCKET) {
                std::cerr << "accept failed with error: " << WSAGetLastError() << std::endl;
                break;
            }

            // Handle the connection in a separate thread
            std::thread connectionThread(handle_connection, clientSocket, std::ref(messageQueue));
            connectionThread.detach();
        }
    });

    std::thread messageThread([&]() {
        while (true) {
            if (clientSocket == INVALID_SOCKET) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            // Wait for a response from the main thread
            std::vector<int> response;
            std::string      responseStr = "";

            while (!responseQueue.tryPop(response)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            // to string
            for (int i = 0; i < response.size(); i++) {
                responseStr += std::to_string(response[i]) + ",";
            }
            // Send the response back to the client
            int sendResult = send(clientSocket, responseStr.c_str(), responseStr.size(), 0);
            if (sendResult == SOCKET_ERROR) {
                std::cerr << "\nsend failed with error: " << WSAGetLastError() << std::endl;
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
    WSACleanup();
    for (auto& thread : threads) {
        thread.join();
    }
}
