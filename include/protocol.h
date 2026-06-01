#pragma once
#include <cstdint>

namespace miso {

/**
 * 操作码定义（范围 1000-9999）
 * 每个模块分配 100 个码，C2S 为偶数，对应 S2C 为奇数
 * 格式：C2S_XXX = 模块起始 + 0, 2, 4...  S2C_XXX = 模块起始 + 1, 3, 5...
 */
enum class Opcode : uint16_t {
    // ========== 通用/系统 1000-1099 ==========
    C2S_Heartbeat      = 1000,   // 发送：客户端本地时间戳
    S2C_Heartbeat      = 1001,   // 回复：服务器时间戳
	S2C_Error		   = 1099,   // 服务器返回错误：errorCode(ErrorCode), errorMessage(std::string)

    // ========== 用户管理 1100-1199 ==========
    C2S_Register       = 1100,   // 发送：username, password
    S2C_Register       = 1101,   // 回复：成功则返回 userId，失败返回错误码
    C2S_Login          = 1102,   // 发送：username, password
    S2C_Login          = 1103,   // 回复：成功返回 userId，失败错误码
    C2S_DeleteUser     = 1104,   // 发送：username (软删除)
    S2C_DeleteUser     = 1105,   // 回复：成功空
    C2S_RestoreUser    = 1106,   // 发送：username
    S2C_RestoreUser    = 1107,   // 回复：成功空
    C2S_PermanentDeleteUser = 1108, // 发送：username
    S2C_PermanentDeleteUser = 1109, // 回复：成功空
    C2S_RenameUser     = 1110,   // 发送：oldUsername, newUsername
    S2C_RenameUser     = 1111,   // 回复：成功空
    C2S_ChangePassword = 1112,   // 发送：username, oldPassword, newPassword
    S2C_ChangePassword = 1113,   // 回复：成功空
    C2S_GetActiveUsers = 1114,   // 发送：空
    S2C_ActiveUsersList = 1115,  // 回复：用户ID列表 + 用户名列表（成对）
    C2S_GetAllUsers    = 1116,   // 发送：空
    S2C_AllUsersList   = 1117,   // 回复：用户ID列表 + 用户名列表 + 删除状态

    // ========== 战队管理 1200-1299 ==========
    C2S_CreateClan       = 1200, // 发送：leaderId, clanName
    S2C_CreateClan       = 1201, // 回复：clanId
    C2S_DissolveClan     = 1202, // 发送：clanId
    S2C_DissolveClan     = 1203, // 回复：空
    C2S_AddClanMember    = 1204, // 发送：clanId, userId
    S2C_AddClanMember    = 1205, // 回复：空
    C2S_RemoveClanMember = 1206, // 发送：clanId, userId
    S2C_RemoveClanMember = 1207, // 回复：空
    C2S_TransferLeadership = 1208, // 发送：clanId, newLeaderId
    S2C_TransferLeadership = 1209, // 回复：空
    C2S_GetActiveClans   = 1210, // 发送：空
    S2C_ActiveClansList  = 1211, // 回复：clan列表 (id, name, leaderId)
    C2S_GetClanMembers   = 1212, // 发送：clanId
    S2C_ClanMembersList  = 1213, // 回复：成员userId列表
    C2S_ApplyToClan      = 1214, // 发送：clanId, applicantId
    S2C_ApplyToClan      = 1215, // 回复：applicationId
    C2S_ProcessApplication = 1216, // 发送：applicationId, handlerId, action(0批准,1拒绝)
    S2C_ProcessApplication = 1217, // 回复：空
    C2S_GetMyApplications = 1218, // 发送：userId, 可选的statusFilter
    S2C_MyApplicationsList = 1219, // 回复：申请列表(id, clanId, status, 时间)

    // ========== 聊天 1300-1399 ==========
    C2S_SendGlobalMessage = 1300, // 发送：senderId, content
    S2C_SendGlobalMessage = 1301, // 回复：messageId
    C2S_SendClanMessage   = 1302, // 发送：senderId, clanId, content
    S2C_SendClanMessage   = 1303, // 回复：messageId
    C2S_GetGlobalMessages = 1304, // 发送：limit, offset
    S2C_GlobalMessagesList = 1305, // 回复：消息列表(id, senderId, content, created_at)
    C2S_GetClanMessages   = 1306, // 发送：clanId, userId, limit, offset
    S2C_ClanMessagesList  = 1307, // 回复：消息列表(同上)

    // ========== 游戏记录 1400-1499 ==========
    C2S_AddGameRecord    = 1400, // 发送：userId, mode, durationSec, threeBv
    S2C_AddGameRecord    = 1401, // 回复：recordId
    C2S_GetLeaderboard   = 1402, // 发送：mode, limit
    S2C_Leaderboard      = 1403, // 回复：排名列表(userId, username, durationSec, threeBv, played_at)
    C2S_GetUserRecords   = 1404, // 发送：userId, mode(-1全部)
    S2C_UserRecordsList  = 1405, // 回复：记录列表

    // ========== 区块管理 1500-1599 ==========
    C2S_GetChunk         = 1500, // 发送：x, y
    S2C_GetChunk         = 1501, // 回复：区块数据(原始hex字符串)或不存在
    C2S_UpdateChunk      = 1502, // 发送：x, y, userId, data (原始hex字符串)
    S2C_UpdateChunk      = 1503, // 回复：成功空
    C2S_DeleteChunk      = 1504, // 发送：x, y
    S2C_DeleteChunk      = 1505, // 回复：空
    C2S_GetChunksInArea  = 1506, // 发送：minX, minY, maxX, maxY
    S2C_GetChunksInArea  = 1507, // 回复：区块列表(id, x, y, data)

    // ========== 在线状态 1600-1699 ==========
    C2S_UserOnline       = 1600, // 发送：userId
    S2C_UserOnline       = 1601, // 回复：空
    C2S_UserOffline      = 1602, // 发送：userId
    S2C_UserOffline      = 1603, // 回复：空
    C2S_GetOnlineUsers   = 1604, // 发送：timeoutSec
    S2C_OnlineUsersList  = 1605, // 回复：在线userId列表
    C2S_IsUserOnline     = 1606, // 发送：userId, timeoutSec
    S2C_IsUserOnline     = 1607, // 回复：bool

    // ========== 通知 1700-1799 ==========
    C2S_PublishNotice    = 1700, // 发送：publisherId, title, content
    S2C_PublishNotice    = 1701, // 回复：noticeId
    C2S_GetActiveNotices = 1702, // 发送：limit, offset
    S2C_ActiveNoticesList = 1703, // 回复：通知列表
    C2S_GetNoticeById    = 1704, // 发送：noticeId
    S2C_GetNoticeById    = 1705, // 回复：title, content, publisherId, time
    C2S_DeleteNotice     = 1706, // 发送：noticeId, deleterId
    S2C_DeleteNotice     = 1707, // 回复：空

    // ========== 讨论 1800-1899 ==========
    C2S_CreateTopic      = 1800, // 发送：authorId, title, content, scope, targetId(可选)
    S2C_CreateTopic      = 1801, // 回复：topicId
    C2S_GetVisibleTopics = 1802, // 发送：viewerId, limit, offset
    S2C_VisibleTopicsList = 1803, // 回复：帖子列表
    C2S_GetTopicById     = 1804, // 发送：topicId, viewerId
    S2C_GetTopicById     = 1805, // 回复：topic详情
    C2S_UpdateTopic      = 1806, // 发送：topicId, editorId, newTitle, newContent
    S2C_UpdateTopic      = 1807, // 回复：空
    C2S_DeleteTopic      = 1808, // 发送：topicId, deleterId
    S2C_DeleteTopic      = 1809, // 回复：空
    C2S_AddComment       = 1810, // 发送：topicId, authorId, content
    S2C_AddComment       = 1811, // 回复：commentId
    C2S_GetComments      = 1812, // 发送：topicId, viewerId, limit, offset
    S2C_CommentsList     = 1813, // 回复：评论列表
    C2S_UpdateComment    = 1814, // 发送：commentId, editorId, newContent
    S2C_UpdateComment    = 1815, // 回复：空
    C2S_DeleteComment    = 1816, // 发送：commentId, deleterId
    S2C_DeleteComment    = 1817, // 回复：空

    // ========== 扫雷游戏 1900-1999 ==========
    C2S_GetCell          = 1900, // 发送：worldX, worldY
    S2C_GetCell          = 1901, // 回复：Cell状态(按二进制打包)
    C2S_RevealCell       = 1902, // 发送：worldX, worldY, userId
    S2C_RevealCell       = 1903, // 回复：hitMine(bool)
    C2S_ToggleFlag       = 1904, // 发送：worldX, worldY, userId
    S2C_ToggleFlag       = 1905, // 回复：空
    C2S_QuickFlag        = 1906, // 发送：worldX, worldY, userId
    S2C_QuickFlag        = 1907, // 回复：空
    C2S_PlaceMine        = 1908, // 发送：worldX, worldY, userId (管理用)
    S2C_PlaceMine        = 1909, // 回复：空
};

/**
 * 错误码定义
 * 0 表示成功，其他为具体错误
 */
enum class ErrorCode : uint16_t {
    Success = 0,

    // 通用错误 1000-1099
    UnknownError        = 1000,
    InvalidRequest      = 1001,
    Timeout             = 1002,

    // 用户错误 2000-2099
    UserNotFound        = 2000,
    UserAlreadyExists   = 2001,
    AuthenticationFailed = 2002,
    UserDeleted         = 2003,

    // 战队错误 2100-2199
    ClanNotFound        = 2100,
    ClanNameConflict    = 2101,
    ClanNotActive       = 2102,
    NotMemberOfClan     = 2103,
    NotAuthorized       = 2104,
    AlreadyApplied      = 2105,
    ApplicationNotFound = 2106,
    AlreadyMember       = 2107,
    CannotRemoveLeader  = 2108,
    LeaderNotInClan     = 2109,

    // 聊天错误 2200-2299
    InvalidMessageTarget = 2200,

    // 游戏记录错误 2300-2399
    InvalidGameMode     = 2300,
    GameRecordNotFound  = 2301,

    // 区块错误 2400-2499
    ChunkNotFound       = 2400,
    ChunkAlreadyExists  = 2401,

    // 在线错误 2500-2599
    UserOffline         = 2500,

    // 通知错误 2600-2699
    NoticeNotFound      = 2600,

    // 讨论错误 2700-2799
    TopicNotFound       = 2700,
    CommentNotFound     = 2701,
    AccessDenied        = 2702,
    InvalidScope        = 2703,

    // 扫雷错误 2800-2899
    CellAccessError     = 2800,
    CellAlreadyRevealed = 2801,
    CellNotRevealed     = 2802,
};

} // namespace miso
