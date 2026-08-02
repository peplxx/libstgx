#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <stgx/graph.hpp>
#include <stgx/model.hpp>
#include <stgx/result.hpp>

namespace stgx {

class SystemBuilder {
 public:
  SystemBuilder& description(std::string text) {
    system_.description = std::move(text);
    return *this;
  }

  /// Number of processors.
  SystemBuilder& m(int processors) {
    system_.m = processors;
    return *this;
  }

  SystemBuilder& task(int c, int d) {
    system_.tasks.push_back(TaskParam{.c = c, .d = d, .name = std::nullopt});
    return *this;
  }

  SystemBuilder& task(int c, int d, std::string name) {
    system_.tasks.push_back(TaskParam{.c = c, .d = d, .name = std::move(name)});
    return *this;
  }

 private:
  friend class GraphBuilder;

  System system_;
};

class NodeBuilder {
 public:
  NodeBuilder& initial(bool value = true) {
    node_.initial = value;
    return *this;
  }

  NodeBuilder& label(std::string text) {
    node_.label = std::move(text);
    return *this;
  }

  /// Node styling, any CSS colour.
  NodeBuilder& border_color(std::string color) {
    node_.border_color = std::move(color);
    return *this;
  }

  NodeBuilder& fill_color(std::string color) {
    node_.fill_color = std::move(color);
    return *this;
  }

  NodeBuilder& task(int c, int d, Release release = Release::None) {
    node_.tasks.push_back(TaskState{.c = c, .d = d, .release = release});
    return *this;
  }

  NodeBuilder& task(TaskState state) {
    node_.tasks.push_back(state);
    return *this;
  }

  /// `children` in YAML.
  NodeBuilder& to(std::string target) {
    edges_.push_back(
        Edge{.source = node_.id, .target = std::move(target), .kind = EdgeKind::Transition});
    return *this;
  }

  /// `loopback` in YAML.
  NodeBuilder& loop_to(std::string target) {
    edges_.push_back(
        Edge{.source = node_.id, .target = std::move(target), .kind = EdgeKind::Loopback});
    return *this;
  }

 private:
  friend class GraphBuilder;

  explicit NodeBuilder(std::string id) { node_.id = std::move(id); }

  Node node_;
  std::vector<Edge> edges_;
};

/// Assemble a graph declaratively.
///
/// Nothing is checked while building,
// `build()` validates once and reports everything
///
/// ```cpp
/// auto graph = stgx::GraphBuilder()
///     .system([](auto& s) { s.m(1).task(2, 5, "τ₁").task(3, 7, "τ₂"); })
///     .node("n00", [](auto& n) { n.initial().task(0, 0).task(0, 0).to("n10"); })
///     .node("n10", [](auto& n) { n.task(2, 5, stgx::Release::Up).task(0, 0).loop_to("n00"); })
///     .build();
/// ```
class GraphBuilder {
 public:
  /// Configure the system through a callback taking `SystemBuilder&`.
  template <std::invocable<SystemBuilder&> SbFn>
  GraphBuilder& system(SbFn&& configure) {
    SystemBuilder builder;
    std::forward<SbFn>(configure)(builder);

    system_ = std::move(builder.system_);
    return *this;
  }

  GraphBuilder& system(System value) {
    system_ = std::move(value);
    return *this;
  }

  /// Add a node with `id`, configured through a callback taking `NodeBuilder&`.
  template <std::invocable<NodeBuilder&> NbFn>
  GraphBuilder& node(std::string id, NbFn&& configure) {
    NodeBuilder builder{std::move(id)};
    std::forward<NbFn>(configure)(builder);

    nodes_.push_back(std::move(builder.node_));
    edges_.insert(edges_.end(), std::make_move_iterator(builder.edges_.begin()),
                  std::make_move_iterator(builder.edges_.end()));
    return *this;
  }

  GraphBuilder& node(Node value);

  /// Add an edge without going through a node callback.
  GraphBuilder& edge(std::string source, std::string target, EdgeKind kind = EdgeKind::Transition);

  /// Build and validate. The builder stays usable afterwards.
  [[nodiscard]] Result<Graph> build() const;

 private:
  System system_;
  std::vector<Node> nodes_;
  std::vector<Edge> edges_;
};

}  // namespace stgx
