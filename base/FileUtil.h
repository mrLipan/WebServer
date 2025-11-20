#ifndef FILEUTIL_H
#define FILEUTIL_H

#include <string>

#include "noncopyable.h"

class AppendFile : noncopyable {
 public:
  explicit AppendFile(std::string filename);
  ~AppendFile();

  void append(const char* logline, const size_t len);
  off_t writtenBytes() const { return writtenBytes_; }
  void flush();

 private:
  size_t write(const char* logline, size_t len);
  FILE* fp_;
  char buffer_[64 * 1024];
  off_t writtenBytes_;
};

#endif  // FILEUTIL_H
