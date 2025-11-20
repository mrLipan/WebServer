#ifndef ASYNCLOGGING_H
#define ASYNCLOGGING_H

#include <atomic>
#include <functional>
#include <string>
#include <vector>

#include "CountDownLatch.h"
#include "LogStream.h"
#include "Mutex.h"
#include "Thread.h"
#include "noncopyable.h"

class AsyncLogging : noncopyable {
 public:
  AsyncLogging(const std::string& basename, off_t rollSize,
               int flushInterval = 3);
  ~AsyncLogging() {
    if (running_) stop();
  }
  void append(const char* logline, int len);

  void start() {
    running_ = true;
    thread_.start();
    latch_.wait();
  }

  void stop() {
    running_ = false;
    cond_.notify();
    thread_.join();
  }

 private:
  void threadFunc();
  typedef FixedBuffer<kLargeBuffer> Buffer;
  typedef std::vector<std::unique_ptr<Buffer>> BufferVector;
  typedef std::unique_ptr<Buffer> BufferPtr;

  const int flushInterval_;
  std::atomic<bool> running_;
  const std::string basename_;
  const off_t rollSize_;
  Thread thread_;
  MutexLock mutex_;
  Condition cond_;
  BufferPtr currentBuffer_;
  BufferPtr nextBuffer_;
  BufferVector buffers_;
  CountDownLatch latch_;
};

#endif  // ASYNCLOGGING_H

