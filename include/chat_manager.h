#pragma once
#include "storage.h"
#include "exceptions.h"
#include <vector>
#include <string>
#include <optional>

namespace miso {

class UserManager;
class ClanManager;

class ChatManager {
public:
    ChatManager(Storage& storage, UserManager& userMgr, ClanManager& clanMgr);

    // 发送全局消息（所有活跃用户可见）
    int sendGlobalMessage(int senderId, const std::string& content);

    // 发送战队消息（仅该战队成员可见，且发送者必须是战队成员）
    int sendClanMessage(int senderId, int clanId, const std::string& content);

    // 获取全局消息（按时间倒序，支持分页）
    std::vector<Message> getGlobalMessages(int limit = 50, int offset = 0) const;

    // 获取某个战队消息（按时间倒序，发送者必须是战队成员才能查看）
    std::vector<Message> getClanMessages(int clanId, int userId, int limit = 50, int offset = 0) const;

private:
    Storage& storage;
    UserManager& userMgr;
    ClanManager& clanMgr;
};

} // namespace miso
