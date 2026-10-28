// shh-nx (2026-10-28): stage 2 scaffolding. The renderer's core (backend, render, guest records) compiles before the
// shader and draw hooks are retargeted to SHH's addresses (docs/d3d-censo.md); the core still calls four record
// lookups that d3d/hooks/shader.cpp defines, so this file stands in for them until it arrives:
//
//   d3d/hooks/shader.cpp      FindShaderLocked, FindGuestDeclarationLocked, ShaderFetchDeclarationLocked, GetHostShader
//
// Every body here is inert on purpose. Delete this file, and the entry it needs in cmake/shh_renut_gpu.cmake, as
// shader.cpp comes in.
#include "renut_engine/gpu/d3d/d3d_native.h"
#include "renut_engine/gpu/guest/resources.h"

namespace renut::gpu {

ShaderRecord *FindShaderLocked(u32 /*shader_va*/, RecordKind /*kind*/) { return nullptr; }

const VertexDeclarationRecord *FindGuestDeclarationLocked(u32 /*declaration_va*/) { return nullptr; }

const VertexDeclarationRecord *ShaderFetchDeclarationLocked(ShaderRecord & /*vs*/) { return nullptr; }

plume::RenderShader *GetHostShader(ShaderRecord * /*shader*/) { return nullptr; }

}  // namespace renut::gpu
