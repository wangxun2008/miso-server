#include <iostream>
#include <string>
#include <memory>
#include <chrono>
#include <optional>
#include <vector>
#include <sqlite_orm/sqlite_orm.h>

using namespace sqlite_orm;

// ====================== 时间戳辅助 ======================
int64_t getCurrentTimestamp() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

// ====================== 用户表（已存在，稍作调整） ======================
struct User {
    int id = 0;
    std::string username;
    std::string password;
    int64_t deleted_at = 0;      // 0:正常; >0:删除时间戳
};

// ====================== 部落表 ======================
struct Clan {
    int id = 0;
    std::string name;
    int leader_id = 0;            // 关联 User.id
    int64_t deleted_at = 0;       // 0:正常; >0:解散时间戳
};

// ====================== 部落成员表 ======================
struct ClanMember {
    int clan_id = 0;
    int user_id = 0;
    int64_t joined_at = 0;        // 加入时间戳
};

// 联合唯一索引：确保同一个用户不能重复加入同一个部落（但结合应用层确保一个用户只能加入一个活跃部落）
// 另外，为了避免一个用户同时存在于多个活跃部落，我们将约束提升到应用层。

// ====================== 数据库存储结构 ======================
auto get_storage() {
    return make_storage("users.db",
        make_table("users",
            make_column("id", &User::id, primary_key().autoincrement()),
            make_column("username", &User::username),
            make_column("password", &User::password),
            make_column("deleted_at", &User::deleted_at, default_value(0)),
            unique(&User::username, &User::deleted_at)
        ),
        make_table("clans",
            make_column("id", &Clan::id, primary_key().autoincrement()),
            make_column("name", &Clan::name),
            make_column("leader_id", &Clan::leader_id),
            make_column("deleted_at", &Clan::deleted_at, default_value(0)),
            unique(&Clan::name, &Clan::deleted_at)
        ),
        make_table("clan_members",
            // 添加 default_value(0) 避免 NOT NULL 约束失败
            make_column("clan_id", &ClanMember::clan_id, default_value(0)),
            make_column("user_id", &ClanMember::user_id, default_value(0)),
            make_column("joined_at", &ClanMember::joined_at, default_value(0)),
            primary_key(&ClanMember::clan_id, &ClanMember::user_id)
        )
    );
}

// ====================== 用户管理器（原功能保持不变，略作调整以兼容部落） ======================
class UserManager {
private:
    decltype(get_storage()) storage;
public:
    UserManager() : storage(get_storage()) {
        storage.sync_schema();
    }

    // 以下方法与原实现相同，为简洁仅保留声明（实际代码可复制之前的实现）
    bool registerUser(const std::string& username, const std::string& password);
    bool login(const std::string& username, const std::string& password);
    bool deleteUser(const std::string& username);            // 软删除用户
    bool restoreUser(const std::string& username);
    bool permanentDelete(const std::string& username);
    bool renameUser(const std::string& oldUsername, const std::string& newUsername);
    bool changePassword(const std::string& username, const std::string& oldPassword, const std::string& newPassword);
    void showActiveUsers();
    void showAllUsers();

    // 为了部落功能，需要额外提供辅助方法
    std::optional<User> findUserByUsername(const std::string& username) {
        auto users = storage.get_all<User>(
            where(c(&User::username) == username && c(&User::deleted_at) == 0)
        );
        if (users.empty()) return std::nullopt;
        return users[0];
    }

    std::optional<User> findUserById(int id) {
        auto users = storage.get_all<User>(
            where(c(&User::id) == id && c(&User::deleted_at) == 0)
        );
        if (users.empty()) return std::nullopt;
        return users[0];
    }
};

// ====================== 部落管理器 ======================
class ClanManager {
private:
    decltype(get_storage()) storage;
    UserManager& userManager;   // 依赖用户管理器验证用户存在

public:
    ClanManager(UserManager& um) : userManager(um), storage(get_storage()) {
        storage.sync_schema();
    }

    // 创建部落 (名称, 领导者用户名)
    bool createClan(const std::string& clanName, const std::string& leaderUsername) {
        // 查找领导者
        auto leaderOpt = userManager.findUserByUsername(leaderUsername);
        if (!leaderOpt) {
            std::cout << "创建部落失败：领导者 " << leaderUsername << " 不存在" << std::endl;
            return false;
        }
        User leader = *leaderOpt;

        // 检查领导者当前是否已经在一个未解散的部落中
        if (getUserActiveClan(leader.id).has_value()) {
            std::cout << "创建部落失败：用户 " << leaderUsername << " 已经属于一个部落" << std::endl;
            return false;
        }

        // 检查部落名称是否已被未解散的部落使用
        auto existingClans = storage.get_all<Clan>(
            where(c(&Clan::name) == clanName && c(&Clan::deleted_at) == 0)
        );
        if (!existingClans.empty()) {
            std::cout << "创建部落失败：部落名称 " << clanName << " 已存在" << std::endl;
            return false;
        }

        // 创建部落
        Clan newClan;
        newClan.name = clanName;
        newClan.leader_id = leader.id;
        newClan.deleted_at = 0;
        int clanId = storage.insert(newClan);

        // 领导者自动加入成员表
        ClanMember member;
        member.clan_id = clanId;
        member.user_id = leader.id;
        member.joined_at = getCurrentTimestamp();
        storage.insert(member);

        std::cout << "部落创建成功！部落ID: " << clanId << ", 名称: " << clanName << ", 领导者: " << leaderUsername << std::endl;
        return true;
    }

    // 解散部落（软删除）
    bool disbandClan(const std::string& clanName) {
        auto clans = storage.get_all<Clan>(
            where(c(&Clan::name) == clanName && c(&Clan::deleted_at) == 0)
        );
        if (clans.empty()) {
            std::cout << "解散失败：未找到活跃部落 " << clanName << std::endl;
            return false;
        }

        Clan clan = clans[0];
        clan.deleted_at = getCurrentTimestamp();
        storage.update(clan);
        std::cout << "部落 " << clanName << " 已解散（软删除）" << std::endl;
        return true;
    }

    // 恢复部落（将 deleted_at 置0）
    bool restoreClan(const std::string& clanName) {
        auto clans = storage.get_all<Clan>(
            where(c(&Clan::name) == clanName && c(&Clan::deleted_at) != 0)
        );
        if (clans.empty()) {
            std::cout << "恢复失败：未找到已解散的部落 " << clanName << std::endl;
            return false;
        }

        // 检查是否存在同名的未解散部落
        auto activeClans = storage.get_all<Clan>(
            where(c(&Clan::name) == clanName && c(&Clan::deleted_at) == 0)
        );
        if (!activeClans.empty()) {
            std::cout << "恢复失败：已存在活跃部落同名 " << clanName << std::endl;
            return false;
        }

        Clan clan = clans[0];
        clan.deleted_at = 0;
        storage.update(clan);
        std::cout << "部落 " << clanName << " 已恢复" << std::endl;
        return true;
    }

    // 修改部落名称（仅活跃部落）
    bool renameClan(const std::string& oldName, const std::string& newName) {
        auto clans = storage.get_all<Clan>(
            where(c(&Clan::name) == oldName && c(&Clan::deleted_at) == 0)
        );
        if (clans.empty()) {
            std::cout << "改名失败：未找到活跃部落 " << oldName << std::endl;
            return false;
        }

        // 检查新名称是否已被其他活跃部落使用
        auto nameClash = storage.get_all<Clan>(
            where(c(&Clan::name) == newName && c(&Clan::deleted_at) == 0)
        );
        if (!nameClash.empty()) {
            std::cout << "改名失败：部落名称 " << newName << " 已被使用" << std::endl;
            return false;
        }

        Clan clan = clans[0];
        clan.name = newName;
        storage.update(clan);
        std::cout << "部落名称修改成功：" << oldName << " → " << newName << std::endl;
        return true;
    }

    // 加入部落（用户加入指定部落，前提：用户未加入任何活跃部落，且部落存在且未解散）
    bool joinClan(const std::string& username, const std::string& clanName) {
        auto userOpt = userManager.findUserByUsername(username);
        if (!userOpt) {
            std::cout << "加入失败：用户 " << username << " 不存在" << std::endl;
            return false;
        }
        int userId = userOpt->id;

        // 检查用户是否已经在一个活跃部落中
        if (getUserActiveClan(userId).has_value()) {
            std::cout << "加入失败：用户 " << username << " 已经属于一个部落" << std::endl;
            return false;
        }

        // 查找目标部落
        auto clans = storage.get_all<Clan>(
            where(c(&Clan::name) == clanName && c(&Clan::deleted_at) == 0)
        );
        if (clans.empty()) {
            std::cout << "加入失败：未找到活跃部落 " << clanName << std::endl;
            return false;
        }
        Clan clan = clans[0];

        // 检查是否已经是成员（理论上不会，但防御）
        auto existing = storage.get_all<ClanMember>(
            where(c(&ClanMember::clan_id) == clan.id && c(&ClanMember::user_id) == userId)
        );
        if (!existing.empty()) {
            std::cout << "加入失败：用户已经是该部落成员" << std::endl;
            return false;
        }

        // 添加成员
        ClanMember member;
        member.clan_id = clan.id;
        member.user_id = userId;
        member.joined_at = getCurrentTimestamp();
        storage.insert(member);
        std::cout << username << " 加入了部落 " << clanName << std::endl;
        return true;
    }

    // 退出部落（不能是领导者，领导者必须先转让）
    bool leaveClan(const std::string& username) {
        auto userOpt = userManager.findUserByUsername(username);
        if (!userOpt) {
            std::cout << "退出失败：用户不存在" << std::endl;
            return false;
        }
        int userId = userOpt->id;

        // 获取用户所在的活跃部落
        auto clanOpt = getUserActiveClan(userId);
        if (!clanOpt) {
            std::cout << "退出失败：用户未加入任何部落" << std::endl;
            return false;
        }
        Clan clan = *clanOpt;

        // 检查是否是领导者
        if (clan.leader_id == userId) {
            std::cout << "退出失败：领导者不能退出部落，请先转让领导权" << std::endl;
            return false;
        }

        // 删除成员关系
        storage.remove_all<ClanMember>(
            where(c(&ClanMember::clan_id) == clan.id && c(&ClanMember::user_id) == userId)
        );
        std::cout << username << " 退出了部落 " << clan.name << std::endl;
        return true;
    }

    // 转让领导者（新领导者必须是当前成员）
    bool transferLeadership(const std::string& clanName, const std::string& currentLeaderUsername, const std::string& newLeaderUsername) {
        // 验证当前领导者
        auto currLeaderOpt = userManager.findUserByUsername(currentLeaderUsername);
        if (!currLeaderOpt) {
            std::cout << "转让失败：当前领导者不存在" << std::endl;
            return false;
        }
        int currLeaderId = currLeaderOpt->id;

        // 查找部落
        auto clans = storage.get_all<Clan>(
            where(c(&Clan::name) == clanName && c(&Clan::deleted_at) == 0)
        );
        if (clans.empty()) {
            std::cout << "转让失败：未找到部落 " << clanName << std::endl;
            return false;
        }
        Clan clan = clans[0];

        if (clan.leader_id != currLeaderId) {
            std::cout << "转让失败：当前用户不是部落领导者" << std::endl;
            return false;
        }

        // 验证新领导者
        auto newLeaderOpt = userManager.findUserByUsername(newLeaderUsername);
        if (!newLeaderOpt) {
            std::cout << "转让失败：新领导者不存在" << std::endl;
            return false;
        }
        int newLeaderId = newLeaderOpt->id;

        // 检查新领导者是否是该部落成员
        auto members = storage.get_all<ClanMember>(
            where(c(&ClanMember::clan_id) == clan.id && c(&ClanMember::user_id) == newLeaderId)
        );
        if (members.empty()) {
            std::cout << "转让失败：新领导者不是部落成员" << std::endl;
            return false;
        }

        // 更新部落的领导者
        clan.leader_id = newLeaderId;
        storage.update(clan);
        std::cout << "部落 " << clanName << " 的领导者已从 " << currentLeaderUsername << " 转让给 " << newLeaderUsername << std::endl;
        return true;
    }

    // 踢出成员（领导者踢人，不能踢自己）
    bool kickMember(const std::string& clanName, const std::string& leaderUsername, const std::string& targetUsername) {
        auto leaderOpt = userManager.findUserByUsername(leaderUsername);
        if (!leaderOpt) {
            std::cout << "踢人失败：领导者不存在" << std::endl;
            return false;
        }
        int leaderId = leaderOpt->id;

        auto clans = storage.get_all<Clan>(
            where(c(&Clan::name) == clanName && c(&Clan::deleted_at) == 0)
        );
        if (clans.empty()) {
            std::cout << "踢人失败：未找到部落 " << clanName << std::endl;
            return false;
        }
        Clan clan = clans[0];

        if (clan.leader_id != leaderId) {
            std::cout << "踢人失败：操作者不是部落领导者" << std::endl;
            return false;
        }

        auto targetOpt = userManager.findUserByUsername(targetUsername);
        if (!targetOpt) {
            std::cout << "踢人失败：目标用户不存在" << std::endl;
            return false;
        }
        int targetId = targetOpt->id;

        if (targetId == leaderId) {
            std::cout << "踢人失败：不能踢出自己" << std::endl;
            return false;
        }

        // 检查目标是否在部落中
        auto members = storage.get_all<ClanMember>(
            where(c(&ClanMember::clan_id) == clan.id && c(&ClanMember::user_id) == targetId)
        );
        if (members.empty()) {
            std::cout << "踢人失败：目标用户不在该部落中" << std::endl;
            return false;
        }

        storage.remove_all<ClanMember>(
            where(c(&ClanMember::clan_id) == clan.id && c(&ClanMember::user_id) == targetId)
        );
        std::cout << targetUsername << " 已被踢出部落 " << clanName << std::endl;
        return true;
    }

    // 列出部落成员（需指定部落名称或ID）
    void listClanMembers(const std::string& clanName) {
        auto clans = storage.get_all<Clan>(
            where(c(&Clan::name) == clanName && c(&Clan::deleted_at) == 0)
        );
        if (clans.empty()) {
            std::cout << "未找到活跃部落 " << clanName << std::endl;
            return;
        }
        Clan clan = clans[0];

        // 查询成员：ClanMember JOIN User
        auto members = storage.select(
            columns(&User::id, &User::username),
            inner_join<ClanMember>(on(c(&ClanMember::user_id) == &User::id)),
            where(c(&ClanMember::clan_id) == clan.id && c(&User::deleted_at) == 0)
        );

        std::cout << "\n===== 部落 [" << clan.name << "] 成员列表 =====" << std::endl;
        std::cout << "领导者ID: " << clan.leader_id << std::endl;
        for (auto& row : members) {
            int uid = std::get<0>(row);
            std::string uname = std::get<1>(row);
            std::cout << "成员ID: " << uid << ", 用户名: " << uname;
            if (uid == clan.leader_id) std::cout << " (领导者)";
            std::cout << std::endl;
        }
        std::cout << "===================================\n" << std::endl;
    }

    // 查询用户所在的部落信息
    void showUserClan(const std::string& username) {
        auto userOpt = userManager.findUserByUsername(username);
        if (!userOpt) {
            std::cout << "用户不存在" << std::endl;
            return;
        }
        int userId = userOpt->id;

        auto clanOpt = getUserActiveClan(userId);
        if (!clanOpt) {
            std::cout << username << " 当前未加入任何部落" << std::endl;
            return;
        }
        Clan clan = *clanOpt;
        std::cout << username << " 所在的部落: " << clan.name << " (ID: " << clan.id << ")" << std::endl;
    }

    // 列出所有未解散部落（简要信息）
    void listActiveClans() {
        auto clans = storage.get_all<Clan>(where(c(&Clan::deleted_at) == 0));
        if (clans.empty()) {
            std::cout << "当前没有活跃的部落" << std::endl;
            return;
        }
        std::cout << "\n===== 活跃部落列表 =====" << std::endl;
        for (const auto& c : clans) {
            std::cout << "ID: " << c.id << ", 名称: " << c.name << ", 领导者ID: " << c.leader_id << std::endl;
        }
        std::cout << "=======================\n" << std::endl;
    }

private:
    // 获取用户所在活跃部落（如果存在）
    std::optional<Clan> getUserActiveClan(int userId) {
        // 查询成员表中该用户的所有 clan_id
        auto memberRows = storage.get_all<ClanMember>(where(c(&ClanMember::user_id) == userId));
        if (memberRows.empty()) return std::nullopt;

        // 遍历找到第一个未解散的部落
        for (const auto& member : memberRows) {
            auto clans = storage.get_all<Clan>(
                where(c(&Clan::id) == member.clan_id && c(&Clan::deleted_at) == 0)
            );
            if (!clans.empty()) {
                return clans[0];
            }
        }
        return std::nullopt;
    }
};

// UserManager 的方法实现（需补全）
bool UserManager::registerUser(const std::string& username, const std::string& password) {
    auto users = storage.get_all<User>(where(c(&User::username) == username and c(&User::deleted_at) == 0));
    if (!users.empty()) { std::cout << "注册失败：用户名已存在" << std::endl; return false; }
    User newUser; newUser.username = username; newUser.password = password; newUser.deleted_at = 0;
    storage.insert(newUser);
    std::cout << "注册成功" << std::endl; return true;
}
bool UserManager::login(const std::string& username, const std::string& password) {
    auto users = storage.get_all<User>(where(c(&User::username) == username && c(&User::password) == password && c(&User::deleted_at) == 0));
    if (users.empty()) { std::cout << "登录失败" << std::endl; return false; }
    std::cout << "登录成功" << std::endl; return true;
}
bool UserManager::deleteUser(const std::string& username) {
    auto users = storage.get_all<User>(where(c(&User::username) == username && c(&User::deleted_at) == 0));
    if (users.empty()) { std::cout << "用户不存在" << std::endl; return false; }
    User u = users[0]; u.deleted_at = getCurrentTimestamp(); storage.update(u);
    std::cout << "软删除成功" << std::endl; return true;
}
bool UserManager::restoreUser(const std::string& username) {
    auto users = storage.get_all<User>(where(c(&User::username) == username && c(&User::deleted_at) != 0));
    if (users.empty()) { std::cout << "未找到已删除用户" << std::endl; return false; }
    User u = users[0]; u.deleted_at = 0; storage.update(u);
    std::cout << "恢复成功" << std::endl; return true;
}
bool UserManager::permanentDelete(const std::string& username) {
    auto users = storage.get_all<User>(where(c(&User::username) == username));
    if (users.empty()) { std::cout << "用户不存在" << std::endl; return false; }
    storage.remove<User>(users[0].id);
    std::cout << "彻底删除成功" << std::endl; return true;
}
bool UserManager::renameUser(const std::string& oldUsername, const std::string& newUsername) {
    auto oldUsers = storage.get_all<User>(where(c(&User::username) == oldUsername && c(&User::deleted_at) == 0));
    if (oldUsers.empty()) { std::cout << "旧用户不存在" << std::endl; return false; }
    auto newUsers = storage.get_all<User>(where(c(&User::username) == newUsername && c(&User::deleted_at) == 0));
    if (!newUsers.empty()) { std::cout << "新用户名已存在" << std::endl; return false; }
    User u = oldUsers[0]; u.username = newUsername; storage.update(u);
    std::cout << "改名成功" << std::endl; return true;
}
bool UserManager::changePassword(const std::string& username, const std::string& oldPassword, const std::string& newPassword) {
    auto users = storage.get_all<User>(where(c(&User::username) == username && c(&User::password) == oldPassword && c(&User::deleted_at) == 0));
    if (users.empty()) { std::cout << "原密码错误" << std::endl; return false; }
    User u = users[0]; u.password = newPassword; storage.update(u);
    std::cout << "密码修改成功" << std::endl; return true;
}
void UserManager::showActiveUsers() {
    auto users = storage.get_all<User>(where(c(&User::deleted_at) == 0));
    for (const auto& u : users) std::cout << "ID: " << u.id << ", 用户名: " << u.username << std::endl;
}
void UserManager::showAllUsers() {
    auto users = storage.get_all<User>();
    for (const auto& u : users) std::cout << "ID: " << u.id << ", 用户名: " << u.username << ", 状态: " << (u.deleted_at == 0 ? "正常" : "已删除") << std::endl;
}

// ====================== 整合菜单 ======================
int main() {
    UserManager um;
    ClanManager cm(um);

    int choice;
    std::string username, password, newUsername, oldPassword, newPassword;
    std::string clanName, newClanName, leaderUsername, targetUsername;

    while (true) {
        std::cout << "\n======== 综合管理系统 ========" << std::endl;
        std::cout << "【用户管理】" << std::endl;
        std::cout << "1. 注册用户" << std::endl;
        std::cout << "2. 登录" << std::endl;
        std::cout << "3. 软删除用户" << std::endl;
        std::cout << "4. 恢复用户" << std::endl;
        std::cout << "5. 彻底删除用户" << std::endl;
        std::cout << "6. 修改用户名" << std::endl;
        std::cout << "7. 修改密码" << std::endl;
        std::cout << "8. 显示活跃用户" << std::endl;
        std::cout << "9. 显示所有用户（含已删除）" << std::endl;
        std::cout << "【部落管理】" << std::endl;
        std::cout << "10. 创建部落" << std::endl;
        std::cout << "11. 解散部落（软删除）" << std::endl;
        std::cout << "12. 恢复部落" << std::endl;
        std::cout << "13. 修改部落名称" << std::endl;
        std::cout << "14. 加入部落" << std::endl;
        std::cout << "15. 退出部落" << std::endl;
        std::cout << "16. 转让部落领导者" << std::endl;
        std::cout << "17. 踢出成员（仅领导者）" << std::endl;
        std::cout << "18. 查看部落成员列表" << std::endl;
        std::cout << "19. 查询用户所在部落" << std::endl;
        std::cout << "20. 列出所有活跃部落" << std::endl;
        std::cout << "0. 退出" << std::endl;
        std::cout << "请选择: ";
        std::cin >> choice;

        switch (choice) {
            case 1: // 注册
                std::cout << "用户名: "; std::cin >> username;
                std::cout << "密码: "; std::cin >> password;
                um.registerUser(username, password); break;
            case 2: // 登录
                std::cout << "用户名: "; std::cin >> username;
                std::cout << "密码: "; std::cin >> password;
                um.login(username, password); break;
            case 3: // 软删除用户
                std::cout << "用户名: "; std::cin >> username;
                um.deleteUser(username); break;
            case 4: // 恢复用户
                std::cout << "用户名: "; std::cin >> username;
                um.restoreUser(username); break;
            case 5: // 彻底删除用户
                std::cout << "用户名: "; std::cin >> username;
                um.permanentDelete(username); break;
            case 6: // 改名
                std::cout << "当前用户名: "; std::cin >> username;
                std::cout << "新用户名: "; std::cin >> newUsername;
                um.renameUser(username, newUsername); break;
            case 7: // 改密码
                std::cout << "用户名: "; std::cin >> username;
                std::cout << "原密码: "; std::cin >> oldPassword;
                std::cout << "新密码: "; std::cin >> newPassword;
                um.changePassword(username, oldPassword, newPassword); break;
            case 8: // 活跃用户
                um.showActiveUsers(); break;
            case 9: // 所有用户
                um.showAllUsers(); break;
            case 10: // 创建部落
                std::cout << "部落名称: "; std::cin >> clanName;
                std::cout << "领导者用户名: "; std::cin >> leaderUsername;
                cm.createClan(clanName, leaderUsername); break;
            case 11: // 解散部落
                std::cout << "部落名称: "; std::cin >> clanName;
                cm.disbandClan(clanName); break;
            case 12: // 恢复部落
                std::cout << "部落名称: "; std::cin >> clanName;
                cm.restoreClan(clanName); break;
            case 13: // 修改部落名称
                std::cout << "当前部落名称: "; std::cin >> clanName;
                std::cout << "新部落名称: "; std::cin >> newClanName;
                cm.renameClan(clanName, newClanName); break;
            case 14: // 加入部落
                std::cout << "用户名: "; std::cin >> username;
                std::cout << "部落名称: "; std::cin >> clanName;
                cm.joinClan(username, clanName); break;
            case 15: // 退出部落
                std::cout << "用户名: "; std::cin >> username;
                cm.leaveClan(username); break;
            case 16: // 转让领导者
                std::cout << "部落名称: "; std::cin >> clanName;
                std::cout << "当前领导者用户名: "; std::cin >> leaderUsername;
                std::cout << "新领导者用户名: "; std::cin >> targetUsername;
                cm.transferLeadership(clanName, leaderUsername, targetUsername); break;
            case 17: // 踢出成员
                std::cout << "部落名称: "; std::cin >> clanName;
                std::cout << "领导者用户名: "; std::cin >> leaderUsername;
                std::cout << "要踢出的成员用户名: "; std::cin >> targetUsername;
                cm.kickMember(clanName, leaderUsername, targetUsername); break;
            case 18: // 查看部落成员
                std::cout << "部落名称: "; std::cin >> clanName;
                cm.listClanMembers(clanName); break;
            case 19: // 查询用户部落
                std::cout << "用户名: "; std::cin >> username;
                cm.showUserClan(username); break;
            case 20: // 列出所有部落
                cm.listActiveClans(); break;
            case 0:
                std::cout << "退出系统" << std::endl;
                return 0;
            default:
                std::cout << "无效选项" << std::endl;
        }
    }
}
