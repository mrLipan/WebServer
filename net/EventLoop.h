#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#include <functional>
#include <iostream>
#include <memory>
#include <vector>

#include "Channel.h"
#include "Epoll.h"
#include "Util.h"
#include "base/CurrentThread.h"
#include "base/Logging.h"
#include "base/Thread.h"
using namespace std;

class EventLoop {
 public:
  typedef std::function<void()> Functor;
  typedef std::vector<Channel*> ChannelList;
  EventLoop();
  ~EventLoop();
  void loop();
  void quit();
  void runInLoop(Functor&& cb);
  void queueInLoop(Functor&& cb);

  bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }
  void assertInLoopThread() { assert(isInLoopThread()); }

  // 针对监听套接字的专属函数
  void shutdown(Channel* channel) { shutDownWR(channel->getFd()); }

  void updateChannel(Channel*);
  void removeChannel(Channel*);
  bool hasChannel(Channel*);

  void handleRead();

 private:
  void wakeup();
  void handleRead();
  void doPendingFunctors();

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
  std::vector<Functor> pendingFunctors_;
};

#endif
