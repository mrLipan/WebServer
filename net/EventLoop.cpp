#include "EventLoop.h"

#include <sys/epoll.h>
#include <sys/eventfd.h>

#include <algorithm>
#include <iostream>

#include "Util.h"
#include "base/Logging.h"

using namespace std;

namespace {
__thread EventLoop* t_loopInThisThread = 0;

const int kPollTimeMs = 10000;

int createEventfd() {
  int evtfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (evtfd < 0) {
    // 日志
    abort();
  }
  return evtfd;
}

}  // namespace

EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      eventHandling_(false),
      callingPendingFunctors_(false),
      threadId_(CurrentThread::tid()),
      poller_(new Epoll(this)),
      wakeupFd_(createEventfd()),
      wakeupChannel_(new Channel(this, wakeupFd_)),
      currentActiveChannel_(NULL) {
  if (t_loopInThisThread) {
    // LOG << "Another EventLoop " << t_loopInThisThread << " exists in this
    // thread " << threadId_;
  } else {
    t_loopInThisThread = this;
  }
  wakeupChannel_->setReadHandler(std::bind(&EventLoop::handleRead, this));
  wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
  wakeupChannel_->disableAll();
  wakeupChannel_->remove();
  close(wakeupFd_);
  t_loopInThisThread = NULL;
}

void EventLoop::loop() {
  assertInLoopThread();
  assert(!looping_);
  looping_ = true;
  quit_ = false;

  while (!quit_) {
    activeChannels_.clear();
    poller_->poll(kPollTimeMs, &activeChannels_);
    eventHandling_ = true;
    for (Channel* channel : activeChannels_) {
      currentActiveChannel_ = channel;
      currentActiveChannel_->handleEvents();
    }
    currentActiveChannel_ = NULL;
    eventHandling_ = false;
    doPendingFunctors();
  }
  looping_ = false;
}

void EventLoop::quit() {
  // 一般由其他线程调用，当前线程在 loop
  quit_ = true;

  if (!isInLoopThread()) {
    // 触发 wakeupFd_ 读事件
    wakeup();
  }
}

void EventLoop::runInLoop(Functor&& cb) {
  if (isInLoopThread()) {
    cb();
  } else {
    queueInLoop(std::move(cb));
  }
}

void EventLoop::queueInLoop(Functor&& cb) {
  {
    MutexLockGuard lock(mutex_);
    pendingFunctors_.push_back(cb);
  }

  if (!isInLoopThread() || callingPendingFunctors_) {
    wakeup();
  }
}

void EventLoop::updateChannel(Channel* channel) {
  assertInLoopThread();
  assert(channel->getLoop() == this);
  poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel) {
  assertInLoopThread();
  assert(channel->getLoop() == this);
  if (eventHandling_) {
    // 确保删除的不是未处理的 channel，或者是当前通道正在处理的是 close
    assert(currentActiveChannel_ == channel ||
           std::find(activeChannels_.begin(), activeChannels_.end(), channel) ==
               activeChannels_.end());
  }
  poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel* channel) {
  assertInLoopThread();
  assert(channel->getLoop() == this);
  return poller_->hasChannel(channel);
}

void EventLoop::doPendingFunctors() {
  std::vector<Functor> functors;
  callingPendingFunctors_ = true;

  {
    MutexLockGuard lock(mutex_);
    functors.swap(pendingFunctors_);
  }

  for (const Functor& functor : functors) {
    functor();
  }
  callingPendingFunctors_ = false;
}

void EventLoop::handleRead() {
  uint64_t one = 1;
  ssize_t n = read(wakeupFd_, &one, sizeof(one));
  if (n != sizeof(one)) {
    // 日志
  }
}
