#ifndef EVENTLOOPTHREADPOOL_H
#define EVENTLOOPTHREADPOOL_H

#include <memory>
#include <vector>
#include <functional>

#include "EventLoopThread.h"
#include "base/noncopyable.h"

class EventLoopThreadPool : noncopyable {
 public:
  typedef std::function<void(EventLoop*)> ThreadInitCallback;
  EventLoopThreadPool(EventLoop* baseLoop, const string& name, const int numThreads);

  ~EventLoopThreadPool();
  void start(const ThreadInitCallback& cb = ThreadInitCallback());

  EventLoop* getNextLoop();
  std::vector<EventLoop*> getAllLoops();

  bool started() const { return started_; }

  const string& name() const { return name_; }

 private:
  EventLoop* baseLoop_;
  string name_;
  bool started_;
  int numThreads_;
  int next_;
  std::vector<std::unique_ptr<EventLoopThread>> threads_;
  std::vector<EventLoop*> loops_;
};

#endif
