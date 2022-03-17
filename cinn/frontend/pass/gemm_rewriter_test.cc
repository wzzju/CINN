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

#include <random>
#include <unordered_set>

#include "cinn/frontend/net_builder.h"
#include "cinn/frontend/pass/use_program_pass.h"
#include "cinn/frontend/program_pass.h"
#include "cinn/hlir/framework/graph.h"
#include "cinn/hlir/framework/graph_compiler.h"
#include "cinn/hlir/framework/pass.h"
#include "cinn/hlir/framework/tensor.h"
#include "cinn/hlir/op/use_ops.h"
#include "cinn/hlir/pass/use_pass.h"
#include "gtest/gtest.h"

namespace cinn::frontend {

namespace {

bool IsCompiledWithCUDA() {
#if !defined(CINN_WITH_CUDA)
  return false;
#else
  return true;
#endif
}

void SetRandData(hlir::framework::Tensor tensor, Target target) {
  auto* data = tensor->mutable_data<float>(target);
  std::random_device seed;
  std::default_random_engine engine(seed());
  std::uniform_real_distribution<float> dist(0.f, 1.f);
  size_t num_ele = tensor->shape().numel();
  std::vector<float> random_data(num_ele);
  for (size_t i = 0; i < num_ele; i++) {
    random_data[i] = dist(engine);  // All random data
  }

#ifdef CINN_WITH_CUDA
  cudaMemcpy(data, random_data.data(), num_ele * sizeof(float), cudaMemcpyHostToDevice);
#else
  std::copy(random_data.begin(), random_data.end(), data);
#endif
}

std::vector<float> GetTensorData(const hlir::framework::Tensor& tensor, Target target) {
  auto size = tensor->shape().numel();
  std::vector<float> data(size);
#ifdef CINN_WITH_CUDA
  cudaMemcpy(
      data.data(), static_cast<const void*>(tensor->data<float>()), size * sizeof(float), cudaMemcpyDeviceToHost);
#else
  std::copy(tensor->data<float>(), tensor->data<float>() + size, data.begin());
#endif
  return data;
}

void RunGraph(std::shared_ptr<hlir::framework::Graph> graph,
              const Target& target,
              const std::shared_ptr<hlir::framework::Scope>& scope,
              std::unordered_set<std::string>&& fetch_ids) {
  hlir::framework::ApplyPass(graph.get(), "OpFusion");
  LOG(INFO) << "Graph Viz:\n" << graph->Visualize();
  hlir::framework::GraphCompiler gc(target, scope, graph);
  auto runtime_program = gc.Build();
  runtime_program->Execute();
}

std::vector<float> RunProgram(const Program& program,
                              const Target& target,
                              std::vector<cinn::frontend::Placeholder> inputs,
                              std::vector<std::string> output_ids) {
  auto graph = std::make_shared<hlir::framework::Graph>(program, target);
  auto scope = hlir::framework::BuildScope(target, graph);
  for (auto& input : inputs) {
    scope->Var<hlir::framework::Tensor>(std::string(input.id()));
    SetRandData(scope->GetTensor(std::string(input.id())), target);
  }
  std::unordered_set<std::string> fetch_ids(output_ids.begin(), output_ids.end());
  RunGraph(graph, target, scope, std::move(fetch_ids));
  return GetTensorData(scope->GetTensor(output_ids.front()), target);
}

}  // namespace

TEST(GemmRwriter, Basic) {
  if (!IsCompiledWithCUDA()) {
    return;
  }
  NetBuilder builder("net_builder");
  auto a       = builder.FillConstant<float>({2, 20}, 2.0f, "A");
  auto b       = builder.Transpose(a, {1, 0});
  auto c       = builder.CreateInput(Float(32), {121, 20}, "C");
  auto d       = builder.Matmul(c, b);
  auto x       = builder.FillConstant<float>({2, 20}, 1.0f, "X");
  auto y       = builder.Transpose(x, {1, 0});
  auto z       = builder.CreateInput(Float(32), {20, 121}, "Z");
  auto l       = builder.Transpose(z, {1, 0});
  auto q       = builder.Matmul(l, y);
  auto p       = builder.Mul(c, a);
  auto m       = builder.Sub(d, p);
  auto n       = builder.Add(d, q);
  auto out     = builder.Add(m, n);
  auto program = builder.Build();

  Target target = common::DefaultNVGPUTarget();
  std::unordered_set<std::string> fetch_ids{out->id};
  // apply common pass
  ProgramPass::Apply(&program, fetch_ids, target, {"Decomposer"});
  ::cinn::frontend::ApplyPass(&program, fetch_ids, "RemoveIdentity");

  // get origin output
  auto origin_out = RunProgram(program, target, {c, z}, {out->id});

  // fuse transpose + add + dot, then run and get the fused output
  ApplyPass(&program, {}, "TransposeFolding");
  ProgramPass::Apply(&program, {}, target, {"GemmRewriter"});
  auto fused_out = RunProgram(program, target, {c, z}, {out->id});
}

}  // namespace cinn::frontend
