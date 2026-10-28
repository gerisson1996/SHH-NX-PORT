// shh-nx (2026-10-28): the empty shader cache of stage 2 (docs/plano-renut.md). Renut's renderer reads the cache
// XenosRecomp generates (gpu/shaders/shader_cache.h); without translated shaders every lookup misses, which is what
// the device, ring, fence and Swap work of stage 2 needs. Stage 3 replaces this file with the generated cache.
#include "renut_engine/gpu/shaders/shader_cache.h"

ShaderLiteral g_shaderLiterals[1] = {};
ShaderCacheEntry g_shaderCacheEntries[1] = {};
const size_t g_shaderCacheEntryCount = 0;
ShaderVertexInput g_shaderVertexInputs[1] = {};
const uint32_t g_shaderCacheLayout = kShaderCacheLayout;
const uint8_t g_compressedDxilCache[1] = {};
const size_t g_dxilCacheCompressedSize = 0;
const size_t g_dxilCacheDecompressedSize = 0;
const uint8_t g_compressedSpirvCache[1] = {};
const size_t g_spirvCacheCompressedSize = 0;
const size_t g_spirvCacheDecompressedSize = 0;
