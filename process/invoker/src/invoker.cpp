#include <praas/process/invoker.hpp>
#include <praas/process/ipc/messages.hpp>

#include <variant>

namespace praas::process {

  Invoker::Invoker(ipc::IPCMode ipc_mode, std::string ipc_name)
  {
    if (ipc_mode == ipc::IPCMode::POSIX_MQ) {
      _ipc_channel_read = std::make_unique<ipc::POSIXMQChannel>(
          ipc_name + "_write", ipc::IPCDirection::READ, false
      );
      _ipc_channel_write = std::make_unique<ipc::POSIXMQChannel>(
          ipc_name + "_read", ipc::IPCDirection::WRITE, false
      );
    }
  }

  praas::function::Invocation Invoker::poll()
  {
    praas::function::Invocation invoc;

    auto [buf, input] = _ipc_channel_read->receive();

    auto parsed_msg = buf.parse();

    std::visit(
        ipc::overloaded{
            [=](ipc::InvocationRequestParsed& req) {
              spdlog::info(
                  "Received invocation request of {}, inputs {}",
                  req.function_name(), req.buffers()
              );
            },
            [](auto&) { spdlog::error("Received unsupported message!"); }},
        parsed_msg
    );

    return invoc;
  }

  void Invoker::finish(praas::function::Invocation&) {}

  void Invoker::shutdown() {}
} // namespace praas::process
