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

#include "cinn/frontend/op_mapper_registry.h"
#include "cinn/frontend/op_mappers/common_utils.h"

namespace cinn {
namespace frontend {
namespace op_mappers {

void SliceOpMapper(const paddle::cpp::OpDesc& op_desc, const OpMapperContext& ctx) {
  CHECK_EQ(op_desc.Input("Input").size(), 1UL);
  auto x_name = op_desc.Input("Input").front();
  CHECK_EQ(op_desc.Output("Out").size(), 1UL);
  auto out_name = op_desc.Output("Out").front();

  CHECK(op_desc.HasAttr("starts"));
  auto starts = op_desc.GetAttr<std::vector<int>>("starts");
  CHECK(op_desc.HasAttr("ends"));
  auto ends = op_desc.GetAttr<std::vector<int>>("ends");
  CHECK(op_desc.HasAttr("axes"));
  auto axes = op_desc.GetAttr<std::vector<int>>("axes");

  auto infer_flags   = utils::GetAttrOrDefault<std::vector<int>>(op_desc, "infer_flags");
  auto decrease_axis = utils::GetAttrOrDefault<std::vector<int>>(op_desc, "decrease_axis");
  auto x             = ctx.GetVar(x_name);
  auto out           = ctx.Builder()->Slice(x, axes, starts, ends, infer_flags, decrease_axis);

  ctx.AddVar(out_name, out);
  ctx.AddVarModelToProgram(out_name, out->id);
}

}  // namespace op_mappers
}  // namespace frontend
}  // namespace cinn

CINN_REGISTER_HELPER(slice) {
  CINN_REGISTER_OP_MAPPER(slice, cinn::frontend::op_mappers::SliceOpMapper)
  return true;
}
