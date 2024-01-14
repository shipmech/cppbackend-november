cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

#find_package(Boost 1.78.0 REQUIRED)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(GameServerLib
	src/http_server.cpp
	src/http_server.h
	src/sdk.h
	src/model.h
	src/model.cpp
	src/util.h
	src/util.cpp
	src/boost_json.cpp
	src/json_loader.h
	src/json_loader.cpp
	src/request_handler.cpp
	src/request_handler.h
    src/server_logger.cpp
    src/server_logger.h
    src/app.h
    src/app.cpp
    src/api_handler.h
    src/api_handler.cpp
    src/map_parser.h
    src/map_parser.cpp
	src/extra_data.h
	src/loot_generator.h
	src/loot_generator.cpp
	src/geom.h
	src/collision_detector.h
	src/collision_detector.cpp
    src/serialization.h
	src/postgres.h
	src/postgres.cpp
	src/retired_players.h
)

target_link_libraries(GameServerLib PUBLIC CONAN_PKG::boost CONAN_PKG::libpqxx Threads::Threads)


add_executable(game_server src/main.cpp)
add_executable(game_server_tests
    tests/loot_generator_tests.cpp
	src/loot_generator.h
	src/loot_generator.cpp
)
add_executable(collision_detection_tests
    tests/collision-detector-tests.cpp
)

target_link_libraries(game_server PRIVATE GameServerLib)
target_link_libraries(game_server_tests PRIVATE CONAN_PKG::catch2 GameServerLib)
target_link_libraries(collision_detection_tests CONAN_PKG::catch2 GameServerLib)
