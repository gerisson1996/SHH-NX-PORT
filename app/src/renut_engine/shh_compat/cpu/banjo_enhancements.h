// shh-nx (2026-10-28): banjo-nx's enhancement switches, as Renut's renderer (through banjo-nx) asks for them, answered
// by SHH's own two masters (src/cpu/shh_enhancements.h): a banjo_* key is active when it is on and its group's master
// is on (shh_cpu_enhancements / shh_noncpu_enhancements).
#pragma once

#include "cpu/shh_enhancements.h"

namespace banjo {

enum class EnhancementGroup { kCpu, kNonCpu };

inline bool EnhancementActive(EnhancementGroup group, bool own_key) {
  return own_key && (group == EnhancementGroup::kCpu ? shh::enh::CpuMaster() : shh::enh::NonCpuMaster());
}

// The generated code's self-checks: SHH has no enhancement report yet, so nothing to collect.
inline void CodegenChecksReport(int) {}

}  // namespace banjo
