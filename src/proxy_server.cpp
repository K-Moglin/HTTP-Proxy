\
#include "proxy_server.h"
#include "connection.h"
#include "platform_sockets.h"
#include <iostream>

ProxyServer::ProxyServer(const std::string& addr, const std::string& port, size_t workers, size_t cache_bytes)
  : addr_(addr), port_(port), pool_(workers), cache_(cache_bytes) {}

void ProxyServer::run(){
  socket_startup();
  addrinfo hints{}; hints.ai_family=AF_UNSPEC; hints.ai_socktype=SOCK_STREAM; hints.ai_flags=AI_PASSIVE; hints.ai_protocol=IPPROTO_TCP;
  addrinfo* res=nullptr;
  if (getaddrinfo(addr_.empty()?nullptr:addr_.c_str(), port_.c_str(), &hints, &res)!=0){ std::cerr << "getaddrinfo failed\n"; return; }
  socket_t lfd=(socket_t)-1;
  for (auto p=res; p; p=p->ai_next){
    lfd=::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (lfd==(socket_t)-1) continue;
#ifndef _WIN32
    int yes=1; setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#endif
    if (::bind(lfd, p->ai_addr, (int)p->ai_addrlen)==0 && ::listen(lfd, SOMAXCONN)==0) break;
    socket_close(lfd); lfd=(socket_t)-1;
  }
  freeaddrinfo(res);
  if (lfd==(socket_t)-1){ std::cerr<<"listen failed\n"; return; }

  std::cout << "Listening on " << (addr_.empty()?"0.0.0.0":addr_) << ":" << port_ << "\n";
  for(;;){
    socket_t cfd = ::accept(lfd, nullptr, nullptr);
    if (cfd==(socket_t)-1) continue;
    pool_.enqueue([cfd,this]{ Connection(cfd, this->cache_, this->pool_)(); });
  }
}
