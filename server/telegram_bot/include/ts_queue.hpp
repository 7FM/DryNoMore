#pragma once

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>

template <class T>
class ts_queue {
private:
  struct node {
    T data;
    std::unique_ptr<node> next;

    node() {}
  };

private:
  // Utility methods without lock
  const node *get_tail() const {
    std::scoped_lock tail_lock(tail_mutex);
    return tail;
  }
  const std::unique_ptr<node> pop_head() {
    std::unique_ptr<node> old_head = std::move(head);
    head = std::move(old_head->next);
    return old_head;
  }

  // Utility methods with locks
  std::unique_ptr<node> try_pop_head() {
    std::scoped_lock head_lock(head_mutex);
    if (head.get() == get_tail())
      return std::unique_ptr<node>();
    return pop_head();
  }

  bool limited_wait_for_data(std::unique_lock<std::mutex> &head_lock) const {
    return data_cond.wait_for(head_lock, std::chrono::milliseconds(500),
                              [&] { return head.get() != get_tail(); });
  }

public:
  ts_queue() : head(new node), tail(head.get()) {}
  ts_queue(const ts_queue &other) = delete;
  ts_queue &operator=(const ts_queue &) = delete;

  std::optional<T> try_pop() {
    const std::unique_ptr<node> old_head = try_pop_head();
    return old_head ? old_head->data : std::nullopt;
  }

  std::optional<T> limited_wait_for_pop() {
    std::unique_lock head_lock(head_mutex);
    bool dataAvailable = limited_wait_for_data(head_lock);
    if (!dataAvailable) {
      return std::nullopt;
    }
    return pop_head()->data;
  }

  void push(T &&new_value) {
    std::unique_ptr<node> p(new node());
    {
      node *new_tail = p.get();
      std::scoped_lock tail_lock(tail_mutex);
      tail->data = std::move(new_value);
      tail->next = std::move(p);
      tail = new_tail;
    }
    data_cond.notify_one();
  }

  bool empty() const {
    std::scoped_lock head_lock(head_mutex);
    return head.get() == get_tail();
  }

private:
  mutable std::mutex head_mutex;
  std::unique_ptr<node> head;

  mutable std::mutex tail_mutex;
  node *tail;

  mutable std::condition_variable data_cond;
};
