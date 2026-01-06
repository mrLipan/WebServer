#include "HttpServer.h"

#include "Channel.h"
#include "EventLoop.h"
#include "Util.h"
#include "base/Logging.h"
#include "Timer.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>

using namespace std;

pthread_once_t MimeType::once_control = PTHREAD_ONCE_INIT;
std::unordered_map<std::string, std::string> MimeType::mime;

const int DEFAULT_KEEP_ALIVE_TIME = 5 * 60 * 1000;  // ms

const string source = "./source";

void MimeType::init() {
  mime[".html"] = "text/html";
  mime[".avi"] = "video/x-msvideo";
  mime[".bmp"] = "image/bmp";
  mime[".c"] = "text/plain";
  mime[".doc"] = "application/msword";
  mime[".gif"] = "image/gif";
  mime[".gz"] = "application/x-gzip";
  mime[".htm"] = "text/html";
  mime[".ico"] = "image/x-icon";
  mime[".jpg"] = "image/jpeg";
  mime[".png"] = "image/png";
  mime[".txt"] = "text/plain";
  mime[".mp3"] = "audio/mp3";
  mime["default"] = "text/html";
}

std::string MimeType::getMime(const std::string &suffix) {
  pthread_once(&once_control, MimeType::init);
  if (mime.find(suffix) == mime.end())
    return mime["default"];
  else
    return mime[suffix];
}

HttpServer::HttpServer(EventLoop *loop, int connfd)
    : loop_(CHECK_NOTNULL(loop)),
      connfd_(connfd),
      channel_(new Channel(loop, connfd)),
      connState_(kConnecting),
      requestParseState_(kExpectRequestLine),
      method_(kInvalid),
      version_(kvUnknown) {
  channel_->setReadHandler(bind(&HttpServer::handleRead, this));
  channel_->setWriteHandler(bind(&HttpServer::handleWrite, this));
  channel_->setCloseHandler(bind(&HttpServer::handleClose, this));
  channel_->setErrorHandler(std::bind(&HttpServer::handleError, this));
  setKeepAlive(connfd, true);
}

HttpServer::~HttpServer()
{
  assert(connState_== kDisconnected);
}

void HttpServer::reset()
{
  requestParseState_ = kExpectRequestLine;
  method_ = kInvalid;
  version_ = kvUnknown;
  path_.clear();
  query_.clear();
  headers_.clear();
  body_.clear();
  seperateTimer();
}

void HttpServer::seperateTimer()
{
  if (timer_.lock())
  {
    shared_ptr<TimerNode> my_timer(timer_.lock());
    my_timer->clear();
    timer_.reset();
  }
}

void HttpServer::send(const std::string_view& message)
{
  if (connState_ == kConnected)
  {
    if (loop_->isInLoopThread())
    {
      sendInLoop(message.data(), message.size());
    }
    else
    {
      void (HttpServer::*fp)(const void* data, size_t len) = &HttpServer::sendInLoop;
      loop_->runInLoop(
          std::bind(fp,
                    this,     // FIXME
                    message.data(), message.size()));
                    //std::forward<string>(message)));
    }
  }
}

void HttpServer::send(Buffer* buf)
{
  if (connState_ == kConnected)
  {
    if (loop_->isInLoopThread())
    {
      sendInLoop(buf->peek(), buf->readableBytes());
      buf->retrieveAll();
    }
    else
    {
      void (HttpServer::*fp)(const void* data, size_t len) = &HttpServer::sendInLoop;
      loop_->runInLoop(
          std::bind(fp,
                    this,     // FIXME
                    buf->peek(), buf->readableBytes()));
                    //std::forward<string>(message)));
    }
  }
}

void HttpServer::sendInLoop(const void* data, size_t len)
{
  loop_->assertInLoopThread();
  ssize_t nwrote = 0;
  size_t remaining = len;
  bool faultError = false;
  if (connState_ == kDisconnected)
  {
    LOG_WARN << "disconnected, give up writing";
    return;
  }
  // if no thing in output queue, try writing directly
  if (!channel_->isWriting() && outBuffer_.readableBytes() == 0)
  {
    nwrote = write(channel_->getFd(), data, len);
    if (nwrote >= 0)
    {
      remaining = len - nwrote;
    }
    else // nwrote < 0
    {
      nwrote = 0;
      if (errno != EAGAIN)
      {
        LOG_SYSERR << "TcpConnection::sendInLoop";
        if (errno == EPIPE || errno == ECONNRESET) // FIXME: any others?
        {
          faultError = true;
        }
      }
    }
  }

  assert(remaining <= len);
  if (!faultError && remaining > 0)
  {
    outBuffer_.append(static_cast<const char*>(data)+nwrote, remaining);
    if (!channel_->isWriting())
    {
      channel_->enableWriting();
    }
  }
}

bool HttpServer::setMethod(const char* start, const char* end)
{
  assert(method_ == kInvalid);
  string m(start, end);
  if (m == "GET")
  {
    method_ = kGet;
  }
  else if (m == "POST")
  {
    method_ = kPost;
  }
  else if (m == "HEAD")
  {
    method_ = kHead;
  }
  else if (m == "PUT")
  {
    method_ = kPut;
  }
  else if (m == "DELETE")
  {
    method_ = kDelete;
  }
  else
  {
    method_ = kInvalid;
  }
  return method_ != kInvalid;
}

void HttpServer::addHeader(const char* start, const char* colon, const char* end)
{
  string field(start, colon);
  ++colon;
  while (colon < end && isspace(*colon))
  {
    ++colon;
  }
  const char *space = end - 1;
  while (space >= colon && isspace(*space)) space--;
  string value(colon, space+1);
  // while (!value.empty() && isspace(value[value.size()-1]))
  // {
  //   value.resize(value.size()-1);
  // }
  headers_[field] = value;
}

string HttpServer::getHeader(const string& field) const
{
  string result;
  std::map<string, string>::const_iterator it = headers_.find(field);
  if (it != headers_.end())
  {
    result = it->second;
  }
  return result;
}

bool HttpServer::parseRequestLine(const char* begin, const char* end)
{
  bool succeed = false;
  const char* start = begin;
  const char* space = std::find(start, end, ' ');
  if (space != end && setMethod(start, space))
  {
    start = space+1;
    space = std::find(start, end, ' ');
    if (space != end)
    {
      const char* question = std::find(start, space, '?');
      if (question != space)
      {
        path_.assign(start, question);
        query_.assign(question, space);
      }
      else
      {
        path_.assign(start, space);
      }
      start = space+1;
      succeed = end-start == 8 && std::equal(start, end-1, "HTTP/1.");
      if (succeed)
      {
        if (*(end-1) == '1')
        {
          version_ = kHttp11;
        }
        else if (*(end-1) == '0')
        {
          version_ = kHttp10;
        }
        else
        {
          succeed = false;
        }
      }
    }
  }
  return succeed;
}

bool HttpServer::parseRequest()
{
  bool ok = true;
  bool hasMore = true;
  while (hasMore)
  {
    if (requestParseState_ == kExpectRequestLine)
    {
      const char* crlf = inBuffer_.findCRLF();
      if (crlf)
      {
        ok = parseRequestLine(inBuffer_.peek(), crlf);
        if (ok)
        {
          // request_.setReceiveTime(receiveTime);
          inBuffer_.retrieveUntil(crlf + 2);
          requestParseState_ = kExpectHeaders;
        }
        else
        {
          hasMore = false;
        }
      }
      else
      {
        hasMore = false;
      }
    }
    else if (requestParseState_ == kExpectHeaders)
    {
      const char* crlf = inBuffer_.findCRLF();
      if (crlf)
      {
        if (crlf == inBuffer_.peek())
        {
          if (method_ == kPost || method_ == kPut) requestParseState_ = kExpectBody;
          else
          {
            body_.clear();
            requestParseState_ = kFinish;
            hasMore = false;
          }
        }
        else
        {
          const char* colon = std::find(inBuffer_.peek(), crlf, ':');
          if (colon != crlf)
          {
            addHeader(inBuffer_.peek(), colon, crlf);
          }
          else
          {
            ok = false;
            hasMore = false;
          }
        }
        inBuffer_.retrieveUntil(crlf + 2);
      }
      else
      {
        hasMore = false;
      }
    }
    else if (requestParseState_ == kExpectBody)
    {
      string content_length = getHeader("Content-length");
      if (content_length.empty())
      {
        body_.clear();
        requestParseState_ = kFinish;
      }
      else
      {
        try {
            int length = std::stoi(content_length);
            if (length < 0) {
                ok = false;
            }
            else if (inBuffer_.readableBytes() >= length)
            {
                body_.assign(inBuffer_.peek(), length);
                inBuffer_.retrieve(length);
                requestParseState_ = kFinish;
            }
        } catch (const std::exception& e) {
            ok = false;
        }
      }
      hasMore = false;
    }
  }
  return ok;
}

bool HttpServer::analysisRequest(bool isclose, Buffer *output)
{
  bool ok = true;
  std::map<string, string> headers;
  HttpStatusCode statusCode;
  string statusMessage, body;
  if (method_ == kPut || method_ == kDelete)
  {
    statusCode = k200Ok;
    statusMessage = "OK";
    return ok;
  }
  else
  {
    if (path_ == "/hello")
    {
      statusCode = k200Ok;
      statusMessage = "OK";
      headers["Content-Type"] = "text/plain";
      body = "<html><title>Hello</title><body bgcolor=\"ffffff\">Hello<hr>\n</body></html>";
    }
    else
    {
      struct stat sbuf;
      if (path_ == "/")
      {
        path_ = "/index.html";
      } 
      path_ = source + path_;
      if (stat(path_.c_str(), &sbuf) < 0)
      {
        statusCode = k404NotFound;
        statusMessage = "Not Found";
        headers["Content-Type"] = "text/html";
        body = "<html><title>NotFound</title><body bgcolor=\"ffffff\">404 Not Found<hr>\n</body></html>";
        ok = false;
      }
      else
      {
        statusCode = k200Ok;
        statusMessage = "OK";
        int pos = path_.rfind('.');
        string filetype;
        if (pos == 0)
          filetype = MimeType::getMime("default");
        else
          filetype = MimeType::getMime(path_.substr(pos));
        headers["Content-Type"] = filetype;

        if (method_ != kHead)
        { 
          int src_fd = open(path_.c_str(), O_RDONLY, 0);
          if (src_fd < 0)
          {
            statusCode = k404NotFound;
            statusMessage = "Not Found";
            headers["Content-Type"] = "text/html";
            body = "<html><title>NotFound</title><body bgcolor=\"ffffff\">404 Not Found<hr>\n</body></html>";
            ok = false;
          }
          else
          {
            void *mmapRet = mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, src_fd, 0);
            close(src_fd);
            if (mmapRet == (void *)-1)
            {
              statusCode = k404NotFound;
              statusMessage = "Not Found";
              headers["Content-Type"] = "text/html";
              body = "<html><title>NotFound</title><body bgcolor=\"ffffff\">404 Not Found<hr>\n</body></html>";
              ok = false;
            }
            else
            {
              body = string(static_cast<char *>(mmapRet), sbuf.st_size);
            }
            munmap(mmapRet, sbuf.st_size);
          }
        }
      }
    }
  }

  if (isclose || !ok)
  {
    headers["Connection"] = "close";
  }
  else
  {
    headers["Connection"] = "Keep-Alive";
    headers["Content-Length"] = body.size();
  }

  char buf[32];
  snprintf(buf, sizeof buf, "HTTP/1.1 %d ", statusCode);
  output->append(buf);
  output->append(statusMessage);
  output->append("\r\n");

  for (const auto& header : headers)
  {
    output->append(header.first);
    output->append(": ");
    output->append(header.second);
    output->append("\r\n");
  }

  output->append("\r\n");
  output->append(body);
  
  return ok;
}

void HttpServer::onMessage()
{
  if (!parseRequest())
  {
    send("HTTP/1.1 400 Bad Request\r\n\r\n");
    shutDown();
  }

  if (requestParseState_ == kFinish)
  {
    onRequest();
    reset();
  }
}

void HttpServer::onRequest()
{
  const string& connection = getHeader("Connection");
  bool close = connection == "close" || (version_ == kHttp10 && connection != "Keep-Alive");
  Buffer buf;
  bool ok = analysisRequest(close, &buf);
  send(&buf);
  if (close || !ok)
  {
    shutDown();
  }
}

void HttpServer::shutDown()
{
  if (connState_ == kConnected)
  {
    connState_ = kDisconnecting;
    loop_->runInLoop(std::bind(&HttpServer::shutDownInLoop, this));
  }
}

void HttpServer::shutDownInLoop()
{
  loop_->assertInLoopThread();
  if (!channel_->isWriting())
  {
    if (shutdown(connfd_, SHUT_WR) < 0)
    {
      LOG_SYSERR << "HttpServer::shutDownInLoop";
    }
  }
}

void HttpServer::connectEstablished()
{
  loop_->assertInLoopThread();
  assert(connState_ == kConnecting);
  connState_ = kConnected;
  channel_->setTie(shared_from_this());
  channel_->setET();
  channel_->enableReading();
}

void HttpServer::connectDestroyed()
{
  loop_->assertInLoopThread();
  if (connState_ == kConnected)
  {
    connState_ = kDisconnected;
    channel_->disableAll();

  }
  channel_->remove();
  seperateTimer();
}

void HttpServer::handleRead()
{
  loop_->assertInLoopThread();
  loop_->add_timer(channel_.get(), DEFAULT_KEEP_ALIVE_TIME);
  int saveErrno = 0;
  ssize_t n;
  while((n = inBuffer_.readFd(connfd_, &saveErrno)) > 0)
  {
    onMessage();
  }
  if (n == 0)
  {
    handleClose();
  }
  else
  {
    if (saveErrno != EAGAIN)
    {
      errno = saveErrno;
      LOG_SYSERR << "HttpServer::handleRead";
      handleError();
    }
  }
}

void HttpServer::handleWrite()
{
  loop_->assertInLoopThread();
  loop_->add_timer(channel_.get(), DEFAULT_KEEP_ALIVE_TIME);
  if (channel_->isWriting())
  {
    channel_->disableWriting();
    ssize_t n = write(connfd_,
                      outBuffer_.peek(),
                      outBuffer_.readableBytes());
    if (n > 0)
    {
      outBuffer_.retrieve(n);
      if (outBuffer_.readableBytes() == 0)
      {
        if (connState_ == kDisconnecting)
        {
          shutDownInLoop();
        }
      }
      else
      {
        channel_->enableWriting();
      }
    }
    else
    {
      LOG_SYSERR << "HttpServer::handleWrite";
    }
  }
}

void HttpServer::handleError()
{
  int optval;
  socklen_t optlen = static_cast<socklen_t>(sizeof optval);
  int err;
  if (getsockopt(connfd_, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
  {
    err = errno;
  }
  else
  {
    err = optval;
  }
  LOG_ERROR << "HttpServer::handleError - SO_ERROR = " << err << " " << strerror_tl(err);
}

void HttpServer::handleClose()
{
  loop_->assertInLoopThread();
  assert(connState_ == kConnected || connState_ == kDisconnecting);
  connState_ = kDisconnected;
  channel_->disableAll();
  seperateTimer();
  HttpServerPtr guardThis(shared_from_this());
  // must be the last line
  closeCallback_(guardThis);
}
