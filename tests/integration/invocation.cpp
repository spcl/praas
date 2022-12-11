
#include <praas/control-plane/config.hpp>
#include <praas/control-plane/server.hpp>
#include <praas/sdk/praas.hpp>

#include <chrono>
#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <cereal/archives/binary.hpp>

struct Input
{
  int arg1;
  int arg2;

  template<typename Ar>
  void save(Ar & archive) const
  {
    archive(CEREAL_NVP(arg1));
    archive(CEREAL_NVP(arg2));
  }

  template<typename Ar>
  void load(Ar & archive)
  {
    archive(CEREAL_NVP(arg1));
    archive(CEREAL_NVP(arg2));
  }
};

struct Output
{
  int result{};

  template<typename Ar>
  void save(Ar & archive) const
  {
    archive(CEREAL_NVP(result));
  }

  template<typename Ar>
  void load(Ar & archive)
  {
    archive(CEREAL_NVP(result));
  }
};

class IntegrationLocalInvocation : public ::testing::Test {
protected:
  void SetUp() override
  {
    praas::control_plane::config::Config cfg;
    cfg.set_defaults();

    _server = std::make_unique<praas::control_plane::Server>(cfg);
  }

  std::unique_ptr<praas::control_plane::Server> _server;
};

// FIXME: one testing framework
std::string generate_input_binary(int arg1, int arg2)
{
  Input input{arg1, arg2};
  std::stringstream str;
  cereal::BinaryOutputArchive archive_out{str};
  archive_out(cereal::make_nvp("input", input));
  return str.str();
}

int get_output_binary(const std::string & response)
{
  Output out;
  std::stringstream stream{response};
  cereal::BinaryInputArchive archive_in{stream};
  out.load(archive_in);

  return out.result;
}

TEST_F(IntegrationLocalInvocation, Invoke)
{
  _server->run();

  std::this_thread::sleep_for(std::chrono::milliseconds(250));

  {
    praas::sdk::PraaS praas{fmt::format("http://127.0.0.1:{}", _server->http_port())};

    EXPECT_TRUE(praas.create_application("test_app"));

    auto input = generate_input_binary(1, 2);
    auto invoc = praas.invoke("test_app", "add", input);
    EXPECT_EQ(invoc.return_code, 0);
    EXPECT_EQ(get_output_binary(invoc.response), 3);
  }


  _server->shutdown();
}
