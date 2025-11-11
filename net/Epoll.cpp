#include "Epoll.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#include <deque>
#include <iostream>
#include <queue>

#include "Util.h"
#include "base/Logging.h"
using namespace std;

const int EVENTSNUM = 4096;
const int EPOLLWAIT_TIME = 10000;

namespace {
const int kNew = -1;
const int kAdded = 1;
const int kDeleted = 2;
}  // namespace

Epoll::Epoll(EventLoop* loop)
    : loop_(loop),
      epollFd_(epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize) {
  assert(epollFd_ > 0);
}
Epoll::~Epoll() { close(epollFd_); }

void Epoll::updateChannel(Channel* channel) {
  assertInLoopThread();
  uint32_t events;
  const int fd = channel->getFd(), state = channel->getState();
  if (state == kNew || state == kDeleted) {
    if (state == kNew) {
      assert(channels_.find(fd) == channels_.end());
      channels_[fd] = channel;
    } else {
      assert(channels_.find(fd) != channels_.end());
      assert(channels_[fd] == channel);
    }
    channel->setState(kAdded);
    update(EPOLL_CTL_ADD, channel);
  } else {
    assert(state == kAdded);
    assert(channels_.find(fd) != channels_.end());
    assert(channels_[fd] == channel);
    if (channel->isNoneEvent()) {
      channel->setState(kDeleted);
      update(EPOLL_CTL_DEL, channel);
    } else {
      update(EPOLL_CTL_MOD, channel);
    }
  }
}

void Epoll::update(int operation, Channel* channel) {
  int fd = channel->getFd();
  struct epoll_event event;
  memset(&event, 0, sizeof(event));
  event.events = channel->getEvents();
  event.data.ptr = channel;
  if (epoll_ctl(epollFd_, operation, fd, &event) < 0) {
    // 日志
  }
}

void Epoll::removeChannel(Channel* channel) {
  assertInLoopThread();
  const int state = channel->getState(), fd = channel->getFd();
  assert(channels_.find(fd) != channels_.end());
  assert(channels_[fd] == channel);
  assert(channel->isNoneEvent());
  assert(state == kDeleted || state == kAdded);
  [[maybe_unused]] size_t n = channels_.erase(fd);
  assert(n == 1);
  if (state == kAdded) {
    update(EPOLL_CTL_DEL, channel);
  }
  channel->setState(kNew);
}

bool Epoll::hasChannel(Channel* channel) const {
  assertInLoopThread();
  ChannelMap::const_iterator it = channels_.find(channel->getFd());
  return it != channels_.end() && it->second == channel;
}

void Epoll::poll(int timeout, ChannelList* activeChannels) {
  assertInLoopThread();
  int numEvents;
  if ((numEvents = epoll_wait(epollFd_, &*events_.begin(),
                              static_cast<int>(events_.size()), timeout)) < 0) {
    if (errno != EINTR) {
      // error happened
    }
  } else if (numEvents > 0) {
    fillActiveChannels(numEvents, activeChannels);
    if (numEvents == static_cast<int>(events_.size()))
      events_.resize(events_.size() << 1);
  } else {
    // nothing happened
  }
}

void Epoll::fillActiveChannels(int numEvents,
                               ChannelList* activeChannels) const {
  int i;
  epoll_event event;
  Channel* channel;
  for (i = 0; i < numEvents; ++i) {
    event = events_[i];
    channel = static_cast<Channel*>(event.data.ptr);
    channel->setRevents(event.events);
    activeChannels->push_back(channel);
  }
}
