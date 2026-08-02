#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace stgx {

/// Job release event attached to a task inside a node.
///
/// Wire values: `up` / `down` / `none`. On load the aliases `↑`, `1` map to
/// `Up` and `↓`, `-1` map to `Down`.
enum class Release : uint8_t { None, Up, Down };

/// What an edge means.
enum class EdgeKind : uint8_t { Transition, Loopback };

[[nodiscard]] std::string_view to_string(Release release);
[[nodiscard]] std::string_view to_string(EdgeKind kind);

/// Task parameters of the system
struct TaskParam {
  int c = 0;
  int d = 0;
  std::optional<std::string> name;

  bool operator==(const TaskParam&) const = default;
};

/// Scheduling-system metadata: what the graph is a graph of.
struct System {
  std::optional<std::string> description;
  /// Number of processors.
  std::optional<int> m;
  std::vector<TaskParam> tasks;

  /// True when nothing is set — such a `system` is omitted on save.
  [[nodiscard]] bool empty() const noexcept {
    return !description.has_value() && !m.has_value() && tasks.empty();
  }

  bool operator==(const System&) const = default;
};

/// State of one task inside one node: remaining computation and deadline.
struct TaskState {
  int c = 0;
  int d = 0;
  Release release = Release::None;

  bool operator==(const TaskState&) const = default;
};

/// A global state of the system.
struct Node {
  std::string id;
  bool initial = false;

  std::vector<TaskState> tasks;
  std::optional<std::string> label;
  std::optional<std::string> border_color;
  std::optional<std::string> fill_color;

  bool operator==(const Node&) const = default;
};

struct Edge {
  std::string source;
  std::string target;
  EdgeKind kind = EdgeKind::Transition;

  bool operator==(const Edge&) const = default;
};

/// Wire-format version this build reads and writes.
inline constexpr int kSchemaVersion = 1;

}  // namespace stgx
