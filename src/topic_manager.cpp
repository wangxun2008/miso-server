#include "topic_manager.h"
#include "user_manager.h"
#include "clan_manager.h"
#include <sqlite_orm/sqlite_orm.h>

namespace app {

using namespace sqlite_orm;

TopicManager::TopicManager(Storage& storage, UserManager& userMgr, ClanManager& clanMgr)
    : storage(storage), userMgr(userMgr), clanMgr(clanMgr) {}

bool TopicManager::canView(const Topic& topic, int viewerId) const {
    if (topic.scope == 0) return true;
    if (topic.scope == 1 && topic.target_id.has_value()) {
        if (!userMgr.isUserActive(viewerId)) return false;
        try {
            auto members = clanMgr.getClanMembers(topic.target_id.value());
            for (const auto& m : members) {
                if (m.user_id == viewerId) return true;
            }
        } catch (...) {
            return false;
        }
    }
    return false;
}

int TopicManager::createTopic(int authorId, const std::string& title, const std::string& content,
                              int scope, std::optional<int> targetId) {
    if (!userMgr.isUserActive(authorId)) {
        throw UserNotFoundException("ID " + std::to_string(authorId));
    }
    if (scope < 0 || scope > 1) {
        throw InvalidScopeException(scope);
    }
    if (scope == 1) {
        if (!targetId.has_value()) {
            throw DiscussionException("target_id required for clan scope");
        }
        Clan clan = clanMgr.getClan(targetId.value());
        auto members = clanMgr.getClanMembers(clan.id);
        bool isMember = false;
        for (const auto& m : members) {
            if (m.user_id == authorId) {
                isMember = true;
                break;
            }
        }
        if (!isMember) {
            throw AccessDeniedException("Author is not a member of target clan");
        }
    } else {
        targetId = std::nullopt;
    }

    Topic topic;
    topic.author_id = authorId;
    topic.title = title;
    topic.content = content;
    topic.scope = scope;
    topic.target_id = targetId;
    topic.created_at = getCurrentTimestamp();
    topic.updated_at = topic.created_at;
    topic.deleted_at = 0;
    return storage.insert(topic);
}

std::vector<Topic> TopicManager::getVisibleTopics(int viewerId, int limit, int offset) const {
    if (!userMgr.isUserActive(viewerId)) {
        throw UserNotFoundException("ID " + std::to_string(viewerId));
    }
    auto allTopics = storage.get_all<Topic>(
        where(c(&Topic::deleted_at) == 0),
        order_by(&Topic::updated_at).desc()
    );
    std::vector<Topic> visible;
    for (auto& t : allTopics) {
        if (canView(t, viewerId)) {
            visible.push_back(std::move(t));
        }
    }
    if (offset < 0) offset = 0;
    if (offset >= (int)visible.size()) return {};
    int end = offset + limit;
    if (end > (int)visible.size()) end = visible.size();
    return std::vector<Topic>(visible.begin() + offset, visible.begin() + end);
}

Topic TopicManager::getTopicById(int topicId, int viewerId) const {
    auto topics = storage.get_all<Topic>(
        where(c(&Topic::id) == topicId and c(&Topic::deleted_at) == 0)
    );
    if (topics.empty()) {
        throw TopicNotFoundException(topicId);
    }
    const Topic& topic = topics.front();
    if (!canView(topic, viewerId)) {
        throw AccessDeniedException("You do not have permission to view this topic");
    }
    return topic;
}

void TopicManager::updateTopic(int topicId, int editorId, const std::string& newTitle, const std::string& newContent) {
    auto topics = storage.get_all<Topic>(
        where(c(&Topic::id) == topicId and c(&Topic::deleted_at) == 0)
    );
    if (topics.empty()) {
        throw TopicNotFoundException(topicId);
    }
    Topic topic = topics.front();
    if (topic.author_id != editorId) {
        throw NotAuthorizedException();
    }
    topic.title = newTitle;
    topic.content = newContent;
    topic.updated_at = getCurrentTimestamp();
    storage.update(topic);
}

void TopicManager::deleteTopic(int topicId, int deleterId) {
    auto topics = storage.get_all<Topic>(
        where(c(&Topic::id) == topicId and c(&Topic::deleted_at) == 0)
    );
    if (topics.empty()) {
        throw TopicNotFoundException(topicId);
    }
    Topic topic = topics.front();
    if (topic.author_id != deleterId) {
        throw NotAuthorizedException();
    }
    topic.deleted_at = getCurrentTimestamp();
    storage.update(topic);
}

} // namespace app
