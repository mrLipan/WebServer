#include "Timer.h"

#include <sys/time.h>

TimerNode::TimerNode(HttpServerPtr httpServer, int timeout)
    : deleted_(false), httpServer_(httpServer) {
  struct timeval now;
  gettimeofday(&now, NULL);
  expiredTime_ = (((now.tv_sec % 10000) * 1000) + (now.tv_usec / 1000)) + timeout;
}

TimerNode::~TimerNode()
{
  if (httpServer_) httpServer_->timeoutClose();
}

TimerNode::TimerNode(TimerNode &tn)
    : httpServer_(tn.httpServer_), expiredTime_(0) {}

void TimerNode::update(int timeout)
{
  struct timeval now;
  gettimeofday(&now, NULL);
  expiredTime_ = (((now.tv_sec % 10000) * 1000) + (now.tv_usec / 1000)) + timeout;
}

bool TimerNode::isValid()
{
  bool ok = true;
  struct timeval now;
  gettimeofday(&now, NULL);
  size_t temp = (((now.tv_sec % 10000) * 1000) + (now.tv_usec / 1000));
  if (temp >= expiredTime_)
  {
    setDeleted();
    ok = false;
  }
  return ok;
}

void TimerNode::clear()
{
  httpServer_.reset();
  setDeleted();
}

TimerManager::TimerManager() {}

TimerManager::~TimerManager() {}

void TimerManager::addTimer(HttpServerPtr httpServer_, int timeout)
{
  TimerNodePtr new_node(new TimerNode(httpServer_, timeout));
  timerNodeQueue.push(new_node);
  httpServer_->linkTimer(new_node);
}

void TimerManager::handleExpiredEvent()
{
  while (!timerNodeQueue.empty()) {
    TimerNodePtr ptimer_now = timerNodeQueue.top();
    if (ptimer_now->isDeleted())
    {
      timerNodeQueue.pop();
    }
    else if (!ptimer_now->isValid())
    {
      timerNodeQueue.pop();
    }
    else break;
  }
}
