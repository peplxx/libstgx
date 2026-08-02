#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <stgx/model.hpp>
#include <stgx/result.hpp>

namespace stgx {

namespace detail {

/// Hash that lets the id maps be queried with `std::string_view` without
/// materializing a `std::string`.
struct StringHash {
  using is_transparent = void;
  [[nodiscard]] std::size_t operator()(std::string_view key) const noexcept {
    return std::hash<std::string_view>{}(key);
  }
};

}  // namespace detail

/// A state-transition graph: system metadata, nodes and edges.
class Graph {
 public:
  Graph() = default;

  /// Build a graph and validate it. On failure returns every diagnostic found.
  [[nodiscard]] static Result<Graph> create(System system, std::vector<Node> nodes,
                                            std::vector<Edge> edges);

  // --- reading

  [[nodiscard]] const System& system() const noexcept { return system_; }
  [[nodiscard]] std::span<const Node> nodes() const noexcept { return nodes_; }
  [[nodiscard]] std::span<const Edge> edges() const noexcept { return edges_; }
  [[nodiscard]] std::size_t node_count() const noexcept { return nodes_.size(); }
  [[nodiscard]] std::size_t edge_count() const noexcept { return edges_.size(); }

  /// Node with this id, or `nullptr`. The pointer is invalidated by mutations.
  [[nodiscard]] const Node* find_node(std::string_view id) const;
  [[nodiscard]] bool has_node(std::string_view id) const;

  /// The node flagged `initial`, or `nullptr` if none is.
  [[nodiscard]] const Node* initial_node() const;

  /// Edges leaving `id`, in insertion order. Empty for an unknown id.
  [[nodiscard]] std::span<const Edge> out_edges(std::string_view id) const;

  [[nodiscard]] bool has_edge(std::string_view source, std::string_view target,
                              EdgeKind kind) const;

  // --- mutating

  void set_system(System system) { system_ = std::move(system); }

  /// Add a node. Fails on an empty or already used id.
  [[nodiscard]] Result<void> add_node(Node node);

  /// Remove a node together with every edge touching it.
  /// Returns false if there was no such node.
  bool remove_node(std::string_view id);

  /// Rename a node and repoint every edge at it.
  /// Fails if `from` is unknown, or `to` is empty or already taken.
  [[nodiscard]] Result<void> rename_node(std::string_view from, std::string_view to);

  /// Add an edge. Fails if either endpoint is unknown. Adding an edge that is
  /// already there succeeds and changes nothing.
  [[nodiscard]] Result<void> add_edge(std::string_view source, std::string_view target,
                                      EdgeKind kind);

  /// Remove one edge. Returns false if there was no such edge.
  bool remove_edge(std::string_view source, std::string_view target, EdgeKind kind);

  /// Flag `id` as the initial node, clearing the flag on whichever node had it.
  /// Fails if `id` is unknown.
  [[nodiscard]] Result<void> set_initial(std::string_view id) {
    return mutate_node(id, [this](Node& node) {
      for (Node& other : nodes_) other.initial = false;
      node.initial = true;
    });
  }

  /// Replace the task states of a node. Fails if `id` is unknown.
  [[nodiscard]] Result<void> set_node_tasks(std::string_view id, std::vector<TaskState> tasks) {
    return mutate_node(id, [&tasks](Node& node) { node.tasks = std::move(tasks); });
  }

  /// Set or clear the label override of a node. Fails if `id` is unknown.
  [[nodiscard]] Result<void> set_node_label(std::string_view id, std::optional<std::string> label) {
    return mutate_node(id, [&label](Node& node) { node.label = std::move(label); });
  }

  /// Set or clear the styling of a node; `std::nullopt` falls back to the theme
  /// of the visualizer. Fails if `id` is unknown.
  [[nodiscard]] Result<void> set_node_border_color(std::string_view id,
                                                   std::optional<std::string> color) {
    return mutate_node(id, [&color](Node& node) { node.border_color = std::move(color); });
  }
  [[nodiscard]] Result<void> set_node_fill_color(std::string_view id,
                                                 std::optional<std::string> color) {
    return mutate_node(id, [&color](Node& node) { node.fill_color = std::move(color); });
  }

  // --- checking

  /// Run every graph-level check. Warnings are never produced here.
  [[nodiscard]] Result<void> validate() const;

  [[nodiscard]] bool operator==(const Graph& other) const;

 private:
  /// Half-open slice of `edges_` belonging to one source node.
  struct EdgeRange {
    std::size_t first = 0;
    std::size_t count = 0;
  };

  template <class V>
  using IdMap = std::unordered_map<std::string, V, detail::StringHash, std::equal_to<>>;

  /// Regroup `edges_` by source node and refill both indexes.
  void rebuild_indexes();

  [[nodiscard]] Node* find_node_mut(std::string_view id);

  System system_;
  std::vector<Node> nodes_;
  /// Grouped by source in node order; within a group, insertion order.
  std::vector<Edge> edges_;
  IdMap<std::size_t> node_index_;
  IdMap<EdgeRange> out_index_;

  [[nodiscard]] Result<void> mutate_node(std::string_view id,
                                         const std::function<void(Node&)>& mutation);
};

}  // namespace stgx
