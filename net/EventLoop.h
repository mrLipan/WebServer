#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#include "base/CurrentThread.h"
#include "base/Mutex.h"

#include <assert.h>
#include <functional>
#include <memory>

class Channel;
class Epoll;

using namespace std;

class EventLoop {
 public:
  typedef std::function<void()> Functor;
  EventLoop();
  ~EventLoop();
  void loop();
  void quit();
  void runInLoop(Functor&& cb);
  void queueInLoop(Functor&& cb);

  bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }
  void assertInLoopThread() { assert(isInLoopThread()); }

  void updateChannel(Channel*);
  void removeChannel(Channel*);
  bool hasChannel(Channel*);

 private:
  void wakeup();
  void handleRead();
  void doPendingFunctors();

  typedef std::vector<Channel*> ChannelList;

  bool looping_;
  bool quit_;
  bool eventHandling_;
  bool callingPendingFunctors_;
  const pid_t threadId_;
  std::unique_ptr<Epoll> poller_;

  int wakeupFd_;
  std::unique_ptr<Channel> wakeupChannel_;

  ChannelList activeChannels_;
  Channel* currentActiveChannel_;

  mutable MutexLock mutex_;
  // 其他线程线程对当前 Loop 的线程安全调用任务队列
  std::vector<Functor> pendingFunctors_;
};

#endif
