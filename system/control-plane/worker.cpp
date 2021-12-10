
#include <thread>

#include <redis++.h>

#include "worker.hpp"
#include "resources.hpp"
#include "backend.hpp"

namespace praas::control_plane {

  Worker::Worker(sw::redis::Redis& redis, Resources& resources, backend::Backend& backend):
    generator{rd()},
    uuid_generator{generator},
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
    spdlog::debug("Request to add process {}", process_name);

    std::string id = uuids::to_string(uuid_generator()).substr(0, 16);
    redis_conn.set("PROCESS_" + id, process_name);

    spdlog::debug("Added instance of {} with id  {}", process_name, id);
    return id;
  }

  std::string Worker::process_client(
    std::string process_id, std::string session_id, std::string function_name,
    std::string function_id, std::string && payload
  )
  {
    spdlog::debug(
      "Request to invoke process {}, function {} with id {}, with session {}, payload size {}",
      process_id, function_name, function_id, session_id, payload.length()
    );
    auto process_name = redis_conn.get("PROCESS_" + process_id);
    if(!process_name.has_value())
      return "Process doesn't exist!";

    // Check if we're swapping
    if(session_id != "") {
      if(redis_conn.sismember("SESSIONS_ALIVE_" + process_id, session_id)) {
        return "Session is already alive!";
      }
      if(redis_conn.sismember("SESSIONS_SWAPPED_" + process_id, session_id)) {
        return "Swapping in not supported yet!";
      }
    }

    // Allocate new process worker and new session
    std::string id = uuids::to_string(uuid_generator()).substr(0, 16);
    spdlog::info("Allocate new process worker: {}!", id);
    std::string new_session_id = uuids::to_string(uuid_generator()).substr(0, 16);
    spdlog::info("Allocate new session: {}!", new_session_id);

    // Allocate subprocess objects.
    Process process;
    process.process_id = id;
    process.allocated_sessions = 0;
    Process & process_ref = resources.add_process(std::move(process));

    Session & session = resources.add_session(process_ref, new_session_id);
    if(payload.length() >= 0) {
      PendingAllocation alloc{std::move(payload), function_name, function_id};
      session.allocations.push_back(std::move(alloc));
    }

    backend.allocate_process(process_name.value(), id, 1);
    redis_conn.sadd("SESSIONS_ALIVE_" + process_id, new_session_id);

    return new_session_id;
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
        // FIXME: user parameters - pass them
        ssize_t size = req.fill(session_id, 4, 4);
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
        }
        spdlog::debug("Succesful allocation of sesssion {}", session_id);
        session->allocated = true;
      }
    }

    // Add sessions to Redis.
    //redis_conn.set(process_name + "_" + id, "session");
    // FIXME: wait for confirmation

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

  void Worker::worker(sockpp::tcp_socket * conn)
  {
    auto thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
    spdlog::debug("Worker {} begins processing a request", thread_id);
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
      spdlog::debug("Closing connection with {}.", conn->peer_address().to_string());
      conn->close();
      delete conn;
    }
    // Correctly received request
    else {

      std::unique_ptr<praas::common::MessageType> msg = worker.header.parse(recv_data);
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
  sw::redis::Redis* Workers::_redis_conn;
  Resources* Workers::_resources;
  backend::Backend* Workers::_backend;

  void Workers::init(sw::redis::Redis& redis_conn, Resources& resources, backend::Backend& backend)
  {
    Workers::_redis_conn = &redis_conn;
    Workers::_resources = &resources;
    Workers::_backend = &backend;
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
      Worker* ptr = new Worker(*_redis_conn, *_resources, *_backend);
      _workers[thread_id] = ptr;
      return *ptr;
    }
  }
}
