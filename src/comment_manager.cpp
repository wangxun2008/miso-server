#include "comment_manager.h"
#include "topic_manager.h"
#include "user_manager.h"
#include <sqlite_orm/sqlite_orm.h>

namespace app {

using namespace sqlite_orm;

CommentManager::CommentManager(Storage& storage, UserManager& userMgr, TopicManager& topicMgr)
    : storage(storage), userMgr(userMgr), topicMgr(topicMgr) {}

int CommentManager::addComment(int topicId, int authorId, const std::string& content) {
    if (!userMgr.isUserActive(authorId)) {
        throw UserNotFoundException("ID " + std::to_string(authorId));
    }
    topicMgr.getTopicById(topicId, authorId); // 检查权限

    Comment comment;
    comment.topic_id = topicId;
    comment.author_id = authorId;
    comment.content = content;
    comment.created_at = getCurrentTimestamp();
    comment.deleted_at = 0;
    return storage.insert(comment);
}

std::vector<Comment> CommentManager::getComments(int topicId, int viewerId, int limit, int offset) const {
    topicMgr.getTopicById(topicId, viewerId); // 权限检查

    return storage.get_all<Comment>(
        where(c(&Comment::topic_id) == topicId and c(&Comment::deleted_at) == 0),
        order_by(&Comment::created_at).asc(),
        sqlite_orm::limit(limit, sqlite_orm::offset(offset))
    );
}

void CommentManager::updateComment(int commentId, int editorId, const std::string& newContent) {
    auto comments = storage.get_all<Comment>(
        where(c(&Comment::id) == commentId and c(&Comment::deleted_at) == 0)
    );
    if (comments.empty()) {
        throw CommentNotFoundException(commentId);
    }
    Comment comment = comments.front();
    if (comment.author_id != editorId) {
        throw NotAuthorizedException();
    }
    comment.content = newContent;
    storage.update(comment);
}

void CommentManager::deleteComment(int commentId, int deleterId) {
    auto comments = storage.get_all<Comment>(
        where(c(&Comment::id) == commentId and c(&Comment::deleted_at) == 0)
    );
    if (comments.empty()) {
        throw CommentNotFoundException(commentId);
    }
    Comment comment = comments.front();
    if (comment.author_id != deleterId) {
        throw NotAuthorizedException();
    }
    comment.deleted_at = getCurrentTimestamp();
    storage.update(comment);
}

} // namespace app
