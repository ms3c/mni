#include <doctest/doctest.h>
#include <mni/interpreter.h>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <thread>

#ifdef MNI_HAVE_HTTP
#  ifndef _WIN32
#    include <arpa/inet.h>
#    include <netinet/in.h>
#    include <poll.h>
#    include <sys/socket.h>
#    include <unistd.h>
#  endif
#endif

using namespace mni;

namespace {
  std::string run(const std::string& src) {
    std::ostringstream os;
    std::istringstream is;
    Interpreter interp(os, is);
    return interp.run(src).inspect();
  }

  std::string errorOf(const std::string& src) {
    std::ostringstream os;
    std::istringstream is;
    Interpreter interp(os, is);
    try {
      interp.run(src);
    } catch (const NuruError& e) {
      return e.what();
    }
    return "<no error>";
  }

  bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
  }
}  // namespace

TEST_CASE("os.kimbiza returns what a command prints") {
  CHECK(run("tumia os\nos.kimbiza(\"echo habari\").gawa()") == "[\"habari\"]");
  CHECK(run("tumia os\naina(os.kimbiza(\"echo x\"))") == "\"NENO\"");
  CHECK(contains(errorOf("tumia os\nos.kimbiza()"), "Idadi ya hoja"));
  CHECK(contains(errorOf("tumia os\nos.kimbiza(5)"), "inahitaji NENO"));
}

TEST_CASE("os.toka asks to exit") {
  std::ostringstream os;
  std::istringstream is;
  Interpreter interp(os, is);
  try {
    interp.run("tumia os\nos.toka(4)");
    FAIL("expected ExitRequest");
  } catch (const ExitRequest& e) {
    CHECK(e.code == 4);
  }
}

#ifdef MNI_HAVE_HTTP

TEST_CASE("mtandao rejects bad arguments before touching the network") {
  CHECK(contains(errorOf("tumia mtandao\nmtandao.peruzi()"), "inahitaji yuareli"));
  CHECK(contains(errorOf("tumia mtandao\nmtandao.peruzi(5)"), "inahitaji NENO"));
  CHECK(contains(errorOf("tumia mtandao\nmtandao.peruzi(\"http://x\", yuareli = \"http://y\")"),
                 "mara mbili"));
  CHECK(contains(errorOf("tumia mtandao\nmtandao.peruzi(yuareli = \"http://x\", foo = 1)"),
                 "haina kigezo 'foo'"));
  CHECK(contains(errorOf("tumia mtandao\nmtandao.peruzi(yuareli = \"http://x\", vichwa = 5)"),
                 "lazima iwe KAMUSI"));
  CHECK(contains(errorOf("tumia mtandao\nmtandao.tuma(yuareli = \"http://x\", mwili = 5)"),
                 "mwili hauwezi kuwa NAMBA"));
}

TEST_CASE("mtandao reports a refused connection as an error") {
  CHECK(contains(errorOf("tumia mtandao\nmtandao.peruzi(\"http://127.0.0.1:1/\")"),
                 "Hitilafu ya mtandao"));
}

#  ifndef _WIN32

namespace {
  // Accepts one connection on a loopback port, records the request and sends a canned response.
  class OneShotServer {
  public:
    explicit OneShotServer(std::string response) : response_(std::move(response)) {
      listener_ = socket(AF_INET, SOCK_STREAM, 0);
      int yes = 1;
      setsockopt(listener_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
      sockaddr_in address{};
      address.sin_family = AF_INET;
      address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      address.sin_port = 0;
      REQUIRE(bind(listener_, reinterpret_cast<sockaddr*>(&address), sizeof address) == 0);
      REQUIRE(listen(listener_, 1) == 0);
      socklen_t length = sizeof address;
      getsockname(listener_, reinterpret_cast<sockaddr*>(&address), &length);
      port_ = ntohs(address.sin_port);
      thread_ = std::thread([this] { serve(); });
    }

    ~OneShotServer() {
      if (thread_.joinable()) thread_.join();
      close(listener_);
    }

    std::string url(const std::string& path) const {
      return "http://127.0.0.1:" + std::to_string(port_) + path;
    }

    // Waits for the exchange to finish and returns the raw request.
    std::string request() {
      thread_.join();
      return request_;
    }

  private:
    void serve() {
      pollfd waiting{listener_, POLLIN, 0};
      if (poll(&waiting, 1, 5000) <= 0) return;  // nobody connected: give up instead of hanging
      int client = accept(listener_, nullptr, nullptr);
      if (client < 0) return;

      char buffer[4096];
      size_t bodyStart = std::string::npos;
      size_t wanted = 0;
      while (true) {
        pollfd readable{client, POLLIN, 0};
        if (poll(&readable, 1, 5000) <= 0) break;
        ssize_t n = recv(client, buffer, sizeof buffer, 0);
        if (n <= 0) break;
        request_.append(buffer, static_cast<size_t>(n));
        if (bodyStart == std::string::npos) {
          size_t end = request_.find("\r\n\r\n");
          if (end == std::string::npos) continue;
          bodyStart = end + 4;
          std::string head = request_.substr(0, end);
          std::transform(head.begin(), head.end(), head.begin(),
                         [](unsigned char c) { return std::tolower(c); });
          size_t at = head.find("content-length:");
          if (at != std::string::npos) wanted = std::stoul(head.substr(at + 15));
        }
        if (request_.size() >= bodyStart + wanted) break;
      }
      send(client, response_.data(), response_.size(), 0);
      close(client);
    }

    std::string response_;
    std::string request_;
    int listener_ = -1;
    uint16_t port_ = 0;
    std::thread thread_;
  };

  std::string http(const std::string& status, const std::string& body) {
    return "HTTP/1.1 " + status + "\r\nContent-Type: text/plain\r\nContent-Length: "
           + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
  }

  std::string lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return text;
  }
}  // namespace

TEST_CASE("mtandao.peruzi sends a GET with query parameters and headers") {
  OneShotServer server(http("200 OK", "hello"));
  const std::string result
      = run("tumia mtandao\nmtandao.peruzi(yuareli = \"" + server.url("/path")
            + "\", vichwa = {\"X-Test\": \"abc\"}, mwili = {\"q\": \"1\", \"n\": 2})");
  CHECK(result == "\"hello\"");
  const std::string request = server.request();
  CHECK(request.rfind("GET /path?q=1&n=2 ", 0) == 0);
  CHECK(contains(lower(request), "x-test: abc"));
}

TEST_CASE("mtandao.peruzi accepts the URL as a positional argument") {
  OneShotServer server(http("200 OK", "plain"));
  CHECK(run("tumia mtandao\nmtandao.peruzi(\"" + server.url("/") + "\")") == "\"plain\"");
  CHECK(server.request().rfind("GET / ", 0) == 0);
}

TEST_CASE("mtandao.tuma posts a dictionary as JSON") {
  OneShotServer server(http("200 OK", "ok"));
  CHECK(run("tumia mtandao\nmtandao.tuma(yuareli = \"" + server.url("/submit")
            + "\", mwili = {\"jina\": \"Juma\", \"namba\": [1, 2]})")
        == "\"ok\"");
  const std::string request = server.request();
  CHECK(request.rfind("POST /submit ", 0) == 0);
  CHECK(contains(lower(request), "content-type: application/json"));
  CHECK(contains(request, "{\"jina\":\"Juma\",\"namba\":[1,2]}"));
}

TEST_CASE("mtandao.tuma sends a string body as is and keeps custom headers") {
  OneShotServer server(http("200 OK", "ok"));
  run("tumia mtandao\nmtandao.tuma(yuareli = \"" + server.url("/")
      + "\", vichwa = {\"Content-Type\": \"text/plain\"}, mwili = \"maandishi\")");
  const std::string request = server.request();
  CHECK(contains(lower(request), "content-type: text/plain"));
  CHECK(request.size() >= 9);
  CHECK(request.substr(request.size() - 9) == "maandishi");
}

TEST_CASE("mtandao returns the body even when the status is an error") {
  OneShotServer server(http("404 Not Found", "haipo"));
  CHECK(run("tumia mtandao\nmtandao.peruzi(\"" + server.url("/x") + "\")") == "\"haipo\"");
}

#  endif  // !_WIN32

#else

TEST_CASE("mtandao is reported as unavailable in builds without HTTP") {
  CHECK(contains(errorOf("tumia mtandao"), "haikujumuishwa"));
}

#endif  // MNI_HAVE_HTTP
