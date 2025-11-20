#include "Logging.h"

#include <assert.h>

#include <iostream>

#include "AsyncLogging.h"
#include "CurrentThread.h"
#include "Thread.h"

__thread char t_errnobuf[512];

const char* strerror_tl(int savedErrno) {
  return strerror_r(savedErrno, t_errnobuf, sizeof t_errnobuf);
}

void defaultOutput(const char* msg, int len) {
  [[maybe_unused]] size_t n = fwrite(msg, 1, len, stdout);
}

void defaultFlush() { fflush(stdout); }

Logger::LogLevel g_logLevel = Logger::INFO;
Logger::OutputFunc g_output = defaultOutput;
Logger::FlushFunc g_flush = defaultFlush;
static std::unique_ptr<AsyncLogging> g_asyncLogger;

void initAsyncLogger(const std::string& filename) {
  g_asyncLogger.reset(new AsyncLogging(filename));
  g_asyncLogger->start();
  Logger::setOutput(asyncOutput);
}

void asyncOutput(const char* msg, int len) {
  if (g_asyncLogger) {
    g_asyncLogger->append(msg, len);
  } else {
    defaultOutput(msg, len);
  }
}

const char* LogLevelName[Logger::NUM_LOG_LEVELS] = {
    "INFO  ",
    "WARN  ",
    "ERROR ",
    "FATAL ",
};

Logger::Impl::Impl(LogLevel level, int savedErrno, const char* fileName,
                   int line)
    : stream_(), level_(level), line_(line), basename_(fileName) {
  formatTime();
  CurrentThread::tid();
  stream_ << CurrentThread::tidString();
  stream_ << LogLevelName[level];
  if (savedErrno != 0) {
    stream_ << strerror_tl(savedErrno) << " (errno=" << savedErrno << ") ";
  }
}

void Logger::Impl::formatTime() {
  time_t now = time(NULL);
  char str_t[26] = {0};
  struct tm* p_time = localtime(&now);
  strftime(str_t, 26, "%F %T\n", p_time);
  stream_ << str_t;
}

Logger::Logger(const char* fileName, int line)
    : impl_(INFO, 0, fileName, line) {}

Logger::Logger(const char* fileName, int line, LogLevel level)
    : impl_(level, 0, fileName, line) {}

Logger::Logger(const char* fileName, int line, bool toAbort)
    : impl_(toAbort ? FATAL : ERROR, errno, fileName, line) {}

Logger::~Logger() {
  impl_.stream_ << " -- " << impl_.basename_ << ':' << impl_.line_ << '\n';
  const LogStream::Buffer& buf(stream().buffer());
  g_output(buf.data(), buf.length());
  if (impl_.level_ == FATAL) {
    g_flush();
    abort();
  }
}

void Logger::setLogLevel(Logger::LogLevel level) { g_logLevel = level; }

void Logger::setOutput(OutputFunc out) { g_output = out; }

void Logger::setFlush(FlushFunc flush) { g_flush = flush; }

