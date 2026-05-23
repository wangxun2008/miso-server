#pragma once
#include "storage.h"
#include "exceptions.h"
#include <vector>

namespace app {

class UserManager;
class TopicManager;

class CommentManager {
public:
    CommentManager(Storage& storage, UserManager& userMgr, TopicManager& topicMgr);

    int addComment(int topicId, int authorId, const std::string& content);

    std::vector<Comment> getComments(int topicId, int viewerId, int limit = 50, int offset = 0) const;

    void updateComment(int commentId, int editorId, const std::string& newContent);

    void deleteComment(int commentId, int deleterId);

private:
    Storage& storage;
    UserManager& userMgr;
    TopicManager& topicMgr;
};

} // namespace app
