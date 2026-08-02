#include <yaml-cpp/yaml.h>

#include <charconv>
#include <cstddef>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <stgx/diagnostics.hpp>
#include <stgx/graph.hpp>
#include <stgx/model.hpp>
#include <stgx/result.hpp>
#include <stgx/yaml.hpp>

#include "validate.hpp"

namespace stgx {
namespace {

// --- paths

std::string field_path(std::string_view base, std::string_view field) {
  std::string out{base};
  if (!out.empty()) out += '.';
  out += field;
  return out;
}

std::string index_path(std::string_view base, std::size_t index) {
  return std::string{base} + "[" + std::to_string(index) + "]";
}

// --- scalar readers

/// A key that is present but null counts as absent.
bool is_absent(const YAML::Node& node) { return !node.IsDefined() || node.IsNull(); }

std::optional<std::string> read_string(const YAML::Node& node, const std::string& path,
                                       Diagnostics& diags) {
  if (!node.IsScalar()) {
    diags.add(DiagCode::WrongType, path, "expected a string");
    return std::nullopt;
  }
  return node.Scalar();
}

/// Strict decimal int. Rejects `2.0`, `2e1`, `0x10`, `true` and empty values —
/// the wire format says `c`, `d` and `m` are ints, and a silent truncation here
/// would be a data bug in whatever produced the file.
std::optional<int> read_int(const YAML::Node& node, const std::string& path, Diagnostics& diags) {
  if (!node.IsScalar()) {
    diags.add(DiagCode::WrongType, path, "expected an integer");
    return std::nullopt;
  }

  const std::string& text = node.Scalar();
  std::string_view digits{text};
  if (digits.starts_with('+')) digits.remove_prefix(1);

  int value = 0;
  const auto* const last = digits.data() + digits.size();
  // from_chars takes both ends, so the view needs no terminator.
  // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
  const auto result = std::from_chars(digits.data(), last, value);
  if (result.ec != std::errc{} || result.ptr != last) {
    diags.add(DiagCode::NotAnInt, path, "expected an integer, got \"" + text + "\"");
    return std::nullopt;
  }
  return value;
}

std::optional<bool> read_bool(const YAML::Node& node, const std::string& path, Diagnostics& diags) {
  if (node.IsScalar()) {
    const std::string& text = node.Scalar();
    for (std::string_view yes : {"true", "True", "TRUE", "yes", "Yes", "on", "On"}) {
      if (text == yes) return true;
    }
    for (std::string_view no : {"false", "False", "FALSE", "no", "No", "off", "Off"}) {
      if (text == no) return false;
    }
  }
  diags.add(DiagCode::WrongType, path, "expected a boolean");
  return std::nullopt;
}

/// `up` / `down` / `none`, plus the aliases the wire format documents:
/// `↑` and `1` for up, `↓` and `-1` for down.
std::optional<Release> read_release(const YAML::Node& node, const std::string& path,
                                    Diagnostics& diags) {
  if (node.IsScalar()) {
    const std::string& text = node.Scalar();
    if (text == "up" || text == "↑" || text == "1") return Release::Up;
    if (text == "down" || text == "↓" || text == "-1") return Release::Down;
    if (text == "none") return Release::None;
    diags.add(DiagCode::BadRelease, path,
              "expected up/down/none (or ↑/↓/1/-1), got \"" + text + "\"");
    return std::nullopt;
  }
  diags.add(DiagCode::BadRelease, path, "expected up/down/none (or ↑/↓/1/-1)");
  return std::nullopt;
}

// --- key checking

bool contains(std::initializer_list<std::string_view> keys, std::string_view key) {
  for (std::string_view candidate : keys) {
    if (candidate == key) return true;
  }
  return false;
}

/// Report fields we recognise but do not model yet, and fields we do not know
/// at all. Both are warnings: the document still loads, but a later save will
/// not carry them over, and silently eating someone's annotations would be
/// worse than saying so.
void check_keys(const YAML::Node& map, const std::string& path,
                std::initializer_list<std::string_view> known,
                std::initializer_list<std::string_view> unsupported, Diagnostics& diags) {
  for (const auto& entry : map) {
    if (!entry.first.IsScalar()) {
      diags.add(DiagCode::UnknownField, path, "non-scalar key", Severity::Warning);
      continue;
    }
    const std::string key = entry.first.Scalar();
    if (contains(unsupported, key)) {
      diags.add(DiagCode::UnsupportedField, field_path(path, key),
                "not modelled in this version; it will be dropped when saving", Severity::Warning);
    } else if (!contains(known, key)) {
      diags.add(DiagCode::UnknownField, field_path(path, key), "not part of the wire format",
                Severity::Warning);
    }
  }
}

// --- section readers ---------------------------------------------------------

TaskParam read_task_param(const YAML::Node& node, const std::string& path, Diagnostics& diags) {
  TaskParam task;
  if (!node.IsMap()) {
    diags.add(DiagCode::WrongType, path, "expected a mapping with c, d and an optional name");
    return task;
  }
  check_keys(node, path, {"c", "d", "name"}, {}, diags);

  if (const auto value = read_int(node["c"], field_path(path, "c"), diags)) task.c = *value;
  if (const auto value = read_int(node["d"], field_path(path, "d"), diags)) task.d = *value;
  if (!is_absent(node["name"])) {
    task.name = read_string(node["name"], field_path(path, "name"), diags);
  }
  return task;
}

System read_system(const YAML::Node& node, const std::string& path, Diagnostics& diags) {
  System system;
  if (!node.IsMap()) {
    diags.add(DiagCode::WrongType, path, "expected a mapping");
    return system;
  }
  check_keys(node, path, {"description", "m", "tasks"}, {}, diags);

  if (!is_absent(node["description"])) {
    system.description = read_string(node["description"], field_path(path, "description"), diags);
  }
  if (!is_absent(node["m"])) {
    system.m = read_int(node["m"], field_path(path, "m"), diags);
  }

  const YAML::Node tasks = node["tasks"];
  if (!is_absent(tasks)) {
    const std::string tasks_path = field_path(path, "tasks");
    if (!tasks.IsSequence()) {
      diags.add(DiagCode::WrongType, tasks_path, "expected a sequence");
    } else {
      system.tasks.reserve(tasks.size());
      for (std::size_t i = 0; i < tasks.size(); ++i) {
        system.tasks.push_back(read_task_param(tasks[i], index_path(tasks_path, i), diags));
      }
    }
  }
  return system;
}

TaskState read_task_state(const YAML::Node& node, const std::string& path, Diagnostics& diags) {
  TaskState task;
  if (!node.IsMap()) {
    diags.add(DiagCode::WrongType, path, "expected a mapping with c, d and an optional release");
    return task;
  }
  check_keys(node, path, {"c", "d", "release"}, {}, diags);

  if (!is_absent(node["c"])) {
    if (const auto value = read_int(node["c"], field_path(path, "c"), diags)) task.c = *value;
  }
  if (!is_absent(node["d"])) {
    if (const auto value = read_int(node["d"], field_path(path, "d"), diags)) task.d = *value;
  }
  if (!is_absent(node["release"])) {
    if (const auto value = read_release(node["release"], field_path(path, "release"), diags)) {
      task.release = *value;
    }
  }
  return task;
}

/// Turn a `children` / `loopback` list into edges leaving `source`.
void read_targets(const YAML::Node& node, const std::string& path, EdgeKind kind,
                  const std::string& source, std::vector<Edge>& edges, Diagnostics& diags) {
  if (is_absent(node)) return;
  if (!node.IsSequence()) {
    diags.add(DiagCode::WrongType, path, "expected a sequence of node ids");
    return;
  }
  for (std::size_t i = 0; i < node.size(); ++i) {
    if (auto target = read_string(node[i], index_path(path, i), diags)) {
      edges.push_back(Edge{.source = source, .target = *std::move(target), .kind = kind});
    }
  }
}

Node read_node(const YAML::Node& node, const std::string& path, std::vector<Edge>& edges,
               Diagnostics& diags) {
  Node out;
  if (!node.IsMap()) {
    diags.add(DiagCode::WrongType, path, "expected a mapping");
    return out;
  }
  check_keys(node, path,
             {"id", "initial", "isInitial", "tasks", "children", "loopback", "label", "borderColor",
              "fillColor"},
             {"hatch", "metadata"}, diags);

  if (auto id = read_string(node["id"], field_path(path, "id"), diags)) {
    out.id = *std::move(id);
  }

  // `isInitial` is a legacy spelling the visualizer still accepts on read.
  for (std::string_view key : {"initial", "isInitial"}) {
    const YAML::Node flag = node[std::string{key}];
    if (is_absent(flag)) continue;
    if (const auto value = read_bool(flag, field_path(path, key), diags)) out.initial |= *value;
  }

  if (!is_absent(node["label"])) {
    out.label = read_string(node["label"], field_path(path, "label"), diags);
  }
  if (!is_absent(node["borderColor"])) {
    out.border_color = read_string(node["borderColor"], field_path(path, "borderColor"), diags);
  }
  if (!is_absent(node["fillColor"])) {
    out.fill_color = read_string(node["fillColor"], field_path(path, "fillColor"), diags);
  }

  const YAML::Node tasks = node["tasks"];
  if (!is_absent(tasks)) {
    const std::string tasks_path = field_path(path, "tasks");
    if (!tasks.IsSequence()) {
      diags.add(DiagCode::WrongType, tasks_path, "expected a sequence");
    } else {
      out.tasks.reserve(tasks.size());
      for (std::size_t i = 0; i < tasks.size(); ++i) {
        out.tasks.push_back(read_task_state(tasks[i], index_path(tasks_path, i), diags));
      }
    }
  }

  // Keep edges grouped by source, children before loopback — the order both
  // Graph and the diagnostics paths assume.
  read_targets(node["children"], field_path(path, "children"), EdgeKind::Transition, out.id, edges,
               diags);
  read_targets(node["loopback"], field_path(path, "loopback"), EdgeKind::Loopback, out.id, edges,
               diags);
  return out;
}

void read_schema_version(const YAML::Node& node, Diagnostics& diags) {
  if (is_absent(node)) return;  // absent means version 1

  const auto version = read_int(node, "schemaVersion", diags);
  if (!version.has_value()) return;
  if (*version < 1) {
    diags.add(DiagCode::SchemaVersion, "schemaVersion",
              "must be a positive integer, got " + std::to_string(*version));
  } else if (*version > kSchemaVersion) {
    diags.add(DiagCode::SchemaVersion, "schemaVersion",
              "document is version " + std::to_string(*version) + ", this build reads up to " +
                  std::to_string(kSchemaVersion));
  }
}

/// Hand the collected diagnostics to the caller and build the graph if the
/// document was sound.
///
/// Field-level problems do not stop the structural checks: one load reports
/// everything that is wrong with the document, not just the first layer of it.
Result<Graph> finish(System system, std::vector<Node> nodes, std::vector<Edge> edges,
                     const Diagnostics& diags, Diagnostics* warnings) {
  if (warnings != nullptr) warnings->merge(diags.warnings());

  Diagnostics errors = diags.errors();
  errors.merge(detail::validate_graph(system, nodes, edges));
  if (!errors.empty()) return fail(std::move(errors));

  return Graph::create(std::move(system), std::move(nodes), std::move(edges));
}

Result<Graph> load_document(const YAML::Node& root, Diagnostics* warnings) {
  if (is_absent(root)) {
    return fail(DiagCode::YamlParse, "", "document is empty");
  }
  if (!root.IsMap()) {
    return fail(DiagCode::WrongType, "", "top level must be a mapping");
  }

  Diagnostics diags;

  check_keys(root, "", {"schemaVersion", "system", "nodes"}, {"areas", "layout"}, diags);
  read_schema_version(root["schemaVersion"], diags);

  System system;
  if (!is_absent(root["system"])) {
    system = read_system(root["system"], "system", diags);
  }

  std::vector<Node> nodes;
  std::vector<Edge> edges;
  const YAML::Node nodes_node = root["nodes"];
  if (is_absent(nodes_node)) {
    // Left to validate_graph, so that "key absent" and "empty list" read alike.
  } else if (!nodes_node.IsSequence()) {
    diags.add(DiagCode::WrongType, "nodes", "expected a sequence");
  } else {
    nodes.reserve(nodes_node.size());
    for (std::size_t i = 0; i < nodes_node.size(); ++i) {
      nodes.push_back(read_node(nodes_node[i], index_path("nodes", i), edges, diags));
    }
  }

  return finish(std::move(system), std::move(nodes), std::move(edges), diags, warnings);
}

}  // namespace

Result<Graph> load_yaml_string(std::string_view yaml, Diagnostics* warnings) {
  YAML::Node root;
  try {
    root = YAML::Load(std::string{yaml});
  } catch (const YAML::Exception& error) {
    return fail(DiagCode::YamlParse, "", error.what());
  }
  return load_document(root, warnings);
}

Result<Graph> load_yaml_file(const std::filesystem::path& path, Diagnostics* warnings) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return fail(DiagCode::IoError, path.string(), "cannot open file for reading");
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();
  if (!file && !file.eof()) {
    return fail(DiagCode::IoError, path.string(), "cannot read file");
  }
  return load_yaml_string(buffer.str(), warnings);
}

}  // namespace stgx
