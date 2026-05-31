#include <cassert>
#include <chrono>
#include <thread>
#include <iostream>
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
#include "game_logic_manager.h"
#include "server.h"
#include "client.h"
#include <thread>
#include <chrono>
#include <cstring>

using namespace miso;

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
	std::this_thread::sleep_for(std::chrono::seconds(1));
    chat.sendGlobalMessage(uid2, "Second");
    auto recentTwo = chat.getGlobalMessages(2, 0);
    assert(recentTwo.size() == 2);
    // 按时间倒序，最新的在前
    assert(recentTwo[0].content == "Second");

    std::cout << "testChat passed.\n";
}

void testClanApplications() {
    auto storage = createStorage(":memory:");
    storage.sync_schema();
    UserManager um(storage);
    ClanManager cm(storage, um);

    int leader = um.registerUser("leader", "pwd");
    int member = um.registerUser("member", "pwd");
    int applicant = um.registerUser("applicant", "pwd");

    int clanId = cm.createClan("TestClan", leader);
    cm.addMember(clanId, member);

    // 正常申请
    int appId = cm.applyToClan(clanId, applicant);
    assert(appId > 0);

    // 重复申请应失败
    try {
        cm.applyToClan(clanId, applicant);
        assert(false);
    } catch (const AlreadyAppliedException&) {}

    // 已是成员不能申请
    try {
        cm.applyToClan(clanId, member);
        assert(false);
    } catch (const AlreadyMemberException&) {}

    // 非活跃用户不能申请
    um.deleteUser("applicant"); // 软删除
    try {
        cm.applyToClan(clanId, applicant);
        assert(false);
    } catch (const UserNotFoundException&) {}
    um.restoreUser("applicant"); // 恢复

    // 领导者审批通过
    cm.processApplication(appId, leader, "approve");
    // 现在 applicant 应该成为成员
    auto members = cm.getClanMembers(clanId);
    bool found = false;
    for (auto& m : members) {
        if (m.user_id == applicant) { found = true; break; }
    }
    assert(found);

    // 查看申请状态
    auto myApps = cm.getMyApplications(applicant, 1); // 已批准
    assert(myApps.size() == 1);
    assert(myApps[0].status == 1);

    // 已是成员无法再次申请
    try {
        cm.applyToClan(clanId, applicant);
        assert(false); // 不应到达这里
    } catch (const AlreadyMemberException&) {
        // 预期异常
    }

    // 非活跃用户不能申请（用字符串删除/恢复）
    um.deleteUser("applicant");   // 软删除（注意参数是用户名）
    try {
        cm.applyToClan(clanId, applicant);
        assert(false);
    } catch (const UserNotFoundException&) {
        // 预期异常
    }
    um.restoreUser("applicant");  // 恢复

    std::cout << "testClanApplications passed.\n";
}

void testGameRecords() {
    auto storage = createStorage(":memory:");
    storage.sync_schema();
    UserManager um(storage);
    GameManager gm(storage, um);

    // 准备用户
    int uid1 = um.registerUser("player1", "pwd");
    int uid2 = um.registerUser("player2", "pwd");

    // 添加记录
    int rec1 = gm.addGameRecord(uid1, 1, 120, 45); // 初级，120秒
	std::this_thread::sleep_for(std::chrono::seconds(1));
    int rec2 = gm.addGameRecord(uid2, 1, 90, 40);  // 初级，90秒（更快）
	std::this_thread::sleep_for(std::chrono::seconds(1));
    int rec3 = gm.addGameRecord(uid1, 2, 300, 150); // 中级

    // 测试排行榜（初级）
    auto lb = gm.getLeaderboard(1, 10);
    assert(lb.size() == 2);
    assert(lb[0].user_id == uid2); // 90秒排第一
    assert(lb[1].user_id == uid1);

    // 测试个人记录（uid1 所有模式）
    auto recs = gm.getUserRecords(uid1, -1);
	assert(recs.size() == 2);
    // 按时间降序，最近的在前面
    assert(recs[0].id == rec3);

    // 测试无效模式
    try {
        gm.addGameRecord(uid1, 4, 100, 50);
        assert(false);
    } catch (const InvalidGameModeException&) {}

    // 测试不存在的用户
    try {
        gm.addGameRecord(9999, 1, 100, 50);
        assert(false);
    } catch (const UserNotFoundException&) {}

    // 测试非活跃用户（软删除后不能添加记录）
    um.deleteUser("player2");
    try {
        gm.addGameRecord(uid2, 1, 100, 50);
        assert(false);
    } catch (const UserNotFoundException&) {}

    std::cout << "testGameRecords passed.\n";
}

void testChunkManager() {
    auto storage = createStorage(":memory:");
    storage.sync_schema();
    UserManager um(storage);
    ChunkManager cm(storage, um);

    int uid = um.registerUser("builder", "pwd");

    // 创建区块
    Chunk c1 = cm.createOrUpdateChunk(10, 20, uid, "grass");
    assert(c1.id > 0);
    assert(c1.data == "grass");
    assert(c1.x == 10 && c1.y == 20);

    // 读取区块
    Chunk c2 = cm.getChunk(10, 20);
    assert(c2.data == "grass");

    // 更新区块（同坐标再次调用）
    Chunk c3 = cm.createOrUpdateChunk(10, 20, uid, "stone");
    assert(c3.id == c1.id);  // id 相同，表示是更新
    assert(c3.data == "stone");

    // 软删除
    cm.deleteChunk(10, 20);
    // 删除后无法获取
    try {
        cm.getChunk(10, 20);
        assert(false);
    } catch (const ChunkNotFoundException&) {}

    // 可以在同一坐标再次创建（因为旧记录已删除）
    Chunk c4 = cm.createOrUpdateChunk(10, 20, uid, "water");
    assert(c4.id != c1.id);  // 新记录
    assert(c4.data == "water");

    // 范围查询
    cm.createOrUpdateChunk(11, 20, uid, "dirt");
    cm.createOrUpdateChunk(10, 21, uid, "sand");
    cm.createOrUpdateChunk(15, 25, uid, "snow");
    auto chunks = cm.getChunksInArea(10, 20, 11, 21);
    // 应返回 (10,20),(11,20),(10,21)
    assert(chunks.size() == 3);

    // 无效用户操作
    try {
        cm.createOrUpdateChunk(0, 0, 9999, "fail");
        assert(false);
    } catch (const UserNotFoundException&) {}

    std::cout << "testChunkManager passed.\n";
}

void testOnlineStatus() {
    auto storage = createStorage(":memory:");
    storage.sync_schema();
    UserManager um(storage);
    OnlineManager om(storage, um);

    int uid = um.registerUser("player1", "pwd");
    int uid2 = um.registerUser("player2", "pwd");

    // 未上线时检查离线
    assert(!om.isUserOnline(uid));

    // 上线
    om.userOnline(uid);
    assert(om.isUserOnline(uid));
    // 检查在线列表
    auto online = om.getOnlineUsers(5);
    assert(online.size() == 1);
    assert(online[0] == uid);

    // 心跳
    om.updateHeartbeat(uid);
    assert(om.isUserOnline(uid));

    // 下线
    om.userOffline(uid);
    assert(!om.isUserOnline(uid));

    // 多用户
    om.userOnline(uid);
    om.userOnline(uid2);
    online = om.getOnlineUsers(5);
    assert(online.size() == 2);

    // 测试超时（模拟心跳过期）
    // 直接修改数据库使心跳过期（用低阶操作）
    auto session = storage.get_all<UserSession>(sqlite_orm::where(sqlite_orm::c(&UserSession::user_id) == uid)).front();
    session.last_heartbeat = getCurrentTimestamp() - 100; // 100秒前
    storage.update(session);
    assert(!om.isUserOnline(uid, 60));  // 超时60秒，判定离线
    // 但好友2还没过期
    assert(om.isUserOnline(uid2, 60));

    // 对离线用户更新心跳应抛异常
	om.userOffline(uid);
	try {
        om.updateHeartbeat(uid);
        assert(false);
    } catch (const UserOfflineException&) {}

    std::cout << "testOnlineStatus passed.\n";
}

void testNotices() {
    auto storage = createStorage(":memory:");
    storage.sync_schema();
    UserManager um(storage);
    NoticeManager nm(storage, um);

    int uid1 = um.registerUser("admin", "pwd");
    int uid2 = um.registerUser("user", "pwd");

    // 发布通知
    int nid1 = nm.publishNotice(uid1, "Welcome", "Hello everyone");
	std::this_thread::sleep_for(std::chrono::seconds(1));
    int nid2 = nm.publishNotice(uid1, "Update", "New features");
    assert(nid1 > 0 && nid2 > 0);

    // 获取有效通知列表
    auto list = nm.getActiveNotices();
    assert(list.size() == 2);
    assert(list[0].id == nid2); // 最新在前

    // 获取详情
    Notice n = nm.getNoticeById(nid1);
    assert(n.title == "Welcome");

    // 非发布者删除应抛出 NotAuthorizedException
    try {
        nm.deleteNotice(nid1, uid2);
        assert(false);
    } catch (const NotAuthorizedException&) {}

    // 发布者删除
    nm.deleteNotice(nid1, uid1);
    // 删除后无法获取
    try {
        nm.getNoticeById(nid1);
        assert(false);
    } catch (const NoticeNotFoundException&) {}

    // 列表只剩一条
    list = nm.getActiveNotices();
    assert(list.size() == 1);

    // 不存在的用户发布通知
    try {
        nm.publishNotice(9999, "Test", "Fail");
        assert(false);
    } catch (const UserNotFoundException&) {}

    std::cout << "testNotices passed.\n";
}

void testDiscussion() {
    auto storage = createStorage(":memory:");
    storage.sync_schema();
    UserManager um(storage);
    ClanManager cm(storage, um);
    TopicManager tm(storage, um, cm);
    CommentManager commgr(storage, um, tm);

    int u1 = um.registerUser("alice", "pwd");
    int u2 = um.registerUser("bob", "pwd");
    int u3 = um.registerUser("charlie", "pwd");

    int clanId = cm.createClan("Dev", u1);
    cm.addMember(clanId, u2);

    // global topic
    int globalPost = tm.createTopic(u1, "Global", "Hello world", 0);
    // clan topic
    int clanPost = tm.createTopic(u1, "ClanOnly", "Secret", 1, clanId);

    // visibility
    auto vis1 = tm.getVisibleTopics(u1);
    assert(vis1.size() == 2);
    auto vis2 = tm.getVisibleTopics(u2);
    assert(vis2.size() == 2);
    auto vis3 = tm.getVisibleTopics(u3);
    assert(vis3.size() == 1);
    assert(vis3[0].id == globalPost);

    // access denied for u3 on clan post
    try {
        tm.getTopicById(clanPost, u3);
        assert(false);
    } catch (const AccessDeniedException&) {}

    // edit permission
    tm.updateTopic(globalPost, u1, "New", "New content");
    try {
        tm.updateTopic(globalPost, u2, "Hack", "Hack");
        assert(false);
    } catch (const NotAuthorizedException&) {}

    // delete topic
    tm.deleteTopic(globalPost, u1);
    try {
        tm.getTopicById(globalPost, u1);
        assert(false);
    } catch (const TopicNotFoundException&) {}

    // comments
    int c1 = commgr.addComment(clanPost, u2, "Nice!");
    try {
        commgr.addComment(clanPost, u3, "Intrude");
        assert(false);
    } catch (const AccessDeniedException&) {}

    auto comments = commgr.getComments(clanPost, u2);
    assert(comments.size() == 1);
    assert(comments[0].content == "Nice!");

    commgr.updateComment(c1, u2, "Updated");
    try {
        commgr.updateComment(c1, u1, "Hack");
        assert(false);
    } catch (const NotAuthorizedException&) {}

    commgr.deleteComment(c1, u2);
    comments = commgr.getComments(clanPost, u2);
    assert(comments.empty());

    std::cout << "testDiscussion passed.\n";
}

void testMineMapManager() {
    auto storage = createStorage(":memory:");
    storage.sync_schema();
    UserManager um(storage);
    ChunkManager cm(storage, um);
    MineMapManager mm(cm, um, 3); // 小缓存

    int uid = um.registerUser("player", "pwd");

    // 设置地雷
    std::vector<std::pair<int,int>> mines = {{0,0}, {1,1}};
    mm.placeMines(mines, uid);

    Cell c = mm.getCell(0, 0);
    assert(c.has_mine);
    assert(!c.is_revealed);

    // 修改领地
    mm.modifyCell(0, 0, uid, [uid](Cell& cell) {
        cell.owner_id = uid;
    });
    c = mm.getCell(0, 0);
    assert(c.owner_id == uid);

    // 跨区块写入
    mm.modifyCell(20, 20, uid, [](Cell& cell) {
        cell.terrain = 3;
    });
    mm.flushAll(uid);
    mm.unloadChunk(1, 1, uid); // 区块 (1,1)
    c = mm.getCell(20, 20);
    assert(c.terrain == 3);

    // 缓存淘汰
    mm.modifyCell(0, 16, uid, [](Cell& cell) { cell.has_mine = true; });
    mm.modifyCell(16, 0, uid, [](Cell& cell) { cell.has_mine = true; });
    assert(mm.cacheSize() <= 3);

    // 序列化正确性
    ChunkGrid grid;
    grid.cells[0][0].has_mine = true;
    grid.cells[5][5].owner_id = 123456;
    grid.cells[15][15].terrain = 255;
    std::string ser = MineMapManager::serializeGrid(grid);
    ChunkGrid deser = MineMapManager::deserializeGrid(ser);
    assert(deser.cells[0][0].has_mine);
    assert(deser.cells[5][5].owner_id == 123456);
	assert(deser.cells[15][15].terrain == 255);

	// 测试初始化标记
	mm.modifyCell(0, 0, uid, [](Cell& c) {
		c.is_initialized = true;
	});
	Cell c2 = mm.getCell(0, 0);
	assert(c2.is_initialized);
	// 序列化/反序列化验证
	ChunkGrid grid2;
	grid2.cells[0][0].is_initialized = true;
	std::string ser2 = MineMapManager::serializeGrid(grid2);
	ChunkGrid deser2 = MineMapManager::deserializeGrid(ser2);
	assert(deser2.cells[0][0].is_initialized);

    std::cout << "testMineMapManager passed.\n";
}

void testGameLogic() {
    auto storage = createStorage(":memory:");
    storage.sync_schema();
    UserManager um(storage);
    ChunkManager cm(storage, um);
    MineMapManager mm(cm, um, 100);
    GameLogicManager glm(mm, um);

    int uid = um.registerUser("player", "pwd");
    int uid2 = um.registerUser("player2", "pwd");

    // 测试初始化与翻开
    // 坐标 (0,0) 未初始化，翻开应自动布雷并返回是否踩雷
    bool dead = glm.revealCell(0, 0, uid);
    // 由于布雷是随机的，我们只检查状态已翻开
    Cell c = mm.getCell(0, 0);
    assert(c.is_revealed);
    assert(c.owner_id == uid);
    if (dead) {
        assert(c.has_mine);
    } else {
        assert(!c.has_mine);
        // 若周围雷数为0，会连锁翻开相邻格
        // 检查相邻格是否也被翻开了（如果该格无雷且周围雷数为0）
        // 这里不确定，暂不深入验证
    }

    // 测试标记
    glm.toggleFlag(10, 10, uid);
    Cell c2 = mm.getCell(10, 10);
    assert(c2.is_flagged);
    assert(c2.owner_id == uid);
    // 再次标记取消
    glm.toggleFlag(10, 10, uid);
    c2 = mm.getCell(10, 10);
    assert(!c2.is_flagged);

    // 测试快速标记
    // 构造一个场景：在 (20,20) 及其周围布雷，然后翻开 (20,20)，再快速标记
    // 为简化，直接使用已经初始化的区域并设置特定状态。
    // 我们手动修改几个格子的状态来模拟。
    // 先确保区域初始化
    glm.revealCell(30, 30, uid); // 可能踩雷，但我们只关心初始化完成
    // 手动设置周围格子：让 (30,30) 数字为2，周围有两个未翻开无标记格子，以触发快速标记
    mm.modifyCell(30,30, uid, [](Cell& cc) { cc.is_revealed = true; cc.adjacent_mines = 2; cc.has_mine = false; });
    // 设置两个邻居未翻开无标记
    mm.modifyCell(31,30, uid, [](Cell& cc) { cc.is_revealed = false; cc.is_flagged = false; cc.has_mine = false; });
    mm.modifyCell(30,31, uid, [](Cell& cc) { cc.is_revealed = false; cc.is_flagged = false; cc.has_mine = false; });

    std::cout << "testGameLogic passed.\n";
}

void testNetwork() {
    using namespace miso;
    const unsigned short tcpPort = 54321;
    const unsigned short udpPort = 54322;

    // 启动服务器
    Server server(tcpPort, udpPort);
    server.start();
    // 给服务器一点时间开始监听
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 创建客户端并连接
    Client client(sf::IpAddress::LocalHost, tcpPort, udpPort);
    bool connected = client.connect();
    assert(connected);
    // 等待初始时间同步稳定
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 客户端发送一条测试消息
    sf::Packet sendPkt;
    std::string testStr = "Hello, Server!";
    sendPkt << testStr;
    bool sent = client.sendMessage(std::move(sendPkt));
    assert(sent);

    // 服务器端获取消息（稍等片刻让网络线程传递）
    std::optional<RawClientMessage> raw = std::nullopt;
    for (int i = 0; i < 50; ++i) { // 尝试多次，总计约 1 秒
        raw = server.getNextMessage(sf::milliseconds(100));
        if (raw.has_value()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    assert(raw.has_value());
    assert(raw->packet.has_value()); // 不是断开消息
    // 解析消息
    std::string receivedStr;
    (*raw->packet) >> receivedStr;
    assert(receivedStr == testStr);

    // 服务器回复客户端
    sf::Packet replyPkt;
    replyPkt << "Hello, Client!";
    server.sendMessage(raw->clientId, std::move(replyPkt));
    // 等待回复到达客户端
    std::optional<sf::Packet> clientReply = std::nullopt;
    for (int i = 0; i < 50; ++i) {
        clientReply = client.receiveMessage();
        if (clientReply.has_value()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    assert(clientReply.has_value());
    std::string replyStr;
    (*clientReply) >> replyStr;
    assert(replyStr == "Hello, Client!");

    // 测试时间同步：客户端 getTimestamp 应接近服务器 getCurrentTimestamp
    int64_t serverTime = server.getCurrentTimestamp();
    int64_t clientTime = client.getTimestamp();
    // 允许一些误差（取决于网络延迟和同步质量）
    assert(std::abs(serverTime - clientTime) < 500000); // 0.5 秒内

    // 客户端断开连接
    client.disconnect();
    // 服务器应收到断开消息
    raw = std::nullopt;
    for (int i = 0; i < 50; ++i) {
        raw = server.getNextMessage(sf::milliseconds(0));
        if (raw.has_value()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    assert(raw.has_value());
    assert(!raw->packet.has_value()); // 断开消息 packet 为空

    server.stop();
    std::cout << "testNetwork passed.\n";
}

int main() {
    try {
        testUserRegistration();
        testClanOperations();
		testChat();
		testClanApplications();
		testGameRecords();
		testChunkManager();
		testOnlineStatus();
		testNotices();
		testDiscussion();
		testMineMapManager();
		testGameLogic();
		testNetwork();
 		std::cout << "All tests passed." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
