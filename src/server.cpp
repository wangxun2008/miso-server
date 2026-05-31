#include "server.h"
#include <iostream>

namespace miso {

Server::Server(unsigned short tcpPort, unsigned short udpPort)
    : tcpPort_(tcpPort), udpPort_(udpPort) {}

Server::~Server() { stop(); }

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

void Server::sendReply(int clientId, int64_t requestId, sf::Packet packet) {
    // 构造完整的发送包（requestId + 用户数据）
    sf::Packet out;
    out << requestId;
    if (packet.getDataSize() > 0)
        out.append(packet.getData(), packet.getDataSize());
    // 将回复放入队列，由网络线程实际发送
    outgoingQueue_.enqueue(OutgoingReply{clientId, requestId, std::move(packet)});
}

std::optional<RawClientMessage> Server::getNextRequest(sf::Time timeWindow) {
    // 第一步：将队列中的所有新消息推入临时堆
    RawClientMessage raw;
    while (incomingQueue_.try_dequeue(raw)) {
        if (!raw.packet.has_value()) {
            // 断开消息无需排序，立即返回
            return raw;
        }
        msgHeap_.push(HeapMessage{raw.timestamp, raw.clientId, raw.requestId,
                                  std::move(*raw.packet), false});
    }

    if (msgHeap_.empty()) return std::nullopt;

    // 第二步：检查堆顶消息是否已超过时间窗口
    const int64_t now = getCurrentTimestamp();
    const int64_t windowUs = timeWindow.asMicroseconds();

    const auto& top = msgHeap_.top();
    if (top.timestamp + windowUs >= now) {
        // 时间窗口内的最早消息，弹出并返回
        auto msg = std::move(const_cast<HeapMessage&>(top));
        msgHeap_.pop();
        return RawClientMessage{msg.clientId, msg.timestamp, msg.requestId,
                                std::move(msg.packet)};
    }
    return std::nullopt;
}

void Server::networkThreadFunc() {
    // 启动 TCP 监听
    if (listener_.listen(tcpPort_) != sf::Socket::Status::Done) {
        std::cerr << "[Server] Failed to listen on TCP port " << tcpPort_ << std::endl;
        running_ = false;
        return;
    }
    std::cout << "[Server] TCP listening on port " << listener_.getLocalPort() << std::endl;

    // 绑定 UDP 端口用于时间同步
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
        // 短暂等待事件，避免忙等
        if (selector.wait(sf::milliseconds(1))) {
            // ---------- UDP 时间同步请求 ----------
            if (selector.isReady(udpSocket_)) {
                sf::Packet packet;
                std::optional<sf::IpAddress> clientAddr;
                unsigned short clientPort = 0;
                if (udpSocket_.receive(packet, clientAddr, clientPort) == sf::Socket::Status::Done &&
                    clientAddr.has_value()) {
                    int64_t t1 = 0;
                    if (packet >> t1) {
                        int64_t t2 = serverClock_.getElapsedTime().asMicroseconds();
                        sf::Packet reply;
                        reply << t1 << t2;
                        if (udpSocket_.send(reply, *clientAddr, clientPort) != sf::Socket::Status::Done) {
                            std::cerr << "[Server] UDP send failed\n";
                        }
                    }
                }
            }

            // ---------- 接受新的 TCP 连接 ----------
            if (selector.isReady(listener_)) {
                sf::TcpSocket client;
                if (listener_.accept(client) == sf::Socket::Status::Done) {
                    int id = nextClientId_++;
                    {
                        std::lock_guard lock(clientsMutex_);
                        clients_.emplace(id, std::move(client));
                    }
                    selector.add(clients_.at(id));
                    std::cout << "[Server] Client connected, id=" << id << std::endl;
                }
            }

            // ---------- 处理已有 TCP 连接的数据 ----------
            {
                std::lock_guard lock(clientsMutex_);
                for (auto it = clients_.begin(); it != clients_.end();) {
                    if (selector.isReady(it->second)) {
                        sf::Packet packet;
                        auto status = it->second.receive(packet);
                        if (status == sf::Socket::Status::Done) {
                            int64_t timestamp = 0;
                            int64_t requestId = 0;
                            // 提取头部两个字段，剩余部分为用户数据
                            if (packet >> timestamp >> requestId) {
                                incomingQueue_.enqueue(RawClientMessage{it->first, timestamp, requestId,
                                                                        std::move(packet)});
                            } else {
                                std::cerr << "[Server] Malformed packet from client " << it->first << std::endl;
                            }
                        } else if (status == sf::Socket::Status::Disconnected) {
                            std::cout << "[Server] Client id=" << it->first << " disconnected\n";
                            incomingQueue_.enqueue(RawClientMessage{it->first, 0, 0, std::nullopt});
                            selector.remove(it->second);
                            it = clients_.erase(it);
                            continue;   // 已删除，跳过 ++it
                        }
                    }
                    ++it;
                }
            }
        }

        // ---------- 发送待发送的回复 ----------
        OutgoingReply out;
        while (outgoingQueue_.try_dequeue(out)) {
            std::lock_guard lock(clientsMutex_);
            auto found = clients_.find(out.clientId);
            if (found != clients_.end()) {
                sf::Packet sendPacket;
                sendPacket << out.requestId;
                if (out.packet.getDataSize() > 0)
                    sendPacket.append(out.packet.getData(), out.packet.getDataSize());
                if (found->second.send(sendPacket) != sf::Socket::Status::Done) {
                    std::cerr << "[Server] Failed to send reply to client " << out.clientId << std::endl;
                }
            }
        }
    }
}

} // namespace miso
