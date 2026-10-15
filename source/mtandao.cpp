#include <string>
#include <utility>
#include <vector>

#include "native.h"

#ifdef MNI_HAVE_HTTP
#  include <cpr/cpr.h>
#endif

namespace mni::native {

#ifdef MNI_HAVE_HTTP

  namespace {
    constexpr int kTimeoutMs = 30'000;

    // Keyword names, as in Nuru: yuareli = URL, vichwa = headers, mwili = parameters or body.
    struct Request {
      std::string url;
      Value headers;
      Value body;
    };

    Request parseRequest(const std::string& function, std::vector<Value>& args, NamedArgs& named,
                         int line) {
      expectArgs(function, args, 0, 1, line);
      Request request;
      bool hasUrl = false;
      if (!args.empty()) {
        request.url = stringArg(function, args[0], line);
        hasUrl = true;
      }
      for (auto& [key, value] : named) {
        if (key == "yuareli") {
          if (hasUrl) throw NuruError(line, "'" + function + "': yuareli imetolewa mara mbili");
          request.url = stringArg(function, value, line);
          hasUrl = true;
        } else if (key == "vichwa") {
          request.headers = value;
        } else if (key == "mwili") {
          request.body = value;
        } else {
          throw NuruError(line, "'" + function + "' haina kigezo '" + key + "'");
        }
      }
      if (!hasUrl || request.url.empty())
        throw NuruError(line, "'" + function + "' inahitaji yuareli (URL)");
      return request;
    }

    // A dictionary of strings, as (name, value) pairs.
    std::vector<std::pair<std::string, std::string>> pairsOf(const std::string& function,
                                                             const char* what, const Value& dict,
                                                             int line) {
      std::vector<std::pair<std::string, std::string>> pairs;
      if (dict.isNull()) return pairs;
      if (dict.type() != Value::Type::Dict)
        throw NuruError(
            line, "'" + function + "': " + what + " lazima iwe KAMUSI, si " + dict.typeName());
      for (const auto& [key, value] : dict.asDict().entries)
        pairs.emplace_back(key.display(), value.display());
      return pairs;
    }

    Value finish(const cpr::Response& response, int line) {
      if (response.error) throw NuruError(line, "Hitilafu ya mtandao: " + response.error.message);
      return Value::string(response.text);  // the body, whatever the status code
    }

    Value get(Interpreter&, std::vector<Value>& args, NamedArgs& named, int line) {
      Request request = parseRequest("peruzi", args, named, line);
      cpr::Header headers;
      for (auto& [k, v] : pairsOf("peruzi", "vichwa", request.headers, line)) headers[k] = v;
      cpr::Parameters parameters;
      for (auto& [k, v] : pairsOf("peruzi", "mwili", request.body, line)) parameters.Add({k, v});
      return finish(cpr::Get(cpr::Url{request.url}, headers, parameters, cpr::Timeout{kTimeoutMs}),
                    line);
    }

    Value post(Interpreter&, std::vector<Value>& args, NamedArgs& named, int line) {
      Request request = parseRequest("tuma", args, named, line);
      cpr::Header headers;
      for (auto& [k, v] : pairsOf("tuma", "vichwa", request.headers, line)) headers[k] = v;

      std::string body;
      switch (request.body.type()) {
        case Value::Type::Null:
          break;
        case Value::Type::String:
          body = request.body.asString();
          break;
        case Value::Type::Dict:
        case Value::Type::Array:  // sent as JSON
          body = jsonEncode(request.body, line);
          if (!headers.count("Content-Type")) headers["Content-Type"] = "application/json";
          break;
        default:
          throw NuruError(line,
                          std::string("'tuma': mwili hauwezi kuwa ") + request.body.typeName());
      }
      return finish(
          cpr::Post(cpr::Url{request.url}, headers, cpr::Body{body}, cpr::Timeout{kTimeoutMs}),
          line);
    }
  }  // namespace

  Value createMtandao() {
    Value pkg = makePackage("mtandao", "PAKEJI");
    addFunction(pkg, "peruzi", get, true);  // GET
    addFunction(pkg, "tuma", post, true);   // POST
    return pkg;
  }

#else

  Value createMtandao() { return Value(); }

#endif

}  // namespace mni::native
