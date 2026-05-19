#pragma once
#include <stdexcept>
#include <string>

namespace app {

class AppException : public std::runtime_error {
public:
    explicit AppException(const std::string& msg) : std::runtime_error(msg) {}
};

class UserException : public AppException {
public:
    explicit UserException(const std::string& msg) : AppException(msg) {}
};

class UserNotFoundException : public UserException {
public:
    explicit UserNotFoundException(const std::string& identifier)
        : UserException("User not found: " + identifier) {}
};

class UserAlreadyExistsException : public UserException {
public:
    explicit UserAlreadyExistsException(const std::string& username)
        : UserException("User already exists: " + username) {}
};

class UserDeletedException : public UserException {
public:
    explicit UserDeletedException(const std::string& username)
        : UserException("User is deleted: " + username) {}
};

class AuthenticationException : public UserException {
public:
    AuthenticationException() : UserException("Invalid username or password") {}
};

class ClanException : public AppException {
public:
    explicit ClanException(const std::string& msg) : AppException(msg) {}
};

class ClanNotFoundException : public ClanException {
public:
    explicit ClanNotFoundException(int clanId)
        : ClanException("Clan not found: " + std::to_string(clanId)) {}
};

class ClanNameConflictException : public ClanException {
public:
    explicit ClanNameConflictException(const std::string& name)
        : ClanException("Clan name already in use: " + name) {}
};

class ClanNotActiveException : public ClanException {
public:
    explicit ClanNotActiveException(int clanId)
        : ClanException("Clan is not active: " + std::to_string(clanId)) {}
};

class MemberAlreadyInClanException : public ClanException {
public:
    explicit MemberAlreadyInClanException(int userId, int clanId)
        : ClanException("User " + std::to_string(userId) + " already in clan " + std::to_string(clanId)) {}
};

class NotMemberException : public ClanException {
public:
    explicit NotMemberException(int userId, int clanId)
        : ClanException("User " + std::to_string(userId) + " is not a member of clan " + std::to_string(clanId)) {}
};

class CannotRemoveLeaderException : public ClanException {
public:
    CannotRemoveLeaderException() : ClanException("Cannot remove the leader directly") {}
};

class LeaderNotInClanException : public ClanException {
public:
    explicit LeaderNotInClanException(int userId, int clanId)
        : ClanException("New leader " + std::to_string(userId) + " is not in clan " + std::to_string(clanId)) {}
};

class ChatException : public AppException {
public:
    explicit ChatException(const std::string& msg) : AppException(msg) {}
};

class InvalidMessageTargetException : public ChatException {
public:
    InvalidMessageTargetException() : ChatException("Invalid message target") {}
};

class NotMemberOfClanException : public ChatException {
public:
    explicit NotMemberOfClanException(int userId, int clanId)
        : ChatException("User " + std::to_string(userId) + " is not a member of clan " + std::to_string(clanId)) {}
};

} // namespace app
