#include <praas/process/controller/messages.hpp>

namespace praas::process::message {

  bool PendingMessages::insert_get(const std::string& key, const std::string& source, FunctionWorker& worker)
  {
    auto [_, success] = _msgs.try_emplace(std::tuple<std::string, std::string>(key, source), PendingMessage::Type::GET, &worker);
    return success;
  }

  bool MessageStore::put(const std::string& key, const std::string& source, runtime::Buffer<char> & payload)
  {
    auto [it, success] = _msgs.try_emplace(key, source, std::move(payload));
    return success;
  }

  std::optional<runtime::Buffer<char>> MessageStore::try_get(const std::string& key, std::string_view source)
  {
    auto it = _msgs.find(key);
    if(it == _msgs.end()) {
      return std::nullopt;
    }

    if(source != ANY_PROCESS && (*it).second.source != source) {
      return std::nullopt;
    }

    auto buf = std::move((*it).second.data);
    _msgs.erase(it);
    return buf;
  }

}
