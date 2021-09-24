#include "cinn/frontend/symbolization/coarse_builder.h"

#include <gtest/gtest.h>

#include <memory>
#include <random>

#include "cinn/common/target.h"
#include "cinn/frontend/syntax.h"
#include "cinn/hlir/framework/graph.h"
#include "cinn/hlir/framework/graph_compiler.h"
#include "cinn/hlir/framework/pass.h"
#include "cinn/hlir/framework/tensor.h"
#include "cinn/hlir/op/use_ops.h"
#include "cinn/hlir/pass/use_pass.h"

namespace cinn {
namespace frontend {
namespace symbolization {

Program CreateAddProgram() {
  constexpr int M = 32;
  constexpr int N = 24;

  CoarseBuilder builder("coarse_builder");
  auto a       = builder.CreateInput(Float(32), {M, N});
  auto b       = builder.CreateInput(Float(32), {M, N});
  auto c       = builder.add(a, b);
  auto d       = builder.add(a, c);
  auto program = builder.Build();
  program.Validate();

  return program;
}

void SetRandData(hlir::framework::Tensor tensor, Target target) {
  auto* data = tensor->mutable_data<float>(target);
  std::random_device seed;
  std::default_random_engine engine(seed());
  std::uniform_real_distribution<float> dist(0.f, 1.f);
  for (size_t j = 0; j < tensor->shape().numel(); j++) {
    data[j] = dist(engine);  // All random data
  }
}

TEST(coarse_build, basic) {
  auto program = CreateAddProgram();
  // output program
  for (int i = 0; i < program.size(); i++) {
    LOG(INFO) << "instruction: " << program[i];
  }
}

TEST(coarse_build, program_execute_multi_elementwise_add) {
  auto program  = CreateAddProgram();
  Target target = common::DefaultHostTarget();
  auto graph    = std::make_shared<hlir::framework::Graph>(program, target);
  LOG(INFO) << "graph:\n" << graph->Visualize();

  hlir::framework::ApplyPass(graph.get(), "InferShape");
  auto scope = BuildScope(target, graph);

  hlir::framework::GraphCompiler gc(target, scope, graph);
  auto runtime_program = gc.Build();

  scope->Var<hlir::framework::Tensor>("A");
  scope->Var<hlir::framework::Tensor>("B");

  auto A = scope->GetTensor("A");
  auto B = scope->GetTensor("B");
  SetRandData(A, target);
  SetRandData(B, target);

  runtime_program->Execute();
}

TEST(coarse_build, program_execute_fc) {
  constexpr int B = 10;  // batch size
  constexpr int M = 32;
  constexpr int K = 18;
  constexpr int N = 24;

  CoarseBuilder builder("coarse_builder");
  auto a = builder.CreateInput(Float(32), {B, M, K}, "A");
  auto w = builder.CreateInput(Float(32), {N, K}, "W");  // weight
  auto b = builder.CreateInput(Float(32), {N}, "B");     // bias

  auto mul_out = builder.mul(a, w, 2, 1);
  auto add_out = builder.add(mul_out, b);
  auto program = builder.Build();
  program.Validate();

  Target target = common::DefaultHostTarget();
  auto graph    = std::make_shared<hlir::framework::Graph>(program, target);

  hlir::framework::ApplyPass(graph.get(), "InferShape");
  auto scope = BuildScope(target, graph);

  hlir::framework::GraphCompiler gc(target, scope, graph);
  auto runtime_program = gc.Build();

  scope->Var<hlir::framework::Tensor>(std::string(a.id()));
  scope->Var<hlir::framework::Tensor>(std::string(w.id()));
  scope->Var<hlir::framework::Tensor>(std::string(b.id()));
  scope->Var<hlir::framework::Tensor>(std::string(mul_out->id));

  auto a_ten        = scope->GetTensor(std::string(a.id()));
  auto w_ten        = scope->GetTensor(std::string(w.id()));
  auto b_ten        = scope->GetTensor(std::string(b.id()));
  auto fake_out_ten = scope->GetTensor(std::string(mul_out->id));
  auto add_out_ten  = scope->GetTensor(std::string(add_out->id));
  SetRandData(a_ten, target);
  SetRandData(w_ten, target);
  SetRandData(b_ten, target);

  runtime_program->Execute();
}

}  // namespace symbolization
}  // namespace frontend
}  // namespace cinn
