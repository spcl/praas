
#ifndef __CONTROLL_PLANE_WORKER_HPP__
#define __CONTROLL_PLANE_WORKER_HPP__

#include <future>
#include <memory>
#include <random>
#include <unordered_map>

#include <spdlog/spdlog.h>
#include <sockpp/tcp_socket.h>
#include <uuid.h>

#include "../common/messages.hpp"

namespace sw::redis {
  struct Redis;
}

namespace praas::control_plane {

  struct Resources;
  namespace backend {
    struct Backend;
  }

  struct Worker
  {
    praas::common::Header header;
    std::unique_ptr<int8_t[]> payload_buffer;
    ssize_t payload_buffer_len;
    std::random_device rd;
    std::mt19937 generator;
    uuids::uuid_random_generator uuid_generator;
    sw::redis::Redis& redis_conn;
    Resources& resources;
    backend::Backend& backend;

    Worker(sw::redis::Redis& redis, Resources& resources, backend::Backend& backend);

    void resize(ssize_t size);
    //void process_client(sockpp::tcp_socket * conn, praas::common::ClientMessage*);
    std::string process_allocation(std::string process_name);
    std::string process_client(
      std::string process_id, std::string session_id, std::string function_name,
      std::string function_id, std::string && payload
    );
    void process_process(sockpp::tcp_socket * conn, praas::common::ProcessMessage*);
    void process_session(sockpp::tcp_socket * conn, praas::common::SessionMessage*);
    static void worker(sockpp::tcp_socket * conn);
  };

  struct Workers
  {
    static std::unordered_map<std::thread::id, Worker*> _workers;
    static sw::redis::Redis* _redis_conn;
    static Resources* _resources;
    static backend::Backend* _backend;

    static void init(sw::redis::Redis& redis_conn, Resources& resources, backend::Backend& backend);
    static void free();
    static Worker& get(std::thread::id thread_id);
  };

}

#endif

