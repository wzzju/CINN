// Copyright (c) 2022 CINN Authors. All Rights Reserved.
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

#include <ios>
#include <unordered_map>
#include <unordered_set>

#include "cinn/frontend/cinn_builder.h"
#include "cinn/frontend/program_pass.h"
#include "glog/logging.h"

namespace cinn {
namespace frontend {
namespace pass {

class GemmRewriterPass : public ProgramPass {
 public:
  using ProgramPass::ProgramPass;

  void ApplyImpl(Program* prog, const std::unordered_set<std::string>& fetch_ids, const common::Target& target) {
    if (target.arch != Target::Arch::NVGPU || !prog->size()) {
      return;
    }

    LOG(INFO) << "-- Origin: " << *prog;

    CollectInfo(*prog);

    CinnBuilder builder("gemm_rewriter_builder");
    for (auto& var : prog->GetInputs()) {
      builder.CreateInput(var);
    }
    for (int i = prog->size() - 1; i >= 0; i--) {
      auto& instr = prog->operator[](i);
      if (instr->op_type == "elementwise_add") {
        auto fused = DoGemmFusion(&builder, instr, fetch_ids);
        if (fused) {
          // the elementwise_add is fused in gemm, just skip it
          continue;
        }
      }
      if (!removed_instrs_.count(instr.get())) {
        builder.AppendInstruction(instr);
      }
    }
    *prog = builder.Build(true);

    // relink old outputs to new outputs
    for (size_t i = 0; i < prog->size(); i++) {
      auto& inputs = (*prog)[i]->inputs;
      for (size_t j = 0; j < inputs.size(); j++) {
        if (origin2new_.count(inputs[j].get())) {
          inputs[j] = origin2new_.at(inputs[j].get());
        }
      }
    }
    LOG(INFO) << "-- Update: " << *prog;
  }

 private:
  void CollectInfo(const Program& prog) {
    for (size_t i = 0; i < prog.size(); i++) {
      auto& instr = prog[i];
      for (auto& var : instr->outputs) {
        output2instr_.emplace(var.get(), instr);
      }
      for (auto& var : instr->inputs) {
        var_used_count_[var.get()]++;
      }
    }
  }

  bool DoGemmFusion(CinnBuilder* builder, const Instruction& instr, const std::unordered_set<std::string>& fetch_ids) {
    CHECK_EQ(instr->inputs.size(), 2) << "elementwise should have only two inputs";
    std::vector<Variable> inputs;
    bool trans_a = false;
    bool trans_b = false;
    for (auto& var : instr->inputs) {
      auto it = output2instr_.find(var.get());
      if (it != output2instr_.end() && it->second->op_type == "matmul") {
        // If the output var of matmul is consumed by more than one instruction or
        // a fetch var, just skip to fuse it.
        CHECK_GT(var_used_count_.count(var.get()), 0);
        if ((var_used_count_.at(var.get()) > 1) || fetch_ids.count(var->id)) {
          continue;
        }

        auto& matmul_instr = it->second;
        // set inputs of mulbias
        inputs     = matmul_instr->inputs;
        auto& bias = instr->inputs[0].get() == var.get() ? instr->inputs[1] : instr->inputs[0];
        inputs.emplace_back(bias);
        // set attrs of mulbias
        auto& attrs = matmul_instr->attrs;
        if (attrs.count("trans_a")) {
          trans_a = absl::get<bool>(attrs.at("trans_a"));
        }
        if (attrs.count("trans_b")) {
          trans_b = absl::get<bool>(attrs.at("trans_b"));
        }

        // After the fusion, matmul and elementwise_add should be removed.
        removed_instrs_.emplace(matmul_instr.get());
        removed_instrs_.emplace(instr.get());
        break;
      }
    }

    if (inputs.size() == 3) {
      LOG(INFO) << "-- trans_a = " << std::boolalpha << trans_a;
      LOG(INFO) << "-- trans_b = " << std::boolalpha << trans_b;
      const auto& new_outs = builder->CustomInstr("cublas_gemm", inputs, {{"trans_a", trans_a}, {"trans_b", trans_b}});
      auto new_out         = new_outs[0];
      auto old_out         = instr.GetOutputs()[0];
      new_out->id          = old_out->id;
      origin2new_.emplace(old_out.get(), new_out);
      return true;
    }

    CHECK_EQ(inputs.size(), 0) << "The gemm should only have three inputs.";
    return false;
  }

  std::unordered_set<_Instruction_*> removed_instrs_;
  std::unordered_map<_Variable_*, Variable> origin2new_;
  std::unordered_map<_Variable_*, Instruction> output2instr_;
  std::unordered_map<_Variable_*, int> var_used_count_;
};

}  // namespace pass
}  // namespace frontend
}  // namespace cinn

namespace fp = ::cinn::frontend::pass;
CINN_REGISTER_HELPER(GemmRewriter) {
  CINN_REGISTER_PROGRAM_PASS(GemmRewriter, fp::GemmRewriterPass);

  return true;
}
