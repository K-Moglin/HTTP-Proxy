\
#include "connection.h"
#include "http_utils.h"
#include "http_client.h"
#include "platform_sockets.h"
#include <cstring>
#include <iostream>

static bool read_until(socket_t fd, const std::string& delim, std::string& out){
  char buf[4096];
  for(;;){
    auto pos = out.find(delim);
    if (pos!=std::string::npos) return true;
#ifdef _WIN32
    int n = ::recv(fd, buf, (int)sizeof(buf), 0);
#else
    ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
#endif
    if (n<=0) return false;
    out.append(buf, buf+n);
  }
}

Connection::Connection(socket_t client_fd, LruCache& cache, ThreadPool& pool)
  : cfd_(client_fd), cache_(cache), pool_(pool) {}

void Connection::operator()(){
  for(;;){ if (!handle_one_request()) break; }
  socket_close(cfd_);
}

bool Connection::handle_one_request(){
  std::string head;
  if (!read_until(cfd_, "\r\n\r\n", head)) return false;
  HttpRequest req; parse_request_head(head, req);
  if (req.method == "CONNECT"){
    std::string host = req.host; std::string port = req.port.empty()?"443":req.port;
    return handle_connect(host, port);
  }
  return handle_regular(head);
}

bool Connection::handle_connect(const std::string& host, const std::string& port){
  addrinfo hints{}; hints.ai_family=AF_UNSPEC; hints.ai_socktype=SOCK_STREAM; hints.ai_protocol=IPPROTO_TCP;
  addrinfo* res=nullptr; if (getaddrinfo(host.c_str(), port.c_str(), &hints, &res)!=0) return false;
  socket_t sfd=(socket_t)-1; for(auto p=res;p;p=p->ai_next){ sfd=::socket(p->ai_family,p->ai_socktype,p->ai_protocol); if(sfd==(socket_t)-1) continue; if(::connect(sfd,p->ai_addr,(int)p->ai_addrlen)==0) break; socket_close(sfd); sfd=(socket_t)-1; }
  freeaddrinfo(res);
  if (sfd==(socket_t)-1) return false;
  const char ok[] = "HTTP/1.1 200 Connection Established\r\nProxy-Agent: cpp-proxy\r\n\r\n";
#ifdef _WIN32
  ::send(cfd_, ok, (int)sizeof(ok)-1, 0);
#else
  ::send(cfd_, ok, sizeof(ok)-1, 0);
#endif
  char buf[8192];
  for(;;){
#ifdef _WIN32
    WSAPOLLFD pfd[2]; pfd[0]={cfd_, POLLIN,0}; pfd[1]={sfd, POLLIN,0};
    int r = WSAPoll(pfd, 2, 300000);
    if (r<=0) break;
    if (pfd[0].revents & POLLIN){ int n = ::recv(cfd_, buf, (int)sizeof(buf), 0); if (n<=0) break; ::send(sfd, buf, n, 0);} 
    if (pfd[1].revents & POLLIN){ int n = ::recv(sfd, buf, (int)sizeof(buf), 0); if (n<=0) break; ::send(cfd_, buf, n, 0);} 
#else
    struct pollfd pfd[2]; pfd[0]={cfd_, POLLIN,0}; pfd[1]={sfd, POLLIN,0};
    int r = ::poll(pfd,2, 300000);
    if (r<=0) break;
    if (pfd[0].revents & POLLIN){ ssize_t n = ::recv(cfd_, buf, sizeof(buf), 0); if (n<=0) break; ::send(sfd, buf, n, 0);} 
    if (pfd[1].revents & POLLIN){ ssize_t n = ::recv(sfd, buf, sizeof(buf), 0); if (n<=0) break; ::send(cfd_, buf, n, 0);} 
#endif
  }
  socket_close(sfd);
  return false; // end keep-alive after CONNECT
}

bool Connection::handle_regular(const std::string& head){
  HttpRequest req; parse_request_head(head, req);
  std::string key = req.url;

  CacheEntry ce; bool hit = cache_.get(key, ce);
  std::string fwd_head = head;
  if (hit){
    bool expired = (ce.expiry != std::chrono::steady_clock::time_point{}) && (std::chrono::steady_clock::now() > ce.expiry);
    if (!expired){
#ifdef _WIN32
      ::send(cfd_, ce.value.data(), (int)ce.value.size(), 0);
#else
      ::send(cfd_, ce.value.data(), ce.value.size(), 0);
#endif
      return true;
    }
    if (!ce.etag.empty()) fwd_head += "If-None-Match: "+ce.etag+"\r\n";
    if (!ce.last_modified.empty()) fwd_head += "If-Modified-Since: "+ce.last_modified+"\r\n";
    fwd_head += "\r\n";
  }

  HttpClient cli; UpstreamResult ur = cli.fetch(req.host, req.port, fwd_head, nullptr);
  if (ur.status==0){
    const char* err = "HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\n\r\n";
#ifdef _WIN32
    ::send(cfd_, err, (int)strlen(err), 0);
#else
    ::send(cfd_, err, strlen(err), 0);
#endif
    return false;
  }

  if (ur.status==304 && hit){
#ifdef _WIN32
    ::send(cfd_, ce.value.data(), (int)ce.value.size(), 0);
#else
    ::send(cfd_, ce.value.data(), ce.value.size(), 0);
#endif
    return true;
  }

  std::string resp = ur.raw_head + ur.raw_body;
#ifdef _WIN32
  ::send(cfd_, resp.data(), (int)resp.size(), 0);
#else
  ::send(cfd_, resp.data(), resp.size(), 0);
#endif

  if (to_lower(req.method)=="get" && ur.cacheable){
    CacheEntry ne; ne.key=key; ne.value=resp; ne.etag=ur.etag; ne.last_modified=ur.last_modified; ne.bytes=ne.value.size();
    size_t max_age_s = 60; { HttpResponse hdr; parse_response_head(ur.raw_head, hdr); auto cc = header_get(hdr.headers, "Cache-Control"); auto pos = cc.find("max-age="); if (pos!=std::string::npos) max_age_s = (size_t)std::stoul(cc.substr(pos+8)); }
    ne.expiry = std::chrono::steady_clock::now() + std::chrono::seconds(max_age_s);
    cache_.put(std::move(ne));
  }
  return true;
}
