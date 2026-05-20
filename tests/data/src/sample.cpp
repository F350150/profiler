#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

int hot_loop(float* a, float* b, int n) {
  float sum = 0;
  for (int i = 0; i < n; ++i) {
    sum += a[i] * b[i];
  }
  return static_cast<int>(sum);
}

void memory_hotspot() {
  char* p = new char[128];
  char buf[128] = {0};
  memcpy(p, buf, 128);
  delete[] p;
}

void io_hotspot(int fd) {
  char buf[64] = {0};
  write(fd, buf, sizeof(buf));
  read(fd, buf, sizeof(buf));
}

void net_hotspot(int s, const sockaddr* addr, socklen_t len) {
  char buf[32] = {0};
  sendto(s, buf, sizeof(buf), 0, addr, len);
  recvfrom(s, buf, sizeof(buf), 0, nullptr, nullptr);
}
