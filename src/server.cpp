#include "server.h"
#include <iostream>

namespace miso {

Server::Server(unsigned short tcpPort, unsigned short udpPort)
    : tcpPort_(tcpPort), udpPort_(udpPort) {}

Server::~Server() {
    stop();
}

void Server::start() {
    if (running_) return;
    running_ = true;
    netThread_ = std::thread(&Server::networkThreadFunc, this);
}

void Server::stop() {
    if (!running_) return;
    running_ = false;
    if (netThread_.joinable()) netThread_.join();
}

int64_t Server::getCurrentTimestamp() const {
    return serverClock_.getElapsedTime().asMicroseconds();
}

void Server::sendMessage(int clientId, sf::Packet packet) {
    outgoingQueue_.enqueue(OutgoingMessage{clientId, std::move(packet)});
}

std::optional<RawClientMessage> Server::getNextMessage(sf::Time timeWindow) {
    // 1. 将网络线程送来的所有原始消息移入堆中
    RawClientMessage raw;
    while (incomingQueue_.try_dequeue(raw)) {
        if (!raw.packet.has_value()) {
            // 断开消息立即作为高优先级返回（不入堆）
            // 直接返回该断开消息，但需要一次性，这里返回后该客户端就没了
            return raw; // 调用方看到 packet 为空即知是断开
        }
        // 正常消息入堆
        msgHeap_.push(HeapMessage{raw.timestamp, raw.clientId, std::move(*raw.packet), false});
    }

    // 2. 检查堆顶是否到期
    if (msgHeap_.empty()) return std::nullopt;

    const int64_t now = getCurrentTimestamp();
    const int64_t windowUs = timeWindow.asMicroseconds();

    const auto& top = msgHeap_.top();
    if (top.timestamp + windowUs >= now) {
        // 到期，弹出并返回
        auto msg = std::move(const_cast<HeapMessage&>(top)); // 安全，因为立刻 pop
        msgHeap_.pop();
        return RawClientMessage{msg.clientId, msg.timestamp, std::move(msg.packet)};
    }

    return std::nullopt;
}

void Server::networkThreadFunc() {
    // 绑定监听
    if (listener_.listen(tcpPort_) != sf::Socket::Status::Done) {
        std::cerr << "[Server] Failed to listen on TCP port " << tcpPort_ << std::endl;
        running_ = false;
        return;
    }
    std::cout << "[Server] TCP listening on port " << listener_.getLocalPort() << std::endl;

    // 绑定 UDP
    if (udpSocket_.bind(udpPort_) != sf::Socket::Status::Done) {
        std::cerr << "[Server] Failed to bind UDP port " << udpPort_ << std::endl;
        running_ = false;
        return;
    }
    std::cout << "[Server] UDP time sync on port " << udpPort_ << std::endl;

    sf::SocketSelector selector;
    selector.add(listener_);
    selector.add(udpSocket_);

    while (running_) {
        if (selector.wait(sf::milliseconds(1))) {
            // ---------- UDP 时间同步 ----------
            if (selector.isReady(udpSocket_)) {
                sf::Packet packet;
                std::optional<sf::IpAddress> clientAddr;
                unsigned short clientPort = 0;
                if (udpSocket_.receive(packet, clientAddr, clientPort) == sf::Socket::Status::Done) {
                    if (clientAddr.has_value()) {
                        int64_t t1 = 0;
                        if (packet >> t1) {
                            int64_t t2 = serverClock_.getElapsedTime().asMicroseconds();
                            sf::Packet reply;
                            reply << t1 << t2;
							if (udpSocket_.send(reply, *clientAddr, clientPort) != sf::Socket::Status::Done)
								std::cerr << "[Server] UDP time sync reply failed\n";
                        }
                    }
                }
            }

            // ---------- TCP 新连接 ----------
            if (selector.isReady(listener_)) {
                sf::TcpSocket client;
                if (listener_.accept(client) == sf::Socket::Status::Done) {
                    int id = nextClientId_++;
                    {
                        std::lock_guard lock(clientsMutex_);
                        clients_.emplace(id, std::move(client));
                    }
                    selector.add(clients_.at(id)); // 安全，因为外面刚添加
                    std::cout << "[Server] Client connected, id=" << id
                              << " from " << clients_.at(id).getRemoteAddress().value() << std::endl;
                }
            }

            // ---------- TCP 数据接收 ----------
            {
                std::lock_guard lock(clientsMutex_);
                for (auto it = clients_.begin(); it != clients_.end(); ) {
                    if (selector.isReady(it->second)) {
                        sf::Packet packet;
                        auto status = it->second.receive(packet);
                        if (status == sf::Socket::Status::Done) {
                            int64_t timestamp = 0;
                            packet >> timestamp;
                            incomingQueue_.enqueue(RawClientMessage{it->first, timestamp, std::move(packet)});
                        } else if (status == sf::Socket::Status::Disconnected) {
                            std::cout << "[Server] Client id=" << it->first << " disconnected\n";
                            incomingQueue_.enqueue(RawClientMessage{it->first, 0, std::nullopt});
                            selector.remove(it->second);
                            it = clients_.erase(it);
                            continue;
                        }
                    }
                    ++it;
                }
            }
        }

        // ---------- 发送回复 ----------
        OutgoingMessage outMsg;
        while (outgoingQueue_.try_dequeue(outMsg)) {
            std::lock_guard lock(clientsMutex_);
            auto found = clients_.find(outMsg.clientId);
            if (found != clients_.end()) {
                if (found->second.send(outMsg.packet) != sf::Socket::Status::Done)
                    std::cerr << "[Server] Failed to send to client " << outMsg.clientId << std::endl;
            }
        }
    }
}

}  // namespace miso
