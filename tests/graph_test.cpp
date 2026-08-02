#include <stgx/graph.hpp>

#include <catch2/catch_test_macros.hpp>
#include <optional>
#include <vector>

#include <stgx/model.hpp>

using stgx::DiagCode;
using stgx::Edge;
using stgx::EdgeKind;
using stgx::Graph;
using stgx::Node;

namespace {

/// a --> b --> c, and c loops back to a.
Graph triangle() {
  Node a;
  a.id = "a";
  a.initial = true;
  Node b;
  b.id = "b";
  Node c;
  c.id = "c";

  auto graph =
      Graph::create({}, {a, b, c},
                    {Edge{"a", "b", EdgeKind::Transition}, Edge{"b", "c", EdgeKind::Transition},
                     Edge{"c", "a", EdgeKind::Loopback}});
  REQUIRE(graph.has_value());
  return *graph;
}

}  // namespace

TEST_CASE("an empty graph is empty", "[graph]") {
  const Graph graph;

  CHECK(graph.node_count() == 0);
  CHECK(graph.edge_count() == 0);
  CHECK(graph.initial_node() == nullptr);
  CHECK(graph.find_node("nope") == nullptr);
  CHECK(graph.out_edges("nope").empty());
  // An empty graph is not a valid document.
  CHECK_FALSE(graph.validate().has_value());
}

TEST_CASE("reading a graph", "[graph]") {
  const Graph graph = triangle();

  CHECK(graph.node_count() == 3);
  CHECK(graph.edge_count() == 3);
  REQUIRE(graph.initial_node() != nullptr);
  CHECK(graph.initial_node()->id == "a");
  CHECK(graph.has_node("b"));
  CHECK_FALSE(graph.has_node("z"));

  const auto out = graph.out_edges("c");
  REQUIRE(out.size() == 1);
  CHECK(out[0].target == "a");
  CHECK(out[0].kind == EdgeKind::Loopback);

  CHECK(graph.has_edge("a", "b", EdgeKind::Transition));
  CHECK_FALSE(graph.has_edge("a", "b", EdgeKind::Loopback));
}

TEST_CASE("out_edges keeps insertion order per source", "[graph]") {
  Graph graph = triangle();
  REQUIRE(graph.add_edge("a", "c", EdgeKind::Transition).has_value());
  REQUIRE(graph.add_edge("a", "b", EdgeKind::Loopback).has_value());

  const auto out = graph.out_edges("a");
  REQUIRE(out.size() == 3);
  CHECK(out[0].target == "b");
  CHECK(out[0].kind == EdgeKind::Transition);
  CHECK(out[1].target == "c");
  CHECK(out[2].kind == EdgeKind::Loopback);
}

TEST_CASE("add_node rejects empty and duplicate ids", "[graph]") {
  Graph graph = triangle();

  Node duplicate;
  duplicate.id = "b";
  const auto added = graph.add_node(duplicate);
  REQUIRE_FALSE(added.has_value());
  CHECK(added.error().contains(DiagCode::NodeIdDuplicate));

  const auto empty = graph.add_node(Node{});
  REQUIRE_FALSE(empty.has_value());
  CHECK(empty.error().contains(DiagCode::NodeIdEmpty));

  CHECK(graph.node_count() == 3);
}

TEST_CASE("remove_node drops the edges that touched it", "[graph]") {
  Graph graph = triangle();

  CHECK(graph.remove_node("b"));
  CHECK_FALSE(graph.remove_node("b"));

  CHECK(graph.node_count() == 2);
  // a->b and b->c are gone, c->a survives.
  REQUIRE(graph.edge_count() == 1);
  CHECK(graph.edges()[0].source == "c");
  CHECK(graph.out_edges("a").empty());
  CHECK(graph.validate().has_value());
}

TEST_CASE("rename_node repoints its edges", "[graph]") {
  Graph graph = triangle();

  REQUIRE(graph.rename_node("a", "start").has_value());
  CHECK(graph.has_node("start"));
  CHECK_FALSE(graph.has_node("a"));
  REQUIRE(graph.initial_node() != nullptr);
  CHECK(graph.initial_node()->id == "start");
  CHECK(graph.has_edge("start", "b", EdgeKind::Transition));
  CHECK(graph.has_edge("c", "start", EdgeKind::Loopback));
  CHECK(graph.validate().has_value());

  SECTION("renaming onto a taken id fails") {
    const auto renamed = graph.rename_node("b", "c");
    REQUIRE_FALSE(renamed.has_value());
    CHECK(renamed.error().contains(DiagCode::NodeIdDuplicate));
  }

  SECTION("renaming an unknown node fails") {
    CHECK_FALSE(graph.rename_node("ghost", "x").has_value());
  }

  SECTION("renaming to itself is a no-op") {
    CHECK(graph.rename_node("b", "b").has_value());
    CHECK(graph.node_count() == 3);
  }
}

TEST_CASE("edges cannot be added to unknown nodes", "[graph]") {
  Graph graph = triangle();

  CHECK_FALSE(graph.add_edge("a", "ghost", EdgeKind::Transition).has_value());
  CHECK_FALSE(graph.add_edge("ghost", "a", EdgeKind::Transition).has_value());
  CHECK(graph.edge_count() == 3);
}

TEST_CASE("adding an edge twice changes nothing", "[graph]") {
  Graph graph = triangle();

  REQUIRE(graph.add_edge("a", "b", EdgeKind::Transition).has_value());
  CHECK(graph.edge_count() == 3);
}

TEST_CASE("remove_edge removes exactly one kind", "[graph]") {
  Graph graph = triangle();

  CHECK_FALSE(graph.remove_edge("c", "a", EdgeKind::Transition));
  CHECK(graph.remove_edge("c", "a", EdgeKind::Loopback));
  CHECK(graph.edge_count() == 2);
  CHECK(graph.out_edges("c").empty());
}

TEST_CASE("set_initial moves the flag", "[graph]") {
  Graph graph = triangle();

  REQUIRE(graph.set_initial("c").has_value());
  REQUIRE(graph.initial_node() != nullptr);
  CHECK(graph.initial_node()->id == "c");
  CHECK_FALSE(graph.find_node("a")->initial);
  CHECK(graph.validate().has_value());

  CHECK_FALSE(graph.set_initial("ghost").has_value());
}

TEST_CASE("node tasks and labels can be replaced", "[graph]") {
  Graph graph = triangle();

  REQUIRE(graph.set_node_tasks("b", {{1, 2, stgx::Release::Up}}).has_value());
  REQUIRE(graph.find_node("b")->tasks.size() == 1);
  CHECK(graph.find_node("b")->tasks[0].release == stgx::Release::Up);

  REQUIRE(graph.set_node_label("b", "burst").has_value());
  CHECK(graph.find_node("b")->label == "burst");
  REQUIRE(graph.set_node_label("b", std::nullopt).has_value());
  CHECK_FALSE(graph.find_node("b")->label.has_value());

  REQUIRE(graph.set_node_border_color("b", "#C0392B").has_value());
  REQUIRE(graph.set_node_fill_color("b", "#F9D5D3").has_value());
  CHECK(graph.find_node("b")->border_color == "#C0392B");
  CHECK(graph.find_node("b")->fill_color == "#F9D5D3");
  REQUIRE(graph.set_node_fill_color("b", std::nullopt).has_value());
  CHECK_FALSE(graph.find_node("b")->fill_color.has_value());

  CHECK_FALSE(graph.set_node_tasks("ghost", {}).has_value());
  CHECK_FALSE(graph.set_node_label("ghost", "x").has_value());
  CHECK_FALSE(graph.set_node_border_color("ghost", "#000").has_value());
  CHECK_FALSE(graph.set_node_fill_color("ghost", "#000").has_value());
}

TEST_CASE("graphs compare by content", "[graph]") {
  const Graph one = triangle();
  Graph other = triangle();

  CHECK(one == other);
  REQUIRE(other.set_initial("b").has_value());
  CHECK_FALSE(one == other);
}
