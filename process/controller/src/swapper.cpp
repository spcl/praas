#include <praas/process/controller/swapper.hpp>

#include <aws/core/utils/memory/stl/AWSAllocator.h>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/ListObjectsV2Request.h>
#include <aws/core/utils/stream/PreallocatedStreamBuf.h>
#include <boost/interprocess/streams/bufferstream.hpp>

namespace fs = std::filesystem;

namespace praas::process::swapper {

  std::optional<S3Swapper::S3API> S3Swapper::api;

  template<typename F>
  size_t directory_recursive(const fs::path& source, const fs::path& destination, F && f)
  {
    size_t total_size = 0;

    if (!fs::exists(source) || !fs::is_directory(source)) {
      return 0;
    }

    if (!fs::exists(destination)) {
        fs::create_directories(destination);
    }

    for (const auto& entry : fs::directory_iterator(source)) {

      const auto& path = entry.path();
      const auto destination_path = destination / path.filename();

      if (fs::is_directory(path)) {
        total_size += directory_recursive(path, destination_path, std::forward<F>(f));
      } else if (fs::is_regular_file(path)) {
        f(path, destination_path);
        total_size += fs::file_size(path);
      }
    }

    return total_size;
  }

  template<typename F>
  size_t directory_recursive(const fs::path& source, F && f)
  {
    size_t total_size = 0;

    if (!fs::exists(source) || !fs::is_directory(source)) {
      return 0;
    }

    for (const auto& entry : fs::directory_iterator(source)) {

      const auto& path = entry.path();

      if (fs::is_directory(path)) {
        total_size += directory_recursive(path, std::forward<F>(f));
      } else if (fs::is_regular_file(path)) {
        f(path);
        total_size += fs::file_size(path);
      }
    }

    return total_size;
  }

  DiskSwapper::DiskSwapper()
  {
    auto *ptr = std::getenv("SWAPS_LOCATION");
    if(ptr  == nullptr) {
      spdlog::error("Disk swapper created but no location specified!");
    }
    swap_location = std::string{ptr};
  }

  template<typename F>
  bool _process_swapin_dir(fs::path directory, F && f)
  {

    for(const auto& file : fs::directory_iterator{directory}) {

      if(!fs::is_regular_file(file)) {
        continue;
      }

      ssize_t size = fs::file_size(file);
      runtime::internal::Buffer<char> data;
      data.resize(size);

      std::ifstream input_str{file.path(), std::ios::in | std::ios::binary};
      if(input_str) {

        input_str.read(data.data(), size);
        if(input_str.gcount() != size) {
          spdlog::error("Couldn't read {} bytes from file {}!", size, file.path().string());
          return false;
        }

        data.len = size;
        if(!f(file.path().filename(), std::move(data))) {
          return false;
        }

      } else {
        spdlog::error("Couldn't read from file {}!", file.path().string());
        return false;
      }

    }

    return true;
  }

  bool DiskSwapper::swap_in(const std::string& location, message::MessageStore & mailbox)
  {
    bool success = false;

    fs::path full_path = fs::path{swap_location} / location / "state";
    if(fs::exists(full_path)) {

      // There is at least some info.
      success = true;

      spdlog::debug("Reading state swap data from {}", full_path.string());
      success = _process_swapin_dir(full_path,
        [&mailbox](const std::string& key, runtime::internal::Buffer<char> && data) mutable {
          spdlog::debug("Put state {}, data size{}", key, data.len);
          mailbox.state(key, data);
          return true;
        }
      );
      if(!success) {
        return false;
      }
    }

    full_path = fs::path{swap_location} / location / "messages";
    if(fs::exists(full_path)) {

      spdlog::debug("Reading messages swap data from {}", full_path.string());
      success = _process_swapin_dir(full_path,
        [&mailbox](const std::string& key, runtime::internal::Buffer<char> && data) mutable {

          size_t pos = key.find('(');
          if(pos == std::string::npos) {
            spdlog::error("Couldn't parse the message key {}", key);
            return false;
          }

          spdlog::debug("Put message {}, data size{}", key, data.len);
          mailbox.put(key.substr(0, pos), key.substr(pos + 1), data);

          return true;
        }
      );
      if(!success) {
        return false;
      }
    }

    full_path = fs::path{swap_location} / location / "files";
    if(fs::exists(full_path)) {

      directory_recursive(
        full_path, FILES_DIRECTORY,
        [](auto & src, auto & dest) {

          fs::copy_file(src, dest, fs::copy_options::overwrite_existing);
          spdlog::debug("Copied state file from {} to {}", src.string(), dest.string());

        }
      );
    };

    return success;
  }

  size_t DiskSwapper::swap_out(const std::string& location, std::vector<std::tuple<std::string, message::Message>>& msgs)
  {
    size_t total_size = 0;

    // Create directory even if nothing present - just to make
    fs::create_directories(fs::path{swap_location} / location / "state");
    fs::create_directories(fs::path{swap_location} / location / "messages");
    fs::create_directories(fs::path{swap_location} / location / "file");

    for(const auto& [key, message] : msgs) {

      fs::path full_path;
      if(message.source.empty()) {
        full_path = fs::path{swap_location} / location / "state" / key;
      } else {
        full_path = fs::path{swap_location} / location / "messages" / fmt::format("{}({}", key, message.source);
      }

      spdlog::debug("Swapping out disk to {}", full_path.string());

      fs::create_directories(full_path.parent_path());

      std::ofstream out_file(full_path, std::ios::binary);
      if(!out_file) {
        spdlog::error("Unable to open file for writing: {}", full_path.string());
        return 0;
      }
      out_file.write(message.data.data(), message.data.len);
      if(!out_file) {
        spdlog::error("Unable to write to file: {}", full_path.string());
        return 0;
      }

      total_size += message.data.len;

    }

    fs::path full_path = fs::path{swap_location} / location / "files";
    total_size += directory_recursive(
      FILES_DIRECTORY, full_path,
      [](auto & src, auto & dest) {

        fs::copy_file(src, dest, fs::copy_options::overwrite_existing);
        spdlog::debug("Copied state file from {} to {}", src.string(), dest.string());

      }
    );

    return total_size;
  }

  S3Swapper::S3API::S3API()
  {
    Aws::InitAPI(_s3_options);
  }

  S3Swapper::S3API::~S3API()
  {
    Aws::ShutdownAPI(_s3_options);
  }

  S3Swapper::S3Swapper(std::string swap_bucket):
    swap_bucket(std::move(swap_bucket))
  {
    if(!api.has_value()) {
      api.emplace();
    }

    // https://github.com/aws/aws-sdk-cpp/issues/1410
    putenv("AWS_EC2_METADATA_DISABLED=true");
    Aws::Client::ClientConfiguration client_cfg("default", true);
    client_cfg.maxConnections = MAX_CONNECTIONS;
    _s3_client.emplace(client_cfg);
  }

  // https://stackoverflow.com/questions/21073655/c-how-do-i-ignore-the-first-directory-path-when-comparing-paths-in-boostfil
  inline fs::path strip(fs::path p)
  {
    p = p.relative_path();
    if (p.empty()) return {};
    return p.lexically_relative(*p.begin());
  }

  struct CustomContext : public Aws::Client::AsyncCallerContext
  {
    CustomContext(size_t size):
      data(new char[size], size),
      streambuf(reinterpret_cast<unsigned char*>(data.data()), size)
    {
      data.len = size;
    }

    runtime::internal::Buffer<char> data;
    Aws::Utils::Stream::PreallocatedStreamBuf streambuf;
  };

  template<int N>
  size_t find_nth(const std::string& input, char pattern, int start_pos = 0)
  {
    if constexpr (N <= 0) {
      return start_pos;
    }

    size_t pos = input.find(pattern, start_pos);
    if(pos == std::string::npos) return pos;
    return find_nth<N-1>(input, pattern, pos + 1);
  }

  template<>
  size_t find_nth<0>(const std::string& input, char pattern, int start_pos)
  {
    return start_pos;
  }

  bool S3Swapper::swap_in(const std::string& location, message::MessageStore & mailbox)
  {
    size_t total_size = 0;
    fs::path full_path = fs::path{SWAPS_DIRECTORY} / location;

    Aws::S3::Model::ListObjectsV2Request list_request;
    list_request.SetBucket(swap_bucket);
    list_request.SetPrefix(full_path.string());
    int requests = 0, requests_total = 0;
    std::condition_variable cv;
    std::mutex mutex;

    auto callback = [&requests, &cv, &mutex](
      const Aws::S3::S3Client* client, const Aws::S3::Model::GetObjectRequest& req,
      const Aws::S3::Model::GetObjectOutcome& outcome, const std::shared_ptr<const Aws::Client::AsyncCallerContext>&
    ) {

      if (outcome.IsSuccess()) {
        spdlog::debug("Successfully downloaded object {}", req.GetKey());

        std::lock_guard<std::mutex> lock(mutex);
        requests++;
        cv.notify_one();
      } else {
        spdlog::error("Error downloading object {}, error {}", req.GetKey(), outcome.GetError().GetMessage());
      }
    };

    while(true) {

      auto result = _s3_client->ListObjectsV2(list_request);

      if(!result.IsSuccess()) {
        break;
      }

      const auto& objects = result.GetResult().GetContents();
      for (const auto& object : objects) {

        spdlog::debug(object.GetKey());
        size_t size = object.GetSize();
        std::string key_path{object.GetKey()};

        // strip swaps/app/proc
        auto pos = find_nth<3>(key_path, '/');
        auto next_pos = key_path.find('/', pos + 1);
        std::string type = key_path.substr(pos, next_pos - pos);
        auto actual_key = key_path.substr(next_pos + 1, key_path.size() - next_pos);

        if(type == "state" || type == "messages") {

          auto ctx = Aws::MakeShared<CustomContext>("whydoineedtag", size);

          Aws::S3::Model::GetObjectRequest req_ptr;
          req_ptr.SetBucket(swap_bucket);
          req_ptr.SetKey(object.GetKey());
          req_ptr.SetResponseStreamFactory([ctx]() { return Aws::New<Aws::IOStream>("", &ctx->streambuf); });

          if(type == "state") {
            spdlog::debug("Put state {}, data size{}", actual_key, ctx->data.len);
            mailbox.state(actual_key, ctx->data);
          } else {

            size_t pos = actual_key.find('(');
            if(pos == std::string::npos) {
              spdlog::error("Couldn't parse the message key {}", actual_key);
              continue;
            }

            spdlog::debug("Put message {}, data size {}", actual_key, ctx->data.len);
            mailbox.put(actual_key.substr(0, pos), actual_key.substr(pos + 1), ctx->data);
          }

          _s3_client->GetObjectAsync(req_ptr, callback, std::move(ctx));
          requests_total++;
        } else if (type == "files") {

          fs::path path = fs::path{FILES_DIRECTORY} / actual_key;
          fs::create_directories(path.parent_path());

          Aws::S3::Model::GetObjectRequest req_ptr;
          req_ptr.SetBucket(swap_bucket);
          req_ptr.SetKey(object.GetKey());
          req_ptr.SetResponseStreamFactory([=]() {
            return Aws::New<Aws::FStream>("anothertag", path.string().c_str(), std::ios_base::out | std::ios_base::binary);
          });

          _s3_client->GetObjectAsync(req_ptr, callback);
          requests_total++;
        } else {
          spdlog::error("Unknown type of swap data: {}", type);
        }

      }

      if (result.GetResult().GetIsTruncated()) {
        list_request.SetContinuationToken(result.GetResult().GetNextContinuationToken());
      } else {
        break;
      }
    }

    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&requests, requests_total]() { return requests == requests_total; });

    spdlog::debug("Swap in finished!");

    return true;
  }

  size_t S3Swapper::swap_out(const std::string& location, std::vector<std::tuple<std::string, message::Message>>& msgs)
  {
    size_t total_size = 0;
    fs::path full_path = fs::path{SWAPS_DIRECTORY} / location;

    int requests = 0, requests_min = msgs.size(), requests_total = msgs.size();
    std::condition_variable cv;
    std::mutex mutex;

    auto callback = [&requests, requests_min, &cv, &mutex](
      const Aws::S3::S3Client* client, const Aws::S3::Model::PutObjectRequest& req,
      const Aws::S3::Model::PutObjectOutcome& outcome, const std::shared_ptr<const Aws::Client::AsyncCallerContext>&
    ) {

      if (outcome.IsSuccess()) {
        spdlog::debug("Successfully uploaded object {}", req.GetKey());
      } else {
        spdlog::error("Error uploading object {}, error {}", req.GetKey(), outcome.GetError().GetMessage());
      }

      {
        std::lock_guard<std::mutex> lock(mutex);
        requests++;
        if(requests >= requests_min) {
          cv.notify_one();
        }
      }
    };

    auto make_request = [this, &callback](const fs::path& new_path, std::shared_ptr<Aws::IOStream>&& ptr)
    {
      Aws::S3::Model::PutObjectRequest req;
      req.SetBucket(swap_bucket);
      req.SetKey(new_path);
      req.SetBody(ptr);

      _s3_client->PutObjectAsync(req, callback);
    };

    for(const auto& [key, message] : msgs) {

      fs::path new_path;
      if(message.source.empty()) {
        new_path = full_path / "state" / key;
      } else {
        new_path = full_path / "messages" / fmt::format("{}({}", key, message.source);
      }
      spdlog::debug("Swapping out S3 to location {}", new_path.string());

      std::shared_ptr<Aws::IOStream> input_data = std::make_shared<boost::interprocess::bufferstream>(message.data.data(), message.data.len);
      make_request(new_path, std::move(input_data));

      total_size += message.data.len;
    }

    full_path = fs::path{SWAPS_DIRECTORY} / location / "files";
    total_size += directory_recursive(
      FILES_DIRECTORY,
      [&full_path, &requests_total, &make_request](auto & src) {

        fs::path new_path = full_path / strip(src);
        spdlog::debug("Swapping out to location {}", new_path.string());

        std::shared_ptr<Aws::IOStream> input_data = Aws::MakeShared<Aws::FStream>("SampleAllocationTag", src.c_str(), std::ios_base::in | std::ios_base::binary);
        make_request(new_path, std::move(input_data));

        requests_total++;

      }
    );

    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&requests, requests_total]() { return requests == requests_total; });

    return total_size;
  }

} // namespace praas::process::swapper
