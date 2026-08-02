#include <yaml-cpp/yaml.h>

#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <stgx/diagnostics.hpp>
#include <stgx/graph.hpp>
#include <stgx/model.hpp>
#include <stgx/result.hpp>
#include <stgx/yaml.hpp>

namespace stgx {
namespace {

void emit_system(YAML::Emitter& out, const System& system) {
  out << YAML::Key << "system" << YAML::Value << YAML::BeginMap;
  if (system.description.has_value()) {
    out << YAML::Key << "description" << YAML::Value << *system.description;
  }
  if (system.m.has_value()) {
    out << YAML::Key << "m" << YAML::Value << *system.m;
  }
  if (!system.tasks.empty()) {
    out << YAML::Key << "tasks" << YAML::Value << YAML::BeginSeq;
    for (const TaskParam& task : system.tasks) {
      out << YAML::Flow << YAML::BeginMap;
      out << YAML::Key << "c" << YAML::Value << task.c;
      out << YAML::Key << "d" << YAML::Value << task.d;
      if (task.name.has_value()) out << YAML::Key << "name" << YAML::Value << *task.name;
      out << YAML::EndMap;
    }
    out << YAML::EndSeq;
  }
  out << YAML::EndMap;
}

/// Write the `children` or `loopback` list of one node, skipping it entirely
/// when the node has no edges of that kind.
void emit_targets(YAML::Emitter& out, std::string_view field, std::span<const Edge> edges,
                  EdgeKind kind) {
  bool opened = false;
  for (const Edge& edge : edges) {
    if (edge.kind != kind) continue;
    if (!opened) {
      out << YAML::Key << std::string{field} << YAML::Value << YAML::Flow << YAML::BeginSeq;
      opened = true;
    }
    out << edge.target;
  }
  if (opened) out << YAML::EndSeq;
}

void emit_node(YAML::Emitter& out, const Node& node, std::span<const Edge> out_edges) {
  out << YAML::BeginMap;
  out << YAML::Key << "id" << YAML::Value << node.id;
  if (node.initial) out << YAML::Key << "initial" << YAML::Value << true;
  if (node.label.has_value()) out << YAML::Key << "label" << YAML::Value << *node.label;
  if (node.border_color.has_value()) {
    out << YAML::Key << "borderColor" << YAML::Value << *node.border_color;
  }
  if (node.fill_color.has_value()) {
    out << YAML::Key << "fillColor" << YAML::Value << *node.fill_color;
  }

  if (!node.tasks.empty()) {
    out << YAML::Key << "tasks" << YAML::Value << YAML::BeginSeq;
    for (const TaskState& task : node.tasks) {
      out << YAML::Flow << YAML::BeginMap;
      out << YAML::Key << "c" << YAML::Value << task.c;
      out << YAML::Key << "d" << YAML::Value << task.d;
      if (task.release != Release::None) {
        out << YAML::Key << "release" << YAML::Value << std::string{to_string(task.release)};
      }
      out << YAML::EndMap;
    }
    out << YAML::EndSeq;
  }

  emit_targets(out, "children", out_edges, EdgeKind::Transition);
  emit_targets(out, "loopback", out_edges, EdgeKind::Loopback);
  out << YAML::EndMap;
}

}  // namespace

Result<std::string> save_yaml_string(const Graph& graph) {
  if (auto valid = graph.validate(); !valid) {
    return fail(std::move(valid).error());
  }

  YAML::Emitter out;
  out << YAML::BeginMap;
  out << YAML::Key << "schemaVersion" << YAML::Value << kSchemaVersion;
  if (!graph.system().empty()) emit_system(out, graph.system());

  out << YAML::Key << "nodes" << YAML::Value << YAML::BeginSeq;
  for (const Node& node : graph.nodes()) {
    emit_node(out, node, graph.out_edges(node.id));
  }
  out << YAML::EndSeq;
  out << YAML::EndMap;

  if (!out.good()) {
    return fail(DiagCode::YamlParse, "", "emitter failed: " + out.GetLastError());
  }
  return std::string{out.c_str()} + "\n";
}

Result<void> save_yaml_file(const Graph& graph, const std::filesystem::path& path) {
  auto text = save_yaml_string(graph);
  if (!text) return fail(std::move(text).error());

  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file) {
    return fail(DiagCode::IoError, path.string(), "cannot open file for writing");
  }
  file << *text;
  if (!file) {
    return fail(DiagCode::IoError, path.string(), "cannot write file");
  }
  return {};
}

}  // namespace stgx
