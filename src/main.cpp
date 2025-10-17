\
#include "proxy_server.h"
#include <string>

int main(int argc, char** argv){
  std::string addr="0.0.0.0"; std::string port="8080"; size_t workers=8; size_t cache_mb=64;
  for (int i=1;i<argc;i++){
    std::string a=argv[i];
    if (a=="--listen" && i+1<argc) addr=argv[++i];
    else if (a=="--port" && i+1<argc) port=argv[++i];
    else if (a=="--workers" && i+1<argc) workers=(size_t)std::stoul(argv[++i]);
    else if (a=="--cache-mb" && i+1<argc) cache_mb=(size_t)std::stoul(argv[++i]);
  }
  ProxyServer srv(addr, port, workers, cache_mb*1024*1024);
  srv.run();
  return 0;
}
