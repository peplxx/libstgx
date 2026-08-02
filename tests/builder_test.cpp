#include <stgx/builder.hpp>

#include <catch2/catch_test_macros.hpp>

#include <stgx/diagnostics.hpp>
#include <stgx/model.hpp>

using stgx::DiagCode;
using stgx::EdgeKind;
using stgx::GraphBuilder;
using stgx::Release;

TEST_CASE("the example from the design document builds", "[builder]") {
  const auto graph =
      GraphBuilder()
          .system([](auto& s) { s.description("EDF").m(2).task(1, 2, "τ1").task(1, 2, "τ2"); })
          .node("n0", [](auto& n) { n.initial().task(0, 0).task(0, 0).to("n11").to("n12"); })
          .node("n11", [](auto& n) { n.task(1, 2, Release::Up).task(0, 0).loop_to("n0"); })
          .node("n12", [](auto& n) { n.task(0, 0).task(1, 2, Release::Up).loop_to("n0"); })
          .build();

  REQUIRE(graph.has_value());
  CHECK(graph->node_count() == 3);
  CHECK(graph->edge_count() == 4);

  const stgx::System& system = graph->system();
  CHECK(system.description == "EDF");
  CHECK(system.m == 2);
  REQUIRE(system.tasks.size() == 2);
  CHECK(system.tasks[1].name == "τ2");

  REQUIRE(graph->initial_node() != nullptr);
  CHECK(graph->initial_node()->id == "n0");

  const auto out = graph->out_edges("n0");
  REQUIRE(out.size() == 2);
  CHECK(out[0].target == "n11");
  CHECK(out[0].kind == EdgeKind::Transition);
  CHECK(out[1].target == "n12");

  const auto loopback = graph->out_edges("n11");
  REQUIRE(loopback.size() == 1);
  CHECK(loopback[0].kind == EdgeKind::Loopback);
  CHECK(loopback[0].target == "n0");

  CHECK(graph->find_node("n11")->tasks[0].release == Release::Up);
}

TEST_CASE("declaration order does not matter", "[builder]") {
  // n0 points at n1 before n1 exists.
  const auto graph = GraphBuilder()
                         .node("n0", [](auto& n) { n.initial().to("n1"); })
                         .node("n1", [](auto& n) { n.loop_to("n0"); })
                         .build();

  REQUIRE(graph.has_value());
  CHECK(graph->has_edge("n0", "n1", EdgeKind::Transition));
  CHECK(graph->has_edge("n1", "n0", EdgeKind::Loopback));
}

TEST_CASE("labels and explicit task states go through", "[builder]") {
  const auto graph = GraphBuilder()
                         .node("only",
                               [](auto& n) {
                                 n.initial()
                                     .label("the only one")
                                     .border_color("#C0392B")
                                     .fill_color("#F9D5D3")
                                     .task(stgx::TaskState{1, 3, Release::Down});
                               })
                         .build();

  REQUIRE(graph.has_value());
  const stgx::Node* only = graph->find_node("only");
  REQUIRE(only != nullptr);
  CHECK(only->label == "the only one");
  CHECK(only->border_color == "#C0392B");
  CHECK(only->fill_color == "#F9D5D3");
  REQUIRE(only->tasks.size() == 1);
  CHECK(only->tasks[0].c == 1);
  CHECK(only->tasks[0].d == 3);
  CHECK(only->tasks[0].release == Release::Down);
}

TEST_CASE("prebuilt model values can be handed over directly", "[builder]") {
  stgx::System system;
  system.m = 1;

  stgx::Node first;
  first.id = "a";
  first.initial = true;
  stgx::Node second;
  second.id = "b";

  const auto graph = GraphBuilder()
                         .system(system)
                         .node(first)
                         .node(second)
                         .edge("a", "b")
                         .edge("b", "a", EdgeKind::Loopback)
                         .build();

  REQUIRE(graph.has_value());
  CHECK(graph->system().m == 1);
  CHECK(graph->has_edge("a", "b", EdgeKind::Transition));
  CHECK(graph->has_edge("b", "a", EdgeKind::Loopback));
}

TEST_CASE("build reports everything that is wrong at once", "[builder]") {
  const auto graph = GraphBuilder()
                         .node("dup", [](auto& n) { n.to("ghost"); })
                         .node("dup", [](auto& n) { n.initial(); })
                         .build();

  REQUIRE_FALSE(graph.has_value());
  CHECK(graph.error().contains(DiagCode::NodeIdDuplicate));
  CHECK(graph.error().contains(DiagCode::DanglingEdge));
}

TEST_CASE("a builder without nodes fails", "[builder]") {
  const auto graph = GraphBuilder().build();

  REQUIRE_FALSE(graph.has_value());
  CHECK(graph.error().contains(DiagCode::NodesMissing));
}

TEST_CASE("the builder stays usable after build", "[builder]") {
  GraphBuilder builder;
  builder.node("a", [](auto& n) { n.initial().to("b"); });

  const auto incomplete = builder.build();
  CHECK_FALSE(incomplete.has_value());  // b is missing

  builder.node("b", [](auto&) {});
  const auto complete = builder.build();
  REQUIRE(complete.has_value());
  CHECK(complete->node_count() == 2);
}
