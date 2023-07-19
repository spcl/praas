#include <praas/process/controller/messages.hpp>

#include <praas/common/util.hpp>

#include <compare>
#include <iostream>

#include <spdlog/spdlog.h>

namespace praas::process::message {

  PendingMessages::PendingMessages()
  {
    _logger = common::util::create_logger("PendingMessages");
  }

  void PendingMessages::insert_get(
      const std::string& key, std::string_view source, FunctionWorker& worker
  )
  {
    SPDLOG_LOGGER_DEBUG(
        _logger, "Inserting worker pending for a message with name {}, from {}", key, source
    );
    _msgs.emplace(
        std::piecewise_construct, std::forward_as_tuple(key),
        std::forward_as_tuple(PendingMessage::Type::GET, std::string{source}, &worker)
    );
  }

  void PendingMessages::insert_invocation(std::string_view key, FunctionWorker& worker)
  {
    _msgs.emplace(
        std::piecewise_construct, std::forward_as_tuple(key),
        std::forward_as_tuple(PendingMessage::Type::INVOCATION, "", &worker)
    );
  }

  const FunctionWorker* PendingMessages::find_get(const std::string& key, std::string_view source)
  {
    auto [begin, end] = _msgs.equal_range(key);

    SPDLOG_LOGGER_DEBUG(
        _logger, "Searching for pending message with name {}, from {}", key, source
    );
    // Find the first matching function to consume the message
    for (auto iter = begin; iter != end; ++iter) {

      if (((*iter).second.source == ANY_PROCESS || (*iter).second.source == source) &&
          (*iter).second.type == PendingMessage::Type::GET) {
        auto* worker = (*iter).second.worker;
        _msgs.erase(iter);
        return worker;
      }
    }
    SPDLOG_LOGGER_DEBUG(
        _logger, "Did not find a worker waiting for message with name {}, from {}", key, source
    );

    return nullptr;
  }

  void
  PendingMessages::find_invocation(std::string_view key, std::vector<const FunctionWorker*>& output)
  {
    // Invocations
    auto [begin, end] = _msgs.equal_range(std::string{key});
    if (begin == end)
      return;

    // Find the first matching function to consume the message
    for (auto iter = begin; iter != end; ++iter) {
      output.emplace_back((*iter).second.worker);
    }
    _msgs.erase(begin, end);
  }

  bool MessageStore::put(
      const std::string& key, const std::string& source, runtime::internal::Buffer<char>& payload
  )
  {
    auto [it, success] = _msgs.try_emplace(key, source, std::move(payload));
    return success;
  }

  bool MessageStore::state(const std::string& key, runtime::internal::Buffer<char>& payload)
  {
    auto [it, success] = _msgs.try_emplace(key, "", std::move(payload));
    return success;
  }

  std::optional<runtime::internal::Buffer<char>>
  MessageStore::try_get(const std::string& key, std::string_view source)
  {
    auto it = _msgs.find(key);
    if (it == _msgs.end()) {
      return std::nullopt;
    }

    if (source != ANY_PROCESS && (*it).second.source != source) {
      return std::nullopt;
    }

    auto buf = std::move((*it).second.data);
    _msgs.erase(it);
    return buf;
  }

  runtime::internal::Buffer<char>* MessageStore::try_state(const std::string& key)
  {
    auto it = _msgs.find(key);
    if (it == _msgs.end()) {
      return nullptr;
    }

    // Copy, do not move
    return &(*it).second.data;
  }

} // namespace praas::process::message
