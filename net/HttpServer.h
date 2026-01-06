#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include "Buffer.h"

#include <memory>
#include <map>

enum HttpRequestParseState
{
  kExpectRequestLine,
  kExpectHeaders,
  kExpectBody,
  kFinish,
};

enum ConnectionState { kDisconnected, kConnecting, kConnected, kDisconnecting };

enum HttpMethod { kInvalid, kGet, kPost, kHead, kPut, kDelete };

enum HttpVersion { kvUnknown, kHttp10, kHttp11 };

enum HttpStatusCode
{
  ksUnknown,
  k200Ok = 200,
  k400BadRequest = 400,
  k404NotFound = 404,
};

class MimeType {
 private:
  static void init();
  static std::unordered_map<std::string, std::string> mime;
  MimeType();
  MimeType(const MimeType &m);

 public:
  static std::string getMime(const std::string &suffix);

 private:
  static pthread_once_t once_control;
};

class EventLoop;
class Channel;
class TimerNode;


class HttpServer : public std::enable_shared_from_this<HttpServer> {
 public:
  static const size_t kCheapPrepend = 8;
  static const size_t kInitialSize = 1024;
  typedef std::shared_ptr<HttpServer> HttpServerPtr;
  typedef std::function<void (const HttpServerPtr&)> CloseCallback;
  HttpServer(EventLoop *loop, int connfd);
  ~HttpServer();
  void reset();
  int getFd() { return connfd_; }
  void setCloseCallback(const CloseCallback& cb) { closeCallback_ = cb; }
  void seperateTimer();
  void timeoutClose() { handleClose(); }
  void linkTimer(std::shared_ptr<TimerNode> mtimer) { seperateTimer(); timer_ = mtimer; }
  EventLoop *getLoop() { return loop_; }
  bool setMethod(const char* start, const char* end);
  void addHeader(const char* start, const char* colon, const char* end);
  std::string getHeader(const std::string& field) const;

  void connectEstablished();
  void connectDestroyed();

  void send(const std::string_view& message);
  void send(Buffer* message);

  void shutDown();
  void shutDownInLoop();

 private:
  EventLoop *loop_;
  int connfd_;
  std::unique_ptr<Channel> channel_;
  Buffer inBuffer_;
  Buffer outBuffer_;
  
  HttpMethod method_;
  HttpVersion version_;
  std::string path_;
  std::string query_;
  std::map<std::string, std::string> headers_;
  std::string body_;
  HttpRequestParseState requestParseState_;
  ConnectionState connState_;
  std::weak_ptr<TimerNode> timer_;
  CloseCallback closeCallback_;

  void handleRead();
  void handleWrite();
  void handleClose();
  void handleError();
  void onMessage();
  void onRequest();
  void sendInLoop(const void* message, size_t len);
  
  bool parseRequest();
  bool parseRequestLine(const char* begin, const char* end);
  bool analysisRequest(bool close, Buffer *output);
};

typedef std::shared_ptr<HttpServer> HttpServerPtr;

#endif // HTTPSERVER_H
