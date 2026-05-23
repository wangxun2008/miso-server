#include "notice_manager.h"
#include "user_manager.h"
#include <sqlite_orm/sqlite_orm.h>

namespace app {

using namespace sqlite_orm;

NoticeManager::NoticeManager(Storage& storage, UserManager& userMgr)
    : storage(storage), userMgr(userMgr) {}

int NoticeManager::publishNotice(int publisherId, const std::string& title, const std::string& content) {
    if (!userMgr.isUserActive(publisherId)) {
        throw UserNotFoundException("ID " + std::to_string(publisherId));
    }
    Notice notice;
    notice.publisher_id = publisherId;
    notice.title = title;
    notice.content = content;
    notice.published_at = getCurrentTimestamp();
    notice.deleted_at = 0;
    return storage.insert(notice);
}

std::vector<Notice> NoticeManager::getActiveNotices(int limit, int offset) const {
    return storage.get_all<Notice>(
        where(c(&Notice::deleted_at) == 0),
        order_by(&Notice::published_at).desc(),
        sqlite_orm::limit(limit, sqlite_orm::offset(offset))
    );
}

Notice NoticeManager::getNoticeById(int noticeId) const {
    auto notices = storage.get_all<Notice>(
        where(c(&Notice::id) == noticeId and c(&Notice::deleted_at) == 0)
    );
    if (notices.empty()) {
        throw NoticeNotFoundException(noticeId);
    }
    return notices.front();
}

void NoticeManager::deleteNotice(int noticeId, int userId) {
    auto notices = storage.get_all<Notice>(
        where(c(&Notice::id) == noticeId and c(&Notice::deleted_at) == 0)
    );
    if (notices.empty()) {
        throw NoticeNotFoundException(noticeId);
    }
    Notice notice = notices.front();
    if (notice.publisher_id != userId) {
        throw NotAuthorizedException();
    }
    notice.deleted_at = getCurrentTimestamp();
    storage.update(notice);
}

} // namespace app
