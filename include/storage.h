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
			foreign_key(&Message::sender_id).references(&User::id)
		//	foreign_key(&Message::target_id).references(&Clan::id)
		)
    );
}

using Storage = decltype(createStorage(std::declval<std::string>()));

inline int64_t getCurrentTimestamp() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

} // namespace app
