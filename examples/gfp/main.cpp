#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <stgx/builder.hpp>
#include <stgx/model.hpp>
#include <stgx/yaml.hpp>

namespace {
using Job = std::pair<int, int>;

struct State {
  std::vector<Job> jobs;
  long sum_c = 0;
  long sum_d = 0;

  State() = default;
  explicit State(std::vector<Job> js) : jobs(std::move(js)) {
    for (const auto& [c, d] : jobs) {
      sum_c += c;
      sum_d += d;
    }
  }

  [[nodiscard]] std::size_t size() const { return jobs.size(); }
  const Job& operator[](std::size_t i) const { return jobs[i]; }

  bool operator==(const State& other) const { return jobs == other.jobs; }
  bool operator<(const State& other) const { return jobs < other.jobs; }

  [[nodiscard]] bool deadline_miss() const {
    return std::ranges::any_of(jobs, [](const Job& job) { return job.first > job.second; });
  }

  [[nodiscard]] bool dominates(const State& other) const {
    for (std::size_t i = 0; i < jobs.size(); ++i) {
      if (jobs[i].first < other.jobs[i].first || jobs[i].second > other.jobs[i].second) {
        return false;
      }
    }
    return true;
  }
};

class Visited {
 public:
  [[nodiscard]] bool contains(const State& state) const {
    const auto by_c = index_.find(state.sum_c);
    if (by_c == index_.end()) return false;
    const auto by_d = by_c->second.find(state.sum_d);
    if (by_d == by_c->second.end()) return false;
    return by_d->second.contains(state);
  }

  void add(const State& state) { index_[state.sum_c][state.sum_d].insert(state); }

  [[nodiscard]] bool is_dominated(const State& state) const {
    for (auto by_c = index_.lower_bound(state.sum_c); by_c != index_.end(); ++by_c) {
      for (const auto& [sum_d, states] : by_c->second) {
        if (sum_d > state.sum_d) break;
        for (const State& seen : states) {
          if (seen == state) continue;
          if (seen.dominates(state)) return true;
        }
      }
    }
    return false;
  }

 private:
  std::map<long, std::map<long, std::set<State>>> index_;
};

State gfp(const State& state, int m) {
  std::vector<Job> scheduled = state.jobs;
  int used_cores = 0;

  for (auto& [c, d] : scheduled) {
    const int next_d = std::max(0, d - 1);
    if (c == 0 || used_cores == m) {
      d = next_d;
      continue;
    }
    c -= 1;
    d = next_d;
    ++used_cores;
  }
  return State{std::move(scheduled)};
}

std::vector<std::vector<int>> all_combinations(int task_count) {
  std::vector<std::vector<int>> result;
  for (int mask = 1; mask < (1 << task_count); ++mask) {
    std::vector<int> combination;
    for (int i = 0; i < task_count; ++i) {
      if ((mask & (1 << i)) != 0) combination.push_back(i);
    }
    result.push_back(std::move(combination));
  }
  return result;
}

std::vector<State> next_states(const State& state, const std::vector<Job>& tasks, int m,
                               const std::vector<std::vector<int>>& combinations) {
  const State scheduled = gfp(state, m);
  std::set<State> result;

  for (const auto& combination : combinations) {
    std::vector<Job> released = scheduled.jobs;
    for (const int i : combination) {
      if (released[i] == Job{0, 0}) released[i] = tasks[i];
    }
    result.insert(State{std::move(released)});
  }
  return {result.begin(), result.end()};
}

std::vector<stgx::Release> releases_of(const State& from, const State& to,
                                       const std::vector<Job>& tasks, int m) {
  const State scheduled = gfp(from, m);
  std::vector<stgx::Release> result(to.size(), stgx::Release::None);

  for (std::size_t i = 0; i < to.size(); ++i) {
    const bool activated = scheduled[i] == Job{0, 0} && to[i] == tasks[i] && tasks[i] != Job{0, 0};
    if (activated) {
      result[i] = stgx::Release::Up;
    } else if (from[i].first > 0 && scheduled[i].first == 0) {
      result[i] = stgx::Release::Down;
    }
  }
  return result;
}

struct Exploration {
  std::vector<State> states;
  std::vector<std::vector<stgx::Release>> releases;
  std::vector<std::vector<std::pair<std::size_t, bool>>> arcs;
  bool schedulable = true;
  std::size_t deadline_misses = 0;
};

Exploration explore(const std::vector<Job>& tasks, int m, bool prune) {
  const State initial{std::vector<Job>(tasks.size(), Job{0, 0})};
  const auto combinations = all_combinations(static_cast<int>(tasks.size()));

  Exploration out;
  Visited visited;
  std::map<State, std::size_t> index_of;
  std::queue<State> frontier;

  const auto discover = [&](const State& state, std::vector<stgx::Release> releases) {
    visited.add(state);
    index_of.emplace(state, out.states.size());
    out.states.push_back(state);
    out.releases.push_back(std::move(releases));
    out.arcs.emplace_back();
    frontier.push(state);
  };

  discover(initial, std::vector<stgx::Release>(tasks.size(), stgx::Release::None));

  while (!frontier.empty()) {
    const State current = frontier.front();
    frontier.pop();
    const std::size_t from = index_of.at(current);

    for (const State& successor : next_states(current, tasks, m, combinations)) {
      if (prune && !visited.contains(successor) && visited.is_dominated(successor)) continue;

      if (visited.contains(successor)) {
        out.arcs[from].emplace_back(index_of.at(successor), true);
        continue;
      }

      if (successor.deadline_miss()) {
        out.schedulable = false;
        ++out.deadline_misses;
      }
      discover(successor, releases_of(current, successor, tasks, m));
      out.arcs[from].emplace_back(out.states.size() - 1, false);
    }
  }
  return out;
}

std::string node_id(std::size_t index) { return "n" + std::to_string(index); }

stgx::Result<stgx::Graph> build_graph(const Exploration& exploration, const std::vector<Job>& tasks,
                                      int m) {
  stgx::GraphBuilder builder;

  builder.system([&](stgx::SystemBuilder& system) {
    system.description("GFP reachability graph").m(m);
    for (std::size_t i = 0; i < tasks.size(); ++i) {
      system.task(tasks[i].first, tasks[i].second, "τ" + std::to_string(i + 1));
    }
  });

  for (std::size_t i = 0; i < exploration.states.size(); ++i) {
    builder.node(node_id(i), [&](stgx::NodeBuilder& node) {
      if (i == 0) node.initial();
      for (std::size_t t = 0; t < exploration.states[i].size(); ++t) {
        const Job& job = exploration.states[i][t];
        node.task(job.first, job.second, exploration.releases[i][t]);
      }
      for (const auto& [target, loopback] : exploration.arcs[i]) {
        if (loopback) {
          node.loop_to(node_id(target));
        } else {
          node.to(node_id(target));
        }
      }
    });
  }
  return builder.build();
}

constexpr int kM = 1;
constexpr bool kPrune = false;
const std::vector<Job> kTasks = {
    {1, 2},
    {1, 2},
};

}  // namespace

int main(int argc, char** argv) {
  const std::string output = argc > 1 ? argv[1] : "gfp-graph.yaml";

  const Exploration exploration = explore(kTasks, kM, kPrune);

  const stgx::Result<stgx::Graph> graph = build_graph(exploration, kTasks, kM);

  if (!graph) {
    std::cerr << graph.error().to_string() << '\n';
    return EXIT_FAILURE;
  }

  if (const stgx::Result<void> saved = stgx::save_yaml_file(*graph, output); !saved) {
    std::cerr << saved.error().to_string() << '\n';
    return EXIT_FAILURE;
  }

  std::cout << "tasks:       ";
  for (const auto& [c, d] : kTasks) std::cout << '(' << c << ", " << d << ") ";
  std::cout << "\nm:           " << kM << "\npruning:     " << (kPrune ? "rule 1" : "off")
            << "\nschedulable: " << (exploration.schedulable ? "yes" : "no")
            << "\nstates:      " << exploration.states.size();
  if (!exploration.schedulable) {
    std::cout << " (" << exploration.deadline_misses << " with a deadline miss)";
  }
  std::cout << "\nwritten:     " << output << '\n';
  return EXIT_SUCCESS;
}
