#ifndef EVENTLOOPTHREAD_H
#define EVENTLOOPTHREAD_H

#include "base/Thread.h"
#include "base/noncopyable.h"
#include "net/EventLoop.h"

class EventLoopThread : noncopyable {
 public:
  typedef std::function<void(EventLoop*)> ThreadInitCallback;

  EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(),
                  const string& name = string());
  ~EventLoopThread();
  EventLoop* startLoop();

 private:
  void threadFunc();
  EventLoop* loop_;
  bool exiting_;
  Thread thread_;
  MutexLock mutex_;
  Condition cond_;
  ThreadInitCallback callback_;
};

#endif
