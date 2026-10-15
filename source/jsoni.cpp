#include <mni/interpreter.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "native.h"

namespace mni::native {

  namespace {
    class JsonParser {
    public:
      JsonParser(const std::string& text, int line) : s_(text), line_(line) {}

      Value parse() {
        skipSpace();
        Value v = parseValue(0);
        skipSpace();
        if (i_ != s_.size()) fail("maandishi ya ziada baada ya JSON");
        return v;
      }

    private:
      [[noreturn]] void fail(const std::string& what) const {
        throw NuruError(line_, "JSON si sahihi: " + what + " (nafasi " + std::to_string(i_) + ")");
      }

      void skipSpace() {
        while (i_ < s_.size()
               && (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\n' || s_[i_] == '\r'))
          ++i_;
      }

      bool consume(const char* word) {
        size_t len = std::char_traits<char>::length(word);
        if (s_.compare(i_, len, word) != 0) return false;
        i_ += len;
        return true;
      }

      Value parseValue(int depth) {
        if (depth > 200) fail("imejaa viwango vingi mno");
        if (i_ >= s_.size()) fail("mwisho wa maandishi usiotarajiwa");
        char c = s_[i_];
        if (c == '{') return parseObject(depth);
        if (c == '[') return parseArray(depth);
        if (c == '"') return Value::string(parseString());
        if (consume("true")) return Value::boolean(true);
        if (consume("false")) return Value::boolean(false);
        if (consume("null")) return Value();
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();
        fail(std::string("herufi isiyotarajiwa '") + c + "'");
      }

      Value parseObject(int depth) {
        ++i_;
        Value dict = Value::dict();
        skipSpace();
        if (i_ < s_.size() && s_[i_] == '}') {
          ++i_;
          return dict;
        }
        for (;;) {
          skipSpace();
          if (i_ >= s_.size() || s_[i_] != '"') fail("tulitegemea ufunguo wa neno");
          std::string key = parseString();
          skipSpace();
          if (i_ >= s_.size() || s_[i_] != ':') fail("tulitegemea ':'");
          ++i_;
          skipSpace();
          dict.asDict().set(Value::string(std::move(key)), parseValue(depth + 1));
          skipSpace();
          if (i_ < s_.size() && s_[i_] == ',') {
            ++i_;
            continue;
          }
          if (i_ < s_.size() && s_[i_] == '}') {
            ++i_;
            return dict;
          }
          fail("tulitegemea ',' au '}'");
        }
      }

      Value parseArray(int depth) {
        ++i_;
        std::vector<Value> items;
        skipSpace();
        if (i_ < s_.size() && s_[i_] == ']') {
          ++i_;
          return Value::array();
        }
        for (;;) {
          skipSpace();
          items.push_back(parseValue(depth + 1));
          skipSpace();
          if (i_ < s_.size() && s_[i_] == ',') {
            ++i_;
            continue;
          }
          if (i_ < s_.size() && s_[i_] == ']') {
            ++i_;
            return Value::array(std::move(items));
          }
          fail("tulitegemea ',' au ']'");
        }
      }

      static void appendUtf8(std::string& out, unsigned cp) {
        if (cp < 0x80) {
          out += static_cast<char>(cp);
        } else if (cp < 0x800) {
          out += static_cast<char>(0xC0 | (cp >> 6));
          out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
          out += static_cast<char>(0xE0 | (cp >> 12));
          out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
          out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
          out += static_cast<char>(0xF0 | (cp >> 18));
          out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
          out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
          out += static_cast<char>(0x80 | (cp & 0x3F));
        }
      }

      unsigned parseHex4() {
        if (i_ + 4 > s_.size()) fail("\\u haijakamilika");
        unsigned v = 0;
        for (int k = 0; k < 4; ++k) {
          char h = s_[i_++];
          v <<= 4;
          if (h >= '0' && h <= '9')
            v |= static_cast<unsigned>(h - '0');
          else if (h >= 'a' && h <= 'f')
            v |= static_cast<unsigned>(h - 'a' + 10);
          else if (h >= 'A' && h <= 'F')
            v |= static_cast<unsigned>(h - 'A' + 10);
          else
            fail("\\u si sahihi");
        }
        return v;
      }

      std::string parseString() {
        ++i_;
        std::string out;
        for (;;) {
          if (i_ >= s_.size()) fail("neno halijafungwa");
          char c = s_[i_++];
          if (c == '"') return out;
          if (static_cast<unsigned char>(c) < 0x20) fail("herufi ya kudhibiti ndani ya neno");
          if (c != '\\') {
            out += c;
            continue;
          }
          if (i_ >= s_.size()) fail("neno halijafungwa");
          char e = s_[i_++];
          switch (e) {
            case '"':
              out += '"';
              break;
            case '\\':
              out += '\\';
              break;
            case '/':
              out += '/';
              break;
            case 'b':
              out += '\b';
              break;
            case 'f':
              out += '\f';
              break;
            case 'n':
              out += '\n';
              break;
            case 'r':
              out += '\r';
              break;
            case 't':
              out += '\t';
              break;
            case 'u': {
              unsigned cp = parseHex4();
              if (cp >= 0xD800 && cp < 0xDC00) {
                if (!(consume("\\u"))) fail("surrogate haijakamilika");
                unsigned low = parseHex4();
                if (low < 0xDC00 || low > 0xDFFF) fail("surrogate si sahihi");
                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
              }
              appendUtf8(out, cp);
              break;
            }
            default:
              fail("escape isiyojulikana");
          }
        }
      }

      Value parseNumber() {
        size_t start = i_;
        if (s_[i_] == '-') ++i_;
        auto digits = [&] {
          size_t from = i_;
          while (i_ < s_.size() && s_[i_] >= '0' && s_[i_] <= '9') ++i_;
          return i_ > from;
        };
        if (!digits()) fail("namba si sahihi");
        bool isFloat = false;
        if (i_ < s_.size() && s_[i_] == '.') {
          isFloat = true;
          ++i_;
          if (!digits()) fail("namba si sahihi");
        }
        if (i_ < s_.size() && (s_[i_] == 'e' || s_[i_] == 'E')) {
          isFloat = true;
          ++i_;
          if (i_ < s_.size() && (s_[i_] == '+' || s_[i_] == '-')) ++i_;
          if (!digits()) fail("namba si sahihi");
        }
        std::string text = s_.substr(start, i_ - start);
        if (!isFloat) {
          try {
            return Value::integer(std::stoll(text));
          } catch (const std::out_of_range&) {
            // too large for int64: fall through to a float
          }
        }
        return Value::real(std::strtod(text.c_str(), nullptr));
      }

      const std::string& s_;
      int line_;
      size_t i_ = 0;
    };

    void encodeString(const std::string& s, std::string& out) {
      out += '"';
      for (char c : s) {
        switch (c) {
          case '"':
            out += "\\\"";
            break;
          case '\\':
            out += "\\\\";
            break;
          case '\n':
            out += "\\n";
            break;
          case '\r':
            out += "\\r";
            break;
          case '\t':
            out += "\\t";
            break;
          default:
            if (static_cast<unsigned char>(c) < 0x20) {
              char buf[8];
              std::snprintf(buf, sizeof buf, "\\u%04x", static_cast<unsigned>(c));
              out += buf;
            } else {
              out += c;
            }
        }
      }
      out += '"';
    }

    void encodeValue(const Value& v, std::string& out, int depth, int line) {
      if (depth > 100) throw NuruError(line, "enkodi: data ina viwango vingi mno (au inajirudia)");
      switch (v.type()) {
        case Value::Type::Null:
          out += "null";
          return;
        case Value::Type::Bool:
          out += v.asBool() ? "true" : "false";
          return;
        case Value::Type::Int:
          out += std::to_string(v.asInt());
          return;
        case Value::Type::Float:
          if (!std::isfinite(v.asFloat()))
            throw NuruError(line, "enkodi: DESIMALI isiyo na kikomo haiwezi kuwa JSON");
          out += formatFloat(v.asFloat());
          return;
        case Value::Type::String:
          encodeString(v.asString(), out);
          return;
        case Value::Type::Array: {
          out += '[';
          bool first = true;
          for (const Value& item : v.asArray().items) {
            if (!first) out += ',';
            first = false;
            encodeValue(item, out, depth + 1, line);
          }
          out += ']';
          return;
        }
        case Value::Type::Dict: {
          out += '{';
          bool first = true;
          for (const auto& [key, value] : v.asDict().entries) {
            if (!first) out += ',';
            first = false;
            encodeString(key.display(), out);
            out += ':';
            encodeValue(value, out, depth + 1, line);
          }
          out += '}';
          return;
        }
        default:
          throw NuruError(line, std::string("enkodi: ") + v.typeName() + " haiwezi kuwa JSON");
      }
    }
  }  // namespace

  Value createJsoni() {
    Value pkg = makePackage("jsoni", "PAKEJI");
    addFunction(pkg, "dikodi", [](Interpreter&, std::vector<Value>& args, NamedArgs&, int line) {
      expectArgs("dikodi", args, 1, 1, line);
      return JsonParser(stringArg("dikodi", args[0], line), line).parse();
    });
    addFunction(pkg, "enkodi", [](Interpreter&, std::vector<Value>& args, NamedArgs&, int line) {
      expectArgs("enkodi", args, 1, 1, line);
      std::string out;
      encodeValue(args[0], out, 0, line);
      return Value::string(std::move(out));
    });
    return pkg;
  }

}  // namespace mni::native
