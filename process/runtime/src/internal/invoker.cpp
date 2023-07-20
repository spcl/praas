#include <praas/process/runtime/internal/invoker.hpp>

#include <praas/common/application.hpp>
#include <praas/common/exceptions.hpp>
#include <praas/common/util.hpp>
#include <praas/process/runtime/internal/buffer.hpp>
#include <praas/process/runtime/internal/ipc/messages.hpp>

#include <optional>
#include <variant>

#include <sys/prctl.h>
#include <sys/signal.h>

#include <spdlog/spdlog.h>

namespace praas::process::runtime::internal {

  Invoker::Invoker(std::string process_id, ipc::IPCMode ipc_mode, const std::string& ipc_name)
      : _process_id(std::move(process_id))
  {
    if (ipc_mode == ipc::IPCMode::POSIX_MQ) {
      _ipc_channel_read = std::make_unique<ipc::POSIXMQChannel>(
          ipc_name + "_write", ipc::IPCDirection::READ, true, false
      );
      _ipc_channel_write = std::make_unique<ipc::POSIXMQChannel>(
          ipc_name + "_read", ipc::IPCDirection::WRITE, false, false
      );
    }

    // Make sure we are killed if the parent controller forgets about us.
    prctl(PR_SET_PDEATHSIG, SIGHUP);

    _logger = common::util::create_logger("Invoker");

    // FIXME: receive full status
    _app_status.active_processes.emplace_back(_process_id);
  }

  std::optional<Invocation> Invoker::poll()
  {
    Invocation invoc;
    bool received_invocation = false;

    while (!received_invocation && !_ending) {

      try {
        auto read = _ipc_channel_read->blocking_receive(_input);

        if (!read) {
          throw praas::common::PraaSException(
              fmt::format("Did not receive a full buffer - failed receive!")
          );
        }

        auto parsed_msg = _ipc_channel_read->message().parse();

        std::visit(
            ipc::overloaded{
                [&](ipc::InvocationRequestParsed& req) mutable {
                  SPDLOG_LOGGER_DEBUG(
                      _logger, "Received invocation request of {}, key {}, inputs {}",
                      req.function_name(), req.invocation_id(), req.buffers()
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

                  received_invocation = true;
                },
                [&](ipc::ApplicationUpdateParsed& req) mutable {
                  SPDLOG_LOGGER_DEBUG(
                      _logger, "Received application update - process change for {}",
                      req.process_id()
                  );
                  _app_status.update(
                      static_cast<common::Application::Status>(req.status_change()),
                      req.process_id()
                  );
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
    }

    return invoc;
  }

  void Invoker::finish(std::string_view invocation_id, BufferAccessor<char> output, int return_code)
  {
    ipc::InvocationResult msg;
    msg.return_code(return_code);
    msg.buffer_length(output.len);
    msg.invocation_id(invocation_id);

    _ipc_channel_write->send(msg, output);
  }

  void Invoker::put(ipc::Message& msg, BufferAccessor<std::byte> payload)
  {
    _ipc_channel_write->send(msg, payload);
  }

  std::tuple<ipc::GetRequestParsed, Buffer<char>> Invoker::get(ipc::Message& msg)
  {
    // Send GET request, zero payload.
    _ipc_channel_write->send(msg, BufferAccessor<std::byte>{});

    return this->get<ipc::GetRequestParsed>();
  }

  void Invoker::shutdown()
  {
    _ending = true;
    _ipc_channel_read.reset();
    _ipc_channel_write.reset();
  }

  Context Invoker::create_context()
  {
    return Context{_process_id, *this};
  }

} // namespace praas::process::runtime::internal
