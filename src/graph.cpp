#include <stgx/graph.hpp>

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <stgx/diagnostics.hpp>
#include <stgx/model.hpp>
#include <stgx/result.hpp>

#include "validate.hpp"

namespace stgx {

Result<void> Graph::mutate_node(std::string_view id, const std::function<void(Node&)>& mutation) {
  Node* node = find_node_mut(id);
  if (node == nullptr) {
    return fail(diag::node_not_found(id));
  }

  mutation(*node);
  return {};
}

Result<Graph> Graph::create(System system, std::vector<Node> nodes, std::vector<Edge> edges) {
  Graph graph;
  graph.system_ = std::move(system);
  graph.nodes_ = std::move(nodes);
  graph.edges_ = std::move(edges);
  graph.rebuild_indexes();

  if (Diagnostics diags = detail::validate_graph(graph.system_, graph.nodes_, graph.edges_);
      diags.has_errors()) {
    return fail(std::move(diags));
  }
  return graph;
}

void Graph::rebuild_indexes() {
  // Build the node lookup table.
  node_index_.clear();
  node_index_.reserve(nodes_.size());

  for (std::size_t node_index = 0; node_index < nodes_.size(); ++node_index) {
    // Keep the first occurrence; validation reports duplicate IDs.
    node_index_.try_emplace(nodes_[node_index].id, node_index);
  }

  // Assign each edge to its source node's bucket.
  // Edges with an unknown source go into the final bucket.
  const std::size_t unknown_source_bucket = nodes_.size();
  const std::size_t bucket_count = nodes_.size() + 1;

  std::vector<std::size_t> edge_counts(bucket_count);
  std::vector<std::size_t> edge_buckets(edges_.size());

  for (std::size_t edge_index = 0; edge_index < edges_.size(); ++edge_index) {
    const auto source = node_index_.find(edges_[edge_index].source);
    const std::size_t bucket = source == node_index_.end() ? unknown_source_bucket : source->second;

    edge_buckets[edge_index] = bucket;
    ++edge_counts[bucket];
  }

  // Compute the start position of each bucket.
  std::vector<std::size_t> bucket_offsets(bucket_count + 1);
  std::partial_sum(edge_counts.begin(), edge_counts.end(), bucket_offsets.begin() + 1);

  // Group edges by source while preserving insertion order.
  std::vector<std::size_t> next_positions(bucket_offsets.begin(), bucket_offsets.end() - 1);

  std::vector<Edge> grouped_edges(edges_.size());

  for (std::size_t edge_index = 0; edge_index < edges_.size(); ++edge_index) {
    const std::size_t bucket = edge_buckets[edge_index];
    grouped_edges[next_positions[bucket]++] = std::move(edges_[edge_index]);
  }

  edges_ = std::move(grouped_edges);

  // Build outgoing-edge ranges.
  out_index_.clear();
  out_index_.reserve(nodes_.size());

  for (std::size_t node_index = 0; node_index < nodes_.size(); ++node_index) {
    out_index_.try_emplace(nodes_[node_index].id, EdgeRange{
                                                      .first = bucket_offsets[node_index],
                                                      .count = edge_counts[node_index],
                                                  });
  }
}

const Node* Graph::find_node(std::string_view id) const {
  const auto it = node_index_.find(id);
  return it == node_index_.end() ? nullptr : &nodes_[it->second];
}

Node* Graph::find_node_mut(std::string_view id) {
  const auto it = node_index_.find(id);
  return it == node_index_.end() ? nullptr : &nodes_[it->second];
}

bool Graph::has_node(std::string_view id) const { return node_index_.contains(id); }

const Node* Graph::initial_node() const {
  const auto it = std::ranges::find_if(nodes_, [](const Node& node) { return node.initial; });
  return it == nodes_.end() ? nullptr : &*it;
}

std::span<const Edge> Graph::out_edges(std::string_view id) const {
  const auto it = out_index_.find(id);
  if (it == out_index_.end()) return {};
  return std::span<const Edge>{edges_}.subspan(it->second.first, it->second.count);
}

bool Graph::has_edge(std::string_view source, std::string_view target, EdgeKind kind) const {
  const std::span<const Edge> out = out_edges(source);
  return std::ranges::any_of(
      out, [target, kind](const Edge& edge) { return edge.kind == kind && edge.target == target; });
}

Result<void> Graph::add_node(Node node) {
  if (node.id.empty()) {
    return fail(diag::node_id_empty());
  }
  if (has_node(node.id)) {
    return fail(diag::node_id_duplicate(node.id));
  }

  nodes_.push_back(std::move(node));
  rebuild_indexes();
  return {};
}

bool Graph::remove_node(std::string_view id) {
  const auto it = node_index_.find(id);
  if (it == node_index_.end()) return false;

  nodes_.erase(nodes_.begin() + static_cast<std::ptrdiff_t>(it->second));
  const auto touches_id = [id](const Edge& edge) { return edge.source == id || edge.target == id; };
  edges_.erase(std::ranges::remove_if(edges_, touches_id).begin(), edges_.end());

  rebuild_indexes();
  return true;
}

Result<void> Graph::rename_node(std::string_view from, std::string_view to) {
  if (to.empty()) {
    return fail(diag::node_id_empty());
  }
  Node* node = find_node_mut(from);
  if (node == nullptr) {
    return fail(diag::node_not_found(from));
  }
  if (from == to) return {};
  if (has_node(to)) {
    return fail(diag::node_id_duplicate(to));
  }
  const std::string old_id = node->id;
  node->id = std::string{to};
  for (Edge& edge : edges_) {
    if (edge.source == old_id) edge.source = to;
    if (edge.target == old_id) edge.target = to;
  }

  rebuild_indexes();
  return {};
}

Result<void> Graph::add_edge(std::string_view source, std::string_view target, EdgeKind kind) {
  if (!has_node(source)) return fail(diag::node_not_found(source));
  if (!has_node(target)) return fail(diag::node_not_found(target));
  if (has_edge(source, target, kind)) return {};

  edges_.push_back(
      Edge{.source = std::string{source}, .target = std::string{target}, .kind = kind});
  rebuild_indexes();
  return {};
}

bool Graph::remove_edge(std::string_view source, std::string_view target, EdgeKind kind) {
  const auto it = std::ranges::find_if(edges_, [source, target, kind](const Edge& edge) {
    return edge.kind == kind && edge.source == source && edge.target == target;
  });
  if (it == edges_.end()) return false;

  edges_.erase(it);
  rebuild_indexes();
  return true;
}

Result<void> Graph::validate() const {
  if (Diagnostics diags = detail::validate_graph(system_, nodes_, edges_); diags.has_errors()) {
    return fail(std::move(diags));
  }
  return {};
}
bool Graph::operator==(const Graph& other) const {
  return system_ == other.system_ && nodes_ == other.nodes_ && edges_ == other.edges_;
}

}  // namespace stgx
