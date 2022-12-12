
#include <praas/function/invocation.hpp>
#include <praas/function/context.hpp>

#include <chrono>

#include <aws/core/Aws.h>
//#include <aws/core/auth/AWSCredentialsProvider.h>
//#include <aws/core/client/ClientConfiguration.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <spdlog/fmt/bundled/core.h>

#include "types.hpp"

bool s3_send(Aws::S3::S3Client& client, Aws::String const &bucket, Aws::String const &key, int size, char* pBuf)
{
  const std::shared_ptr<Aws::IOStream> input_data = std::make_shared<boost::interprocess::bufferstream>(pBuf, size);

  Aws::S3::Model::PutObjectRequest request;
  request.WithBucket(bucket).WithKey(key);
  request.SetBody(input_data);
  Aws::S3::Model::PutObjectOutcome outcome = client.PutObject(request);
  if (!outcome.IsSuccess()) {
    std::cerr << "Error: PutObject: " << outcome.GetError().GetMessage() << std::endl;
    return false;
  }
  return true;
}

long s3_receive(Aws::S3::S3Client& client, Aws::String const &bucket, Aws::String const &key, int &required_retries, bool with_backoff)
{
  Aws::S3::Model::GetObjectRequest request;
  request.WithBucket(bucket).WithKey(key);

  auto begin = std::chrono::high_resolution_clock::now();
  int retries = 0;
  const int MAX_RETRIES = 1500;
  while (retries < MAX_RETRIES) {
      auto outcome = client.GetObject(request);
      if (outcome.IsSuccess()) {
          auto end = std::chrono::high_resolution_clock::now();
          auto& s = outcome.GetResult().GetBody();
          //uint64_t finishedTime = timeSinceEpochMillisec();
          // Perform NOP on result to prevent optimizations
          //std::stringstream ss;
          //ss << s.rdbuf();
          //std::string first(" ");
          //ss.get(&first[0], 1);
          //return finishedTime - bef;
          return std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count();
      } else {
          retries += 1;
          if(with_backoff) {
            int sleep_time = retries;
            //if (retries > 100) {
            //    sleep_time = retries * 2;
            //}
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
          }
      }
  }
  return 0;
}

extern "C" int s3_sender(praas::function::Invocation invocation, praas::function::Context& context)
{
  Invocations in;
  invocation.args[0].deserialize(in);
  std::cerr << "Start benchmark, input size " << invocation.args[0].len << " bucket " << in.bucket << std::endl;

  Results res;

  Aws::SDKOptions options;
  Aws::InitAPI(options);
  {

  Aws::String bucket = in.bucket;
  Aws::S3::S3Client client;
  std::cerr << "Max size " << in.sizes.back() << std::endl;
  std::unique_ptr<char[]> ptr{new char[in.sizes.back()]};

  long poll_time = 0;
  for(int size : in.sizes) {

    res.measurements.emplace_back();
    auto my_id = context.process_id();
    for(int i = 0; i  < in.repetitions + 1; ++i) {

      Aws::String first_key = fmt::format("send_{}_{}", size, i);
      Aws::String second_key = fmt::format("recv_{}_{}", size, i);

      int retries = 0;
      auto begin = std::chrono::high_resolution_clock::now();
      if(in.sender) {
        if(!s3_send(client, bucket, first_key, size, ptr.get())) {
          return 1;
        }
        poll_time = s3_receive(client, bucket, second_key, retries, true);
        if(poll_time <= 0) {
          return 1;
        }
      } else {
        poll_time = s3_receive(client, bucket, first_key, retries, true);
        if(poll_time <= 0) {
          return 1;
        }
        if(!s3_send(client, bucket, second_key, size, ptr.get())) {
          return 1;
        }
      }
      auto end = std::chrono::high_resolution_clock::now();

      if(i > 0)
        res.measurements.back().emplace_back(
          std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count(),
          poll_time
        );

      if(i % 10 == 0) {
       std::cerr << i << std::endl;
      }

    }

  }

  auto& output_buf = context.get_output_buffer(in.repetitions * in.sizes.size() * sizeof(long) *2 + 64);
  output_buf.serialize(res);
  }
  Aws::ShutdownAPI(options);


  return 0;
}

