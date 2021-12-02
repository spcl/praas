
#ifndef __CONTROLL_PLANE_WORKER_HPP__
#define __CONTROLL_PLANE_WORKER_HPP__

#include <future>
#include <memory>
#include <random>

#include <spdlog/spdlog.h>
#include <sockpp/tcp_socket.h>
#include <redis++.h>
#include <uuid.h>

namespace praas::control_plane {

  struct Header
  {
    // 4 bytes of packet size
    // 12 bytes of function name
    // 16 bytes of session id
    static constexpr uint16_t BUF_SIZE = 32;
    int8_t data[BUF_SIZE];
  };

  struct Worker
  {
    Header header;
    std::unique_ptr<int8_t[]> payload_buffer;
    ssize_t payload_buffer_len;
    std::random_device rd;
    std::mt19937 generator;
    uuids::uuid_random_generator uuid_generator;

    Worker():
      generator{rd()},
      uuid_generator{generator}
    {
      payload_buffer_len = 1024;
      payload_buffer.reset(new int8_t[payload_buffer_len]);

    }

    void resize(ssize_t size)
    {
      if(payload_buffer_len < size) {
        payload_buffer_len = size;
        payload_buffer.reset(new int8_t[payload_buffer_len]);
      }
    }
  };

  void worker(sockpp::tcp_socket * conn, sw::redis::Redis* redis_conn)
  {
    static Worker worker;

    conn->read(worker.header.data, Header::BUF_SIZE);
    ssize_t size = *reinterpret_cast<int*>(worker.header.data);
    std::string fname{reinterpret_cast<char*>(worker.header.data + 4)};
    std::string session_name{reinterpret_cast<char*>(worker.header.data + 18)};
    spdlog::debug("Request to invoke {} with session {}, payload size {}", fname, session_name, size);
    worker.resize(size);

    // Reiceve user payload
    auto user_payload  = std::async(
      std::launch::async,
      [&, conn]() { return conn->read(worker.payload_buffer.get(), worker.payload_buffer_len); }
    );
    // Read Redis key
    auto val = redis_conn->get(fname + "_" + session_name);
    // There's a session, redirect!
    if(val) {
      spdlog::info("Redirect invocation to worker");
    } else {
      spdlog::info("Allocate new session");
      uuids::uuid id = worker.uuid_generator();
      redis_conn->set(fname + "_" + uuids::to_string(id), "session");
    }
    // Wait to transfer payload from the user
    ssize_t payload_size = user_payload.get();

    spdlog::info(reinterpret_cast<char*>(worker.payload_buffer.get()));
    conn->write("hello client", 13);
    conn->close();
    delete conn;
  }

}

#endif

