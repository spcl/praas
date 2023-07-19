#ifndef PRAAS_PROCESS_RUNTIME_FUNCTIONS_HPP
#define PRAAS_PROCESS_RUNTIME_FUNCTIONS_HPP

#include <memory>
#include <string>
#include <unordered_map>

#include <cereal/external/rapidjson/fwd.h>

namespace praas::process::runtime::internal {

  enum class Language { CPP = 0, PYTHON, NONE };

  std::string language_to_string(Language val);

  Language string_to_language(std::string language);

  struct TriggerVisitor;

  struct Trigger {

    enum class Type { DIRECT, MULTI_SOURCE, BATCH, PIPELINE, DEPENDENCY };

    Trigger(std::string name) : _name(std::move(name)) {}

    ~Trigger() = default;

    virtual Type type() const = 0;

    virtual void accept(TriggerVisitor&) const = 0;

    static std::unique_ptr<Trigger> parse(const std::string& fname, const rapidjson::Value& obj);

    std::string_view name() const;

  private:
    std::string _name;
  };

  struct DirectTrigger : Trigger {

    DirectTrigger(std::string name) : Trigger(std::move(name)) {}

    Type type() const override;

    void accept(TriggerVisitor&) const;
  };

  struct TriggerVisitor {

    virtual void visit(const DirectTrigger&) = 0;
  };

  struct Function {

    Function(
        const std::string& module_name, const std::string& function_name,
        std::unique_ptr<Trigger> trigger
    )
        : trigger(std::move(trigger)), module_name(module_name), function_name(function_name)
    {
    }

    std::unique_ptr<Trigger> trigger;

    std::string module_name;
    std::string function_name;

    void load_config(std::istream&);
  };

  struct Functions {

    using container_t = std::unordered_map<std::string, Function>;
    using citer_t = typename container_t::const_iterator;

    void initialize(std::istream& in_stream, Language language);

    const Trigger* get_trigger(std::string name) const;

    const Function* get_function(std::string name) const;

    citer_t begin() const
    {
      return _functions.begin();
    }

    citer_t end() const
    {
      return _functions.end();
    }

  private:
    std::unordered_map<std::string, Function> _functions;
  };

} // namespace praas::process::runtime::internal

#endif
