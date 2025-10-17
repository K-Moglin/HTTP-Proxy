\
#pragma once
#include <string>
#include <unordered_map>
#include <sstream>
#include <algorithm>

struct HttpRequest {
  std::string method;
  std::string url;
  std::string host;
  std::string port = "80";
  std::unordered_map<std::string, std::string> headers;
  std::string body;
};

struct HttpResponse {
  int status = 0;
  std::unordered_map<std::string, std::string> headers;
  std::string head_raw;
  std::string body;
  bool chunked = false;
};

inline std::string to_lower(std::string s){ std::transform(s.begin(), s.end(), s.begin(), ::tolower); return s; }
inline std::string header_get(const std::unordered_map<std::string,std::string>& h, const std::string &k){ auto it = h.find(k); return it==h.end() ? "" : it->second; }

inline void parse_request_head(const std::string &raw, HttpRequest &req){
  std::istringstream iss(raw);
  std::string line; std::getline(iss, line); if(!line.empty() && line.back()=='\r') line.pop_back();
  { std::istringstream ls(line); ls >> req.method >> req.url; }
  while (std::getline(iss, line)){
    if(!line.empty() && line.back()=='\r') line.pop_back();
    if(line.empty()) break;
    auto pos = line.find(":"); if(pos==std::string::npos) continue;
    auto key = line.substr(0,pos); auto val=line.substr(pos+1); if(!val.empty() && val[0]==' ') val.erase(0,1);
    req.headers[key] = val;
  }
  if (req.headers.count("Host")){
    auto hostline = req.headers["Host"]; auto p = hostline.find(":");
    if (p==std::string::npos) req.host = hostline; else { req.host=hostline.substr(0,p); req.port=hostline.substr(p+1); }
  }
}

inline void parse_response_head(const std::string &raw, HttpResponse &resp){
  resp.head_raw = raw;
  std::istringstream iss(raw);
  std::string line; std::getline(iss, line); if(!line.empty() && line.back()=='\r') line.pop_back();
  { std::istringstream ls(line); std::string httpver; ls >> httpver >> resp.status; }
  while (std::getline(iss, line)){
    if(!line.empty() && line.back()=='\r') line.pop_back();
    if(line.empty()) break;
    auto pos = line.find(":"); if(pos==std::string::npos) continue;
    auto key = line.substr(0,pos); auto val=line.substr(pos+1); if(!val.empty() && val[0]==' ') val.erase(0,1);
    resp.headers[key] = val;
  }
  auto te = header_get(resp.headers, "Transfer-Encoding");
  resp.chunked = to_lower(te).find("chunked") != std::string::npos;
}
