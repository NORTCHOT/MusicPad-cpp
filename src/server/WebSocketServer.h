#pragma once

#include "../backend/IMediaBackend.h"
#include <ixwebsocket/IXWebSocketServer.h>
#include <string>
#include <memory>
#include <mutex>

namespace musicpad::server {

class WebSocketServer {
public:
    WebSocketServer(int port, std::shared_ptr<backend::IMediaBackend> backend);
    ~WebSocketServer();

    bool start();
    void stop();

    void broadcastStatus(bool includeArtwork = false);

private:
    void handleClientMessage(std::shared_ptr<ix::ConnectionState> connectionState,
                            ix::WebSocket& webSocket,
                            const ix::WebSocketMessagePtr& msg);

    void processCommand(ix::WebSocket& webSocket, const std::string& jsonStr);

    int port_;
    std::shared_ptr<backend::IMediaBackend> backend_;
    std::unique_ptr<ix::WebSocketServer> server_;

    std::mutex stateMutex_;
    std::string lastBroadcastArtworkHash_;
};

} // namespace musicpad::server
