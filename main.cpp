#include <iostream>
#include <memory>
#include "storage.h"
#include "exceptions.h"
#include "user_manager.h"
#include "clan_manager.h"
#include "chat_manager.h"

using namespace app;

void printUser(const User& u) {
    std::cout << "ID: " << u.id
              << ", 用户名: " << u.username
              << ", 状态: " << (u.deleted_at == 0 ? "正常" : "已删除")
              << std::endl;
}

void printClan(const Clan& c, UserManager& um) {
    std::cout << "ID: " << c.id
              << ", 名称: " << c.name
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

        int choice;
        while (true) {
            std::cout << "\n===== 主菜单 =====" << std::endl;
            std::cout << "1. 用户管理" << std::endl;
            std::cout << "2. 战队管理" << std::endl;
            std::cout << "0. 退出" << std::endl;
            std::cout << "选择: ";
            std::cin >> choice;

            if (choice == 0) break;
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
            } else if (choice == 2) {
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
                        default:
                            std::cout << "无效选项" << std::endl;
                        }
                    } catch (const AppException& e) {
                        std::cerr << "操作失败: " << e.what() << std::endl;
                    }
                }
            } else if (choice == 3) {
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
							std::cin.ignore(); // 忽略换行
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
							std::cin.ignore();
							std::cout << "消息内容: ";
							std::getline(std::cin, content);
							{
								int msgId = chat.sendClanMessage(senderId, clanId, content);
								std::cout << "战队消息发送成功，ID: " << msgId << std::endl;
							}
							break;
						case 3:
							{
								auto msgs = chat.getGlobalMessages(20, 0); // 最近20条
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
			} else {
                std::cout << "无效选项" << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "致命错误: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
