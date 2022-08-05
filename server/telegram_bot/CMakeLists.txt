set(CMAKE_BUILD_TYPE Release)
#set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

cmake_minimum_required(VERSION 3.12)
project(drynomore-telegram-bot CXX)

set(Boost_USE_MULTITHREADED ON)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost COMPONENTS system REQUIRED)
find_package(CURL)
include_directories(/usr/local/include ${OPENSSL_INCLUDE_DIR} ${Boost_INCLUDE_DIR})
if (CURL_FOUND)
    include_directories(${CURL_INCLUDE_DIRS})
    add_definitions(-DHAVE_CURL)
endif()

# Source & Header files
file(GLOB_RECURSE cppHeader CONFIGURE_DEPENDS
"include/*.hpp"
)

file(GLOB_RECURSE cppSrcs CONFIGURE_DEPENDS
"include/*.cpp"
"src/*.cpp"
)

# Target specific compile flags
if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)
endif()

# C++ telegram bot api library
add_subdirectory(tgbot-cpp)

# Target specific include directories
target_include_directories(${PROJECT_NAME}
PRIVATE "include/"
)

add_executable(${PROJECT_NAME} ${cppSrcs} ${cppHeader})
target_link_libraries(${PROJECT_NAME} TgBot ${CMAKE_THREAD_LIBS_INIT} ${OPENSSL_LIBRARIES} ${Boost_LIBRARIES} ${CURL_LIBRARIES})

