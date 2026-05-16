cmake_minimum_required(VERSION 3.10)
project(miso_server LANGUAGES CXX)

find_package(SqliteOrm CONFIG REQUIRED)

add_executable(main main.cpp)

target_link_libraries(main PRIVATE sqlite_orm::sqlite_orm)

set_property(TARGET main PROPERTY CXX_STANDARD 17)
