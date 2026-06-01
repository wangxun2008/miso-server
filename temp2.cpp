#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <thread>
#include <chrono>
#include <SFML/Network.hpp>
#include "client.h"
#include "protocol.h"

using namespace miso;

// 工具函数：从 sf::Packet 中提取 Opcode
static Opcode readOpcode(sf::Packet& p) {
    uint16_t raw;
    if (p >> raw)
        return static_cast<Opcode>(raw);
    return Opcode::C2S_Heartbeat; // 非法时返回一个占位
}

// 工具函数：等待特定 requestId 的回复（阻塞式）
static sf::Packet waitForReply(Client& client, int64_t requestId, Opcode expectedOpcode, int timeoutMs = 5000) {
    auto start = std::chrono::steady_clock::now();
    while (true) {
        auto replyOpt = client.receiveReply(requestId);
        if (replyOpt.has_value()) {
            sf::Packet& p = *replyOpt;
            Opcode op = readOpcode(p);
            if (op == Opcode::S2C_Error) {
                uint16_t errCode;
                std::string errMsg;
                if (p >> errCode >> errMsg) {
                    std::cerr << "Server error " << errCode << ": " << errMsg << std::endl;
                } else {
                    std::cerr << "Server error (malformed)" << std::endl;
                }
                return sf::Packet(); // 返回空包表示错误
            } else if (op == expectedOpcode) {
                return p;
            } else {
                std::cerr << "Unexpected opcode " << (uint16_t)op << ", expected " << (uint16_t)expectedOpcode << std::endl;
                return sf::Packet();
            }
        }
        if (std::chrono::steady_clock::now() - start > std::chrono::milliseconds(timeoutMs)) {
            std::cerr << "Request timed out" << std::endl;
            return sf::Packet();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

int main() {
    // 连接到本地服务器
    Client client(sf::IpAddress::LocalHost, 53000, 54000);
    if (!client.connect()) {
        std::cerr << "Failed to connect to server" << std::endl;
        return 1;
    }

    std::string line;
    std::cout << "Connected. Type 'help' for commands." << std::endl;

    while (true) {
        std::cout << "miso> ";
        if (!std::getline(std::cin, line) || line == "exit" || line == "quit")
            break;

        // 解析命令
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "help") {
            std::cout << "Commands:\n"
                      << "  register <username> <password>\n"
                      << "  login <username> <password>\n"
                      << "  sendglobal <message>\n"
                      << "  getglobal [limit] [offset]\n"
                      << "  exit/quit\n";
        }
        else if (cmd == "register") {
            std::string username, password;
            if (!(iss >> username >> password)) {
                std::cout << "Usage: register <username> <password>\n";
                continue;
            }
            // 构造请求包
            sf::Packet request;
            request << static_cast<uint16_t>(Opcode::C2S_Register) << username << password;
            int64_t reqId = client.sendRequest(std::move(request));

            sf::Packet reply = waitForReply(client, reqId, Opcode::S2C_Register);
            if (reply.getDataSize() > 0) {
                int userId;
                if (reply >> userId) {
                    std::cout << "Registered successfully, user ID: " << userId << std::endl;
                }
            }
        }
        else if (cmd == "login") {
            std::string username, password;
            if (!(iss >> username >> password)) {
                std::cout << "Usage: login <username> <password>\n";
                continue;
            }
            sf::Packet request;
            request << static_cast<uint16_t>(Opcode::C2S_Login) << username << password;
            int64_t reqId = client.sendRequest(std::move(request));

            sf::Packet reply = waitForReply(client, reqId, Opcode::S2C_Login);
            if (reply.getDataSize() > 0) {
                int userId;
                if (reply >> userId) {
                    std::cout << "Login successful, user ID: " << userId << std::endl;
                }
            }
        }
        else if (cmd == "sendglobal") {
            std::string message;
            std::getline(iss, message);
            // 去除前导空格
            size_t start = message.find_first_not_of(" \t");
            if (start != std::string::npos) {
                message = message.substr(start);
            }
            if (message.empty()) {
                std::cout << "Usage: sendglobal <message>\n";
                continue;
            }
            int senderId;
            std::cout << "Your user ID: ";
            std::cin >> senderId;
            std::cin.ignore(); // 清除换行符

            sf::Packet request;
            request << static_cast<uint16_t>(Opcode::C2S_SendGlobalMessage) << senderId << message;
            int64_t reqId = client.sendRequest(std::move(request));

            sf::Packet reply = waitForReply(client, reqId, Opcode::S2C_SendGlobalMessage);
            if (reply.getDataSize() > 0) {
                int msgId;
                if (reply >> msgId) {
                    std::cout << "Message sent, ID: " << msgId << std::endl;
                }
            }
        }
        else if (cmd == "getglobal") {
            int limit = 20, offset = 0;
            if (!(iss >> limit)) {
                // 无参数则使用默认
            } else {
                if (!(iss >> offset)) {
                    // 只有 limit 参数
                }
            }
            sf::Packet request;
            request << static_cast<uint16_t>(Opcode::C2S_GetGlobalMessages) << limit << offset;
            int64_t reqId = client.sendRequest(std::move(request));

            sf::Packet reply = waitForReply(client, reqId, Opcode::S2C_GlobalMessagesList);
            if (reply.getDataSize() > 0) {
                uint32_t count;
                if (reply >> count) {
                    for (uint32_t i = 0; i < count; ++i) {
                        int id, senderId;
                        std::string content;
                        int64_t createdAt;
                        if (reply >> id >> senderId >> content >> createdAt) {
                            std::cout << "[" << createdAt << "] " << senderId << ": " << content << std::endl;
                        }
                    }
                }
            }
        }
        else {
            std::cout << "Unknown command: " << cmd << ". Type 'help' for commands.\n";
        }
    }

    client.disconnect();
    return 0;
}
