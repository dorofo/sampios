// Minimal HTTP endpoint that exposes the client log over the local network:
//
//     http://<iphone-ip>:8088/log
//
// It exists so a session on the device can be diagnosed from a PC on the same
// Wi-Fi without Xcode, a cable or libimobiledevice.
#pragma once

#include <functional>
#include <string>

namespace samp {

class LogServer {
public:
    using Source = std::function<std::string()>;

    LogServer() = default;
    ~LogServer();

    LogServer(const LogServer&) = delete;
    LogServer& operator=(const LogServer&) = delete;

    bool Start(unsigned short port, Source source);
    void Stop();
    bool Running() const { return m_socket >= 0; }
    unsigned short Port() const { return m_port; }

private:
    void Loop(intptr_t handle);

    int m_socket = -1;
    unsigned short m_port = 0;
    Source m_source;
};

}  // namespace samp
