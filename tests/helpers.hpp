#pragma once

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

#include <stgx/diagnostics.hpp>

namespace stgx_test {

inline std::filesystem::path data_file(std::string_view name) {
  return std::filesystem::path{STGX_TEST_DATA_DIR} / name;
}

inline std::string read_file(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  return std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
}

/// First diagnostic carrying `code`, so a test can assert on its path.
inline std::optional<stgx::Diagnostic> find(const stgx::Diagnostics& diags, stgx::DiagCode code) {
  const auto it = std::ranges::find_if(
      diags, [code](const stgx::Diagnostic& diag) { return diag.code == code; });
  if (it == diags.end()) return std::nullopt;
  return *it;
}

}  // namespace stgx_test
