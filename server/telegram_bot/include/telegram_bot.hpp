#pragma once

#include <atomic>
#include <set>

#include "ts_queue.hpp"
#include "types.hpp"

void runDryNoMoreTelegramBot(const std::string &token,
                             const std::set<std::int64_t> &userWhitelist,
                             std::set<std::int64_t> &broadcastChats,
                             StateWrapper &state, ts_queue<Message> &msgQueue,
                             const std::atomic<bool> &running);
