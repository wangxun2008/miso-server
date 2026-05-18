#include <cassert>
#include <iostream>
#include "storage.h"
#include "exceptions.h"
#include "user_manager.h"
#include "clan_manager.h"

using namespace app;

void testUserRegistration() {
    auto storage = createStorage(":memory:");
    storage.sync_schema();
    UserManager um(storage);

    // 注册
    int id1 = um.registerUser("alice", "123");
    assert(id1 > 0);
    try {
        um.registerUser("alice", "123");
        assert(false); // 应抛出异常
    } catch (const UserAlreadyExistsException&) {}

    // 登录
    User u = um.login("alice", "123");
    assert(u.username == "alice");
    try {
        um.login("alice", "wrong");
        assert(false);
    } catch (const AuthenticationException&) {}

    // 软删除
    um.deleteUser("alice");
    assert(!um.isUserActive(id1));
    try {
        um.login("alice", "123");
        assert(false);
    } catch (const AuthenticationException&) {}

    // 恢复
    um.restoreUser("alice");
    assert(um.isUserActive(id1));
    u = um.login("alice", "123");
    assert(u.id == id1);

    // 永久删除
    um.permanentDeleteUser("alice");
    try {
        um.getUserById(id1);
        assert(!um.getUserById(id1).has_value());
    } catch (const UserNotFoundException&) {}

    // 改名测试
    int id2 = um.registerUser("bob", "pwd");
    um.renameUser("bob", "charlie");
    assert(um.getUserById(id2)->username == "charlie");
    try {
        um.renameUser("bob", "charlie"); // 原用户名不存在
        assert(false);
    } catch (const UserNotFoundException&) {}

    std::cout << "testUserRegistration passed.\n";
}

void testClanOperations() {
    auto storage = createStorage(":memory:");
    storage.sync_schema();
    UserManager um(storage);
    ClanManager cm(storage, um);

    int uid1 = um.registerUser("leader", "pwd");
    int uid2 = um.registerUser("member", "pwd");
    int uid3 = um.registerUser("outsider", "pwd");

    // 创建战队
    int clanId = cm.createClan("Warriors", uid1);
    assert(clanId > 0);
    try {
        cm.createClan("Warriors", uid1);
        assert(false);
    } catch (const ClanNameConflictException&) {}

    // 添加成员
    cm.addMember(clanId, uid2);
    try {
        cm.addMember(clanId, uid2);
        assert(false);
    } catch (const MemberAlreadyInClanException&) {}

    // 不能移除领导者
    try {
        cm.removeMember(clanId, uid1);
        assert(false);
    } catch (const CannotRemoveLeaderException&) {}

    // 转让领导权
    cm.transferLeadership(clanId, uid2);
    Clan c = cm.getClan(clanId);
    assert(c.leader_id == uid2);

    // 现在可以移除原领导者
    cm.removeMember(clanId, uid1);
    try {
        cm.removeMember(clanId, uid1);
        assert(false);
    } catch (const NotMemberException&) {}

    // 解散战队
    cm.dissolveClan(clanId);
    assert(!cm.isClanActive(clanId));
    try {
        cm.addMember(clanId, uid3);
        assert(false);
    } catch (const ClanNotActiveException&) {}

    // 创建第二个战队，测试 getActiveClans
    int clanId2 = cm.createClan("Knights", uid3);
    auto active = cm.getActiveClans();
    assert(active.size() == 1);
    assert(active[0].id == clanId2);

    std::cout << "testClanOperations passed.\n";
}

int main() {
    try {
        testUserRegistration();
        testClanOperations();
        std::cout << "All tests passed." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
