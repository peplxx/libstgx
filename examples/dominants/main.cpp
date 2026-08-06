#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <stgx/graph.hpp>
#include <stgx/model.hpp>
#include <stgx/yaml.hpp>

namespace {

constexpr std::string_view kDominantBorder = "#C0392B";
constexpr std::string_view kDominantFill = "#F9D5D3";

using Job = std::pair<int, int>;

/// Rule 1: `state` is at least as bad as `other` in every component.
bool dominates(const std::vector<Job>& state, const std::vector<Job>& other) {
  for (std::size_t i = 0; i < state.size(); ++i) {
    if (state[i].first < other[i].first || state[i].second > other[i].second) return false;
  }
  return true;
}

std::vector<Job> state_of(const stgx::Node& node) {
  std::vector<Job> state;
  state.reserve(node.tasks.size());
  for (const stgx::TaskState& task : node.tasks) {
    state.emplace_back(task.c, task.d);
  }
  return state;
}

std::vector<std::size_t> absolute_dominants(const std::vector<std::vector<Job>>& states) {
  std::vector<std::size_t> result;
  for (std::size_t i = 0; i < states.size(); ++i) {
    bool dominant = true;
    for (std::size_t j = 0; j < states.size() && dominant; ++j) {
      if (states[j] == states[i]) continue;
      dominant = !dominates(states[j], states[i]);
    }
    if (dominant) result.push_back(i);
  }
  return result;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " <in.yaml> [out.yaml]\n";
    return EXIT_FAILURE;
  }
  const std::string input = argv[1];
  const std::string output = argc > 2 ? argv[2] : "dominants.yaml";

  stgx::Diagnostics warnings;
  stgx::Result<stgx::Graph> graph = stgx::load_yaml_file(input, &warnings);
  if (!graph) {
    std::cerr << graph.error().to_string() << '\n';
    return EXIT_FAILURE;
  }
  if (!warnings.empty()) std::cerr << warnings.to_string() << '\n';

  std::vector<std::vector<Job>> states;
  states.reserve(graph->node_count());
  for (const stgx::Node& node : graph->nodes()) {
    if (!states.empty() && node.tasks.size() != states.front().size()) {
      std::cerr << "node \"" << node.id << "\" has " << node.tasks.size()
                << " tasks, the first node has " << states.front().size()
                << " — states of different arity cannot be compared\n";
      return EXIT_FAILURE;
    }
    states.push_back(state_of(node));
  }

  const std::vector<std::size_t> dominants = absolute_dominants(states);

  std::vector<std::string> painted;
  painted.reserve(dominants.size());

  for (const std::size_t index : dominants) {
    const std::string id = graph->nodes()[index].id;
    if (const auto set = graph->set_node_border_color(id, std::string{kDominantBorder}); !set) {
      std::cerr << set.error().to_string() << '\n';
      return EXIT_FAILURE;
    }
    if (const auto set = graph->set_node_fill_color(id, std::string{kDominantFill}); !set) {
      std::cerr << set.error().to_string() << '\n';
      return EXIT_FAILURE;
    }
    painted.push_back(id);
  }

  if (const stgx::Result<void> saved = stgx::save_yaml_file(*graph, output); !saved) {
    std::cerr << saved.error().to_string() << '\n';
    return EXIT_FAILURE;
  }

  std::cout << "read:      " << input << "\nstates:    " << graph->node_count()
            << "\ndominant:  " << painted.size() << "\n           ";
  for (const std::string& id : painted) std::cout << id << ' ';
  std::cout << "\nwritten:   " << output << '\n';
  return EXIT_SUCCESS;
}
