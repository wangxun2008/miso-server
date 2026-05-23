cmake_minimum_required(VERSION 3.14)
project(miso_server)

set(CMAKE_CXX_STANDARD 17)

# sqlite_orm header-only，假设路径为 ../sqlite_orm/include
include_directories(include ../sqlite_orm/include)

# 链接 sqlite3
find_package(SQLite3 REQUIRED)

add_library(miso STATIC
    src/user_manager.cpp
    src/clan_manager.cpp
	src/chat_manager.cpp
	src/game_manager.cpp
	src/chunk_manager.cpp
	src/online_manager.cpp
	src/notice_manager.cpp
	src/topic_manager.cpp
	src/comment_manager.cpp
	src/mine_map_manager.cpp
	src/game_logic_manager.cpp
)
target_include_directories(miso PUBLIC include)
target_link_libraries(miso PUBLIC SQLite::SQLite3)

add_executable(main main.cpp)
target_link_libraries(main miso)

add_executable(run_tests tests.cpp)
target_link_libraries(run_tests miso)

# add_executable(minesweeper_game temp/minesweeper_game.cpp)
# target_link_libraries(minesweeper_game miso)
