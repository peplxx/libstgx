#include <catch2/catch_test_macros.hpp>

#include <stgx/graph.hpp>

TEST_CASE("Graph default construction", "[graph]") {
  stgx::Graph g;
  SUCCEED("Graph constructs");
}
