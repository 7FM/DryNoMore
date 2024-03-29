set(CMAKE_BUILD_TYPE Release)
#set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

cmake_minimum_required(VERSION 3.12)
project(drynomore-telegram-bot CXX)

find_package (Threads REQUIRED)
find_package (yaml-cpp REQUIRED)

# Source & Header files
file(GLOB_RECURSE cppSrcs CONFIGURE_DEPENDS
"include/*.cpp"
"src/*.cpp"
)

# C++ telegram bot api library
#add_subdirectory(lib/tgbot-cpp)
find_package(Boost 1.65.1 COMPONENTS system REQUIRED) # Required for tgbot
find_package(TgBot REQUIRED)

# Define the target executable
add_executable(${PROJECT_NAME} ${cppSrcs})

# Target specific compile flags
if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)
endif()

# Target specific include directories
target_include_directories(${PROJECT_NAME}
PRIVATE "include"
${YAML_CPP_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT} ${YAML_CPP_LIBRARIES} TgBot::TgBot)

install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
