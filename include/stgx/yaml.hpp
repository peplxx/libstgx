#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include <stgx/diagnostics.hpp>
#include <stgx/graph.hpp>
#include <stgx/result.hpp>

namespace stgx {

/// Parse a graph document.
///
/// Errors are collected, not thrown: a failed load reports every problem it
/// found at once. Warnings — fields of the wire format this version does not
/// model, unknown fields — cannot ride on the error side of a successful load,
/// so pass `warnings` if you care about them. They are filled in on failure
/// too.
///
/// ```cpp
/// stgx::Diagnostics warnings;
/// auto graph = stgx::load_yaml_file("in.yaml", &warnings);
/// if (!graph) return std::cerr << graph.error().to_string() << '\n', 1;
/// if (!warnings.empty()) std::cerr << warnings.to_string() << '\n';
/// ```
[[nodiscard]] Result<Graph> load_yaml_string(std::string_view yaml,
                                             Diagnostics* warnings = nullptr);

/// Read and parse a graph document from disk.
[[nodiscard]] Result<Graph> load_yaml_file(const std::filesystem::path& path,
                                           Diagnostics* warnings = nullptr);

/// Serialize a graph. Validates first — an invalid graph is never written.
///
/// Fields left at their default are omitted, so the output stays as close to a
/// hand-written document as the model allows.
[[nodiscard]] Result<std::string> save_yaml_string(const Graph& graph);

/// Serialize a graph to disk. Validates first.
[[nodiscard]] Result<void> save_yaml_file(const Graph& graph, const std::filesystem::path& path);

}  // namespace stgx
