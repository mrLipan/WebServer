#ifndef UTIL_H
#define UTIL_H

#include "base/Logging.h"

#define CHECK_NOTNULL(val) CheckNotNull(__FILE__, __LINE__, "'" #val "' Must be non NULL", (val))

template <typename T>
T* CheckNotNull(const char* file, int line, const char *names, T* ptr)
{
  if (ptr == NULL)
  {
   Logger(file, line, Logger::FATAL).stream() << names;
  }
  return ptr;
}

void handle_for_sigpipe();
void setSocketNoLinger(int fd);
bool setReuseAddr(int sockfd, bool on);
bool setReusePort(int sockfd, bool on);
bool setKeepAlive(int sockfd, bool on);
bool setNoDelay(int sockfd, bool on);

#endif  // UTIL_H
