
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
    ([this](const crow::request& req) -> std::string {
      try {

        if(!req.body.length())
          return "Error";

        auto x = crow::json::load(req.body);
        Worker& worker = Workers::get(std::this_thread::get_id());
        return worker.process_allocation(x["process-name"].s());

      } catch (std::exception & e) {
        std::cerr << e.what() << std::endl;
        return "Error";
      } catch (...) {
        std::cerr << "err" << std::endl;
        return "Error";
      }
    });

    CROW_ROUTE(_server, "/add_session").methods(crow::HTTPMethod::POST)
    ([this](const crow::request& req) -> std::string {
      try {

        if(!req.body.length())
          return "Error";

        auto x = crow::json::load(req.body);
        std::string session_id = x["session-id"].s();
        std::string process_id = x["process-id"].s();
        spdlog::info(
          "Request to allocate session {} at process id {}.",
          session_id, process_id
        );
        Worker& worker = Workers::get(std::this_thread::get_id());
        return worker.process_client(process_id, session_id, "", "", "");

      } catch (std::exception & e) {
        std::cerr << e.what() << std::endl;
        return "Error";
      } catch (...) {
        std::cerr << "err" << std::endl;
        return "Error";
      }
    });

    CROW_ROUTE(_server, "/invoke").methods(crow::HTTPMethod::POST)
    ([this](const crow::request& req) -> std::string {
      try {

        if(!req.body.length())
          return "Error";

        crow::multipart::message msg(req);
        if(msg.parts.size() != 2)
          return "Error";

        auto x = crow::json::load(msg.parts[0].body);
        std::string function_name = x["function-name"].s();
        std::string function_id = x["function-id"].s();
        std::string session_id = x["session-id"].s();
        std::string process_id = x["process-id"].s();
        spdlog::info(
          "Request to invoke {} with id {}, at session {}, process id {}, payload size {}",
          function_name, function_id, session_id, process_id, msg.parts[1].body.length()
        );
        Worker& worker = Workers::get(std::this_thread::get_id());
        return worker.process_client(
          process_id, session_id, function_name, function_id, std::move(msg.parts[1].body)
        );

      } catch (std::exception & e) {
        std::cerr << e.what() << std::endl;
        return "Error";
      } catch (...) {
        std::cerr << "err" << std::endl;
        return "Error";
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
