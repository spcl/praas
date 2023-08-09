#ifndef PRAAS_PROCESS_RUNTIME_INVOKER_HPP
#define PRAAS_PROCESS_RUNTIME_INVOKER_HPP

#include <praas/common/application.hpp>
#include <praas/process/runtime/context.hpp>
#include <praas/process/runtime/internal/buffer.hpp>
#include <praas/process/runtime/internal/ipc/ipc.hpp>
#include <praas/process/runtime/invocation.hpp>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

namespace praas::process::runtime::internal {

  struct Invoker {

    Invoker(std::string process_id, ipc::IPCMode ipc_mode, const std::string& ipc_name);

    std::optional<Invocation> poll();

    void finish(std::string_view invocation_id, BufferAccessor<const char> output, int return_code);

    void finish(std::string_view invocation_id, std::string_view error_message);

    void shutdown();

    void put(ipc::Message& msg, BufferAccessor<std::byte> payload);
    void put(ipc::Message& msg, BufferAccessor<const char> payload);

    std::tuple<ipc::GetRequestParsed, Buffer<char>> get(ipc::Message& msg);

    template <typename MsgType>
    std::tuple<MsgType, Buffer<char>> get()
    {
      auto [read, data] = _ipc_channel_read->receive();
      if (!read) {
        throw common::FunctionGetFailure{"Failed get - forgot to send reason"};
      }

      // Receive GET request result with payload.
      auto parsed_msg = _ipc_channel_read->message().parse();
      if (!std::holds_alternative<MsgType>(parsed_msg)) {
        throw common::FunctionGetFailure{"Received incorrect message!"};
      }

      auto& req = std::get<MsgType>(parsed_msg);
      return std::make_tuple(req, std::move(data));
    }

    Context create_context();

    common::Application& application()
    {
      return _app_status;
    }

  private:
    // Standard input size = 5 MB
    static constexpr int BUFFER_SIZE = 1024 * 1024 * 5;

    std::string _process_id;

    Buffer<std::byte> _input;

    common::Application _app_status;

    std::atomic<bool> _ending{};

    std::unique_ptr<ipc::IPCChannel> _ipc_channel_read;
    std::unique_ptr<ipc::IPCChannel> _ipc_channel_write;

    std::shared_ptr<spdlog::logger> _logger;
  };

} // namespace praas::process::runtime::internal

#endif
