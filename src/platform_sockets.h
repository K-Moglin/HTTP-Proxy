\
#pragma once

#ifdef _WIN32
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "Ws2_32.lib")
  using socket_t = SOCKET;
  inline void socket_startup(){ static bool inited=false; if(!inited){ WSADATA w; WSAStartup(MAKEWORD(2,2), &w); inited=true; } }
  inline int socket_close(socket_t s){ return ::closesocket(s); }
  inline int socket_last_error(){ return WSAGetLastError(); }
  inline bool socket_would_block(int e){ return e==WSAEWOULDBLOCK; }
  #ifndef ssize_t
    #if defined(_WIN64)
      using ssize_t = long long;
    #else
      using ssize_t = long;
    #endif
  #endif
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <poll.h>
  #include <errno.h>
  using socket_t = int;
  inline void socket_startup(){}
  inline int socket_close(socket_t s){ return ::close(s); }
  inline int socket_last_error(){ return errno; }
  inline bool socket_would_block(int e){ return e==EWOULDBLOCK || e==EAGAIN; }
#endif
