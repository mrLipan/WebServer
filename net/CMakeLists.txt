set(SRCS
    Buffer.cpp
    Channel.cpp
    Epoll.cpp
    EventLoop.cpp
    EventLoopThread.cpp
    EventLoopThreadPool.cpp
    HttpServer.cpp
    Main.cpp
    Server.cpp
    Timer.cpp
    Util.cpp
)
include_directories(${PROJECT_SOURCE_DIR})


add_executable(WebServer ${SRCS})
target_link_libraries(WebServer libserver_base)
