#ifndef CHANNEL_H
#define CHANNEL_H

#include "base/noncopyable.h"

#include <sys/epoll.h>
#include <functional>
#include <memory>

class EventLoop;

class Channel : noncopyable {
 public:
  typedef std::function<void()> EventCallback;

  Channel(EventLoop* loop, int fd);
  ~Channel();

  EventLoop* getLoop() { return loop_; }
  int getFd() { return fd_; };
  uint32_t getEvents() { return events_; }

  // set event
  void enableReading() {
    events_ |= (EPOLLIN | EPOLLPRI);
    update();
  }
  void disableReading() {
    events_ &= ~(EPOLLIN | EPOLLPRI);
    update();
  }
  void enableWriting() {
    events_ |= EPOLLOUT;
    update();
  }
  void disableWriting() {
    events_ &= ~EPOLLOUT;
    update();
  }
  void setET() {
    events_ |= EPOLLET;
    update();
  }
  void disableAll() {
    events_ = 0;
    update();
  }

  void setRevents(int revt) { revents_ = revt; }
  int getState() { return state_; }
  void setState(int state) { state_ = state; }
  void setTie(const std::shared_ptr<void>& obj) {
    tie_ = obj;
    tied_ = true;
  }
  std::shared_ptr<void> getTie() { return tie_.lock(); }

  void setReadHandler(EventCallback&& cb) { readHandler_ = cb; }
  void setWriteHandler(EventCallback&& cb) { writeHandler_ = cb; }
  void setErrorHandler(EventCallback&& cb) { errorHandler_ = cb; }
  void setCloseHandler(EventCallback&& cb) { closeHandler_ = cb; }

  // handle event
  void handleEvents();
  void handleRead();
  void handleWrite();
  void handleError();
  void handleClose();

  bool isNoneEvent() const { return events_ == 0; }
  bool isWriting() const { return events_ & EPOLLOUT; }
  bool isReading() const { return events_ & (EPOLLIN | EPOLLPRI); }

  void remove();

 private:
  void update();
  void handleEventWithGuard();

  EventLoop* loop_;
  int fd_;
  uint32_t events_;
  uint32_t revents_;
  int state_;

  // 连接的一个 weak_ptr 指针，确保在处理事件时连接不被销毁
  std::weak_ptr<void> tie_;
  bool tied_;
  bool eventHandling_;  // 是否有事件在处理
  bool addedToLoop_;

  EventCallback readHandler_;
  EventCallback writeHandler_;
  EventCallback errorHandler_;
  EventCallback closeHandler_;
};

#endif
