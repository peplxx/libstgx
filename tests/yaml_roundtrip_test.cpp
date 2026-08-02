#include <catch2/catch_test_macros.hpp>
#include <string>
#include <string_view>

#include <stgx/graph.hpp>
#include <stgx/model.hpp>
#include <stgx/yaml.hpp>

#include "helpers.hpp"

using stgx_test::data_file;

TEST_CASE("the canonical document survives a round trip byte for byte", "[roundtrip]") {
  const std::string original = stgx_test::read_file(data_file("two-tasks.yaml"));
  REQUIRE_FALSE(original.empty());

  const auto graph = stgx::load_yaml_string(original);
  REQUIRE(graph.has_value());

  const auto written = stgx::save_yaml_string(*graph);
  REQUIRE(written.has_value());
  CHECK(*written == original);
}

TEST_CASE("every valid fixture round trips through the model", "[roundtrip]") {
  for (std::string_view name :
       {"minimal.yaml", "two-tasks.yaml", "aliases.yaml", "unsupported.yaml"}) {
    CAPTURE(name);
    const auto first = stgx::load_yaml_file(data_file(name));
    REQUIRE(first.has_value());

    const auto written = stgx::save_yaml_string(*first);
    REQUIRE(written.has_value());

    const auto second = stgx::load_yaml_string(*written);
    REQUIRE(second.has_value());
    CHECK(*second == *first);
  }
}

TEST_CASE("saving omits everything left at its default", "[roundtrip]") {
  stgx::Node only;
  only.id = "n0";
  only.initial = true;

  const auto graph = stgx::Graph::create({}, {only}, {});
  REQUIRE(graph.has_value());

  const auto written = stgx::save_yaml_string(*graph);
  REQUIRE(written.has_value());
  CHECK(*written == "schemaVersion: 1\nnodes:\n  - id: n0\n    initial: true\n");
}

TEST_CASE("node styling survives a round trip", "[roundtrip]") {
  const std::string original =
      "schemaVersion: 1\n"
      "nodes:\n"
      "  - id: n0\n"
      "    initial: true\n"
      "    borderColor: \"#C0392B\"\n"
      "    fillColor: \"#F9D5D3\"\n";

  const auto graph = stgx::load_yaml_string(original);
  REQUIRE(graph.has_value());
  CHECK(graph->find_node("n0")->border_color == "#C0392B");
  CHECK(graph->find_node("n0")->fill_color == "#F9D5D3");

  const auto written = stgx::save_yaml_string(*graph);
  REQUIRE(written.has_value());
  CHECK(*written == original);
}

TEST_CASE("an invalid graph is never written", "[roundtrip]") {
  const stgx::Graph empty;

  const auto written = stgx::save_yaml_string(empty);
  REQUIRE_FALSE(written.has_value());
  CHECK(written.error().contains(stgx::DiagCode::NodesMissing));
}

TEST_CASE("saving to a file produces the same bytes", "[roundtrip]") {
  const auto graph = stgx::load_yaml_file(data_file("two-tasks.yaml"));
  REQUIRE(graph.has_value());

  const std::filesystem::path out =
      std::filesystem::temp_directory_path() / "stgx_roundtrip_two_tasks.yaml";
  REQUIRE(stgx::save_yaml_file(*graph, out).has_value());

  const auto written = stgx::save_yaml_string(*graph);
  REQUIRE(written.has_value());
  CHECK(stgx_test::read_file(out) == *written);

  std::filesystem::remove(out);
}

TEST_CASE("saving to an unwritable path is an io error", "[roundtrip]") {
  const auto graph = stgx::load_yaml_file(data_file("minimal.yaml"));
  REQUIRE(graph.has_value());

  const auto written =
      stgx::save_yaml_file(*graph, std::filesystem::path{"/no/such/directory/out.yaml"});
  REQUIRE_FALSE(written.has_value());
  CHECK(written.error().contains(stgx::DiagCode::IoError));
}
