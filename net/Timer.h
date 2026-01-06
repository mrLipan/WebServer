#ifndef TIMER_H
#define TIMER_H

#include <unistd.h>
#include <deque>
#include <memory>
#include <queue>

#include "HttpServer.h"

class TimerNode {
 public:
  TimerNode(HttpServerPtr requestServer, int timeout);
  ~TimerNode();
  TimerNode(TimerNode &tn);
  void update(int timeout);
  bool isValid();
  void clear();
  void setDeleted() { deleted_ = true; }
  bool isDeleted() const { return deleted_; }
  size_t getExpTime() const { return expiredTime_; }

 private:
  bool deleted_;
  size_t expiredTime_;
  HttpServerPtr httpServer_;
};

struct TimerCmp {
  bool operator()(std::shared_ptr<TimerNode> &a,
                  std::shared_ptr<TimerNode> &b) const {
    return a->getExpTime() > b->getExpTime();
  }
};

class TimerManager {
 public:
  TimerManager();
  ~TimerManager();
  void addTimer(std::shared_ptr<HttpServer> SPHttpServer, int timeout);
  void handleExpiredEvent();

 private:
  typedef std::shared_ptr<TimerNode> TimerNodePtr;
  std::priority_queue<TimerNodePtr, std::deque<TimerNodePtr>, TimerCmp> timerNodeQueue;
};

#endif  // TIMER_H
