#pragma once
#include <string>
namespace sms_frontend { namespace ra_http {
struct Response { int status=0; std::string body,error; };
bool available();
Response request(const std::string&,const std::string&,const std::string&,const std::string&);
} }
