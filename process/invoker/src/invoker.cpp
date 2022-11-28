#include <praas/process/invoker.hpp>

#include <praas/common/exceptions.hpp>
#include <praas/process/runtime/ipc/messages.hpp>

#include <optional>
#include <variant>

#include <sys/prctl.h>
#include <sys/signal.h>

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

    // Make sure we are killed if the parent controller forgets about us.
    prctl(PR_SET_PDEATHSIG, SIGHUP);
  }

  std::optional<praas::function::Invocation> Invoker::poll()
  {
    praas::function::Invocation invoc;

    try {
      auto read = _ipc_channel_read->blocking_receive(_input);

      if(!read) {
        throw praas::common::PraaSException(fmt::format(
            "Did not receive a full buffer - failed receive!"
        ));
      }

      auto parsed_msg = _ipc_channel_read->message().parse();

      std::visit(
          runtime::ipc::overloaded{
              [&](runtime::ipc::InvocationRequestParsed& req) mutable {
                spdlog::info(
                    "Received invocation request of {}, key {}, inputs {}", req.function_name(),
                    req.invocation_id(), req.buffers()
                );

                // Validate
                size_t total_len = 0;
                for (int i = 0; i < req.buffers(); ++i) {
                  total_len += req.buffers_lengths()[i];
                }
                if (total_len > _input.len) {
                  throw praas::common::PraaSException(fmt::format(
                      "Header declared {} bytes, but we only received {}!", total_len, _input.len
                  ));
                }

                invoc.key = req.invocation_id();
                invoc.function_name = req.function_name();

                std::byte* ptr = _input.ptr.get();
                for (int i = 0; i < req.buffers(); ++i) {

                  size_t len = req.buffers_lengths()[i];
                  invoc.args.emplace_back(ptr, len, len);
                  ptr += len;
                };
              },
              [](auto&) { spdlog::error("Received unsupported message!"); }},
          parsed_msg
      );

    } catch (praas::common::PraaSException& exc) {
      if (_ending) {
        spdlog::info("Shutting down the invoker");
      } else {
        spdlog::error("Unexpected end of the invoker {}", exc.what());
        return std::nullopt;
      }
    }

    return invoc;
  }

  void Invoker::finish(praas::function::Context& context, int return_code)
  {
    runtime::BufferAccessor<char> buf = context.as_buffer();

    runtime::ipc::InvocationResult msg;
    msg.return_code(return_code);
    msg.buffer_length(buf.len);
    msg.invocation_id(context.invocation_id());

    // FIXME: restore buffer

    _ipc_channel_write->send(msg, buf);
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
