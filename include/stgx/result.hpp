#pragma once

#include <expected>

#ifndef __cpp_lib_expected
#error "libstgx needs std::expected: build as C++23 with a toolchain that provides <expected>."
#endif

#include <stgx/diagnostics.hpp>

namespace stgx {

/// Fallible outcome of a libstgx operation.
///
/// On failure the error side carries every diagnostic that was found, not just
/// the first one:
///
/// ```cpp
/// auto graph = stgx::load_yaml_file("in.yaml");
/// if (!graph) {
///   std::cerr << graph.error().to_string() << '\n';
///   return 1;
/// }
/// ```
template <class T>
using Result = std::expected<T, Diagnostics>;

/// Wrap a single diagnostic as a failed `Result`.
[[nodiscard]] inline std::unexpected<Diagnostics> fail(Diagnostic diag) {
  Diagnostics diags;
  diags.add(std::move(diag));
  return std::unexpected{std::move(diags)};
}

/// Wrap a single diagnostic as a failed `Result`.
[[nodiscard]] inline std::unexpected<Diagnostics> fail(DiagCode code, std::string path,
                                                       std::string message) {
  return fail(Diagnostic{.code = code,
                         .severity = Severity::Error,
                         .path = std::move(path),
                         .message = std::move(message)});
}

/// Wrap a collection of diagnostics as a failed `Result`.
[[nodiscard]] inline std::unexpected<Diagnostics> fail(Diagnostics diags) {
  return std::unexpected{std::move(diags)};
}

}  // namespace stgx
