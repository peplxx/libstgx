#include <catch2/catch_test_macros.hpp>
#include <string>
#include <string_view>

#include <stgx/diagnostics.hpp>
#include <stgx/model.hpp>
#include <stgx/yaml.hpp>

#include "helpers.hpp"

using stgx::DiagCode;
using stgx::EdgeKind;
using stgx::Release;
using stgx_test::data_file;

TEST_CASE("a minimal document loads", "[load]") {
  stgx::Diagnostics warnings;
  const auto graph = stgx::load_yaml_file(data_file("minimal.yaml"), &warnings);

  REQUIRE(graph.has_value());
  CHECK(warnings.empty());
  CHECK(graph->node_count() == 1);
  CHECK(graph->edge_count() == 0);
  REQUIRE(graph->initial_node() != nullptr);
  CHECK(graph->initial_node()->id == "n0");
  CHECK(graph->initial_node()->tasks.empty());
  CHECK(graph->system().empty());
}

TEST_CASE("a full document loads", "[load]") {
  const auto graph = stgx::load_yaml_file(data_file("two-tasks.yaml"));

  REQUIRE(graph.has_value());
  CHECK(graph->node_count() == 3);
  CHECK(graph->edge_count() == 4);

  const stgx::System& system = graph->system();
  REQUIRE(system.description.has_value());
  CHECK(*system.description == "EDF feasibility graph");
  CHECK(system.m == 1);
  REQUIRE(system.tasks.size() == 2);
  CHECK(system.tasks[0].c == 2);
  CHECK(system.tasks[0].d == 5);
  CHECK(system.tasks[0].name == "τ₁");

  const auto children = graph->out_edges("n00");
  REQUIRE(children.size() == 2);
  CHECK(children[0].target == "n10");
  CHECK(children[0].kind == EdgeKind::Transition);
  CHECK(children[1].target == "n20");

  const auto loopback = graph->out_edges("n20");
  REQUIRE(loopback.size() == 1);
  CHECK(loopback[0].kind == EdgeKind::Loopback);
  CHECK(loopback[0].target == "n00");

  const stgx::Node* n20 = graph->find_node("n20");
  REQUIRE(n20 != nullptr);
  CHECK(n20->label == "custom: label");
  REQUIRE(n20->tasks.size() == 2);
  CHECK(n20->tasks[0].release == Release::Down);
  CHECK(n20->tasks[1].release == Release::None);
}

TEST_CASE("release aliases and omitted fields are normalized", "[load]") {
  stgx::Diagnostics warnings;
  const auto graph = stgx::load_yaml_file(data_file("aliases.yaml"), &warnings);

  REQUIRE(graph.has_value());
  CHECK(warnings.empty());

  // isInitial is the legacy spelling of initial.
  REQUIRE(graph->initial_node() != nullptr);
  CHECK(graph->initial_node()->id == "a");

  const stgx::Node* a = graph->find_node("a");
  REQUIRE(a != nullptr);
  CHECK(a->tasks[0].release == Release::Up);  // ↑
  // An omitted c defaults to 0, d comes from the file.
  CHECK(a->tasks[1].c == 0);
  CHECK(a->tasks[1].d == 2);
  CHECK(a->tasks[1].release == Release::None);

  CHECK(graph->find_node("b")->tasks[0].release == Release::Down);  // ↓
  CHECK(graph->find_node("c")->tasks[0].release == Release::Up);    // 1
  CHECK(graph->find_node("c")->tasks[1].release == Release::Down);  // -1
}

TEST_CASE("fields we do not model yet warn instead of vanishing quietly", "[load]") {
  stgx::Diagnostics warnings;
  const auto graph = stgx::load_yaml_file(data_file("unsupported.yaml"), &warnings);

  REQUIRE(graph.has_value());
  CHECK_FALSE(warnings.has_errors());
  CHECK(warnings.contains(DiagCode::UnsupportedField));
  CHECK(warnings.contains(DiagCode::UnknownField));

  const std::string report = warnings.to_string();
  for (std::string_view expected : {"areas", "layout", "nodes[0].hatch", "nodes[0].metadata"}) {
    CHECK(report.find(expected) != std::string::npos);
  }
  CHECK(report.find("nodes[1].dangerouslyUnknown") != std::string::npos);

  // Styling is modelled, so it loads instead of warning.
  CHECK(report.find("Color") == std::string::npos);
  const stgx::Node* styled = graph->find_node("n0");
  REQUIRE(styled != nullptr);
  CHECK(styled->border_color == "#C0392B");
  CHECK(styled->fill_color == "#E8F4F8");
}

TEST_CASE("warnings are optional", "[load]") {
  // Passing no sink must not crash and must not change the outcome.
  CHECK(stgx::load_yaml_file(data_file("unsupported.yaml")).has_value());
}

namespace {

struct BadFixture {
  std::string_view file;
  DiagCode code;
  std::string_view path;
};

}  // namespace

TEST_CASE("broken documents report the expected diagnostic", "[load]") {
  const BadFixture fixtures[] = {
      {.file = "invalid_no_nodes.yaml", .code = DiagCode::NodesMissing, .path = "nodes"},
      {.file = "invalid_empty_nodes.yaml", .code = DiagCode::NodesMissing, .path = "nodes"},
      {.file = "invalid_duplicate_id.yaml",
       .code = DiagCode::NodeIdDuplicate,
       .path = "nodes[1].id"},
      {.file = "invalid_empty_id.yaml", .code = DiagCode::NodeIdEmpty, .path = "nodes[0].id"},
      {.file = "invalid_dangling.yaml",
       .code = DiagCode::DanglingEdge,
       .path = "nodes[1].loopback[0]"},
      {.file = "invalid_two_initial.yaml", .code = DiagCode::InitialCount, .path = "nodes"},
      {.file = "invalid_no_initial.yaml", .code = DiagCode::InitialCount, .path = "nodes"},
      {.file = "invalid_task_arity.yaml",
       .code = DiagCode::TaskArityMismatch,
       .path = "nodes[0].tasks"},
      {.file = "invalid_bad_int.yaml", .code = DiagCode::NotAnInt, .path = "nodes[0].tasks[0].c"},
      {.file = "invalid_bad_release.yaml",
       .code = DiagCode::BadRelease,
       .path = "nodes[0].tasks[0].release"},
      {.file = "invalid_schema_version.yaml",
       .code = DiagCode::SchemaVersion,
       .path = "schemaVersion"},
      {.file = "invalid_wrong_type.yaml", .code = DiagCode::WrongType, .path = "nodes[0].initial"},
  };

  for (const BadFixture& fixture : fixtures) {
    CAPTURE(fixture.file);
    const auto graph = stgx::load_yaml_file(data_file(fixture.file));

    REQUIRE_FALSE(graph.has_value());
    const auto diag = stgx_test::find(graph.error(), fixture.code);
    REQUIRE(diag.has_value());
    CHECK(diag->path == fixture.path);
    CHECK(diag->severity == stgx::Severity::Error);
  }
}

TEST_CASE("malformed YAML is a parse error", "[load]") {
  const auto graph = stgx::load_yaml_file(data_file("invalid_bad_yaml.yaml"));

  REQUIRE_FALSE(graph.has_value());
  CHECK(graph.error().contains(DiagCode::YamlParse));
}

TEST_CASE("a missing file is an io error", "[load]") {
  const auto graph = stgx::load_yaml_file(data_file("does-not-exist.yaml"));

  REQUIRE_FALSE(graph.has_value());
  CHECK(graph.error().contains(DiagCode::IoError));
}

TEST_CASE("an empty document is rejected", "[load]") {
  const auto graph = stgx::load_yaml_string("");

  REQUIRE_FALSE(graph.has_value());
  CHECK(graph.error().contains(DiagCode::YamlParse));
}

TEST_CASE("a scalar document is rejected", "[load]") {
  const auto graph = stgx::load_yaml_string("just a string");

  REQUIRE_FALSE(graph.has_value());
  CHECK(graph.error().contains(DiagCode::WrongType));
}

TEST_CASE("loading collects every problem at once", "[load]") {
  const auto graph = stgx::load_yaml_string(R"(nodes:
  - id: n0
    tasks:
      - {c: oops, d: 1}
    children: [ghost]
)");

  REQUIRE_FALSE(graph.has_value());
  CHECK(graph.error().contains(DiagCode::NotAnInt));
  // Structural checks still run once the fields have been read.
  CHECK(graph.error().contains(DiagCode::InitialCount));
  CHECK(graph.error().contains(DiagCode::DanglingEdge));
}
