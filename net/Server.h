#ifndef SERVER_H
#define SERVER_H

#include "Channel.h"
#include "EventLoop.h"
#include "EventLoopThreadPool.h"
#include "HttpServer.h"

#define LISTENQ  4096

class Server {
 public:
  typedef std::function<void(EventLoop*)> ThreadInitCallback;
  Server(EventLoop* loop, const string port, const string name, int numThreads, bool reuseport);
  ~Server();
  EventLoop* getLoop() const { return serverLoop_; }
  void start();
  int socket_bind(const char *port, bool reuseport);
  void handleNewConn();

 private:
  void newConnection(const int connfd, const struct sockaddr *clientAddr);
  void removeConnection(const HttpServerPtr& conn);
  void removeConnectionInLoop(const HttpServerPtr& conn);
  
  typedef std::map<int, HttpServerPtr> ConnectionMap;

  bool started_;
  bool listening_;
  EventLoop* serverLoop_;
  const string port_;
  const string name_;
  int listenFd_;
  std::shared_ptr<EventLoopThreadPool> eventLoopThreadPool_;
  // 初始化 Loop 回调
  ThreadInitCallback threadInitCallback_;
  std::unique_ptr<Channel> acceptChannel_;

  ConnectionMap connections_;
  static const int MAXFDS = 100000;
};

#endif
