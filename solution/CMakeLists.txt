cmake_minimum_required(VERSION 3.11)

project(samples CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(game_server
	src/main.cpp
	src/http_server.cpp
	src/http_server.h
	src/sdk.h
	src/model.h
	src/model.cpp
	src/tagged.h
	src/boost_json.cpp
	src/json_loader.h
	src/json_loader.cpp
	src/request_handler.cpp
	src/request_handler.h
	src/player_tokens.h
	src/loot_generator.h
	src/loot_generator.cpp
	src/collision_detector.h
	src/collision_detector.cpp
	src/bag.h
	src/geom.h
)


target_include_directories(game_server PRIVATE CONAN_PKG::boost)
target_link_libraries(game_server PRIVATE CONAN_PKG::boost Threads::Threads)

cmake_minimum_required(VERSION 3.11)
project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)