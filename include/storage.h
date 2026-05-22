#pragma once
#include <sqlite_orm/sqlite_orm.h>
#include <string>
#include <memory>
#include <chrono>

namespace app {

// ---------- 模型 ----------
struct User {
    int id = 0;
    std::string username;
    std::string password;
    int64_t deleted_at = 0;
};

struct Clan {
    int id = 0;
    std::string name;
    int leader_id = 0;
    int64_t deleted_at = 0;
};

struct ClanMember {
    int id = 0;
    int clan_id = 0;
    int user_id = 0;
};

struct Message {
    int id = 0;
    int sender_id = 0;
    int target_type = 0;        // 0: global, 1: clan
    std::optional<int> target_id; // clan_id，若 target_type 为 global 则为 nullopt
    std::string content;
    int64_t created_at = 0;
};

struct ClanApplication {
    int id = 0;
    int clan_id = 0;
    int applicant_id = 0;
    int status = 0;            // 0: pending, 1: approved, 2: rejected
    int64_t created_at = 0;
    std::optional<int64_t> updated_at = std::nullopt; // 处理时间
};

struct GameRecord {
    int id = 0;
    int user_id = 0;
    int mode = 0;              // 1: 初级, 2: 中级, 3: 高级
    int64_t played_at = 0;     // 游戏结束时间戳
    int duration_seconds = 0;  // 用时（秒）
    int three_bv = 0;          // 3BV 值
};

struct Chunk {
    int id = 0;
    int x = 0;
    int y = 0;
    int64_t created_at = 0;
    int64_t updated_at = 0;
    int last_updated_by = 0;
    int64_t deleted_at = 0;      // 0: active
    std::string data;
};

// ---------- 存储工厂 ----------
inline auto createStorage(const std::string& dbPath) {
    using namespace sqlite_orm;
    return make_storage(dbPath,
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
            unique(&Clan::name, &Clan::deleted_at),
            foreign_key(&Clan::leader_id).references(&User::id)
        ),
        make_table("clan_members",
            make_column("id", &ClanMember::id, primary_key().autoincrement()),
            make_column("clan_id", &ClanMember::clan_id),
            make_column("user_id", &ClanMember::user_id),
            unique(&ClanMember::clan_id, &ClanMember::user_id),
            foreign_key(&ClanMember::clan_id).references(&Clan::id).on_delete.cascade(),
            foreign_key(&ClanMember::user_id).references(&User::id)
        ),
		make_table("messages",
			make_column("id", &Message::id, primary_key().autoincrement()),
			make_column("sender_id", &Message::sender_id),
			make_column("target_type", &Message::target_type, default_value(0)),
			make_column("target_id", &Message::target_id),  // nullable
			make_column("content", &Message::content),
			make_column("created_at", &Message::created_at),
			foreign_key(&Message::sender_id).references(&User::id),
			foreign_key(&Message::target_id).references(&Clan::id)
		),
		make_table("clan_applications",
			make_column("id", &ClanApplication::id, primary_key().autoincrement()),
			make_column("clan_id", &ClanApplication::clan_id),
			make_column("applicant_id", &ClanApplication::applicant_id),
			make_column("status", &ClanApplication::status, default_value(0)),
			make_column("created_at", &ClanApplication::created_at),
			make_column("updated_at", &ClanApplication::updated_at),
			unique(&ClanApplication::clan_id, &ClanApplication::applicant_id, &ClanApplication::status),
			foreign_key(&ClanApplication::clan_id).references(&Clan::id).on_delete.cascade(),
			foreign_key(&ClanApplication::applicant_id).references(&User::id)
		),
		make_table("game_records",
			make_column("id", &GameRecord::id, primary_key().autoincrement()),
			make_column("user_id", &GameRecord::user_id),
			make_column("mode", &GameRecord::mode),
			make_column("played_at", &GameRecord::played_at),
			make_column("duration_seconds", &GameRecord::duration_seconds),
			make_column("three_bv", &GameRecord::three_bv),
			foreign_key(&GameRecord::user_id).references(&User::id)
		),
		make_table("chunks",
			make_column("id", &Chunk::id, primary_key().autoincrement()),
			make_column("x", &Chunk::x),
			make_column("y", &Chunk::y),
			make_column("created_at", &Chunk::created_at),
			make_column("updated_at", &Chunk::updated_at),
			make_column("last_updated_by", &Chunk::last_updated_by),
			make_column("deleted_at", &Chunk::deleted_at, default_value(0)),
			make_column("data", &Chunk::data),
			unique(&Chunk::x, &Chunk::y, &Chunk::deleted_at),
			foreign_key(&Chunk::last_updated_by).references(&User::id)
		)
    );
}

using Storage = decltype(createStorage(std::declval<std::string>()));

inline int64_t getCurrentTimestamp() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

} // namespace app
