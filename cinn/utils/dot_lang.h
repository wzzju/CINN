// Copyright (c) 2021 CINN Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once
#include <glog/logging.h>

#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace cinn {
namespace utils {

static size_t dot_node_counter{0};

struct Node;
struct Edge;
struct Attr;
/*
 * A Dot template that helps to build a DOT graph definition.
 */
class DotLang {
 public:
  DotLang() = default;

  explicit DotLang(const std::vector<Attr>& attrs) : attrs_(attrs) {}
  /**
   * Add a node ot DOT graph.
   * @param id Unique ID for this node.
   * @param attrs DOT attributes.
   * @param label Name of the node.
   */

  void AddNode(const std::string& id, const std::vector<Attr>& attrs, std::string label = "");

  /**
   * Add an edge to the DOT graph.
   * @param source The id of the source of the edge.
   * @param target The id of the sink of the edge.
   * @param attrs The attributes of the edge.
   */
  void AddEdge(const std::string& source, const std::string& target, const std::vector<Attr>& attrs);

  std::string operator()() const { return Build(); }

 private:
  // Compile to DOT language codes.
  std::string Build() const;

  std::map<std::string, Node> nodes_;
  std::vector<Edge> edges_;
  std::vector<Attr> attrs_;
};

struct Attr {
  std::string key;
  std::string value;

  Attr(const std::string& key, const std::string& value) : key(key), value(value) {}

  std::string repr() const;
};

struct Node {
  std::string name;
  std::vector<Attr> attrs;

  Node(const std::string& name, const std::vector<Attr>& attrs);

  std::string id() const { return id_; }

  std::string repr() const;

 private:
  std::string id_;
};

struct Edge {
  std::string source;
  std::string target;
  std::vector<Attr> attrs;

  Edge(const std::string& source, const std::string& target, const std::vector<Attr>& attrs)
      : source(source), target(target), attrs(attrs) {}

  std::string repr() const;
};

}  // namespace utils
}  // namespace cinn
