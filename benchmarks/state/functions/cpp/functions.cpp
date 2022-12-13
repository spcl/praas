
#include <praas/function/invocation.hpp>
#include <praas/function/context.hpp>

#include <chrono>

#include <aws/core/Aws.h>
//#include <aws/core/auth/AWSCredentialsProvider.h>
//#include <aws/core/client/ClientConfiguration.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <hiredis/hiredis.h>
#include <spdlog/fmt/bundled/core.h>

#include "types.hpp"

bool redis_send(redisContext* context, std::string const &key, int size, char* pBuf)
{

  std::string comm = "SET " + key + " %b";

  redisReply* reply = (redisReply*) redisCommand(context, comm.c_str(), pBuf, size);

  if (reply->type == REDIS_REPLY_NIL || reply->type == REDIS_REPLY_ERROR) {
    std::cerr << "Failed to write in Redis!" << std::endl;
    return false;
  }
  freeReplyObject(reply);
  return true;
}

long redis_receive(redisContext* context, std::string const &key, int &required_retries, bool with_backoff)
{
  std::string comm = "GET " + key;
  int retries = 0;
  const int MAX_RETRIES = 50000;

  auto begin = std::chrono::high_resolution_clock::now();
  while (retries < MAX_RETRIES) {

    redisReply* reply = (redisReply*) redisCommand(context, comm.c_str());

    if (reply->type == REDIS_REPLY_NIL || reply->type == REDIS_REPLY_ERROR) {

      retries += 1;
      if(with_backoff) {
        int sleep_time = retries;
        //if (retries > 100) {
        //    sleep_time = retries * 2;
        //}
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
      }

    } else {
      auto end = std::chrono::high_resolution_clock::now();
      freeReplyObject(reply);
      return std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count();
    }
    freeReplyObject(reply);
  }
  return 0;
}

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

std::vector<long>* result;
Aws::S3::S3Client* client;

extern "C" int aws_sdk_init(praas::function::Invocation invocation, praas::function::Context& context)
{
  std::cerr << "Start" << std::endl;
  Aws::SDKOptions options;
  Aws::InitAPI(options);
  client = new Aws::S3::S3Client();
  result = new std::vector<long>();
  std::cerr << "finish" << std::endl;
  return 0;
}

extern "C" int s3_sender(praas::function::Invocation invocation, praas::function::Context& context)
{
  Invocations in;
  invocation.args[0].deserialize(in);
  std::cerr << "Start benchmark, input size " << invocation.args[0].len << " data size" << in.data.size() << std::endl;

  if(result->size() != in.data.size()) {
    result->resize(in.data.size());
    memset(result->data(), 0, sizeof(long)*in.data.size());
  }

  // Now perform the accumulation
  for(int i = 0; i < in.data.size(); ++i) {
    (*result)[i] += in.data[i];
  }

  // Save the result in S3
  Aws::String key = fmt::format("reduce_result_{}", result->size());
  s3_send(*client, in.bucket, key, result->size() * sizeof(long), reinterpret_cast<char*>(result->data()));

  //Results res;
  //auto& output_buf = context.get_output_buffer(in.repetitions * in.sizes.size() * sizeof(long) *2 + 64);
  //output_buf.serialize(res);

  return 0;
}

//extern "C" int redis_sender(praas::function::Invocation invocation, praas::function::Context& context)
//{
//  Invocations in;
//  invocation.args[0].deserialize(in);
//  std::cerr << "Start benchmark, input size " << invocation.args[0].len << " bucket " << in.bucket << std::endl;
//
//  redisContext* r_context = redisConnect(in.redis_hostname.c_str(), 6379);
//  if (r_context == nullptr || r_context->err) {
//    if (r_context) {
//      std::cerr << "Redis Error: " << r_context->errstr << '\n';
//    } else {
//      std::cerr << "Can't allocate redis context\n";
//    }
//    return 1;
//  }
//
//  Results res;
//
//  std::cerr << "Max size " << in.sizes.back() << std::endl;
//  std::unique_ptr<char[]> ptr{new char[in.sizes.back()]};
//
//  long poll_time = 0;
//  for(int size : in.sizes) {
//
//    res.measurements.emplace_back();
//    auto my_id = context.process_id();
//    for(int i = 0; i  < in.repetitions + 1; ++i) {
//
//      std::string first_key = fmt::format("send_{}_{}", size, i);
//      std::string second_key = fmt::format("recv_{}_{}", size, i);
//
//      int retries = 0;
//      auto begin = std::chrono::high_resolution_clock::now();
//      if(in.sender) {
//        if(!redis_send(r_context, first_key, size, ptr.get())) {
//          return 1;
//        }
//        poll_time = redis_receive(r_context, second_key, retries, true);
//        if(poll_time <= 0) {
//          return 1;
//        }
//      } else {
//        poll_time = redis_receive(r_context, first_key, retries, true);
//        if(poll_time <= 0) {
//          return 1;
//        }
//        if(!redis_send(r_context, second_key, size, ptr.get())) {
//          return 1;
//        }
//      }
//      auto end = std::chrono::high_resolution_clock::now();
//
//      if(i > 0)
//        res.measurements.back().emplace_back(
//          std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count(),
//          poll_time
//        );
//
//      if(i % 10 == 0) {
//       std::cerr << i << std::endl;
//      }
//
//    }
//
//  }
//
//  auto& output_buf = context.get_output_buffer(in.repetitions * in.sizes.size() * sizeof(long) *2 + 64);
//  output_buf.serialize(res);
//
//  return 0;
//}
//
//extern "C" int put_get(praas::function::Invocation invoc, praas::function::Context& context)
//{
//  std::string other_process_id;
//  if(context.active_processes()[0] == context.process_id()) {
//    other_process_id = context.active_processes()[1];
//  } else {
//    other_process_id = context.active_processes()[0];
//  }
//
//  Invocations in;
//  invoc.args[0].deserialize(in);
//  std::cerr << "Start benchmark, input size " << invoc.args[0].len << " my id "
//    << context.process_id() << " other id " << other_process_id << std::endl;
//
//  praas::function::Buffer buf = context.get_buffer(in.sizes.back());
//  Results res;
//
//  for(int size : in.sizes) {
//
//    buf.len = size;
//
//    res.measurements.emplace_back();
//    auto my_id = context.process_id();
//    for(int i = 0; i  < in.repetitions + 1; ++i) {
//
//      std::string msg_key = fmt::format("send_{}_{}", size, i);
//      std::string second_key = fmt::format("recv_{}_{}", size, i);
//
//      int retries = 0;
//      auto begin = std::chrono::high_resolution_clock::now();
//      if(in.sender) {
//        context.put(other_process_id, msg_key, buf.ptr, buf.len);
//        praas::function::Buffer get_buf = context.get(praas::function::Context::ANY, second_key);
//        if(get_buf.len <= 0)
//          return 1;
//      } else {
//        praas::function::Buffer get_buf = context.get(praas::function::Context::ANY, msg_key);
//        if(get_buf.len <= 0)
//          return 1;
//        context.put(other_process_id, second_key, buf.ptr, buf.len);
//      }
//      auto end = std::chrono::high_resolution_clock::now();
//
//      if(i > 0)
//        res.measurements.back().emplace_back(
//          std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count(),
//          0
//        );
//
//      if(i % 10 == 0) {
//       std::cerr << i << std::endl;
//      }
//
//    }
//
//  }
//
//  auto& output_buf = context.get_output_buffer(in.repetitions * in.sizes.size() * sizeof(long) *2 + 64);
//  output_buf.serialize(res);
//
//  return 0;
//}
//
