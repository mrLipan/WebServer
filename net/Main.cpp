#include <getopt.h>

#include <string>

#include "EventLoop.h"
#include "Server.h"
#include "base/Logging.h"

#define DEFAULT_ROLL_SIZE 100 * 1024 * 1024

int main(int argc, char* argv[]) {
  int threadNum = 8;
  int port = 8888;
  std::string logPath = "WebServer.log";
  std::string webName = "LP's WebServer";

  int opt;
  const char* str = "t:l:p:";
  while ((opt = getopt(argc, argv, str)) != -1) {
    switch (opt) {
      case 't': {
        threadNum = atoi(optarg);
        break;
      }
      case 'l': {
        logPath = optarg;
        if (logPath.size() < 2 || optarg[0] != '/') {
          printf("logPath should start with \"/\"\n");
          abort();
        }
        break;
      }
      case 'p': {
        port = atoi(optarg);
        break;
      }
      default:
        break;
    }
  }
  initAsyncLogger(logPath, DEFAULT_ROLL_SIZE);

#ifndef _PTHREADS
  LOG_INFO << "_PTHREADS is not defined !";
#endif
  EventLoop mainLoop;
  Server myHTTPServer(&mainLoop, port, webName, threadNum,
                      true);
  myHTTPServer.start();
  mainLoop.loop();
  return 0;
}
