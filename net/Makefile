MAINSOURCE := net/Main.cpp tests/HTTPClient.cpp
SOURCE  := $(wildcard net/*.cpp base/*.cpp tests/*.cpp)
override SOURCE := $(filter-out $(MAINSOURCE),$(SOURCE))
OBJS    := $(patsubst %.cpp,%.o,$(SOURCE))

TARGET  := WebServer
CC      := g++
LIBS    := -lpthread
INCLUDE:= -I./usr/local/lib
CFLAGS  := -std=c++17 -g -Wall -O3 -D_PTHREADS
CXXFLAGS:= $(CFLAGS)

SUBTARGET := HTTPClient

.PHONY : objs clean veryclean rebuild all tests debug
all : $(TARGET) $(SUBTARGET)
objs : $(OBJS)
rebuild: veryclean all

tests : $(SUBTARGET)
clean :
	find . -name '*.o' | xargs rm -f
veryclean :
	find . -name '*.o' | xargs rm -f
	find . -name $(TARGET) | xargs rm -f
	find . -name $(SUBTARGET) | xargs rm -f
debug:
	@echo $(SOURCE)

$(TARGET) : $(OBJS) net/Main.o
	$(CC) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

$(SUBTARGET) : $(OBJS) tests/HTTPClient.o
	$(CC) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)
