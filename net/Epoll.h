#ifndef EPOLLER_H
#define EPOLLER_H

#include <map>
#include <vector>

#include "Channel.h"
#include "EventLoop.h"
#include "Timer.h"

class Epoll : noncopyable {
 public:
  typedef std::vector<Channel*> ChannelList;

  Epoll(EventLoop* loop);
  ~Epoll();

  void poll(int timeout, ChannelList* activeChannels);

  void updateChannel(Channel* channel);
  void removeChannel(Channel* channel);
  void add_timer(Channel *channel, int timeout);
  void handleExpired() { timerManager_.handleExpiredEvent(); }

  bool hasChannel(Channel* channel) const;

  void assertInLoopThread() const { loop_->assertInLoopThread(); }

 private:
  void update(int operation, Channel* channel);
  void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;

  typedef std::map<int, Channel*> ChannelMap;
  typedef std::vector<struct epoll_event> EventList;

  static const int kInitEventListSize = 64;
  EventLoop* loop_;
  int epollFd_;
  ChannelMap channels_;
  EventList events_;
  TimerManager timerManager_;
};

#endif
