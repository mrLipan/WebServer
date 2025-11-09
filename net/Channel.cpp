#include "Channel.h"

#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <queue>

#include "Epoll.h"
#include "EventLoop.h"
#include "Util.h"

using namespace std;

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop),
      fd_(0),
      events_(0),
      revents_(0),
      state_(-1),
      tied_(false),
      eventHandling_(false),
      addedToLoop_(false) {}

Channel::~Channel() {
  assert(!eventHandling_);
  assert(!addedToLoop_);
  if (loop_->isInLoopThread()) {
    assert(!loop_->hasChannel(this));
  }
}

void Channel::update() {
  addedToLoop_ = true;
  loop_->updateChannel(this);
}

void Channel::remove() {
  assert(isNoneEvent());
  addedToLoop_ = false;
  loop_->removeChannel(this);
}

void Channel::handleEvents() {
  std::shared_ptr<void> guard;
  if (tied_) {
    guard = tie_.lock();
    if (guard) handleEventWithGuard();
  } else
    handleEventWithGuard();
}

void Channel::handleEventWithGuard() {
  eventHandling_ = true;
  if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
    handleClose();
  }

  if (revents_ & EPOLLERR) {
    handleError();
  }
  if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
    handleRead();
  }
  if (revents_ & EPOLLOUT) {
    handleWrite();
  }
  eventHandling_ = false;
}

void Channel::handleRead() {
  if (readHandler_) readHandler_();
}

void Channel::handleWrite() {
  if (writeHandler_) writeHandler_();
}

void Channel::handleClose() {
  if (closeHandler_) closeHandler_();
}

void Channel::handleError() {
  if (errorHandler_) errorHandler_();
}
