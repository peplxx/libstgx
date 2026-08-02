#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace stgx {

/// Severity of a single diagnostic.
enum class Severity : uint8_t { Error, Warning };

/// Machine-readable diagnostic codes.
// NOLINTNEXTLINE(performance-enum-size)
enum class DiagCode : uint16_t {
  /// The input is not well-formed YAML, or its shape is not a graph document.
  YamlParse,
  /// `schemaVersion` is not a positive int, or names a version we cannot read.
  SchemaVersion,
  /// `nodes` is absent or empty — a graph needs at least one node.
  NodesMissing,
  /// A node has an empty `id`.
  NodeIdEmpty,
  /// Two nodes share the same `id`.
  NodeIdDuplicate,
  /// A requested node does not exist.
  NodeNotFound,
  /// An edge points at a node that does not exist.
  DanglingEdge,
  /// `initial` is not set on exactly one node.
  InitialCount,
  /// `system.tasks` is set and a node carries a different number of tasks.
  TaskArityMismatch,
  /// A value that must be an int is not one (`2.0`, `2e1`, `true`, ...).
  NotAnInt,
  /// A value has the wrong YAML shape — a scalar where a sequence is required,
  /// a sequence where a mapping is required, a non-boolean `initial`, ...
  WrongType,
  /// A `release` value is neither `up`/`down`/`none` nor a known alias.
  BadRelease,
  /// A field of the wire format that this version does not model (warning).
  UnsupportedField,
  /// A field that is not part of the wire format at all (warning).
  UnknownField,
  /// A file could not be read or written.
  IoError,
};

/// Stable lower_case name of a diagnostic code.
[[nodiscard]] std::string_view to_string(DiagCode code);

/// Stable lower_case name of a severity.
[[nodiscard]] std::string_view to_string(Severity severity);

/// One problem found while loading, validating or saving a graph.
struct Diagnostic {
  DiagCode code = DiagCode::YamlParse;
  Severity severity = Severity::Error;
  /// Location in wire-format terms, e.g. `nodes[3].children[0]`. May be empty.
  std::string path;
  /// Human-readable explanation.
  std::string message;

  [[nodiscard]] bool is_error() const noexcept { return severity == Severity::Error; }

  /// `error[dangling_edge] nodes[3].children[0]: unknown node "n99"`
  [[nodiscard]] std::string to_string() const;

  bool operator==(const Diagnostic&) const = default;
};

class Diagnostics {
 public:
  using value_type = Diagnostic;
  using const_iterator = std::vector<Diagnostic>::const_iterator;

  Diagnostics() = default;
  Diagnostics(std::initializer_list<Diagnostic> items) : items_(items) {}

  void add(Diagnostic diag) { items_.push_back(std::move(diag)); }

  void add(DiagCode code, std::string path, std::string message,
           Severity severity = Severity::Error) {
    items_.push_back(Diagnostic{.code = code,
                                .severity = severity,
                                .path = std::move(path),
                                .message = std::move(message)});
  }

  /// Append everything from `other`, keeping order.
  void merge(const Diagnostics& other) {
    items_.insert(items_.end(), other.items_.begin(), other.items_.end());
  }

  [[nodiscard]] bool empty() const noexcept { return items_.empty(); }
  [[nodiscard]] std::size_t size() const noexcept { return items_.size(); }
  [[nodiscard]] const Diagnostic& operator[](std::size_t index) const { return items_[index]; }
  [[nodiscard]] const_iterator begin() const noexcept { return items_.begin(); }
  [[nodiscard]] const_iterator end() const noexcept { return items_.end(); }

  /// True if at least one diagnostic has `Severity::Error`.
  [[nodiscard]] bool has_errors() const noexcept;

  /// True if any diagnostic carries `code` — handy in tests and dispatch.
  [[nodiscard]] bool contains(DiagCode code) const noexcept;

  /// Only the diagnostics with `Severity::Error`.
  [[nodiscard]] Diagnostics errors() const;

  /// Only the diagnostics with `Severity::Warning`.
  [[nodiscard]] Diagnostics warnings() const;

  /// One diagnostic per line, in order. No trailing newline.
  [[nodiscard]] std::string to_string() const;

  bool operator==(const Diagnostics&) const = default;

 private:
  std::vector<Diagnostic> items_;
};

namespace diag {

[[nodiscard]] Diagnostic node_not_found(std::string_view id);
[[nodiscard]] Diagnostic node_id_empty();
[[nodiscard]] Diagnostic node_id_duplicate(std::string_view id);

}  // namespace diag

}  // namespace stgx
