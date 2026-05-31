#include "client.h"
#include <iostream>
#include <vector>
#include <chrono>

namespace miso {

Client::Client(sf::IpAddress serverIp, unsigned short tcpPort, unsigned short udpPort)
    : serverIp_(std::move(serverIp)), tcpPort_(tcpPort), udpPort_(udpPort) {}

Client::~Client() {
    disconnect();
}

bool Client::connect() {
    if (running_) return true;

    if (tcpSocket_.connect(serverIp_, tcpPort_, sf::seconds(5)) != sf::Socket::Status::Done) {
        std::cerr << "[Client] TCP connection failed\n";
        return false;
    }
    // ✅ 关键：设置为非阻塞模式，避免网络线程卡在 receive
    tcpSocket_.setBlocking(false);

    std::cout << "[Client] TCP connected to " << serverIp_ << ":" << tcpPort_ << std::endl;

    if (udpSocket_.bind(sf::Socket::AnyPort) != sf::Socket::Status::Done) {
        std::cerr << "[Client] Failed to bind UDP socket\n";
        tcpSocket_.disconnect();
        return false;
    }

    running_ = true;
    stopSync_ = false;
    syncReady_ = false;

    netThread_ = std::thread(&Client::networkThreadFunc, this);
    syncThread_ = std::thread(&Client::timeSyncThreadFunc, this);

    // 等待初始时间同步完成（最多 3 秒）
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

    // 清空队列
    sf::Packet dummy;
    while (incomingQueue_.try_dequeue(dummy)) {}
    while (outgoingQueue_.try_dequeue(dummy)) {}
}

void Client::syncTime() {
    std::unique_lock lock(syncMutex_);
    syncReady_ = false;
    syncCv_.wait(lock, [this] { return syncReady_; });
}

int64_t Client::getTimestamp() const {
    return localClock_.getElapsedTime().asMicroseconds() + timeOffsetUs_.load();
}

bool Client::sendMessage(sf::Packet packet) {
    if (!running_) return false;

    // 获取原始数据（用户构造的包，尚未被读取）
    const void* data = packet.getData();
    std::size_t size = packet.getDataSize();

    // 构建新包：时间戳 + 原始数据
    sf::Packet newPacket;
    newPacket << getTimestamp();
    if (size > 0)
        newPacket.append(data, size);

    return outgoingQueue_.enqueue(std::move(newPacket));
}

std::optional<sf::Packet> Client::receiveMessage() {
    sf::Packet packet;
    if (incomingQueue_.try_dequeue(packet))
        return packet;
    return std::nullopt;
}

// ---------- 网络线程 ----------
void Client::networkThreadFunc() {
    while (running_) {
        // 1. 处理发送队列
        sf::Packet outPacket;
        while (outgoingQueue_.try_dequeue(outPacket)) {
            if (tcpSocket_.send(outPacket) == sf::Socket::Status::Done)
                ; // 发送成功
            else
                std::cerr << "[Client] Send failed\n";
        }

        // 2. 尝试接收数据（非阻塞）
        sf::Packet inPacket;
        auto status = tcpSocket_.receive(inPacket);
        if (status == sf::Socket::Status::Done) {
            incomingQueue_.enqueue(std::move(inPacket));
        } else if (status == sf::Socket::Status::Disconnected) {
            running_ = false;
        }
        // 其他状态（NotReady、Error 等）忽略

        // 短暂休眠，防止 CPU 空转
        sf::sleep(sf::milliseconds(1));
    }
}

// ---------- 时间同步线程 ----------
void Client::timeSyncThreadFunc() {
    constexpr float SYNC_INTERVAL = 0.5f;
    constexpr int SAMPLE_COUNT = 10;
    std::vector<int64_t> offsetSamples;

    while (!stopSync_) {
        auto frameStart = localClock_.getElapsedTime().asSeconds();

        // 发送请求 t1
        int64_t t1 = localClock_.getElapsedTime().asMicroseconds();
        sf::Packet request;
        request << t1;
        if (udpSocket_.send(request, serverIp_, udpPort_) != sf::Socket::Status::Done) {
            // 可以忽略错误
        }

        // 接收回应
        sf::Packet response;
        std::optional<sf::IpAddress> senderAddr;
        unsigned short senderPort = 0;
        if (udpSocket_.receive(response, senderAddr, senderPort) == sf::Socket::Status::Done) {
            int64_t t1_recv = 0, t2 = 0;
            if (response >> t1_recv >> t2) {
                int64_t t3 = localClock_.getElapsedTime().asMicroseconds();
                int64_t rttUs = t3 - t1_recv;
                int64_t offsetUs = t2 - (t3 - rttUs / 2);

                offsetSamples.push_back(offsetUs);
                if (offsetSamples.size() > SAMPLE_COUNT)
                    offsetSamples.erase(offsetSamples.begin());

                int64_t avgOffset = 0;
                for (auto v : offsetSamples) avgOffset += v;
                avgOffset = offsetSamples.empty() ? 0 : avgOffset / static_cast<int64_t>(offsetSamples.size());

                timeOffsetUs_ = avgOffset;

                {
                    std::lock_guard lock(syncMutex_);
                    syncReady_ = true;
                }
                syncCv_.notify_one();
            }
        }

        // 控制频率
        auto elapsed = localClock_.getElapsedTime().asSeconds() - frameStart;
        if (elapsed < SYNC_INTERVAL)
            sf::sleep(sf::seconds(SYNC_INTERVAL - elapsed));
    }
}

}  // namespace miso
