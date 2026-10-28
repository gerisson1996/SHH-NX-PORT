// banjo-nx - PC build only: the Switch profiler counters the renderer reports to (sdk/src/ui/switch_perf.cpp on the
// Switch) have nothing to feed on PC. Copied unchanged into shh-nx (2026-10-28).
#include <cstdint>

extern "C" void RexSwitchPerfCount(unsigned) {}
extern "C" void RexSwitchPerfHitch(uint64_t, uint64_t) {}
