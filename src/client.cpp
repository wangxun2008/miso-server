#include "client.h"
#include <iostream>
#include <vector>
#include <chrono>

namespace miso {

Client::Client(sf::IpAddress serverIp, unsigned short tcpPort, unsigned short udpPort)
    : serverIp_(std::move(serverIp)), tcpPort_(tcpPort), udpPort_(udpPort) {}

Client::~Client() { disconnect(); }

bool Client::connect() {
    if (running_) return true;

    // 建立 TCP 连接，非阻塞模式
    if (tcpSocket_.connect(serverIp_, tcpPort_, sf::seconds(5)) != sf::Socket::Status::Done) {
        std::cerr << "[Client] TCP connection failed\n";
        return false;
    }
    tcpSocket_.setBlocking(false);
    std::cout << "[Client] TCP connected to " << serverIp_ << ":" << tcpPort_ << std::endl;

    // 绑定 UDP 随机端口
    if (udpSocket_.bind(sf::Socket::AnyPort) != sf::Socket::Status::Done) {
        std::cerr << "[Client] Failed to bind UDP socket\n";
        tcpSocket_.disconnect();
        return false;
    }

    // 启动线程
    running_ = true;
    stopSync_ = false;
    syncReady_ = false;
    netThread_ = std::thread(&Client::networkThreadFunc, this);
    syncThread_ = std::thread(&Client::timeSyncThreadFunc, this);

    // 等待首次时间同步完成（最多3秒）
    {
        std::unique_lock lock(syncMutex_);
        syncCv_.wait_for(lock, std::chrono::seconds(3), [this] { return syncReady_; });
    }
    if (!syncReady_) {
        std::cerr << "[Client] Initial time sync timeout\n";
        disconnect();
        return false;
    }

    std::cout << "[Client] Initial time sync done, offset=" << timeOffsetUs_.load() << " us\n";
    return true;
}

void Client::disconnect() {
    if (!running_) return;

    running_ = false;
    stopSync_ = true;

    tcpSocket_.disconnect();
    udpSocket_.unbind();

    if (netThread_.joinable()) netThread_.join();
    if (syncThread_.joinable()) syncThread_.join();

    // 清空队列和缓存
    RawServerMessage dummy;
    while (incomingQueue_.try_dequeue(dummy)) {}
    sf::Packet p;
    while (outgoingQueue_.try_dequeue(p)) {}
    pendingReplies_.clear();
}

void Client::syncTime() {
    // 手动触发同步：等待下一次 syncReady 信号
    std::unique_lock lock(syncMutex_);
    syncReady_ = false;
    syncCv_.wait(lock, [this] { return syncReady_; });
}

int64_t Client::getTimestamp() const {
    return localClock_.getElapsedTime().asMicroseconds() + timeOffsetUs_.load();
}

int64_t Client::sendRequest(sf::Packet packet) {
    int64_t reqId = nextRequestId_++;
    // 构造完整请求包：时间戳 + requestId + 用户数据
    sf::Packet out;
    out << getTimestamp() << reqId;
    if (packet.getDataSize() > 0)
        out.append(packet.getData(), packet.getDataSize());
    outgoingQueue_.enqueue(std::move(out));
    return reqId;
}

std::optional<sf::Packet> Client::receiveReply(int64_t requestId) {
    // 将网络线程收到的所有原始回复移入 pendingReplies_
    RawServerMessage raw;
    while (incomingQueue_.try_dequeue(raw)) {
        pendingReplies_.emplace(raw.requestId, std::move(raw.packet));
    }
    auto it = pendingReplies_.find(requestId);
    if (it != pendingReplies_.end()) {
        sf::Packet packet = std::move(it->second);
        pendingReplies_.erase(it);
        return packet;
    }
    return std::nullopt;
}

// ---------- 网络线程 ----------
void Client::networkThreadFunc() {
    while (running_) {
        // 发送所有待发送的请求
        sf::Packet outPacket;
        while (outgoingQueue_.try_dequeue(outPacket)) {
            if (tcpSocket_.send(outPacket) != sf::Socket::Status::Done) {
                std::cerr << "[Client] Send failed\n";
            }
        }

        // 接收 TCP 回复
        sf::Packet inPacket;
        auto status = tcpSocket_.receive(inPacket);
        if (status == sf::Socket::Status::Done) {
            int64_t reqId = 0;
            // 提取 requestId，剩余部分为用户数据
            if (inPacket >> reqId) {
                incomingQueue_.enqueue(RawServerMessage{reqId, std::move(inPacket)});
            } else {
                std::cerr << "[Client] Malformed reply packet\n";
            }
        } else if (status == sf::Socket::Status::Disconnected) {
            running_ = false;
        }

        sf::sleep(sf::milliseconds(1));   // 避免空转
    }
}

// ---------- 时间同步线程 ----------
void Client::timeSyncThreadFunc() {
    constexpr float SYNC_INTERVAL = 0.5f;   // 同步间隔（秒）
    constexpr int SAMPLE_COUNT = 10;        // 平滑移动平均样本数
    std::vector<int64_t> offsetSamples;

    while (!stopSync_) {
        auto frameStart = localClock_.getElapsedTime().asSeconds();

        // 发送 UDP 时间同步请求
        int64_t t1 = localClock_.getElapsedTime().asMicroseconds();
        sf::Packet request;
        request << t1;
        if (udpSocket_.send(request, serverIp_, udpPort_) != sf::Socket::Status::Done) {
            std::cerr << "[Client] UDP send failed\n";
        }

        // 接收服务端回复
        sf::Packet response;
        std::optional<sf::IpAddress> senderAddr;
        unsigned short senderPort = 0;
        if (udpSocket_.receive(response, senderAddr, senderPort) == sf::Socket::Status::Done) {
            int64_t t1_recv = 0, t2 = 0;
            if (response >> t1_recv >> t2) {
                int64_t t3 = localClock_.getElapsedTime().asMicroseconds();
                int64_t rttUs = t3 - t1_recv;
                int64_t offsetUs = t2 - (t3 - rttUs / 2);   // 简单 NTP 偏移计算

                offsetSamples.push_back(offsetUs);
                if (offsetSamples.size() > SAMPLE_COUNT)
                    offsetSamples.erase(offsetSamples.begin());

                int64_t avgOffset = 0;
                for (auto v : offsetSamples) avgOffset += v;
                avgOffset = offsetSamples.empty() ? 0 : avgOffset / static_cast<int64_t>(offsetSamples.size());

                timeOffsetUs_ = avgOffset;   // 原子更新偏移量

                // 通知同步已更新
                {
                    std::lock_guard lock(syncMutex_);
                    syncReady_ = true;
                }
                syncCv_.notify_one();
            }
        }

        // 控制同步周期
        auto elapsed = localClock_.getElapsedTime().asSeconds() - frameStart;
        if (elapsed < SYNC_INTERVAL)
            sf::sleep(sf::seconds(SYNC_INTERVAL - elapsed));
    }
}

} // namespace miso
