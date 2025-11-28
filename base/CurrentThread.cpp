#include "CurrentThread.h"

#include <syscall.h>
#include <unistd.h>
#include <stdio.h>

namespace CurrentThread
{
__thread int t_cachedTid = 0;
__thread char t_tidString[32];
__thread int t_tidStringLength = 6;
__thread const char* t_threadName = "unknown";

void cacheTid()
{
  if (t_cachedTid == 0)
  {
    t_cachedTid = static_cast<pid_t>(syscall(SYS_gettid));
    t_tidStringLength = snprintf(t_tidString, sizeof t_tidString, "%5d ", t_cachedTid);
  }
}

bool isMainThread()
{
  return tid() == getpid();
}
}
