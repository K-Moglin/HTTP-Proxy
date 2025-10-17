\
#pragma once
#include <string>
#include "http_utils.h"

struct UpstreamResult {
  int status = 0;
  std::string raw_head;
  std::string raw_body;
  std::string etag;
  std::string last_modified;
  bool cacheable = false;
};

class HttpClient {
public:
  UpstreamResult fetch(const std::string& host, const std::string& port,
                       const std::string& head, const std::string* body,
                       int max_retries = 2, int timeout_ms = 8000);
};
