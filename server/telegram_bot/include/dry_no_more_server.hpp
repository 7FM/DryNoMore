#pragma once

#include <atomic>

#include "ts_queue.hpp"
#include "types.hpp"

void runDryNoMoreStatusServer(int fd, StateWrapper &state,
                              ts_queue<Message> &msgQueue,
                              const std::atomic<bool> &running);
