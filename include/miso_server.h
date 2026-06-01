#pragma once
#include "server.h"
#include "protocol.h"
#include "user_manager.h"
#include "clan_manager.h"
#include "chat_manager.h"
#include "game_manager.h"
#include "chunk_manager.h"
#include "online_manager.h"
#include "notice_manager.h"
#include "topic_manager.h"
#include "comment_manager.h"
#include "mine_map_manager.h"
#include "game_logic_manager.h"
#include <unordered_map>
#include <optional>
#include <atomic>
#include <memory>

namespace miso {

class MisoServer {
public:
    MisoServer(unsigned short tcpPort, unsigned short udpPort);
    ~MisoServer();

    void start();   // 启动主循环（阻塞）
    void stop();    // 请求停止

private:
    void handleRequest(RawClientMessage& msg);
    void sendError(int clientId, int64_t requestId, ErrorCode code, const std::string& msg);
    void sendReply(int clientId, int64_t requestId, sf::Packet data, Opcode opcode);
    bool checkAuth(int clientId, int userId, int64_t requestId);

    Server server_;
    std::unique_ptr<Storage> storage_;
    UserManager userMgr_;
    ClanManager clanMgr_;
    ChatManager chatMgr_;
    GameManager gameMgr_;
    ChunkManager chunkMgr_;
    OnlineManager onlineMgr_;
    NoticeManager noticeMgr_;
    TopicManager topicMgr_;
    CommentManager commentMgr_;
    MineMapManager mineMapMgr_;
    GameLogicManager gameLogicMgr_;

    std::unordered_map<int, std::optional<int>> clientAccounts_;
    std::atomic<bool> running_{false};
};

} // namespace miso
