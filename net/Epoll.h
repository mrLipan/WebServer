#ifndef EPOLLER_H
#define EPOLLER_H

#include <sys/epoll.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include "Channel.h"
#include "EventLoop.h"
#include "HttpData.h"
#include "Timer.h"

class Epoll : noncopyable {
 public:
  typedef std::vector<Channel*> ChannelList;
  typedef std::map<int, Channel*> ChannelMap;
  typedef std::vector<struct epoll_event> EventList;

  Epoll(EventLoop* Loop);
  ~Epoll();

  void poll(int timeout, ChannelList* activeChannels);

  void updateChannel(Channel* channel);
  void removeChannel(Channel* channel);

  bool hasChannel(Channel* channel) const;

  void assertInLoopThread() const { loop_->assertInLoopThread(); }

 private:
  void update(int operation, Channel* channel);
  void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;

  static const int kInitEventListSize = 64;
  EventLoop* loop_;
  int epollFd_;
  ChannelMap channels_;
  EventList events_;
};

#endif
