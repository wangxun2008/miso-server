#pragma once
#include "storage.h"
#include "exceptions.h"
#include <vector>

namespace app {

class UserManager;

class NoticeManager {
public:
    NoticeManager(Storage& storage, UserManager& userMgr);

    // 发布通知，返回通知 ID
    int publishNotice(int publisherId, const std::string& title, const std::string& content);

    // 获取未删除的通知（按发布时间倒序，支持分页）
    std::vector<Notice> getActiveNotices(int limit = 50, int offset = 0) const;

    // 获取单个通知详情（未删除）
    Notice getNoticeById(int noticeId) const;

    // 删除通知（仅发布者可删除，软删除）
    void deleteNotice(int noticeId, int userId);

private:
    Storage& storage;
    UserManager& userMgr;
};

} // namespace app
