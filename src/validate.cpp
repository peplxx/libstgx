#include "validate.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <stgx/diagnostics.hpp>
#include <stgx/model.hpp>

namespace stgx::detail {
namespace {

std::string node_path(std::size_t index) { return "nodes[" + std::to_string(index) + "]"; }

std::string_view edge_field(EdgeKind kind) {
  return kind == EdgeKind::Transition ? "children" : "loopback";
}

/// Per-source running counters, so a dangling edge can be reported at the
/// position it occupies in the source node's `children` / `loopback` list.
struct EdgeOrdinals {
  std::size_t transitions = 0;
  std::size_t releases = 0;

  std::size_t next(EdgeKind kind) {
    return kind == EdgeKind::Transition ? transitions++ : releases++;
  }
};

void check_nodes(std::span<const Node> nodes, Diagnostics& diags,
                 std::unordered_map<std::string_view, std::size_t>& first_index) {
  std::size_t initial_count = 0;
  for (std::size_t i = 0; i < nodes.size(); ++i) {
    const Node& node = nodes[i];

    if (node.id.empty()) {
      diags.add(DiagCode::NodeIdEmpty, node_path(i) + ".id", "node id must not be empty");
    } else if (const auto [it, inserted] = first_index.try_emplace(node.id, i); !inserted) {
      diags.add(DiagCode::NodeIdDuplicate, node_path(i) + ".id",
                "duplicate node id \"" + node.id + "\", first seen at " + node_path(it->second));
    }

    if (node.initial) ++initial_count;
  }

  if (initial_count != 1) {
    diags.add(DiagCode::InitialCount, "nodes",
              "exactly one node must be initial, found " + std::to_string(initial_count));
  }
}

void check_task_arity(const System& system, std::span<const Node> nodes, Diagnostics& diags) {
  if (system.tasks.empty()) return;

  const std::size_t expected = system.tasks.size();
  for (std::size_t i = 0; i < nodes.size(); ++i) {
    if (nodes[i].tasks.size() == expected) continue;
    diags.add(DiagCode::TaskArityMismatch, node_path(i) + ".tasks",
              "node \"" + nodes[i].id + "\" has " + std::to_string(nodes[i].tasks.size()) +
                  " task(s), expected " + std::to_string(expected) + " from system.tasks");
  }
}

void check_edges(std::span<const Edge> edges, Diagnostics& diags,
                 const std::unordered_map<std::string_view, std::size_t>& first_index) {
  std::unordered_map<std::string_view, EdgeOrdinals> ordinals;
  std::unordered_set<std::string_view> unknown_sources;

  for (std::size_t e = 0; e < edges.size(); ++e) {
    const Edge& edge = edges[e];
    const auto source_it = first_index.find(edge.source);

    if (source_it == first_index.end()) {
      // Report an unknown source once, not once per outgoing edge.
      if (unknown_sources.insert(edge.source).second) {
        diags.add(DiagCode::DanglingEdge, "edges[" + std::to_string(e) + "].source",
                  "edge leaves unknown node \"" + edge.source + "\"");
      }
      continue;
    }

    const std::size_t ordinal = ordinals[edge.source].next(edge.kind);
    if (!first_index.contains(edge.target)) {
      diags.add(DiagCode::DanglingEdge,
                node_path(source_it->second) + "." + std::string{edge_field(edge.kind)} + "[" +
                    std::to_string(ordinal) + "]",
                "unknown node \"" + edge.target + "\" referenced from \"" + edge.source + "\"");
    }
  }
}

}  // namespace

Diagnostics validate_graph(const System& system, std::span<const Node> nodes,
                           std::span<const Edge> edges) {
  Diagnostics diags;

  if (nodes.empty()) {
    diags.add(DiagCode::NodesMissing, "nodes", "a graph needs at least one node");
    // Every remaining check is about nodes; with none there is nothing to say.
    return diags;
  }

  std::unordered_map<std::string_view, std::size_t> first_index;
  first_index.reserve(nodes.size());

  check_nodes(nodes, diags, first_index);
  check_task_arity(system, nodes, diags);
  check_edges(edges, diags, first_index);

  return diags;
}

}  // namespace stgx::detail
