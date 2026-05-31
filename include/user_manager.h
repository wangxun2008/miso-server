#pragma once
#include "storage.h"
#include "exceptions.h"
#include <vector>
#include <string>
#include <optional>

namespace miso {

class UserManager {
public:
    explicit UserManager(Storage& storage);

    int registerUser(const std::string& username, const std::string& password);
    User login(const std::string& username, const std::string& password);
    void deleteUser(const std::string& username);
    void restoreUser(const std::string& username);
    void permanentDeleteUser(const std::string& username);
    void renameUser(const std::string& oldUsername, const std::string& newUsername);
    void changePassword(const std::string& username,
                        const std::string& oldPassword,
                        const std::string& newPassword);

    std::vector<User> getActiveUsers() const;
    std::vector<User> getAllUsers() const;
    std::optional<User> getUserById(int userId) const;
    bool isUserActive(int userId) const;
    std::string getUserName(int userId) const;

private:
    Storage& storage;
};

} // namespace miso
