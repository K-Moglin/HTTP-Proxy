\
#include "http_client.h"
#include "http_utils.h"
#include "platform_sockets.h"
#include <string>
#include <thread>
#include <chrono>

static socket_t connect_with_timeout(const char* host, const char* port, int timeout_ms){
  addrinfo hints{}; hints.ai_family=AF_UNSPEC; hints.ai_socktype=SOCK_STREAM; hints.ai_protocol=IPPROTO_TCP;
  addrinfo* res=nullptr; if (getaddrinfo(host, port, &hints, &res)!=0) return (socket_t)-1;
  socket_t sock = (socket_t)-1;
  for (auto p=res; p; p=p->ai_next){
    sock = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sock==(socket_t)-1) continue;
#ifdef _WIN32
    u_long nb = 1; ioctlsocket(sock, FIONBIO, &nb);
    int r = ::connect(sock, p->ai_addr, (int)p->ai_addrlen);
    if (r==SOCKET_ERROR && WSAGetLastError()!=WSAEWOULDBLOCK){ socket_close(sock); sock=(socket_t)-1; continue; }
    WSAPOLLFD pfd{sock, POLLOUT, 0};
    int pr = WSAPoll(&pfd, 1, timeout_ms);
    if (pr==1 && (pfd.revents & POLLOUT)) { nb = 0; ioctlsocket(sock, FIONBIO, &nb); break; }
    socket_close(sock); sock=(socket_t)-1; continue;
#else
    int flags = fcntl(sock, F_GETFL, 0); fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    int r = ::connect(sock, p->ai_addr, p->ai_addrlen);
    if (r<0 && errno!=EINPROGRESS){ socket_close(sock); sock=(socket_t)-1; continue; }
    struct pollfd pfd{sock, POLLOUT, 0};
    int pr = ::poll(&pfd, 1, timeout_ms);
    if (pr==1 && (pfd.revents & POLLOUT)) { fcntl(sock, F_SETFL, flags); break; }
    socket_close(sock); sock=(socket_t)-1; continue;
#endif
  }
  freeaddrinfo(res);
  return sock;
}

UpstreamResult HttpClient::fetch(const std::string& host, const std::string& port,
                                 const std::string& head, const std::string* body,
                                 int max_retries, int timeout_ms){
  UpstreamResult ur;
  for (int attempt=0; attempt<=max_retries; ++attempt){
    socket_t fd = connect_with_timeout(host.c_str(), port.c_str(), timeout_ms);
    if (fd==(socket_t)-1) { std::this_thread::sleep_for(std::chrono::milliseconds(200*(attempt+1))); continue; }
#ifdef _WIN32
    int w = ::send(fd, head.data(), (int)head.size(), 0);
#else
    ssize_t w = ::send(fd, head.data(), head.size(), 0);
#endif
    if (w < 0 || (size_t)w != head.size()){ socket_close(fd); continue; }
    if (body && !body->empty()) {
#ifdef _WIN32
      ::send(fd, body->data(), (int)body->size(), 0);
#else
      ::send(fd, body->data(), body->size(), 0);
#endif
    }
    std::string resp_head; char buf[4096]; bool got_head=false;
    for(;;){
#ifdef _WIN32
      int n = ::recv(fd, buf, (int)sizeof(buf), 0);
#else
      ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
#endif
      if (n<=0){ socket_close(fd); break; }
      resp_head.append(buf, buf+n);
      auto pos = resp_head.find("\r\n\r\n");
      if (pos!=std::string::npos){
        ur.raw_head = resp_head.substr(0, pos+4);
        std::string rest = resp_head.substr(pos+4);
        HttpResponse hdr; parse_response_head(ur.raw_head, hdr);
        ur.status = hdr.status;
        ur.etag = header_get(hdr.headers, "ETag");
        ur.last_modified = header_get(hdr.headers, "Last-Modified");
        auto cc = header_get(hdr.headers, "Cache-Control");
        ur.cacheable = (to_lower(cc).find("no-store")==std::string::npos);
        if (!hdr.chunked){
          size_t cl = 0; { auto s = header_get(hdr.headers, "Content-Length"); if(!s.empty()) cl = (size_t)std::stoul(s); }
          ur.raw_body = std::move(rest);
          while (ur.raw_body.size() < cl){
#ifdef _WIN32
            n = ::recv(fd, buf, (int)sizeof(buf), 0);
#else
            n = ::recv(fd, buf, sizeof(buf), 0);
#endif
            if (n<=0) break; ur.raw_body.append(buf, buf+n);
          }
        } else {
          ur.raw_body = std::move(rest);
        }
        got_head=true; break;
      }
    }
    socket_close(fd);
    if (got_head) return ur;
    std::this_thread::sleep_for(std::chrono::milliseconds(200*(attempt+1)));
  }
  return ur;
}
