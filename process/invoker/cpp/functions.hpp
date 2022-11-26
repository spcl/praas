#ifndef PRAAS_PROCESS_INVOKER_FUNCTIONS_HPP
#define PRAAS_PROCESS_INVOKER_FUNCTIONS_HPP

#include <praas/function/context.hpp>
#include <praas/function/invocation.hpp>
#include <praas/process/runtime/functions.hpp>

#include <memory>
#include <string>
#include <unordered_map>

namespace praas::process {

  struct FunctionsLibrary {

    static constexpr runtime::functions::Language LANGUAGE = runtime::functions::Language::CPP;

    using FuncType = int (*)(praas::function::Invocation, praas::function::Context&);

    FunctionsLibrary(std::string code_location, std::string config_location);
    FunctionsLibrary(const FunctionsLibrary&) = default;
    FunctionsLibrary(FunctionsLibrary&&) = default;
    FunctionsLibrary& operator=(const FunctionsLibrary&) = default;
    FunctionsLibrary& operator=(FunctionsLibrary&&) = delete;
    ~FunctionsLibrary();

    FuncType get_function(std::string name);

    bool _load_function(
        const std::string& function_name, const std::string& library_name,
        const std::string& library_function
    );

  private:
    std::unordered_map<std::string, void*> _libraries;
    std::unordered_map<std::string, void*> _functions;

    runtime::functions::Functions _func_db;
  };

} // namespace praas::process

#endif
