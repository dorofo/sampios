#include "LogServer.h"

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <cstring>
#include <thread>

namespace samp {
namespace {

#if defined(_WIN32)
using SocketHandle = SOCKET;
constexpr SocketHandle kBadSocket = INVALID_SOCKET;
void CloseSocket(SocketHandle s) { closesocket(s); }
void ShutdownSocket(SocketHandle s) { shutdown(s, SD_BOTH); }
#else
using SocketHandle = int;
constexpr SocketHandle kBadSocket = -1;
void CloseSocket(SocketHandle s) { ::close(s); }
void ShutdownSocket(SocketHandle s) { ::shutdown(s, SHUT_RDWR); }
#endif

constexpr const char* kNotFound =
    "HTTP/1.1 404 Not Found\r\n"
    "Content-Type: text/plain\r\n"
    "Connection: close\r\n\r\n"
    "use /log";

}  // namespace

LogServer::~LogServer() {
    Stop();
}

bool LogServer::Start(unsigned short port, Source source) {
    Stop();
    m_source = std::move(source);
    m_port = port;

    const SocketHandle handle = ::socket(AF_INET, SOCK_STREAM, 0);
    if (handle == kBadSocket) return false;

    int yes = 1;
    ::setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes),
                 sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (::bind(handle, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
        ::listen(handle, 4) != 0) {
        CloseSocket(handle);
        return false;
    }

    m_socket = static_cast<int>(handle);
    const intptr_t raw = static_cast<intptr_t>(handle);
    std::thread([this, raw] { Loop(raw); }).detach();
    return true;
}

void LogServer::Stop() {
    if (m_socket < 0) return;
    const SocketHandle handle = static_cast<SocketHandle>(m_socket);
    m_socket = -1;
    ShutdownSocket(handle);
    CloseSocket(handle);
}

void LogServer::Loop(intptr_t raw) {
    const SocketHandle handle = static_cast<SocketHandle>(raw);
    while (true) {
        sockaddr_in peer{};
#if defined(_WIN32)
        int peerLen = sizeof(peer);
#else
        socklen_t peerLen = sizeof(peer);
#endif
        const SocketHandle client = ::accept(handle, reinterpret_cast<sockaddr*>(&peer), &peerLen);
        if (client == kBadSocket) {
            if (m_socket < 0) return;
            continue;
        }

        char request[512] = {};
        const int received = static_cast<int>(::recv(client, request, sizeof(request) - 1, 0));
        std::string response;
        if (received > 0 && std::strstr(request, "/log") != nullptr) {
            const std::string body = m_source ? m_source() : std::string("no data\n");
            response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\n"
                       "Cache-Control: no-store\r\nConnection: close\r\n"
                       "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
        } else {
            response = kNotFound;
        }

        ::send(client, response.data(), static_cast<int>(response.size()), 0);
        ShutdownSocket(client);
        CloseSocket(client);

        if (m_socket < 0) return;
    }
}

}  // namespace samp
