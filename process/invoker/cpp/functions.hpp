#ifndef PRAAS_PROCESS_INVOKER_FUNCTIONS_HPP
#define PRAAS_PROCESS_INVOKER_FUNCTIONS_HPP

#include <praas/process/runtime/context.hpp>
#include <praas/process/runtime/internal/functions.hpp>
#include <praas/process/runtime/invocation.hpp>

#include <memory>
#include <string>
#include <unordered_map>

namespace praas::process {

  struct FunctionsLibrary {

    static constexpr runtime::internal::Language LANGUAGE = runtime::internal::Language::CPP;

    using FuncType = int (*)(runtime::Invocation, runtime::Context&);

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

    runtime::internal::Functions _func_db;
  };

} // namespace praas::process

#endif
