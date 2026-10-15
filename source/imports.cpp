#include <mni/interpreter.h>
#include <mni/parser.h>

#include <filesystem>
#include <fstream>
#include <sstream>

#include "native.h"

namespace mni {

  void Interpreter::setScriptPath(const std::string& path) {
    std::error_code ec;
    auto dir = std::filesystem::absolute(std::filesystem::path(path), ec).parent_path();
    currentDir_ = ec ? std::string() : dir.string();
  }

  Value Interpreter::evalPackage(const PackageExpression* e) {
    auto pkg = std::make_shared<Package>();
    pkg->name = e->name;
    pkg->typeName = "PAKEJI";
    pkg->env = std::make_shared<Environment>(env_->shared_from_this());
    pkg->env->setBoundary();
    Value value = Value::package(pkg);
    pkg->env->define("@", value);

    {
      struct Restore {
        Environment*& env;
        Environment* saved;
        ~Restore() { env = saved; }
      } restore{env_, env_};
      env_ = pkg->env.get();
      execBlock(e->body.get());
      if (flow_ != Flow::None) {
        flow_ = Flow::None;
        throw NuruError(
            e->token.line,
            "'rudisha', 'vunja' na 'endelea' haziruhusiwi moja kwa moja ndani ya pakeji");
      }
    }
    env_->define(e->name, value);
    return value;
  }

  Value Interpreter::evalImport(const ImportExpression* e) {
    for (const std::string& name : e->names) {
      const std::string bindName = std::filesystem::path(name).stem().string();
      env_->define(bindName, loadModule(name, e->token.line));
    }
    return Value();
  }

  Value Interpreter::loadModule(const std::string& name, int line) {
    const std::filesystem::path given(name);
    const bool plainName = given.extension().empty() && given.parent_path().empty();
    const std::string stem = given.stem().string();

    if (plainName) {
      if (auto it = modules_.find("native:" + stem); it != modules_.end()) return it->second;
      Value native = createNativeModule(stem);
      if (!native.isNull()) {
        modules_["native:" + stem] = native;
        return native;
      }
      if (isUnavailableModule(stem))
        throw NuruError(line, "Moduli '" + stem + "' haikujumuishwa kwenye ujenzi huu");
    }

    std::filesystem::path file = given;
    if (file.extension().empty()) file += ".nr";
    std::vector<std::filesystem::path> candidates;
    if (file.is_relative() && !currentDir_.empty())
      candidates.push_back(std::filesystem::path(currentDir_) / file);
    candidates.push_back(file);
    for (const auto& c : candidates) {
      std::error_code ec;
      if (std::filesystem::is_regular_file(c, ec)) return importFile(c.string(), stem, line);
    }
    throw NuruError(line, "Moduli au faili '" + name + "' haipatikani");
  }

  Value Interpreter::importFile(const std::string& path, const std::string& packageName, int line) {
    std::error_code ec;
    const std::string key = "file:" + std::filesystem::weakly_canonical(path, ec).string();
    if (auto it = modules_.find(key); it != modules_.end()) return it->second;
    for (const std::string& active : loading_)
      if (active == key)
        throw NuruError(line, "Faili '" + path + "' linajitumia lenyewe (mzunguko wa 'tumia')");

    std::ifstream in(path, std::ios::binary);
    if (!in) throw NuruError(line, "Imeshindwa kusoma faili: " + path);
    std::stringstream contents;
    contents << in.rdbuf();

    Parser parser(contents.str());
    auto program = std::make_unique<Program>(parser.parseProgram());
    if (!parser.errors().empty()) {
      std::vector<std::string> errors;
      for (const std::string& msg : parser.errors()) errors.push_back(path + ": " + msg);
      throw SyntaxError(std::move(errors));
    }
    programs_.push_back(std::move(program));
    const Program& prog = *programs_.back();

    // The file runs in its own scope (boundary: it cannot overwrite the importer's variables),
    // and its own imports resolve relative to its directory.
    auto moduleEnv = std::make_shared<Environment>(globals_);
    moduleEnv->setBoundary();
    struct Restore {
      Interpreter& self;
      Environment* env;
      std::string dir;
      ~Restore() {
        self.env_ = env;
        self.currentDir_ = std::move(dir);
        self.loading_.pop_back();
      }
    };
    loading_.push_back(key);
    Restore restore{*this, env_, currentDir_};
    env_ = moduleEnv.get();
    currentDir_ = std::filesystem::path(path).parent_path().string();
    execProgram(prog);

    Value* pkg = moduleEnv->lookupOwn(packageName);
    if (!pkg || pkg->type() != Value::Type::Package)
      throw NuruError(line, "Faili '" + path + "' halina pakeji '" + packageName + "'");
    modules_[key] = *pkg;
    return *pkg;
  }

}  // namespace mni
