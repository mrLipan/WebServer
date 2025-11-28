#ifndef LOGGING_H
#define LOGGING_H

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include <string>

#include "LogStream.h"

class AsyncLogging;

class Logger {
 public:
  enum LogLevel
  {
    INFO,
    WARN,
    ERROR,
    FATAL,
    NUM_LOG_LEVELS,
  };

  Logger(const char* fileName, int line);
  Logger(const char* fileName, int line, LogLevel level);
  Logger(const char* fileName, int line, bool toAbort);

  ~Logger();

  static LogLevel logLevel();
  static void setLogLevel(LogLevel level);

  LogStream& stream() { return impl_.stream_; }

  typedef void (*OutputFunc)(const char* msg, int len);
  typedef void (*FlushFunc)();
  static void setOutput(OutputFunc);
  static void setFlush(FlushFunc);

 private:
  class Impl {
   public:
    typedef Logger::LogLevel LogLevel;
    Impl(LogLevel level, int old_errno, const char* fileName, int line);
    void formatTime();

    LogStream stream_;
    LogLevel level_;
    int line_;
    std::string basename_;
  };
  Impl impl_;
};

extern Logger::LogLevel g_logLevel;

inline Logger::LogLevel Logger::logLevel() { return g_logLevel; }

#define LOG_INFO \
  if (Logger::logLevel() <= Logger::INFO) Logger(__FILE__, __LINE__).stream()
#define LOG_WARN Logger(__FILE__, __LINE__, Logger::WARN).stream()
#define LOG_ERROR Logger(__FILE__, __LINE__, Logger::ERROR).stream()
#define LOG_FATAL Logger(__FILE__, __LINE__, Logger::FATAL).stream()
#define LOG_SYSERR Logger(__FILE__, __LINE__, false).stream()
#define LOG_SYSFATAL Logger(__FILE__, __LINE__, true).stream()

const char* strerror_tl(int savedErrno);
void initAsyncLogger(const std::string& filename, off_t rollSize);

#endif  // LOGGING_H
