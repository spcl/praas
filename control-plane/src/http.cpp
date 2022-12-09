
#include <praas/control-plane/http.hpp>

#include <praas/common/util.hpp>
#include <praas/control-plane/worker.hpp>

#include <drogon/HttpAppFramework.h>
#include <future>
#include <thread>

#include <spdlog/spdlog.h>

namespace praas::control_plane {

  HttpServer::HttpServer(worker::Workers & workers, int port):
    _workers(workers),
    _port(port)
  {
    _logger = common::util::create_logger("HttpServer");
    std::cerr << _logger.get() << std::endl;
    _logger->info("test");
    std::cerr << this << std::endl;
    //_server.port(port).ssl_file(server_cert, server_key);

    // create application
    // creates application with given name
    // delete application

    // create process(id, settings, cloud settings) -> return data plane connection
    // delete process(id)
    // invoke(fname, id)

    // accept metrics(id, token, metrics)

    // CROW_ROUTE(_server, "/add_process")
    //     .methods(crow::HTTPMethod::POST
    //     )([this](const crow::request& req) -> crow::response {
    //       try {

    //        if (!req.body.length())
    //          return crow::response(400, "Error");

    //        auto x = crow::json::load(req.body);
    //        Worker& worker = Workers::get(std::this_thread::get_id());
    //        return worker.process_allocation(x["process-name"].s());

    //      } catch (std::exception& e) {
    //        std::cerr << e.what() << std::endl;
    //        return crow::response(400, "Error");
    //      } catch (...) {
    //        std::cerr << "err" << std::endl;
    //        return crow::response(400, "Error");
    //      }
    //    });

    // CROW_ROUTE(_server, "/add_session")
    //     .methods(crow::HTTPMethod::POST
    //     )([this](const crow::request& req) -> crow::response {
    //       try {

    //        if (!req.body.length())
    //          return crow::response(400, "Error");

    //        auto x = crow::json::load(req.body);
    //        std::string session_id = x["session-id"].s();
    //        std::string process_id = x["process-id"].s();
    //        // Session retrieval, we don't need that
    //        int32_t max_functions = 0;
    //        int32_t memory_size = 0;
    //        if (session_id.empty()) {
    //          max_functions = x["max-functions"].i();
    //          memory_size = x["memory-size"].i();
    //        }

    //        spdlog::info(
    //            "Request to allocate/retrieve session {} at process id {}.",
    //            session_id, process_id
    //        );
    //        Worker& worker = Workers::get(std::this_thread::get_id());
    //        auto [code, msg] = worker.process_client(
    //            process_id, session_id, "", "", max_functions, memory_size, ""
    //        );

    //        return crow::response(code, msg);
    //      } catch (std::exception& e) {
    //        std::cerr << e.what() << std::endl;
    //        return crow::response(400, "Error");
    //      } catch (...) {
    //        std::cerr << "err" << std::endl;
    //        return crow::response(400, "Error");
    //      }
    //    });

    // CROW_ROUTE(_server, "/invoke")
    //     .methods(crow::HTTPMethod::POST
    //     )([this](const crow::request& req) -> crow::response {
    //       try {

    //        if (!req.body.length())
    //          return crow::response(400, "Error");

    //        crow::multipart::message msg(req);
    //        if (msg.parts.size() != 2)
    //          return crow::response(400, "Error");

    //        auto x = crow::json::load(msg.parts[0].body);
    //        std::string function_name = x["function-name"].s();
    //        std::string function_id = x["function-id"].s();
    //        std::string session_id = x["session-id"].s();
    //        std::string process_id = x["process-id"].s();
    //        // Session retrieval, we don't need that
    //        int32_t max_functions = 0;
    //        int32_t memory_size = 0;
    //        if (session_id.empty()) {
    //          max_functions = x["max-functions"].i();
    //          memory_size = x["memory-size"].i();
    //        }

    //        spdlog::info(
    //            "Request to invoke {} with id {}, at session {}, process id "
    //            "{}, payload size {}, session parameters: {} {}",
    //            function_name, function_id, session_id, process_id,
    //            msg.parts[1].body.length(), max_functions, memory_size
    //        );
    //        Worker& worker = Workers::get(std::this_thread::get_id());
    //        auto [code, ret_msg] = worker.process_client(
    //            process_id, session_id, function_name, function_id,
    //            max_functions, memory_size, std::move(msg.parts[1].body)
    //        );

    //        return crow::response(code, ret_msg);
    //      } catch (std::exception& e) {
    //        std::cerr << e.what() << std::endl;
    //        return crow::response(400, "Error");
    //      } catch (...) {
    //        std::cerr << "err" << std::endl;
    //        return crow::response(400, "Error");
    //      }
    //    });

    // We have our own handling of signals
    //_server.signal_clear();
    //if (verbose)
    //  _server.loglevel(crow::LogLevel::INFO);
    //else
    //  _server.loglevel(crow::LogLevel::ERROR);
  }

  void HttpServer::run()
  {
    //_server_thread = std::thread(&crow::App<>::run, &_server);
    // FIXME: number of threads - config file
    drogon::app().registerController(shared_from_this());
    _server_thread = std::thread{
      [this]() {
        std::cerr << this << std::endl;
        drogon::app().addListener("0.0.0.0", _port).run();
      }
    };
  }

  void HttpServer::shutdown()
  {
    spdlog::info("Stopping HTTP server");
    drogon::app().getLoop()->queueInLoop([]() { drogon::app().quit(); });
    _server_thread.join();
    spdlog::info("Stopped HTTP server");
  }

  drogon::HttpResponsePtr failed_response(const std::string& reason)
  {
      Json::Value json;
      json["reason"] = reason;
      auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
      resp->setStatusCode(drogon::k500InternalServerError);
      return resp;
  }

  void HttpServer::invoke(
    const drogon::HttpRequestPtr& request,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    std::string app_id, std::string fname
  )
  {
    Json::Value empty_val;
    auto json = request->jsonObject();
    if(!json) {
      callback(failed_response("Couldn't parse JSON"));
    }

    auto func_name = request->getParameter("function");
    auto invocation_id = request->getParameter("invocation_id");
    //if(!func_name.isNull()) {
    //  std::cerr << func_name << std::endl;
    //} else {
    //  std::cerr << "empty" << std::endl;
    //}
    std::cerr << func_name << " " << invocation_id << std::endl;

    //std::cerr << request->getParameter("function") << std::endl;
    //std::cerr << request->getBody() << std::endl;
    //std::cerr << request->getJsonError() << std::endl;
    //for(auto [key, value] : request->getHeaders())
    //  std::cerr << key << " " << value << std::endl;

    //std::cerr << _logger << std::endl;
    _logger->info("Received request");
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setBody(
        "Hello, this is a generic hello message from the SayHello "
        "controller");
    callback(resp);
  }

} // namespace praas::control_plane
