#include <cassert>
#include <iostream>
#include "storage.h"
#include "exceptions.h"
#include "user_manager.h"
#include "clan_manager.h"
#include "chat_manager.h"

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

void testChat() {
    auto storage = createStorage(":memory:");
    storage.sync_schema();
    UserManager um(storage);
    ClanManager cm(storage, um);
    ChatManager chat(storage, um, cm);

    // 准备用户
    int uid1 = um.registerUser("alice", "pwd");
    int uid2 = um.registerUser("bob", "pwd");
    int uid3 = um.registerUser("charlie", "pwd");

    // 创建战队
    int clanId = cm.createClan("DreamTeam", uid1);
    cm.addMember(clanId, uid2); // uid2 加入

    // 发送全局消息
    int gMsgId = chat.sendGlobalMessage(uid1, "Hello everyone!");
    assert(gMsgId > 0);
    // 未活跃用户不能发（测试 uid3 是活跃的，所以可以发）
    // 尝试用不存在的用户发消息
    try {
        chat.sendGlobalMessage(9999, "Ghost");
        assert(false);
    } catch (const UserNotFoundException&) {}

    // 发送战队消息
    int cMsgId = chat.sendClanMessage(uid1, clanId, "Hi team");
    assert(cMsgId > 0);
    // 非成员不能发战队消息
    try {
        chat.sendClanMessage(uid3, clanId, "Intruder");
        assert(false);
    } catch (const NotMemberOfClanException&) {}

    // 获取全局消息
    auto globals = chat.getGlobalMessages();
    assert(globals.size() == 1);
    assert(globals[0].content == "Hello everyone!");

    // 获取战队消息（uid2 是成员，可以查看）
    auto clanMsgs = chat.getClanMessages(clanId, uid2);
    assert(clanMsgs.size() == 1);
    assert(clanMsgs[0].content == "Hi team");
    // uid3 不是成员，查看战队消息应抛异常
    try {
        chat.getClanMessages(clanId, uid3);
        assert(false);
    } catch (const NotMemberOfClanException&) {}

    // 再发一条全局消息，检查分页
    chat.sendGlobalMessage(uid2, "Second");
    auto recentTwo = chat.getGlobalMessages(2, 0);
    assert(recentTwo.size() == 2);
    // 按时间倒序，最新的在前
    assert(recentTwo[0].content == "Second");

    std::cout << "testChat passed.\n";
}

int main() {
    try {
        testUserRegistration();
        testClanOperations();
		testChat();
        std::cout << "All tests passed." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
