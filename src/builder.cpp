#include <stgx/builder.hpp>

#include <string>
#include <utility>

#include <stgx/graph.hpp>
#include <stgx/model.hpp>
#include <stgx/result.hpp>

namespace stgx {

GraphBuilder& GraphBuilder::node(Node value) {
  nodes_.push_back(std::move(value));
  return *this;
}

GraphBuilder& GraphBuilder::edge(std::string source, std::string target, EdgeKind kind) {
  edges_.push_back(Edge{.source = std::move(source), .target = std::move(target), .kind = kind});
  return *this;
}

Result<Graph> GraphBuilder::build() const { return Graph::create(system_, nodes_, edges_); }

}  // namespace stgx
