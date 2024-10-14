#include <praas/process/controller/swapper.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

namespace praas::process::swapper {
  using std::ifstream;

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

      spdlog::info("Reading state swap data from {}", full_path.string());
      success = _process_swapin_dir(full_path,
        [&mailbox](const std::string& key, runtime::internal::Buffer<char> && data) mutable {
          spdlog::info("Put state {}, data size{}", key, data.len);
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

      spdlog::info("Reading messages swap data from {}", full_path.string());
      success = _process_swapin_dir(full_path,
        [&mailbox](const std::string& key, runtime::internal::Buffer<char> && data) mutable {

          size_t pos = key.find('(');
          if(pos == std::string::npos) {
            spdlog::error("Couldn't parse the message key {}", key);
            return false;
          }

          spdlog::info("Put message {}, data size{}", key, data.len);
          mailbox.put(key.substr(0, pos), key.substr(pos + 1), data);

          return true;
        }
      );

    }

    full_path = fs::path{swap_location} / location / "files";
    if(fs::exists(full_path)) {

      directory_recursive(
        full_path, FILES_DIRECTORY,
        [](auto & src, auto & dest) {

          fs::copy_file(src, dest, fs::copy_options::overwrite_existing);
          spdlog::info("Copied state file from {} to {}", src.string(), dest.string());

        }
      );
    };

    return success;
  }

  size_t DiskSwapper::swap_out(const std::string& location, std::vector<std::tuple<std::string, message::Message>>& msgs)
  {
    size_t total_size = 0;

    for(const auto& [key, message] : msgs) {

      fs::path full_path;
      if(message.source.empty()) {
        full_path = fs::path{swap_location} / location / "state" / key;
      } else {
        full_path = fs::path{swap_location} / location / "messages" / fmt::format("{}({}", key, message.source);
      }

      spdlog::debug("Swapping out to {}", full_path.string());

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
        spdlog::info("Copied state file from {} to {}", src.string(), dest.string());

      }
    );

    return total_size;
  }

} // namespace praas::process::swapper
