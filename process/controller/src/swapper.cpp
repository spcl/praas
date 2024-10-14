#include <praas/process/controller/swapper.hpp>

#include <fstream>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace praas::process::swapper {

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

  size_t DiskSwapper::swap_out(const std::string& location, std::vector<std::tuple<std::string, message::Message>>& msgs)
  {
    size_t total_size = 0;

    for(const auto& [key, message] : msgs) {

      std::string path = message.source.empty() ? "state" : "messages";

      fs::path full_path = fs::path{swap_location} / location / fs::path(path) / key;

      spdlog::debug("Swapping out to {}", full_path.string());

      fs::create_directories(full_path.parent_path());

      std::ofstream out_file(full_path, std::ios::binary);
      if(!out_file) {
        spdlog::error("Unable to open file for writing: {}", full_path.string());
        return false;
      }
      out_file.write(message.data.data(), message.data.len);
      if(!out_file) {
        spdlog::error("Unable to write to file: {}", full_path.string());
        return false;
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
