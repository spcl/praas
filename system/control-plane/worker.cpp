
#include <charconv>

#include <sockpp/tcp_connector.h>
#include <thread>

#include <redis++.h>

#include "worker.hpp"
#include "server.hpp"
#include "resources.hpp"
#include "backend.hpp"

namespace praas::control_plane {

  // FIXME: move to utils
  std::tuple<std::string_view, std::string_view> split(std::string & str, char split_character)
  {
    auto pos = str.find(split_character);
    if(pos == std::string::npos)
      return std::make_tuple("", "");
    else
      return std::make_tuple(
        std::string_view(str.data(), pos),
        std::string_view(str.data() + pos + 1, str.length() - pos - 1)
      );
  }

  Worker::Worker(Server& server, sw::redis::Redis& redis, Resources& resources, backend::Backend& backend):
    generator{rd()},
    uuid_generator{generator},
    server(server),
    redis_conn(redis),
    resources(resources),
    backend(backend)
  {
    payload_buffer_len = 1024;
    payload_buffer.reset(new int8_t[payload_buffer_len]);

  }

  void Worker::resize(ssize_t size)
  {
    if(payload_buffer_len < size || !payload_buffer) {
      payload_buffer_len = size;
      payload_buffer.reset(new int8_t[payload_buffer_len]);
    }
  }

  std::string Worker::process_allocation(std::string process_name)
  {
    std::string id = uuids::to_string(uuid_generator()).substr(0, 16);
    redis_conn.set("PROCESS_" + id, process_name);

    spdlog::debug("Added instance of {} with id  {}", process_name, id);
    return id;
  }

  std::tuple<int, std::string> Worker::process_client(
    std::string process_id, std::string session_id, std::string function_name,
    std::string function_id, int32_t max_functions, int32_t memory_size, std::string && payload
  )
  {
    spdlog::debug(
      "Request to invoke process {}, function {} with id {}, with session {}, payload size {}",
      process_id, function_name, function_id, session_id, payload.length()
    );
    auto process_name = redis_conn.get("PROCESS_" + process_id);
    if(!process_name.has_value())
      return std::make_tuple(400, "Process doesn't exist!");

    bool swap_in = false;
    // Check if we're swapping
    if(!session_id.empty()) {

      if(redis_conn.sismember("SESSIONS_ALIVE_" + process_id, session_id)) {
        return std::make_tuple(400, "Session is already alive!");
      } else if(redis_conn.srem("SESSIONS_SWAPPED_" + process_id, session_id) == 1) {

        spdlog::info("Swapping a session {} back in!", session_id);
        auto val = redis_conn.get("SESSION_" + session_id);
        if(!val.has_value()) {
          return std::make_tuple(400, "Couldn't locate session details!");
        }

        // Retrieve configuration from Redis
        auto [mem, func] = split(val.value(), ';');
        std::from_chars(func.data(), func.data() + func.length(), max_functions);
        std::from_chars(mem.data(), mem.data() + mem.length(), memory_size);
        memory_size *= -1;
        swap_in = true;

        spdlog::info(
          "Retrieve session: {}, max functions {}, memory size {}.",
          session_id, max_functions, memory_size
        );

      } else {
        return std::make_tuple(400, "Session ID unknown!");
      }
    } else {

      session_id = uuids::to_string(uuid_generator()).substr(0, 16);
      spdlog::info("Allocate new session: {}!", session_id);

      redis_conn.set(
        "SESSION_" + session_id,
        std::to_string(memory_size) + ";" + std::to_string(max_functions)
      );
    }

    // Allocate new process worker and new session
    std::string id = uuids::to_string(uuid_generator()).substr(0, 16);
    spdlog::info("Allocate new process worker: {}!", id);

    // Allocate subprocess objects.
    Process process;
    process.process_id = id;
    process.global_process_id = process_id;
    process.allocated_sessions = 0;
    Process & process_ref = resources.add_process(std::move(process));

    Session & session = resources.add_session(process_ref, session_id);
    session.max_functions = max_functions;
    session.memory_size = memory_size;
    session.swap_in = swap_in;
    if(!function_name.empty()) {
      PendingAllocation alloc{std::move(payload), function_name, function_id};
      session.allocations.push_back(std::move(alloc));
    }

    backend.allocate_process(process_name.value(), id, 1);
    redis_conn.sadd("SESSIONS_ALIVE_" + process_id, session_id);

    return std::make_tuple(200, session_id);
  }

  void Worker::process_process(sockpp::tcp_socket * conn, praas::common::ProcessMessage* msg)
  {
    std::string process_id = msg->process_id();
    spdlog::debug(
      "Received connection from a process {}",
      process_id
    );
    Process* proc = resources.get_process(process_id);
    if(!proc) {
      spdlog::error("Incorrect process identification! Incorrect id {}!", process_id);
      // FIXME: send error message to client?
      conn->close();
      delete conn;
      return;
    }
    // Store process connection
    proc->connection = std::move(*conn);
    delete conn;

    // Check if we have unallocated session
    praas::common::SessionRequest req;
    for(Session * session : proc->sessions) {
      if(!session->allocated) {
        std::string session_id = session->session_id;
        // if memory is swapped in, then we send a negative amount of memory
        ssize_t size = req.fill(session_id, session->max_functions, session->memory_size);
        // Send allocation request
        proc->connection.write(req.data, size);

        // Wait for reply
        int32_t ret = 0;
        ssize_t bytes = proc->connection.read(&ret, sizeof(ret));
        if(bytes != sizeof(ret) || ret) {
          spdlog::error(
            "Incorrect allocation of sesssion {}, received {} bytes, error code {}",
            session_id, bytes, ret
          );
          // FIXME: notify user
        } else {
          spdlog::debug("Succesful allocation of sesssion {}", session_id);
          session->allocated = true;
        }
      }
    }

    // Add for future message polling
    if(!server.add_epoll(proc->connection.handle(), proc, EPOLLIN | EPOLLPRI)) {
      spdlog::error("Couldn't add the process to epoll,closing connection");
      proc->connection.close();
      resources.remove_process(process_id);
      return;
    }

  }

  void Worker::process_session(sockpp::tcp_socket * conn, praas::common::SessionMessage* msg)
  {
    std::string session_id = msg->session_id();
    spdlog::debug(
      "Received connection from a session {}",
      session_id
    );
    Session* session = resources.get_session(session_id);
    if(!session) {
      spdlog::error("Incorrect session identification! Incorrect session {}!", session_id);
      // FIXME: send error message to client?
      conn->close();
      delete conn;
      return;
    }
    // Store process connection
    session->connection = std::move(*conn);
    delete conn;
    spdlog::debug(
      "Succesful connection of sesssion {} from {}.",
      session_id, session->connection.peer_address().to_string()
    );

    // Process invocations
    auto & allocations = session->allocations;
    praas::common::FunctionRequest req;
    // No allocations - let the session know to stop polling.
    if(!allocations.size()) {
      // No invocations.
      req.fill("", "", 0);
      // Send function header
      session->connection.write(req.data, req.EXPECTED_LENGTH);
    } else {
      while(!allocations.empty()) {

        auto& item = allocations.front();

        size_t payload_size = item.payload.length();
        // Invoke function
        req.fill(item.function_name, item.function_id, payload_size);
        // Send function header
        session->connection.write(req.data, req.EXPECTED_LENGTH);
        // Send function payload
        session->connection.write(item.payload.c_str(), payload_size);

        spdlog::debug(
          "Invoking function {}, invocation id {}, on session {}, sending {} bytes.",
          item.function_name, item.function_id, session_id, payload_size
        );

        allocations.pop_front();
      }
    }

  }

  void Worker::handle_process_closure(Process*)
  {
    throw std::runtime_error("Not implemented yet");
  }

  void Worker::handle_session_closure(Process* process, int32_t, std::string session_id)
  {
    // Remove resource
    resources.remove_session(*process, session_id);

    // Update Redis - move session from swapped to alive
    redis_conn.srem("SESSIONS_ALIVE_" + process->process_id, session_id);

    // Store the following format: <session-id>;<swapped-memory-size>
    // Semicolon is not permitted in the session id
    redis_conn.sadd(
      "SESSIONS_SWAPPED_" + process->global_process_id,
      session_id
    );
    redis_conn.srem(
      "SESSIONS_ALIVE_" + process->global_process_id,
      session_id
    );
  }

  void Worker::handle_message(Process* process, praas::common::Header buffer, ssize_t recv_data)
  {
    auto thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
    spdlog::debug("Worker {} begins processing a message request", thread_id);
    Worker& worker = Workers::get(std::this_thread::get_id());
    sockpp::tcp_socket& conn = process->connection;

    // EOF
    if(recv_data == 0) {
      spdlog::debug("End of file on connection with {}.", conn.peer_address().to_string());
      worker.handle_process_closure(process);
      return;
    }
    // Incorrect read
    // FIXME: handle timeout & interruption
    else if(recv_data == -1) {
      spdlog::debug(
        "Closing connection with {}. Reason: {}", conn.peer_address().to_string(),
        strerror(errno)
      );
      worker.handle_process_closure(process);
      return;
    }

    // Correctly received request
    std::unique_ptr<praas::common::MessageType> msg = buffer.parse();
    if(!msg) {
      spdlog::error(
        "Incorrect message of size {} received from {}.",
        recv_data, conn.peer_address().to_string()
      );
      return;
    }

    if(msg.get()->type() == praas::common::MessageType::Type::SESSION_CLOSURE) {
      auto casted_msg = dynamic_cast<praas::common::SessionClosureMessage*>(msg.get());
      worker.handle_session_closure(process, casted_msg->memory_size(), casted_msg->session_id());
    } else {
      spdlog::error("Unknown type of request!");
    }

    spdlog::debug("Worker {} ends processing a request", thread_id);
  }

  void Worker::worker(sockpp::tcp_socket * conn)
  {
    auto thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
    Worker& worker = Workers::get(std::this_thread::get_id());

    ssize_t recv_data = conn->read(worker.header.data, praas::common::Header::BUF_SIZE);

    // EOF
    if(recv_data == 0) {
      spdlog::debug("End of file on connection with {}.", conn->peer_address().to_string());
      conn->close();
      delete conn;
    }
    // Incorrect read
    else if(recv_data == -1) {
      spdlog::debug(
        "Closing connection with {}. Reason: {}", conn->peer_address().to_string(),
        strerror(errno)
      );
      conn->close();
      delete conn;
    }
    // Correctly received request
    else {

      std::unique_ptr<praas::common::MessageType> msg = worker.header.parse();
      if(!msg) {
        spdlog::error(
          "Incorrect message of size {} received, closing connection with {}.",
          recv_data, conn->peer_address().to_string()
        );
        conn->close();
        delete conn;
        return;
      }

      if(msg.get()->type() == praas::common::MessageType::Type::CLIENT) {
        // FIXME: no longer supported
        conn->close();
        delete conn;
      } else if(msg.get()->type() == praas::common::MessageType::Type::PROCESS) {
        worker.process_process(conn, dynamic_cast<praas::common::ProcessMessage*>(msg.get()));
      } else if(msg.get()->type() == praas::common::MessageType::Type::SESSION) {
        worker.process_session(conn, dynamic_cast<praas::common::SessionMessage*>(msg.get()));
      } else {
        // FIXME: implement handling sessions
        spdlog::error("Unknown type of request!");
        conn->close();
        delete conn;
      }

    }
    spdlog::debug("Worker {} ends processing a request", thread_id);
  }

  std::unordered_map<std::thread::id, Worker*> Workers::_workers;
  Server* Workers::_server;
  sw::redis::Redis* Workers::_redis_conn;
  Resources* Workers::_resources;
  backend::Backend* Workers::_backend;

  void Workers::init(Server& server, sw::redis::Redis& redis_conn, Resources& resources, backend::Backend& backend)
  {
    Workers::_redis_conn = &redis_conn;
    Workers::_resources = &resources;
    Workers::_backend = &backend;
    Workers::_server = &server;
  }

  void Workers::free()
  {
    for(auto & v : _workers)
      delete v.second;
  }

  Worker& Workers::get(std::thread::id thread_id)
  {
    auto it = _workers.find(thread_id);
    if(it != _workers.end()) {
      return *(*it).second;
    } else {
      Worker* ptr = new Worker(*_server, *_redis_conn, *_resources, *_backend);
      _workers[thread_id] = ptr;
      return *ptr;
    }
  }
}
