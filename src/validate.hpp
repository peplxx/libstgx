#pragma once

#include <span>

#include <stgx/diagnostics.hpp>
#include <stgx/model.hpp>

namespace stgx::detail {

/// Run every graph-level check over a raw model triple.
///
/// Works on loose vectors rather than a `Graph` so that both the YAML loader
/// and `Graph::create()` can validate before a graph exists. Collects all
/// findings instead of stopping at the first one.
///
/// Diagnostic paths are expressed in wire-format terms — `nodes[3].id`,
/// `nodes[3].children[0]` — which requires `edges` to be grouped by source in
/// node order, exactly how `Graph` and the loader keep them. Edges whose source
/// is unknown are reported as `edges[i]`.
[[nodiscard]] Diagnostics validate_graph(const System& system, std::span<const Node> nodes,
                                         std::span<const Edge> edges);

}  // namespace stgx::detail
