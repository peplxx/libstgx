#include <stgx/diagnostics.hpp>

#include <algorithm>
#include <format>
#include <string>
#include <string_view>

#include <stgx/model.hpp>

namespace stgx::diag {

Diagnostic node_not_found(std::string_view id) {
  return {
      .code = DiagCode::NodeNotFound,
      .severity = Severity::Error,
      .path = {},
      .message = std::format(R"(node "{}" does not exist)", id),
  };
}

Diagnostic node_id_empty() {
  return {
      .code = DiagCode::NodeIdEmpty,
      .severity = Severity::Error,
      .path = {},
      .message = "node id must not be empty",
  };
}

Diagnostic node_id_duplicate(std::string_view id) {
  return {
      .code = DiagCode::NodeIdDuplicate,
      .severity = Severity::Error,
      .path = {},
      .message = std::format(R"(node "{}" already exists)", id),
  };
}
}  // namespace stgx::diag

namespace stgx {

std::string_view to_string(DiagCode code) {
  switch (code) {
    case DiagCode::YamlParse:
      return "yaml_parse";
    case DiagCode::SchemaVersion:
      return "schema_version";
    case DiagCode::NodesMissing:
      return "nodes_missing";
    case DiagCode::NodeIdEmpty:
      return "node_id_empty";
    case DiagCode::NodeIdDuplicate:
      return "node_id_duplicate";
    case DiagCode::NodeNotFound:
      return "node_not_found";
    case DiagCode::DanglingEdge:
      return "dangling_edge";
    case DiagCode::InitialCount:
      return "initial_count";
    case DiagCode::TaskArityMismatch:
      return "task_arity_mismatch";
    case DiagCode::NotAnInt:
      return "not_an_int";
    case DiagCode::WrongType:
      return "wrong_type";
    case DiagCode::BadRelease:
      return "bad_release";
    case DiagCode::UnsupportedField:
      return "unsupported_field";
    case DiagCode::UnknownField:
      return "unknown_field";
    case DiagCode::IoError:
      return "io_error";
  }
  return "unknown";
}

std::string_view to_string(Severity severity) {
  switch (severity) {
    case Severity::Error:
      return "error";
    case Severity::Warning:
      return "warning";
  }
  return "unknown";
}

std::string_view to_string(Release release) {
  switch (release) {
    case Release::None:
      return "none";
    case Release::Up:
      return "up";
    case Release::Down:
      return "down";
  }
  return "none";
}

std::string_view to_string(EdgeKind kind) {
  switch (kind) {
    case EdgeKind::Transition:
      return "transition";
    case EdgeKind::Loopback:
      return "loopback";
  }
  return "transition";
}

std::string Diagnostic::to_string() const {
  std::string out = std::format("{}[{}]", stgx::to_string(severity), stgx::to_string(code));

  if (!path.empty()) {
    std::format_to(std::back_inserter(out), " {}", path);
  }

  if (!message.empty()) {
    std::format_to(std::back_inserter(out), ": {}", message);
  }

  return out;
}

bool Diagnostics::has_errors() const noexcept {
  return std::ranges::any_of(items_, [](const Diagnostic& diag) { return diag.is_error(); });
}

bool Diagnostics::contains(DiagCode code) const noexcept {
  return std::ranges::any_of(items_, [code](const Diagnostic& diag) { return diag.code == code; });
}

Diagnostics Diagnostics::errors() const {
  Diagnostics out;
  for (const Diagnostic& diag : items_) {
    if (diag.is_error()) out.add(diag);
  }
  return out;
}

Diagnostics Diagnostics::warnings() const {
  Diagnostics out;
  for (const Diagnostic& diag : items_) {
    if (!diag.is_error()) out.add(diag);
  }
  return out;
}

std::string Diagnostics::to_string() const {
  std::string out;
  for (const Diagnostic& diag : items_) {
    if (!out.empty()) out += '\n';
    out += diag.to_string();
  }
  return out;
}

}  // namespace stgx
