set(CMAKE_BUILD_TYPE Release)
#set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

cmake_minimum_required(VERSION 3.12)
project(drynomore-telegram-bot CXX)

# Source & Header files
file(GLOB_RECURSE cppHeader CONFIGURE_DEPENDS
"include/*.hpp"
)

file(GLOB_RECURSE cppSrcs CONFIGURE_DEPENDS
"include/*.cpp"
"src/*.cpp"
)

# C++ telegram bot api library
add_subdirectory(lib/tgbot-cpp)

# Define the target executable
add_executable(${PROJECT_NAME} ${cppSrcs} ${cppHeader})

# Target specific compile flags
if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)
endif()

# Target specific include directories
target_include_directories(${PROJECT_NAME}
PRIVATE "include/"
)

target_link_libraries(${PROJECT_NAME} TgBot)

