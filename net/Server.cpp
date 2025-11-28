#include "Server.h"
#include "Util.h"
#include "base/Logging.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>

Server::Server(EventLoop* loop, const string port,
               const string name, int numThreads, bool reuseport)
    : serverLoop_(CHECK_NOTNULL(loop)),
      port_(port),
      name_(name),
      listenFd_(socket_bind(port_.c_str(), reuseport)),
      eventLoopThreadPool_(new EventLoopThreadPool(serverLoop_, name, numThreads)),
      started_(false),
      listening_(false),
      acceptChannel_(new Channel(serverLoop_, listenFd_))
{
  handle_for_sigpipe();
  acceptChannel_->setReadHandler(std::bind(&Server::handleNewConn, this));
}

Server::~Server()
{
  serverLoop_->assertInLoopThread();

  for (auto& item : connections_)
  {
    HttpServerPtr conn(item.second);
    item.second.reset();
    conn->getLoop()->runInLoop(
      std::bind(&HttpServer::connectDestroyed, conn));
  }
}

void Server::start()
{
  assert(!started_);
  assert(!listening_);
  started_ = true;
  listening_ = true;
  int ret = listen(listenFd_, LISTENQ);
  if (ret < 0)
  {
    LOG_SYSFATAL << "Server::start";
  }
  eventLoopThreadPool_->start(threadInitCallback_);
  acceptChannel_->setET();
  acceptChannel_->enableReading();
}

int Server::socket_bind(const char* port, bool reuseport)
{
  serverLoop_->assertInLoopThread();
  struct addrinfo hints, *listp, *p;
  int listenfd;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_ADDRCONFIG | AI_PASSIVE | AI_NUMERICSERV;
  if (getaddrinfo(NULL, port, &hints, &listp) != 0)
  {
    LOG_SYSFATAL << "Server::socket_bind";
  }

  for (p = listp; p; p = p->ai_next) {
    if ((listenfd =
             socket(p->ai_family, p->ai_socktype | SOCK_NONBLOCK | SOCK_CLOEXEC,
                    p->ai_protocol)) < 0)
      continue;

    if (!setReuseAddr(listenfd, true) || !setReusePort(listenfd, reuseport)) continue; 

    if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0) break;

    close(listenfd);
  }

  freeaddrinfo(listp);
  if (!p)
  {
    LOG_SYSFATAL << "Server::socket_bind";
  }

  return listenfd;
}

void Server::handleNewConn()
{
  serverLoop_->assertInLoopThread();
  assert(listening_);
  struct sockaddr clientAddr;
  memset(&clientAddr, 0, sizeof(struct sockaddr));
  socklen_t addrLen = sizeof(clientAddr);
  int connfd;
  while ((connfd = accept4(listenFd_, &clientAddr, &addrLen, SOCK_NONBLOCK | SOCK_CLOEXEC)) > 0)
  {
     if (connfd >= MAXFDS)
     {
      close(connfd);
      continue;
    }
    setNoDelay(connfd, true);
    newConnection(connfd, &clientAddr);
  }
  int savedErrno = errno;
  switch (savedErrno)
  {
    case EAGAIN:
    case ECONNABORTED:
    case EINTR:
    case EPROTO:
    case EPERM:
    case EMFILE:
      break;
    case EBADF:
    case EFAULT:
    case EINVAL:
    case ENFILE:
    case ENOBUFS:
    case ENOMEM:
    case ENOTSOCK:
    case EOPNOTSUPP:
      // unexpected errors
      LOG_FATAL << "unexpected error of accept4 " << savedErrno;
      break;
    default:
      LOG_FATAL << "unknown error of accept4 " << savedErrno;
      break;
  }
}

void Server::newConnection(const int connfd, const struct sockaddr *clientAddr)
{
  serverLoop_->assertInLoopThread();
  EventLoop* loop = eventLoopThreadPool_->getNextLoop();
  char ip[INET6_ADDRSTRLEN];
  uint16_t port;
  if (clientAddr->sa_family == AF_INET)
  {
      struct sockaddr_in *clientAddrIn = (struct sockaddr_in *)clientAddr;
      inet_ntop(AF_INET, &clientAddrIn->sin_addr, ip, sizeof(ip));
      port = ntohs(clientAddrIn->sin_port);
  } 
  else if (clientAddr->sa_family == AF_INET6)
  {
      struct sockaddr_in6 *clientAddrIn6 = (struct sockaddr_in6 *)clientAddr;
      inet_ntop(AF_INET6, &clientAddrIn6->sin6_addr, ip, sizeof(ip));
      port = ntohs(clientAddrIn6->sin6_port);
  }
  LOG_INFO << "Server::newConnection [" << name_
           << "] - new connection from " << ip << ":"
           << port;

  std::shared_ptr<HttpServer> conn(new HttpServer(loop, connfd));
  connections_[connfd] = conn;
  loop->runInLoop(std::bind(&HttpServer::connectEstablished, conn));
}

void Server::removeConnection(const HttpServerPtr& conn)
{
  serverLoop_->runInLoop(std::bind(&Server::removeConnectionInLoop, this, conn));
}

void Server::removeConnectionInLoop(const HttpServerPtr& conn)
{
  serverLoop_->assertInLoopThread();
  size_t n = connections_.erase(conn->getFd());
  (void)n;
  assert(n == 1);
  EventLoop* Loop = conn->getLoop();
  Loop->queueInLoop(std::bind(&HttpServer::connectDestroyed, conn));
}
