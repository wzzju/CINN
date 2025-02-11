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

#include "cinn/backends/cuda_util.h"
#include "cinn/frontend/op_mapper_registry.h"
#include "cinn/frontend/op_mappers/common_utils.h"

namespace cinn {
namespace frontend {
namespace op_mappers {

void MulOpMapper(const paddle::cpp::OpDesc& op_desc, const OpMapperContext& ctx) {
  CHECK_EQ(op_desc.Input("X").size(), 1UL);
  auto x_name = op_desc.Input("X").front();
  CHECK_EQ(op_desc.Input("Y").size(), 1UL);
  auto y_name = op_desc.Input("Y").front();
  auto x      = ctx.GetVar(x_name);
  // We replace `TransposeVar(y_name, ctx)` by reshape op and transpose op to
  // support CINN for training because we don't operate on data during net
  // building in training stage. Consider a better code to integrate infer
  // and training.
  // TransposeVar(y_name, ctx);
  auto y = ctx.GetVar(y_name);

  CHECK_EQ(y->shape.size(), 2UL) << "The y data's shape size of op [mul] is not equal to 2! Please check.";
  VLOG(4) << "input y shape: " << cinn::utils::Join(y->shape, ",");

  // In origin `TransposeVar(y_name, ctx)`, it transpose the variable when
  // not using CUDNN, else just reshape.
  Variable tran_y;
  if (ctx.Target().arch == Target::Arch::NVGPU) {
#ifdef CINN_WITH_CUDNN
    auto tran_shape = y->shape;
    std::swap(tran_shape[0], tran_shape[1]);
    tran_y = ctx.Builder()->Reshape(y, tran_shape);
    VLOG(4) << "Run with CUDNN and reshape y to" << cinn::utils::Join(tran_y->shape, ",");
#else
    tran_y = ctx.Builder()->Transpose(y, {1, 0});
    VLOG(4) << "Run Not with CUDNN and transpose y to" << cinn::utils::Join(tran_y->shape, ",");
#endif
  } else {
    tran_y = ctx.Builder()->Transpose(y, {1, 0});
    VLOG(4) << "Run Not with CUDNN and transpose y to" << cinn::utils::Join(tran_y->shape, ",");
  }

  auto x_num_col_dims = utils::GetAttrOrDefault<int>(op_desc, "x_num_col_dims", 1);
  auto y_num_col_dims = utils::GetAttrOrDefault<int>(op_desc, "y_num_col_dims", 1);

  VLOG(4) << "Mul x_num_col_dims: " << x_num_col_dims;
  VLOG(4) << "Mul y_num_col_dims: " << y_num_col_dims;
  VLOG(4) << "x shape: " << cinn::utils::Join(x->shape, ",");
  VLOG(4) << "y shape: " << cinn::utils::Join(tran_y->shape, ",");
  auto out = ctx.Builder()->Mul(x, tran_y, x_num_col_dims, y_num_col_dims);
  CHECK_EQ(op_desc.Output("Out").size(), 1UL);
  auto out_name = op_desc.Output("Out").front();
  ctx.AddVar(out_name, out);
  ctx.AddVarModelToProgram(out_name, out->id);
}

void MulBiasOpMapper(const paddle::cpp::OpDesc& op_desc, const OpMapperContext& ctx) {
  CHECK_EQ(op_desc.Input("X").size(), 1UL);
  auto x_name = op_desc.Input("X").front();
  CHECK_EQ(op_desc.Input("Y").size(), 1UL);
  auto y_name = op_desc.Input("Y").front();
  CHECK_EQ(op_desc.Input("Z").size(), 1UL);
  auto z_name = op_desc.Input("Z").front();

  auto x = ctx.GetVar(x_name);
  // We replace `TransposeVar(y_name, ctx)` by reshape op and transpose op to
  // support CINN for training because we don't operate on data during net
  // building in training stage. Consider a better code to integrate infer
  // and training.
  // TransposeVar(y_name, ctx);
  auto y = ctx.GetVar(y_name);
  auto z = ctx.GetVar(z_name);

  CHECK_EQ(y->shape.size(), 2UL) << "The y data's shape size of op [mul] is not equal to 2! Please check.";
  VLOG(4) << "input y shape: " << cinn::utils::Join(y->shape, ",");

  // In origin `TransposeVar(y_name, ctx)`, it transpose the variable when
  // not using CUDNN, else just reshape.
  Variable tran_y;
  if (ctx.Target().arch == Target::Arch::NVGPU) {
#ifdef CINN_WITH_CUDNN
    auto tran_shape = y->shape;
    std::swap(tran_shape[0], tran_shape[1]);
    tran_y = ctx.Builder()->Reshape(y, tran_shape);
    VLOG(4) << "Run with CUDNN and reshape y to" << cinn::utils::Join(tran_y->shape, ",");
#else
    tran_y = ctx.Builder()->Transpose(y, {1, 0});
    VLOG(4) << "Run Not with CUDNN and transpose y to" << cinn::utils::Join(tran_y->shape, ",");
#endif
  } else {
    tran_y = ctx.Builder()->Transpose(y, {1, 0});
    VLOG(4) << "Run Not with CUDNN and transpose y to" << cinn::utils::Join(tran_y->shape, ",");
  }

  auto x_num_col_dims = utils::GetAttrOrDefault<int>(op_desc, "x_num_col_dims", 1);
  auto y_num_col_dims = utils::GetAttrOrDefault<int>(op_desc, "y_num_col_dims", 1);

  VLOG(4) << "Mul x_num_col_dims: " << x_num_col_dims;
  VLOG(4) << "Mul y_num_col_dims: " << y_num_col_dims;
  VLOG(4) << "x shape: " << cinn::utils::Join(x->shape, ",");
  VLOG(4) << "y shape: " << cinn::utils::Join(tran_y->shape, ",");
  VLOG(4) << "z shape: " << cinn::utils::Join(z->shape, ",");
  auto out = ctx.Builder()->MulBias(x, tran_y, z, x_num_col_dims, y_num_col_dims);

  CHECK_EQ(op_desc.Output("Out").size(), 1UL);
  auto out_name = op_desc.Output("Out").front();
  ctx.AddVar(out_name, out);
  ctx.AddVarModelToProgram(out_name, out->id);
}

}  // namespace op_mappers
}  // namespace frontend
}  // namespace cinn

CINN_REGISTER_HELPER(mul) {
  CINN_REGISTER_OP_MAPPER(mul, cinn::frontend::op_mappers::MulOpMapper)
  CINN_REGISTER_OP_MAPPER(mulbias, cinn::frontend::op_mappers::MulBiasOpMapper)
  return true;
}
