#include "Util.h"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>

void handle_for_sigpipe() {
  struct sigaction sa;
  memset(&sa, '\0', sizeof(sa));
  sa.sa_handler = SIG_IGN;
  sa.sa_flags = 0;
  if (sigaction(SIGPIPE, &sa, NULL)) return;
}

bool setReuseAddr(int sockfd, bool on)
{
  int optval = on ? 1 : 0;
  int ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval,
                       static_cast<socklen_t>(sizeof(optval)));
  return ret == 0 || !on;
}

bool setReusePort(int sockfd, bool on)
{
#ifdef SO_REUSEPORT
  int optval = on ? 1 : 0;
  int ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &optval,
                       static_cast<socklen_t>(sizeof(optval)));
  return ret == 0 || !on;
#else
  return !on;
#endif
}

bool setKeepAlive(int sockfd, bool on)
{
  int optval = on ? 1 : 0;
  int ret = setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE,
                       &optval, static_cast<socklen_t>(sizeof optval));
  return ret == 0 || !on;
}

bool setNoDelay(int sockfd, bool on)
{
  int optval = on ? 1 : 0;
  int ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, 
                       &optval, static_cast<socklen_t>(sizeof optval));

  return ret == 0 || !on;
}
