// shh-nx (2026-10-28): the two master switches Renut's banjo_* keys are grouped under, as SA2's enhancement file
// (sa2-nx's src/cpu/sa2_enhancements.h) gives them to banjo-nx's renderer. SHH has no enhancement list yet: the two
// masters only decide whether a banjo_* key of Renut (banjo_renut_record_worker and the like) is honoured, and both
// default to on so those keys work as they do in the references.
#pragma once

namespace shh::enh {

// shh_cpu_enhancements / shh_noncpu_enhancements: REXCVAR_DEFINE_BOOL in shh_compat.cpp.
bool CpuMaster();
bool NonCpuMaster();

}  // namespace shh::enh
