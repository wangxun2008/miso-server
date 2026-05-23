#include <iostream>
#include <memory>
#include <limits>
#include "storage.h"
#include "exceptions.h"
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

using namespace app;

// 辅助：清空输入缓冲区
void clearCin() {
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

// 打印用户信息
void printUser(const User& u) {
    std::cout << "ID: " << u.id << ", 用户名: " << u.username
              << ", 状态: " << (u.deleted_at == 0 ? "正常" : "已删除")
              << std::endl;
}

// 打印战队信息
void printClan(const Clan& c, UserManager& um) {
    std::cout << "ID: " << c.id << ", 名称: " << c.name
              << ", 领导者: " << um.getUserName(c.leader_id)
              << " (ID:" << c.leader_id << ")";
    if (c.deleted_at != 0) std::cout << " [已删除]";
    std::cout << std::endl;
}

int main() {
    try {
        auto storage = createStorage("users.db");
        storage.sync_schema();

        UserManager um(storage);
        ClanManager cm(storage, um);
        ChatManager chat(storage, um, cm);
        GameManager gm(storage, um);
        ChunkManager chunkMgr(storage, um);
        OnlineManager om(storage, um);
        NoticeManager nm(storage, um);
        TopicManager tm(storage, um, cm);
        CommentManager commgr(storage, um, tm);
        MineMapManager mm(chunkMgr, um, 3);

        int choice;
        while (true) {
            std::cout << "\n===== 主菜单 =====" << std::endl;
            std::cout << "1. 用户管理" << std::endl;
            std::cout << "2. 战队管理" << std::endl;
            std::cout << "3. 聊天功能" << std::endl;
            std::cout << "4. 扫雷游戏记录" << std::endl;
            std::cout << "5. 地图区块管理" << std::endl;
            std::cout << "6. 在线状态管理" << std::endl;
            std::cout << "7. 全局通知" << std::endl;
            std::cout << "8. 讨论板块" << std::endl;
            std::cout << "0. 退出" << std::endl;
            std::cout << "选择: ";
            std::cin >> choice;
            if (choice == 0) break;

            // ---------- 1. 用户管理 ----------
            if (choice == 1) {
                int userChoice;
                std::string username, newUsername, password, oldPassword, newPassword;
                while (true) {
                    std::cout << "\n--- 用户管理 ---" << std::endl;
                    std::cout << "1. 注册" << std::endl;
                    std::cout << "2. 登录" << std::endl;
                    std::cout << "3. 软删除" << std::endl;
                    std::cout << "4. 恢复" << std::endl;
                    std::cout << "5. 永久删除" << std::endl;
                    std::cout << "6. 改名" << std::endl;
                    std::cout << "7. 改密码" << std::endl;
                    std::cout << "8. 显示活跃用户" << std::endl;
                    std::cout << "9. 显示全部用户" << std::endl;
                    std::cout << "0. 返回主菜单" << std::endl;
                    std::cout << "选择: ";
                    std::cin >> userChoice;
                    if (userChoice == 0) break;

                    try {
                        switch (userChoice) {
                        case 1:
                            std::cout << "用户名: "; std::cin >> username;
                            std::cout << "密码: "; std::cin >> password;
                            {
                                int id = um.registerUser(username, password);
                                std::cout << "注册成功，ID: " << id << std::endl;
                            }
                            break;
                        case 2:
                            std::cout << "用户名: "; std::cin >> username;
                            std::cout << "密码: "; std::cin >> password;
                            {
                                User user = um.login(username, password);
                                std::cout << "登录成功，欢迎 " << user.username << std::endl;
                            }
                            break;
                        case 3:
                            std::cout << "要软删除的用户名: "; std::cin >> username;
                            um.deleteUser(username);
                            std::cout << "软删除成功" << std::endl;
                            break;
                        case 4:
                            std::cout << "要恢复的用户名: "; std::cin >> username;
                            um.restoreUser(username);
                            std::cout << "恢复成功" << std::endl;
                            break;
                        case 5:
                            std::cout << "要永久删除的用户名: "; std::cin >> username;
                            um.permanentDeleteUser(username);
                            std::cout << "永久删除成功" << std::endl;
                            break;
                        case 6:
                            std::cout << "当前用户名: "; std::cin >> username;
                            std::cout << "新用户名: "; std::cin >> newUsername;
                            um.renameUser(username, newUsername);
                            std::cout << "改名成功" << std::endl;
                            break;
                        case 7:
                            std::cout << "用户名: "; std::cin >> username;
                            std::cout << "原密码: "; std::cin >> oldPassword;
                            std::cout << "新密码: "; std::cin >> newPassword;
                            um.changePassword(username, oldPassword, newPassword);
                            std::cout << "密码修改成功" << std::endl;
                            break;
                        case 8: {
                            auto users = um.getActiveUsers();
                            if (users.empty()) {
                                std::cout << "暂无活跃用户" << std::endl;
                            } else {
                                for (const auto& u : users) printUser(u);
                            }
                            break;
                        }
                        case 9: {
                            auto users = um.getAllUsers();
                            if (users.empty()) {
                                std::cout << "数据库无用户" << std::endl;
                            } else {
                                for (const auto& u : users) printUser(u);
                            }
                            break;
                        }
                        default:
                            std::cout << "无效选项" << std::endl;
                        }
                    } catch (const AppException& e) {
                        std::cerr << "操作失败: " << e.what() << std::endl;
                    }
                }
            }
            // ---------- 2. 战队管理 ----------
            else if (choice == 2) {
                int clanChoice;
                int id1, id2;
                std::string name;
                while (true) {
                    std::cout << "\n--- 战队管理 ---" << std::endl;
                    std::cout << "1. 创建战队" << std::endl;
                    std::cout << "2. 解散战队" << std::endl;
                    std::cout << "3. 邀请成员" << std::endl;
                    std::cout << "4. 移除成员" << std::endl;
                    std::cout << "5. 转让领导权" << std::endl;
                    std::cout << "6. 活跃战队列表" << std::endl;
                    std::cout << "7. 查看战队成员" << std::endl;
                    std::cout << "8. 全部战队列表" << std::endl;
                    std::cout << "9. 申请加入战队" << std::endl;
                    std::cout << "10. 查看我的申请" << std::endl;
                    std::cout << "11. 审批申请（领导者）" << std::endl;
                    std::cout << "0. 返回主菜单" << std::endl;
                    std::cout << "选择: ";
                    std::cin >> clanChoice;
                    if (clanChoice == 0) break;

                    try {
                        switch (clanChoice) {
                        case 1:
                            std::cout << "领导者用户ID: "; std::cin >> id1;
                            std::cout << "战队名称: "; std::cin >> name;
                            {
                                int cid = cm.createClan(name, id1);
                                std::cout << "创建成功，战队ID: " << cid << std::endl;
                            }
                            break;
                        case 2:
                            std::cout << "要解散的战队ID: "; std::cin >> id1;
                            cm.dissolveClan(id1);
                            std::cout << "解散成功" << std::endl;
                            break;
                        case 3:
                            std::cout << "战队ID: "; std::cin >> id1;
                            std::cout << "邀请用户ID: "; std::cin >> id2;
                            cm.addMember(id1, id2);
                            std::cout << "邀请成功" << std::endl;
                            break;
                        case 4:
                            std::cout << "战队ID: "; std::cin >> id1;
                            std::cout << "移除用户ID: "; std::cin >> id2;
                            cm.removeMember(id1, id2);
                            std::cout << "移除成功" << std::endl;
                            break;
                        case 5:
                            std::cout << "战队ID: "; std::cin >> id1;
                            std::cout << "新领导者用户ID: "; std::cin >> id2;
                            cm.transferLeadership(id1, id2);
                            std::cout << "转让成功" << std::endl;
                            break;
                        case 6: {
                            auto clans = cm.getActiveClans();
                            if (clans.empty()) {
                                std::cout << "暂无活跃战队" << std::endl;
                            } else {
                                for (const auto& c : clans) printClan(c, um);
                            }
                            break;
                        }
                        case 7:
                            std::cout << "战队ID: "; std::cin >> id1;
                            try {
                                Clan clan = cm.getClan(id1);
                                std::cout << "战队 " << clan.name << " 成员:" << std::endl;
                                auto members = cm.getClanMembers(id1);
                                for (const auto& m : members) {
                                    std::string name = um.getUserName(m.user_id);
                                    std::cout << "  " << name << " (ID:" << m.user_id << ")";
                                    if (m.user_id == clan.leader_id) std::cout << " [领导者]";
                                    std::cout << std::endl;
                                }
                            } catch (const AppException& e) {
                                std::cerr << "查看失败: " << e.what() << std::endl;
                            }
                            break;
                        case 8: {
                            auto clans = cm.getAllClans();
                            if (clans.empty()) {
                                std::cout << "数据库无战队" << std::endl;
                            } else {
                                for (const auto& c : clans) printClan(c, um);
                            }
                            break;
                        }
                        case 9:
                            std::cout << "战队ID: "; std::cin >> id1;
                            std::cout << "你的用户ID: "; std::cin >> id2;
                            {
                                int appId = cm.applyToClan(id1, id2);
                                std::cout << "申请已提交，ID: " << appId << std::endl;
                            }
                            break;
                        case 10: {
                            std::cout << "你的用户ID: "; std::cin >> id1;
                            {
                                auto apps = cm.getMyApplications(id1);
                                if (apps.empty()) {
                                    std::cout << "暂无申请记录" << std::endl;
                                } else {
                                    for (const auto& a : apps) {
                                        std::string statusStr = (a.status == 0 ? "待审批" : (a.status == 1 ? "已批准" : "已拒绝"));
                                        std::cout << "ID:" << a.id << " 战队:" << a.clan_id
                                                  << " 状态:" << statusStr << " 时间:" << a.created_at << std::endl;
                                    }
                                }
                            }
                            break;
						}
                        case 11: {
                            int appId;
                            std::string action;
                            std::cout << "申请ID: "; std::cin >> appId;
                            std::cout << "处理人ID（领导者）: "; std::cin >> id1;
                            std::cout << "操作（approve/reject）: "; std::cin >> action;
                            cm.processApplication(appId, id1, action);
                            std::cout << "处理成功" << std::endl;
                            break;
						}
                        default:
                            std::cout << "无效选项" << std::endl;
                        }
                    } catch (const AppException& e) {
                        std::cerr << "操作失败: " << e.what() << std::endl;
                    }
                }
            }
            // ---------- 3. 聊天功能 ----------
            else if (choice == 3) {
                int chatChoice;
                int senderId, clanId, userId;
                std::string content;
                while (true) {
                    std::cout << "\n--- 聊天功能 ---" << std::endl;
                    std::cout << "1. 发送全局消息" << std::endl;
                    std::cout << "2. 发送战队消息" << std::endl;
                    std::cout << "3. 查看全局消息" << std::endl;
                    std::cout << "4. 查看战队消息" << std::endl;
                    std::cout << "0. 返回主菜单" << std::endl;
                    std::cout << "选择: ";
                    std::cin >> chatChoice;
                    if (chatChoice == 0) break;

                    try {
                        switch (chatChoice) {
                        case 1:
                            std::cout << "发送者用户ID: "; std::cin >> senderId;
                            clearCin();
                            std::cout << "消息内容: ";
                            std::getline(std::cin, content);
                            {
                                int msgId = chat.sendGlobalMessage(senderId, content);
                                std::cout << "全局消息发送成功，ID: " << msgId << std::endl;
                            }
                            break;
                        case 2:
                            std::cout << "发送者用户ID: "; std::cin >> senderId;
                            std::cout << "目标战队ID: "; std::cin >> clanId;
                            clearCin();
                            std::cout << "消息内容: ";
                            std::getline(std::cin, content);
                            {
                                int msgId = chat.sendClanMessage(senderId, clanId, content);
                                std::cout << "战队消息发送成功，ID: " << msgId << std::endl;
                            }
                            break;
                        case 3:
                            {
                                auto msgs = chat.getGlobalMessages(20, 0);
                                if (msgs.empty()) {
                                    std::cout << "暂无全局消息" << std::endl;
                                } else {
                                    for (const auto& msg : msgs) {
                                        std::cout << "[" << msg.created_at << "] "
                                                  << um.getUserName(msg.sender_id) << ": "
                                                  << msg.content << std::endl;
                                    }
                                }
                            }
                            break;
                        case 4:
                            std::cout << "你的用户ID: "; std::cin >> userId;
                            std::cout << "战队ID: "; std::cin >> clanId;
                            {
                                auto msgs = chat.getClanMessages(clanId, userId, 20, 0);
                                if (msgs.empty()) {
                                    std::cout << "暂无战队消息" << std::endl;
                                } else {
                                    for (const auto& msg : msgs) {
                                        std::cout << "[" << msg.created_at << "] "
                                                  << um.getUserName(msg.sender_id) << ": "
                                                  << msg.content << std::endl;
                                    }
                                }
                            }
                            break;
                        default:
                            std::cout << "无效选项" << std::endl;
                        }
                    } catch (const AppException& e) {
                        std::cerr << "操作失败: " << e.what() << std::endl;
                    }
                }
            }
            // ---------- 4. 扫雷游戏记录 ----------
            else if (choice == 4) {
                int gameChoice;
                int userId, mode, durationSec, threeBv, limit;
                while (true) {
                    std::cout << "\n--- 扫雷游戏记录 ---" << std::endl;
                    std::cout << "1. 提交记录" << std::endl;
                    std::cout << "2. 查看排行榜" << std::endl;
                    std::cout << "3. 查看个人记录" << std::endl;
                    std::cout << "0. 返回主菜单" << std::endl;
                    std::cout << "选择: ";
                    std::cin >> gameChoice;
                    if (gameChoice == 0) break;

                    try {
                        switch (gameChoice) {
                        case 1:
                            std::cout << "用户ID: "; std::cin >> userId;
                            std::cout << "模式 (1=初级 2=中级 3=高级): "; std::cin >> mode;
                            std::cout << "用时(秒): "; std::cin >> durationSec;
                            std::cout << "3BV: "; std::cin >> threeBv;
                            {
                                int id = gm.addGameRecord(userId, mode, durationSec, threeBv);
                                std::cout << "记录已保存，ID: " << id << std::endl;
                            }
                            break;
                        case 2:
                            std::cout << "模式 (1=初级 2=中级 3=高级): "; std::cin >> mode;
                            std::cout << "显示前几名? "; std::cin >> limit;
                            {
                                auto leaderboard = gm.getLeaderboard(mode, limit);
                                if (leaderboard.empty()) {
                                    std::cout << "暂无记录" << std::endl;
                                } else {
                                    std::cout << "排名\t用户\t用时(s)\t3BV\t时间" << std::endl;
                                    int rank = 1;
                                    for (const auto& rec : leaderboard) {
                                        std::string name = um.getUserName(rec.user_id);
                                        std::cout << rank << "\t" << name << "\t"
                                                  << rec.duration_seconds << "\t"
                                                  << rec.three_bv << "\t"
                                                  << rec.played_at << std::endl;
                                        ++rank;
                                    }
                                }
                            }
                            break;
                        case 3:
                            std::cout << "用户ID: "; std::cin >> userId;
                            std::cout << "模式 (1-3，或 -1 全部): "; std::cin >> mode;
                            {
                                auto records = gm.getUserRecords(userId, mode);
                                if (records.empty()) {
                                    std::cout << "暂无记录" << std::endl;
                                } else {
                                    std::cout << "ID\t模式\t用时(s)\t3BV\t时间" << std::endl;
                                    for (const auto& rec : records) {
                                        std::string modeStr = (rec.mode == 1 ? "初级" : rec.mode == 2 ? "中级" : "高级");
                                        std::cout << rec.id << "\t" << modeStr << "\t"
                                                  << rec.duration_seconds << "\t"
                                                  << rec.three_bv << "\t"
                                                  << rec.played_at << std::endl;
                                    }
                                }
                            }
                            break;
                        default:
                            std::cout << "无效选项" << std::endl;
                        }
                    } catch (const AppException& e) {
                        std::cerr << "操作失败: " << e.what() << std::endl;
                    }
                }
            }
            // ---------- 5. 地图区块管理 ----------
            else if (choice == 5) {
                int chunkChoice;
                int x, y, userId;
                std::string data;
                while (true) {
                    std::cout << "\n--- 区块管理 ---" << std::endl;
                    std::cout << "1. 创建/更新区块" << std::endl;
                    std::cout << "2. 读取区块" << std::endl;
                    std::cout << "3. 删除区块" << std::endl;
                    std::cout << "4. 范围查询" << std::endl;
                    std::cout << "0. 返回主菜单" << std::endl;
                    std::cout << "选择: ";
                    std::cin >> chunkChoice;
                    if (chunkChoice == 0) break;

                    try {
                        switch (chunkChoice) {
                        case 1:
                            std::cout << "X坐标: "; std::cin >> x;
                            std::cout << "Y坐标: "; std::cin >> y;
                            std::cout << "操作用户ID: "; std::cin >> userId;
                            clearCin();
                            std::cout << "区块数据: ";
                            std::getline(std::cin, data);
                            {
                                Chunk chunk = chunkMgr.createOrUpdateChunk(x, y, userId, data);
                                std::cout << "操作成功，区块ID: " << chunk.id << std::endl;
                            }
                            break;
                        case 2:
                            std::cout << "X坐标: "; std::cin >> x;
                            std::cout << "Y坐标: "; std::cin >> y;
                            {
                                Chunk chunk = chunkMgr.getChunk(x, y);
                                std::cout << "区块ID: " << chunk.id
                                          << ", 数据: " << chunk.data
                                          << ", 最后更新者: " << um.getUserName(chunk.last_updated_by.value())
                                          << ", 更新时间: " << chunk.updated_at << std::endl;
                            }
                            break;
                        case 3:
                            std::cout << "X坐标: "; std::cin >> x;
                            std::cout << "Y坐标: "; std::cin >> y;
                            chunkMgr.deleteChunk(x, y);
                            std::cout << "区块已删除" << std::endl;
                            break;
                        case 4:
                            int minX, minY, maxX, maxY;
                            std::cout << "最小X: "; std::cin >> minX;
                            std::cout << "最小Y: "; std::cin >> minY;
                            std::cout << "最大X: "; std::cin >> maxX;
                            std::cout << "最大Y: "; std::cin >> maxY;
                            {
                                auto chunks = chunkMgr.getChunksInArea(minX, minY, maxX, maxY);
                                if (chunks.empty()) {
                                    std::cout << "范围内无区块" << std::endl;
                                } else {
                                    for (const auto& c : chunks) {
                                        std::cout << "(" << c.x << "," << c.y << ") ID:" << c.id
                                                  << " 数据:" << c.data << std::endl;
                                    }
                                }
                            }
                            break;
                        default:
                            std::cout << "无效选项" << std::endl;
                        }
                    } catch (const AppException& e) {
                        std::cerr << "操作失败: " << e.what() << std::endl;
                    }
                }
            }
            // ---------- 6. 在线状态管理 ----------
            else if (choice == 6) {
                int onlineChoice;
                int userId, timeout;
                while (true) {
                    std::cout << "\n--- 在线状态管理 ---" << std::endl;
                    std::cout << "1. 标记上线" << std::endl;
                    std::cout << "2. 标记下线" << std::endl;
                    std::cout << "3. 更新心跳" << std::endl;
                    std::cout << "4. 检查是否在线" << std::endl;
                    std::cout << "5. 查看在线用户列表" << std::endl;
                    std::cout << "0. 返回主菜单" << std::endl;
                    std::cout << "选择: ";
                    std::cin >> onlineChoice;
                    if (onlineChoice == 0) break;

                    try {
                        switch (onlineChoice) {
                        case 1:
                            std::cout << "用户ID: "; std::cin >> userId;
                            om.userOnline(userId);
                            std::cout << "用户 " << userId << " 已上线" << std::endl;
                            break;
                        case 2:
                            std::cout << "用户ID: "; std::cin >> userId;
                            om.userOffline(userId);
                            std::cout << "用户 " << userId << " 已下线" << std::endl;
                            break;
                        case 3:
                            std::cout << "用户ID: "; std::cin >> userId;
                            om.updateHeartbeat(userId);
                            std::cout << "心跳已更新" << std::endl;
                            break;
                        case 4:
                            std::cout << "用户ID: "; std::cin >> userId;
                            std::cout << "超时秒数(默认60): "; std::cin >> timeout;
                            if (om.isUserOnline(userId, timeout)) {
                                std::cout << "用户 " << userId << " 在线" << std::endl;
                            } else {
                                std::cout << "用户 " << userId << " 离线或心跳超时" << std::endl;
                            }
                            break;
                        case 5:
                            std::cout << "超时秒数(默认60): "; std::cin >> timeout;
                            {
                                auto onlineIds = om.getOnlineUsers(timeout);
                                if (onlineIds.empty()) {
                                    std::cout << "当前无在线用户" << std::endl;
                                } else {
                                    std::cout << "在线用户ID: ";
                                    for (int id : onlineIds) {
                                        std::cout << id << " ";
                                    }
                                    std::cout << std::endl;
                                }
                            }
                            break;
                        default:
                            std::cout << "无效选项" << std::endl;
                        }
                    } catch (const AppException& e) {
                        std::cerr << "操作失败: " << e.what() << std::endl;
                    }
                }
            }
            // ---------- 7. 全局通知 ----------
            else if (choice == 7) {
                int nChoice;
                int userId, noticeId, limit, offset;
                std::string title, content;
                while (true) {
                    std::cout << "\n--- 全局通知 ---" << std::endl;
                    std::cout << "1. 发布通知" << std::endl;
                    std::cout << "2. 查看通知列表" << std::endl;
                    std::cout << "3. 查看通知详情" << std::endl;
                    std::cout << "4. 删除通知" << std::endl;
                    std::cout << "0. 返回主菜单" << std::endl;
                    std::cout << "选择: ";
                    std::cin >> nChoice;
                    if (nChoice == 0) break;

                    try {
                        switch (nChoice) {
                        case 1:
                            std::cout << "发布者用户ID: "; std::cin >> userId;
                            clearCin();
                            std::cout << "标题: ";
                            std::getline(std::cin, title);
                            std::cout << "内容: ";
                            std::getline(std::cin, content);
                            {
                                int nid = nm.publishNotice(userId, title, content);
                                std::cout << "通知已发布，ID: " << nid << std::endl;
                            }
                            break;
                        case 2:
                            std::cout << "每页条数: "; std::cin >> limit;
                            std::cout << "偏移量: "; std::cin >> offset;
                            {
                                auto notices = nm.getActiveNotices(limit, offset);
                                if (notices.empty()) {
                                    std::cout << "暂无通知" << std::endl;
                                } else {
                                    for (const auto& n : notices) {
                                        std::cout << "ID:" << n.id << " 标题:" << n.title
                                                  << " 发布者:" << um.getUserName(n.publisher_id)
                                                  << " 时间:" << n.published_at << std::endl;
                                    }
                                }
                            }
                            break;
                        case 3:
                            std::cout << "通知ID: "; std::cin >> noticeId;
                            {
                                Notice n = nm.getNoticeById(noticeId);
                                std::cout << "标题: " << n.title << std::endl;
                                std::cout << "内容: " << n.content << std::endl;
                                std::cout << "发布者: " << um.getUserName(n.publisher_id) << std::endl;
                                std::cout << "时间: " << n.published_at << std::endl;
                            }
                            break;
                        case 4:
                            std::cout << "通知ID: "; std::cin >> noticeId;
                            std::cout << "操作用户ID: "; std::cin >> userId;
                            nm.deleteNotice(noticeId, userId);
                            std::cout << "删除成功" << std::endl;
                            break;
                        default:
                            std::cout << "无效选项" << std::endl;
                        }
                    } catch (const AppException& e) {
                        std::cerr << "操作失败: " << e.what() << std::endl;
                    }
                }
            }
            // ---------- 8. 讨论板块 ----------
            else if (choice == 8) {
                int dChoice;
                int userId, topicId, commentId, scope, limit, offset, targetId;
                std::string title, content, newTitle, newContent;
                while (true) {
                    std::cout << "\n--- 讨论板块 ---" << std::endl;
                    std::cout << "1. 发布帖子" << std::endl;
                    std::cout << "2. 查看可见帖子列表" << std::endl;
                    std::cout << "3. 查看帖子详情" << std::endl;
                    std::cout << "4. 编辑帖子" << std::endl;
                    std::cout << "5. 删除帖子" << std::endl;
                    std::cout << "6. 添加评论" << std::endl;
                    std::cout << "7. 查看评论" << std::endl;
                    std::cout << "8. 编辑评论" << std::endl;
                    std::cout << "9. 删除评论" << std::endl;
                    std::cout << "0. 返回主菜单" << std::endl;
                    std::cout << "选择: ";
                    std::cin >> dChoice;
                    if (dChoice == 0) break;

                    try {
                        switch (dChoice) {
                        case 1:
                            std::cout << "发帖人用户ID: "; std::cin >> userId;
                            clearCin();
                            std::cout << "标题: ";
                            std::getline(std::cin, title);
                            std::cout << "内容: ";
                            std::getline(std::cin, content);
                            std::cout << "可见范围 (0=全局 1=战队): "; std::cin >> scope;
                            if (scope == 1) {
                                std::cout << "目标战队ID: "; std::cin >> targetId;
                                {
                                    int tid = tm.createTopic(userId, title, content, scope, targetId);
                                    std::cout << "帖子已发布，ID: " << tid << std::endl;
                                }
                            } else {
                                int tid = tm.createTopic(userId, title, content, scope);
                                std::cout << "帖子已发布，ID: " << tid << std::endl;
                            }
                            break;
                        case 2:
                            std::cout << "查看者用户ID: "; std::cin >> userId;
                            std::cout << "每页条数: "; std::cin >> limit;
                            std::cout << "偏移量: "; std::cin >> offset;
                            {
                                auto topics = tm.getVisibleTopics(userId, limit, offset);
                                if (topics.empty()) {
                                    std::cout << "无可见帖子" << std::endl;
                                } else {
                                    for (const auto& t : topics) {
                                        std::string scopeStr = (t.scope == 0 ? "全局" : ("战队:" + std::to_string(t.target_id.value_or(0))));
                                        std::cout << "ID:" << t.id << " 标题:" << t.title
                                                  << " 作者:" << um.getUserName(t.author_id)
                                                  << " 范围:" << scopeStr
                                                  << " 时间:" << t.created_at << std::endl;
                                    }
                                }
                            }
                            break;
                        case 3:
                            std::cout << "帖子ID: "; std::cin >> topicId;
                            std::cout << "查看者用户ID: "; std::cin >> userId;
                            {
                                Topic t = tm.getTopicById(topicId, userId);
                                std::cout << "标题: " << t.title << std::endl;
                                std::cout << "内容: " << t.content << std::endl;
                                std::cout << "作者: " << um.getUserName(t.author_id) << std::endl;
                                std::cout << "范围: " << (t.scope == 0 ? "全局" : ("战队:" + std::to_string(t.target_id.value_or(0)))) << std::endl;
                            }
                            break;
                        case 4:
                            std::cout << "帖子ID: "; std::cin >> topicId;
                            std::cout << "编辑者用户ID: "; std::cin >> userId;
                            clearCin();
                            std::cout << "新标题: ";
                            std::getline(std::cin, newTitle);
                            std::cout << "新内容: ";
                            std::getline(std::cin, newContent);
                            tm.updateTopic(topicId, userId, newTitle, newContent);
                            std::cout << "帖子已更新" << std::endl;
                            break;
                        case 5:
                            std::cout << "帖子ID: "; std::cin >> topicId;
                            std::cout << "操作用户ID: "; std::cin >> userId;
                            tm.deleteTopic(topicId, userId);
                            std::cout << "帖子已删除" << std::endl;
                            break;
                        case 6:
                            std::cout << "帖子ID: "; std::cin >> topicId;
                            std::cout << "评论者用户ID: "; std::cin >> userId;
                            clearCin();
                            std::cout << "评论内容: ";
                            std::getline(std::cin, content);
                            {
                                int cid = commgr.addComment(topicId, userId, content);
                                std::cout << "评论已添加，ID: " << cid << std::endl;
                            }
                            break;
                        case 7:
                            std::cout << "帖子ID: "; std::cin >> topicId;
                            std::cout << "查看者用户ID: "; std::cin >> userId;
                            {
                                auto comments = commgr.getComments(topicId, userId);
                                if (comments.empty()) {
                                    std::cout << "暂无评论" << std::endl;
                                } else {
                                    for (const auto& c : comments) {
                                        std::cout << "ID:" << c.id << " 用户:" << um.getUserName(c.author_id)
                                                  << " 内容:" << c.content << " 时间:" << c.created_at << std::endl;
                                    }
                                }
                            }
                            break;
                        case 8:
                            std::cout << "评论ID: "; std::cin >> commentId;
                            std::cout << "编辑者用户ID: "; std::cin >> userId;
                            clearCin();
                            std::cout << "新内容: ";
                            std::getline(std::cin, newContent);
                            commgr.updateComment(commentId, userId, newContent);
                            std::cout << "评论已更新" << std::endl;
                            break;
                        case 9:
                            std::cout << "评论ID: "; std::cin >> commentId;
                            std::cout << "操作用户ID: "; std::cin >> userId;
                            commgr.deleteComment(commentId, userId);
                            std::cout << "评论已删除" << std::endl;
                            break;
                        default:
                            std::cout << "无效选项" << std::endl;
                        }
                    } catch (const AppException& e) {
                        std::cerr << "操作失败: " << e.what() << std::endl;
                    }
                }
            }
			else if (choice == 9) {
				int mChoice;
				int wx, wy, uid, terrain, owner;
				while (true) {
					std::cout << "\n--- 扫雷地图 ---" << std::endl;
					std::cout << "1. 查看单元格" << std::endl;
					std::cout << "2. 修改领地" << std::endl;
					std::cout << "3. 修改地形" << std::endl;
					std::cout << "4. 放置地雷" << std::endl;
					std::cout << "5. 查看缓存数量" << std::endl;
					std::cout << "0. 返回" << std::endl;
					std::cout << "选择: ";
					std::cin >> mChoice;
					if (mChoice == 0) break;
					try {
						switch (mChoice) {
						case 1:
							std::cout << "世界坐标 x y: "; std::cin >> wx >> wy;
							{
								Cell cell = mm.getCell(wx, wy);
								std::cout << "地雷:" << cell.has_mine << " 翻开:" << cell.is_revealed
										  << " 标记:" << cell.is_flagged << " 相邻雷数:" << cell.adjacent_mines
										  << " 领主ID:" << cell.owner_id << " 地形:" << cell.terrain << std::endl;
							}
							break;
						case 2:
							std::cout << "世界坐标 x y 用户ID 新领主ID(-1清空): "; std::cin >> wx >> wy >> uid >> owner;
							mm.modifyCell(wx, wy, uid, [owner](Cell& cell) {
								cell.owner_id = owner;
							});
							std::cout << "领地已更新" << std::endl;
							break;
						case 3:
							std::cout << "世界坐标 x y 用户ID 地形值: "; std::cin >> wx >> wy >> uid >> terrain;
							mm.modifyCell(wx, wy, uid, [terrain](Cell& cell) {
								cell.terrain = terrain;
							});
							std::cout << "地形已更新" << std::endl;
							break;
						case 4:
							std::cout << "世界坐标 x y 用户ID: "; std::cin >> wx >> wy >> uid;
							mm.modifyCell(wx, wy, uid, [](Cell& cell) {
								cell.has_mine = true;
							});
							std::cout << "地雷已放置" << std::endl;
							break;
						case 5:
							std::cout << "缓存区块数: " << mm.cacheSize() << std::endl;
							break;
						default:
							std::cout << "无效选项" << std::endl;
						}
					} catch (const AppException& e) {
						std::cerr << "操作失败: " << e.what() << std::endl;
					}
				}
			}
            else {
                std::cout << "无效选项" << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "致命错误: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
