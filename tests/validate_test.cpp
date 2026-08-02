#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

#include <stgx/graph.hpp>
#include <stgx/model.hpp>

#include "helpers.hpp"

using stgx::DiagCode;
using stgx::Edge;
using stgx::EdgeKind;
using stgx::Graph;
using stgx::Node;
using stgx::System;

namespace {

Node node(std::string id, bool initial = false) {
  Node out;
  out.id = std::move(id);
  out.initial = initial;
  return out;
}

}  // namespace

TEST_CASE("a graph needs at least one node", "[validate]") {
  const auto graph = Graph::create({}, {}, {});

  REQUIRE_FALSE(graph.has_value());
  CHECK(graph.error().contains(DiagCode::NodesMissing));
}

TEST_CASE("exactly one node must be initial", "[validate]") {
  SECTION("none") {
    const auto graph = Graph::create({}, {node("a"), node("b")}, {});
    REQUIRE_FALSE(graph.has_value());

    const auto diag = stgx_test::find(graph.error(), DiagCode::InitialCount);
    REQUIRE(diag.has_value());
    CHECK(diag->message.find("found 0") != std::string::npos);
  }

  SECTION("two") {
    const auto graph = Graph::create({}, {node("a", true), node("b", true)}, {});
    REQUIRE_FALSE(graph.has_value());

    const auto diag = stgx_test::find(graph.error(), DiagCode::InitialCount);
    REQUIRE(diag.has_value());
    CHECK(diag->message.find("found 2") != std::string::npos);
  }
}

TEST_CASE("node ids must be unique and non-empty", "[validate]") {
  SECTION("duplicate") {
    const auto graph = Graph::create({}, {node("a", true), node("a")}, {});
    REQUIRE_FALSE(graph.has_value());

    const auto diag = stgx_test::find(graph.error(), DiagCode::NodeIdDuplicate);
    REQUIRE(diag.has_value());
    CHECK(diag->path == "nodes[1].id");
  }

  SECTION("empty") {
    const auto graph = Graph::create({}, {node("", true)}, {});
    REQUIRE_FALSE(graph.has_value());

    const auto diag = stgx_test::find(graph.error(), DiagCode::NodeIdEmpty);
    REQUIRE(diag.has_value());
    CHECK(diag->path == "nodes[0].id");
  }
}

TEST_CASE("edges may not dangle", "[validate]") {
  SECTION("unknown target is reported at its position in the source node") {
    const std::vector<Node> nodes{node("a", true), node("b")};
    const std::vector<Edge> edges{
        Edge{"b", "a", EdgeKind::Transition},
        Edge{"b", "gone", EdgeKind::Transition},
        Edge{"b", "also-gone", EdgeKind::Loopback},
    };
    const auto graph = Graph::create({}, nodes, edges);
    REQUIRE_FALSE(graph.has_value());

    const auto diag = stgx_test::find(graph.error(), DiagCode::DanglingEdge);
    REQUIRE(diag.has_value());
    CHECK(diag->path == "nodes[1].children[1]");
    // The release arc is reported separately, indexed within loopback.
    CHECK(graph.error().to_string().find("nodes[1].loopback[0]") != std::string::npos);
  }

  SECTION("unknown source is reported once") {
    const std::vector<Edge> edges{
        Edge{"ghost", "a", EdgeKind::Transition},
        Edge{"ghost", "a", EdgeKind::Loopback},
    };
    const auto graph = Graph::create({}, {node("a", true)}, edges);
    REQUIRE_FALSE(graph.has_value());
    CHECK(graph.error().size() == 1);
    CHECK(graph.error()[0].code == DiagCode::DanglingEdge);
  }
}

TEST_CASE("node task count must match system.tasks", "[validate]") {
  System system;
  system.tasks = {{1, 2}, {1, 2}};

  Node only = node("a", true);
  only.tasks = {{0, 0}};

  const auto graph = Graph::create(system, {only}, {});
  REQUIRE_FALSE(graph.has_value());

  const auto diag = stgx_test::find(graph.error(), DiagCode::TaskArityMismatch);
  REQUIRE(diag.has_value());
  CHECK(diag->path == "nodes[0].tasks");
}

TEST_CASE("validation collects every problem at once", "[validate]") {
  const auto graph =
      Graph::create({}, {node("a"), node("a")}, {Edge{"a", "nowhere", EdgeKind::Transition}});

  REQUIRE_FALSE(graph.has_value());
  CHECK(graph.error().contains(DiagCode::NodeIdDuplicate));
  CHECK(graph.error().contains(DiagCode::InitialCount));
  CHECK(graph.error().contains(DiagCode::DanglingEdge));
}

TEST_CASE("a sound graph validates", "[validate]") {
  System system;
  system.m = 1;
  system.tasks = {{2, 5, "τ₁"}};

  Node first = node("a", true);
  first.tasks = {{0, 0}};
  Node second = node("b");
  second.tasks = {{2, 5, stgx::Release::Up}};

  const auto graph =
      Graph::create(system, {first, second},
                    {Edge{"a", "b", EdgeKind::Transition}, Edge{"b", "a", EdgeKind::Loopback}});

  REQUIRE(graph.has_value());
  CHECK(graph->validate().has_value());
}
