#include "user_manager.h"
#include <algorithm>
#include <sqlite_orm/sqlite_orm.h>

namespace app {

using namespace sqlite_orm;

UserManager::UserManager(Storage& storage) : storage(storage) {}

int UserManager::registerUser(const std::string& username, const std::string& password) {
    auto existing = storage.get_all<User>(
        where(c(&User::username) == username and c(&User::deleted_at) == 0)
    );
    if (!existing.empty()) {
        throw UserAlreadyExistsException(username);
    }
    User newUser{-1, username, password, 0};
    return storage.insert(newUser);
}

User UserManager::login(const std::string& username, const std::string& password) {
    auto users = storage.get_all<User>(
        where(c(&User::username) == username
              and c(&User::password) == password
              and c(&User::deleted_at) == 0)
    );
    if (users.empty()) {
        throw AuthenticationException();
    }
    return users.front();
}

void UserManager::deleteUser(const std::string& username) {
    auto users = storage.get_all<User>(
        where(c(&User::username) == username and c(&User::deleted_at) == 0)
    );
    if (users.empty()) {
        throw UserNotFoundException(username);
    }
    User user = users.front();
    user.deleted_at = getCurrentTimestamp();
    storage.update(user);
}

void UserManager::restoreUser(const std::string& username) {
    // 查找已删除的用户
    auto deletedUsers = storage.get_all<User>(
        where(c(&User::username) == username and c(&User::deleted_at) != 0)
    );
    if (deletedUsers.empty()) {
        throw UserNotFoundException("deleted user " + username);
    }
    // 检查是否已有同名活跃用户
    auto active = storage.get_all<User>(
        where(c(&User::username) == username and c(&User::deleted_at) == 0)
    );
    if (!active.empty()) {
        throw UserAlreadyExistsException(username);
    }
    // 恢复最早删除的那条（如有多个已删除同名用户，只恢复第一条）
    User user = deletedUsers.front();
    user.deleted_at = 0;
    storage.update(user);
}

void UserManager::permanentDeleteUser(const std::string& username) {
    auto users = storage.get_all<User>(
        where(c(&User::username) == username)
    );
    if (users.empty()) {
        throw UserNotFoundException(username);
    }
    // 删除第一个匹配的（通常只有一个）
    storage.remove<User>(users.front().id);
}

void UserManager::renameUser(const std::string& oldUsername, const std::string& newUsername) {
    // 查找要改名的活跃用户
    auto oldUsers = storage.get_all<User>(
        where(c(&User::username) == oldUsername and c(&User::deleted_at) == 0)
    );
    if (oldUsers.empty()) {
        throw UserNotFoundException(oldUsername);
    }
    // 检查新用户名是否被活跃用户占用
    auto newUsers = storage.get_all<User>(
        where(c(&User::username) == newUsername and c(&User::deleted_at) == 0)
    );
    if (!newUsers.empty()) {
        throw UserAlreadyExistsException(newUsername);
    }
    User user = oldUsers.front();
    user.username = newUsername;
    storage.update(user);
}

void UserManager::changePassword(const std::string& username,
                                 const std::string& oldPassword,
                                 const std::string& newPassword) {
    auto users = storage.get_all<User>(
        where(c(&User::username) == username
              and c(&User::password) == oldPassword
              and c(&User::deleted_at) == 0)
    );
    if (users.empty()) {
        throw AuthenticationException(); // 或者 UserNotFoundException?
    }
    User user = users.front();
    user.password = newPassword;
    storage.update(user);
}

std::vector<User> UserManager::getActiveUsers() const {
    return storage.get_all<User>(where(c(&User::deleted_at) == 0));
}

std::vector<User> UserManager::getAllUsers() const {
    return storage.get_all<User>();
}

std::optional<User> UserManager::getUserById(int userId) const {
    auto users = storage.get_all<User>(where(c(&User::id) == userId));
    if (users.empty()) return std::nullopt;
    return users.front();
}

bool UserManager::isUserActive(int userId) const {
    auto users = storage.get_all<User>(
        where(c(&User::id) == userId and c(&User::deleted_at) == 0)
    );
    return !users.empty();
}

std::string UserManager::getUserName(int userId) const {
    auto user = getUserById(userId);
    if (!user) {
        throw UserNotFoundException("ID " + std::to_string(userId));
    }
    return user->username;
}

} // namespace app
