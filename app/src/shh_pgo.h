// shh-nx (2026-11-11, from mgsv-nx and sa2-nx): profile collection of the PGO-instrumented Switch build (SHH_PGO=generate), see shh_pgo.cpp.
// In every other build these do nothing.

#pragma once

namespace shh::pgo {

// Picks this session's file (pgo/shh_pgo_<N>.stream next to the executable) and starts the 2-minute dumps.
void Start();
// Writes every count so far to this session's file (the whole session each time, replacing the previous dump).
void DumpNow();
// The last dump, then the dump thread ends.
void Stop();

}  // namespace shh::pgo
