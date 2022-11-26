#include <praas/process/invoker.hpp>
#include <praas/process/runtime/ipc/messages.hpp>
#include <praas/common/exceptions.hpp>

#include <optional>
#include <variant>

namespace praas::process {

  Invoker::Invoker(runtime::ipc::IPCMode ipc_mode, std::string ipc_name)
  {
    if (ipc_mode == runtime::ipc::IPCMode::POSIX_MQ) {
      _ipc_channel_read = std::make_unique<runtime::ipc::POSIXMQChannel>(
          ipc_name + "_write", runtime::ipc::IPCDirection::READ, false
      );
      _ipc_channel_write = std::make_unique<runtime::ipc::POSIXMQChannel>(
          ipc_name + "_read", runtime::ipc::IPCDirection::WRITE, false
      );
    }
  }

  std::optional<praas::function::Invocation> Invoker::poll()
  {
    praas::function::Invocation invoc;

    try {
      auto [buf, input] = _ipc_channel_read->receive();

      auto parsed_msg = buf.parse();

      std::visit(
          runtime::ipc::overloaded{
              [&](runtime::ipc::InvocationRequestParsed& req) mutable {
                spdlog::info(
                    "Received invocation request of {}, id {}, inputs {}",
                    req.function_name(), req.invocation_id(), req.buffers()
                );
                invoc.key = req.invocation_id();
                invoc.function_name = req.function_name();

                char* ptr = input.val;
                for(int i = 0; i < req.buffers(); ++i) {

                  size_t len = req.buffers_lengths()[i];
                  invoc.args.emplace_back(ptr, len);
                  ptr += len;
                };
              },
              [](auto&) { spdlog::error("Received unsupported message!"); }},
          parsed_msg
      );

    } catch(praas::common::PraaSException & exc) {
      if(_ending) {
        spdlog::info("Shutting down the invoker");
      } else {
        spdlog::error("Unexpected end of the invoker {}", exc.what());
        return std::nullopt;
      }
    }

    return invoc;
  }

  void Invoker::finish(praas::function::Invocation&)
  {
  }

  void Invoker::shutdown()
  {
    _ending = true;
  }

  function::Context Invoker::create_context()
  {
    return function::Context{*this};
  }

} // namespace praas::process
