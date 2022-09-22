
#include <future>

#include <thread>
#include <thread_pool.hpp>
#include <spdlog/spdlog.h>

#include "http.hpp"
#include "worker.hpp"

using praas::control_plane::Worker;
using praas::control_plane::Workers;

namespace praas::http {

  HttpServer::HttpServer(int port, std::string server_cert, std::string server_key, thread_pool & pool, bool verbose):
    _pool(pool)
  {
    spdlog::info(
      "Configuring HTTPS sever at port {}, server cert {}, server key {}",
      port, server_cert, server_key
    );
    _server.port(port).ssl_file(server_cert, server_key);

    CROW_ROUTE(_server, "/add_process").methods(crow::HTTPMethod::POST)
    ([this](const crow::request& req) -> crow::response {
      try {

        if(!req.body.length())
          return crow::response(400, "Error");

        auto x = crow::json::load(req.body);
        Worker& worker = Workers::get(std::this_thread::get_id());
        return worker.process_allocation(x["process-name"].s());

      } catch (std::exception & e) {
        std::cerr << e.what() << std::endl;
        return crow::response(400, "Error");
      } catch (...) {
        std::cerr << "err" << std::endl;
        return crow::response(400, "Error");
      }
    });

    CROW_ROUTE(_server, "/add_session").methods(crow::HTTPMethod::POST)
    ([this](const crow::request& req) -> crow::response {
      try {

        if(!req.body.length())
          return crow::response(400, "Error");

        auto x = crow::json::load(req.body);
        std::string session_id = x["session-id"].s();
        std::string process_id = x["process-id"].s();
        // Session retrieval, we don't need that
        int32_t max_functions = 0;
        int32_t memory_size = 0;
        if(session_id.empty()) {
          max_functions = x["max-functions"].i();
          memory_size = x["memory-size"].i();
        }

        spdlog::info(
          "Request to allocate/retrieve session {} at process id {}.",
          session_id, process_id
        );
        Worker& worker = Workers::get(std::this_thread::get_id());
        auto [code, msg] = worker.process_client(process_id, session_id, "", "", max_functions, memory_size, "");

        return crow::response(code, msg);
      } catch (std::exception & e) {
        std::cerr << e.what() << std::endl;
        return crow::response(400, "Error");
      } catch (...) {
        std::cerr << "err" << std::endl;
        return crow::response(400, "Error");
      }
    });

    CROW_ROUTE(_server, "/invoke").methods(crow::HTTPMethod::POST)
    ([this](const crow::request& req) -> crow::response {
      try {

        if(!req.body.length())
          return crow::response(400, "Error");

        crow::multipart::message msg(req);
        if(msg.parts.size() != 2)
          return crow::response(400, "Error");

        auto x = crow::json::load(msg.parts[0].body);
        std::string function_name = x["function-name"].s();
        std::string function_id = x["function-id"].s();
        std::string session_id = x["session-id"].s();
        std::string process_id = x["process-id"].s();
        // Session retrieval, we don't need that
        int32_t max_functions = 0;
        int32_t memory_size = 0;
        if(session_id.empty()) {
          max_functions = x["max-functions"].i();
          memory_size = x["memory-size"].i();
        }

        spdlog::info(
          "Request to invoke {} with id {}, at session {}, process id {}, payload size {}, session parameters: {} {}",
          function_name, function_id, session_id, process_id, msg.parts[1].body.length(),
          max_functions, memory_size
        );
        Worker& worker = Workers::get(std::this_thread::get_id());
        auto [code, ret_msg] = worker.process_client(
          process_id, session_id, function_name, function_id, max_functions,
          memory_size, std::move(msg.parts[1].body)
        );

        return crow::response(code, ret_msg);
      } catch (std::exception & e) {
        std::cerr << e.what() << std::endl;
        return crow::response(400, "Error");
      } catch (...) {
        std::cerr << "err" << std::endl;
        return crow::response(400, "Error");
      }
    });

    // We have our own handling of signals
    _server.signal_clear();
    if(verbose)
      _server.loglevel(crow::LogLevel::INFO);
    else
      _server.loglevel(crow::LogLevel::ERROR);
  }

  void HttpServer::run()
  {
    _server_thread = std::thread(&crow::App<>::run, &_server);
  }

  void HttpServer::shutdown()
  {
    spdlog::info("Stopping HTTP server");
    _server.stop();
    if(_server_thread.joinable())
      _server_thread.join();
    spdlog::info("Stopped HTTP server");
  }

}
