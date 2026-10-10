#include "WebSocketServer.h"
#include "../utils/Logger.h"
#include <nlohmann/json.hpp>
#include <ixwebsocket/IXNetSystem.h>

using json = nlohmann::json;

namespace musicpad::server {

WebSocketServer::WebSocketServer(int port, std::shared_ptr<backend::IMediaBackend> backend)
    : port_(port), backend_(std::move(backend)) {
    ix::initNetSystem();
    // 0.0.0.0 binds to all local network interfaces (Wi-Fi, Ethernet, localhost)
    server_ = std::make_unique<ix::WebSocketServer>(port_, "0.0.0.0");
    server_->disablePerMessageDeflate();
    server_->enablePong();
}

WebSocketServer::~WebSocketServer() {
    stop();
}

bool WebSocketServer::start() {
    utils::Logger::info("Starting WebSocketServer on port " + std::to_string(port_) + "...");

    server_->setOnClientMessageCallback(
        [this](std::shared_ptr<ix::ConnectionState> connectionState,
               ix::WebSocket& webSocket,
               const ix::WebSocketMessagePtr& msg) {
            handleClientMessage(connectionState, webSocket, msg);
        }
    );

    if (!server_->listenAndStart()) {
        utils::Logger::error("Failed to start WebSocket server on port " + std::to_string(port_));
        return false;
    }

    // Hook backend change callback to automatically broadcast lightweight status
    if (backend_) {
        backend_->setOnChangeCallback([this](const backend::MediaInfo& /*info*/) {
            broadcastStatus(false);
        });
    }

    utils::Logger::info("WebSocketServer listening on ws://0.0.0.0:" + std::to_string(port_));
    return true;
}

void WebSocketServer::stop() {
    if (server_) {
        utils::Logger::info("Stopping WebSocketServer...");
        server_->stop();
        ix::uninitNetSystem();
    }
}

void WebSocketServer::handleClientMessage(std::shared_ptr<ix::ConnectionState> connectionState,
                                         ix::WebSocket& webSocket,
                                         const ix::WebSocketMessagePtr& msg) {
    if (msg->type == ix::WebSocketMessageType::Open) {
        utils::Logger::info("Client connected: " + connectionState->getRemoteIp() + " (ID: " + connectionState->getId() + ")");
        if (backend_) {
            backend::MediaInfo info = backend_->getArtwork();
            json response = {
                {"type", "now_playing"},
                {"data", info}
            };
            webSocket.sendText(response.dump());
        }
    } else if (msg->type == ix::WebSocketMessageType::Close) {
        utils::Logger::info("Client disconnected: " + connectionState->getRemoteIp() + " (ID: " + connectionState->getId() + ")");
    } else if (msg->type == ix::WebSocketMessageType::Error) {
        utils::Logger::warn("Client error: " + msg->errorInfo.reason);
    } else if (msg->type == ix::WebSocketMessageType::Message) {
        processCommand(webSocket, msg->str);
    }
}

void WebSocketServer::processCommand(ix::WebSocket& webSocket, const std::string& jsonStr) {
    if (!backend_) return;

    try {
        json j = json::parse(jsonStr);
        std::string cmd = j.value("cmd", "");

        if (cmd.empty()) {
            utils::Logger::warn("Received JSON without 'cmd' field: " + jsonStr);
            return;
        }

        utils::Logger::debug("Executing command: " + cmd);

        if (cmd == "play") {
            backend_->play();
        } else if (cmd == "pause") {
            backend_->pause();
        } else if (cmd == "toggle") {
            backend_->togglePlayPause();
        } else if (cmd == "next") {
            backend_->next();
        } else if (cmd == "prev") {
            backend_->previous();
        } else if (cmd == "seek") {
            int64_t pos = j.value("position", int64_t(0));
            backend_->seek(pos);
        } else if (cmd == "setVolume") {
            int vol = j.value("volume", 50);
            backend_->setVolume(vol);
        } else if (cmd == "setMute") {
            bool mute = j.value("mute", false);
            backend_->setMute(mute);
        } else if (cmd == "getStatus") {
            backend::MediaInfo info = backend_->getNowPlaying();
            info.artwork = ""; // Omit heavy base64 artwork string
            json response = {
                {"type", "now_playing"},
                {"data", info}
            };
            webSocket.sendText(response.dump());
        } else if (cmd == "getArtwork") {
            backend::MediaInfo info = backend_->getArtwork();
            json response = {
                {"type", "artwork"},
                {"artwork", info.artwork},
                {"artworkHash", info.artworkHash}
            };
            webSocket.sendText(response.dump());
        } else if (cmd == "ping") {
            json response = {{"type", "pong"}};
            webSocket.sendText(response.dump());
        } else {
            utils::Logger::warn("Unknown command: " + cmd);
        }
    } catch (const std::exception& e) {
        utils::Logger::error(std::string("Error processing command: ") + e.what());
    }
}

void WebSocketServer::broadcastStatus(bool includeArtwork) {
    if (!server_ || !backend_) return;

    backend::MediaInfo info = backend_->getNowPlaying();

    bool sendFullArtwork = includeArtwork;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (!info.artworkHash.empty() && info.artworkHash != lastBroadcastArtworkHash_ && !info.artwork.empty()) {
            // Artwork just changed or became available: include full base64 in this broadcast!
            sendFullArtwork = true;
            lastBroadcastArtworkHash_ = info.artworkHash;
        } else if (info.artworkHash.empty()) {
            lastBroadcastArtworkHash_.clear();
        }
    }

    if (!sendFullArtwork) {
        // Omit heavy base64 string to conserve bandwidth, but keep artworkHash
        info.artwork = "";
    }

    json response = {
        {"type", "now_playing"},
        {"data", info}
    };
    std::string serialized = response.dump();

    auto clients = server_->getClients();
    for (auto& client : clients) {
        if (client) {
            client->sendText(serialized);
        }
    }
}

} // namespace musicpad::server
