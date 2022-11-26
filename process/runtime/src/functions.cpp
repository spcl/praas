#include <praas/process/runtime/functions.hpp>

#include <praas/common/exceptions.hpp>

#include <fstream>

#include <cereal/external/rapidjson/document.h>
#include <cereal/external/rapidjson/istreamwrapper.h>

#include <fmt/format.h>
#include <tuple>
#include <utility>

namespace praas::process::runtime::functions {

  std::string language_to_string(Language val)
  {
    switch (val) {
    case Language::CPP:
      return "cpp";
    case Language::PYTHON:
      return "python";
    case Language::NONE:
      return "";
    }
    return "";
  }

  Language string_to_language(std::string language)
  {
    if (language == "cpp") {
      return Language::CPP;
    }
    if (language == "python") {
      return Language::PYTHON;
    }
    return Language::NONE;
  }

  std::unique_ptr<Trigger> Trigger::parse(const std::string& fname, const rapidjson::Value & obj)
  {
    auto it = obj.FindMember("trigger");
    if(it == obj.MemberEnd() || !it->value.IsObject()) {
      throw common::InvalidJSON{fmt::format("Could not parse trigger configuration for {}", fname)};
    }

    std::string trigger_type = it->value["type"].GetString();
    if(trigger_type == "direct") {
      return std::make_unique<DirectTrigger>(fname);
    }

    throw common::InvalidJSON{fmt::format("Could not parse trigger type {}", trigger_type)};
  }

  void Functions::initialize(std::istream & in_stream, runtime::functions::Language language)
  {
    rapidjson::Document doc;
    rapidjson::IStreamWrapper wrapper{in_stream};
    doc.ParseStream(wrapper);

    std::string lang_str = language_to_string(language);

    auto it = doc["functions"].FindMember(lang_str.c_str());
    if(it == doc.MemberEnd() || !it->value.IsObject()) {
      throw common::InvalidJSON{fmt::format("Could not parse configuration for language {}", lang_str)};
    }

    for(const auto& function_cfg : it->value.GetObject()) {

      std::string fname = function_cfg.name.GetString();

      auto trigger = Trigger::parse(fname, function_cfg.value);

      auto it = function_cfg.value.FindMember("code");
      if (it == function_cfg.value.MemberEnd() || !it->value.IsObject()) {
        throw common::InvalidJSON{fmt::format("Could not parse code configuration for {}", fname)};
      }

      std::string module_name = it->value["module"].GetString();
      std::string function_name = it->value["function"].GetString();

      _functions.emplace(
          std::piecewise_construct,
          std::forward_as_tuple(fname),
          std::make_tuple(module_name, function_name, std::move(trigger))
      );

    }
  }

  const Trigger* Functions::get_trigger(std::string name) const
  {
    auto it = _functions.find(name);
    if(it != _functions.end()) {
      return it->second.trigger.get();
    }
    return nullptr;
  }

  const Function* Functions::get_function(std::string name) const
  {
    auto it = _functions.find(name);
    if(it != _functions.end()) {
      return &it->second;
    }
    return nullptr;
  }

  void DirectTrigger::accept(TriggerVisitor & visitor) const
  {
    visitor.visit(*this);
  }

  Trigger::Type DirectTrigger::type() const
  {
    return Type::DIRECT;
  }

  std::string_view Trigger::name() const
  {
    return _name;
  }

}
