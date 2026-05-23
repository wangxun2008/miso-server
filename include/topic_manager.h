#pragma once
#include "storage.h"
#include "exceptions.h"
#include <vector>
#include <optional>

namespace app {

class UserManager;
class ClanManager;

class TopicManager {
public:
    TopicManager(Storage& storage, UserManager& userMgr, ClanManager& clanMgr);

    int createTopic(int authorId, const std::string& title, const std::string& content,
                    int scope, std::optional<int> targetId = std::nullopt);

    std::vector<Topic> getVisibleTopics(int viewerId, int limit = 50, int offset = 0) const;

    Topic getTopicById(int topicId, int viewerId) const;

    void updateTopic(int topicId, int editorId, const std::string& newTitle, const std::string& newContent);

    void deleteTopic(int topicId, int deleterId);

private:
    Storage& storage;
    UserManager& userMgr;
    ClanManager& clanMgr;

    bool canView(const Topic& topic, int viewerId) const;
};

} // namespace app
