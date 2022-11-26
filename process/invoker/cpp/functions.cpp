#include "functions.hpp"

#include <praas/common/exceptions.hpp>

#include <filesystem>
#include <fstream>

#include <cereal/external/rapidjson/document.h>
#include <cereal/external/rapidjson/istreamwrapper.h>

#include <dlfcn.h>

namespace praas::process {

  FunctionsLibrary::FunctionsLibrary(std::string code_location, std::string config_location)
  {
    auto path = std::filesystem::path{code_location} / config_location;
    std::ifstream in_stream{path};
    if (!in_stream.is_open()) {
      throw praas::common::PraaSException{fmt::format("Could not find file {}", path.c_str())};
    }
    _func_db.initialize(in_stream, LANGUAGE);

    for (const auto& func : _func_db) {
      if (!_load_function(func.first, func.second.module_name, func.second.function_name)) {
        spdlog::error(
            "Could not load {} from {}!", func.second.function_name, func.second.module_name
        );
      }
    }
  }

  bool FunctionsLibrary::_load_function(
      const std::string& function_name, const std::string& library_name,
      const std::string& library_function
  )
  {

    auto library = _libraries.find(library_name);
    void* library_handle = nullptr;
    if (library == _libraries.end()) {
      library_handle = dlopen(library_name.c_str(), RTLD_NOW);
      if (library_handle == nullptr) {
        spdlog::error("Couldn't open the library, reason: {}", dlerror());
        return false;
      }
      assert(library_handle);
      _libraries[library_name] = library_handle;
    } else {
      library_handle = library->second;
    }

    void* func_handle = dlsym(library_handle, library_function.c_str());
    if (func_handle == nullptr) {
      spdlog::error("Couldn't get the function, reason: {}", dlerror());
      return false;
    }
    assert(func_handle);
    _functions[function_name] = func_handle;

    return true;
  }

  FunctionsLibrary::~FunctionsLibrary()
  {
    for (auto [k, v] : _libraries) {
      dlclose(v);
    }
  }

  FunctionsLibrary::FuncType FunctionsLibrary::get_function(std::string name)
  {
    auto it = _functions.find(name);
    if (it == _functions.end()) {
      return nullptr;
    }
    return reinterpret_cast<FuncType>((*it).second);
  }
} // namespace praas::process
