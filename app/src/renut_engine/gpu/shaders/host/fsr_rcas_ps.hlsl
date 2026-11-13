// shh-nx (2026-10-31, shh_fsr): AMD FidelityFX Super Resolution 1.0, its sharpening pass (RCAS: robust contrast adaptive
// sharpening), over fsr_easu_ps's output, then the scan-out's own steps (scan_out.hlsli: the game's display ramp and
// the output gamma). After AMD's ffx_fsr1.h (MIT, Copyright (c) 2021 Advanced Micro Devices, Inc.;
// THIRD_PARTY_NOTICES.md), its 32-bit path without the denoise option.
//
// The destination pixel plus g_Push.translate is the source texel (EASU's target has the viewport's size, at 0, 0), and
// g_Push.scale its last texel (this shader does not read present_vs's uv); samplerIndex's bits = the sharpness as
// RCAS's con.x: 2^-stops, 1 the most.
#include "scan_out.hlsli"

// The most a lobe can take away: below it the sharpening rings.
#define FSR_RCAS_LIMIT (0.25 - (1.0 / 16.0))

float3 Texel(int2 p)
{
    const int2 last = int2(g_Push.scale);
    return g_Textures[NonUniformResourceIndex(g_Push.textureIndex)].Load(int3(clamp(p, int2(0, 0), last), 0)).rgb;
}

float4 main(float4 position : SV_Position) : SV_Target0
{
    const int2 sp = int2(position.xy) + int2(g_Push.translate);
    // The cross:
    //     b
    //   d e f
    //     h
    const float3 b = Texel(sp + int2(0, -1));
    const float3 d = Texel(sp + int2(-1, 0));
    const float3 e = Texel(sp);
    const float3 f = Texel(sp + int2(1, 0));
    const float3 h = Texel(sp + int2(0, 1));

    // The ring's minimum and maximum, per channel.
    const float3 mn4 = min(min(b, d), min(f, h));
    const float3 mx4 = max(max(b, d), max(f, h));
    // The limits that keep the result in 0-1: how far the lobe can go before the darkest or the brightest of them
    // passes 0 or 1 (with peak = 1 and -4). The small terms keep the reciprocals finite on black and white.
    const float2 peakC = float2(1.0, -1.0 * 4.0);
    const float3 hitMin = min(mn4, e) / max(4.0 * mx4, 1.0 / 65536.0);
    const float3 hitMax = (peakC.x - max(mx4, e)) / min(4.0 * mn4 + peakC.y, -1.0 / 65536.0);
    const float3 lobeRGB = max(-hitMin, hitMax);
    const float sharpness = asfloat(g_Push.samplerIndex);
    const float lobe = max(-FSR_RCAS_LIMIT, min(max(lobeRGB.r, max(lobeRGB.g, lobeRGB.b)), 0.0)) * sharpness;
    const float3 color = (lobe * (b + d + f + h) + e) / (4.0 * lobe + 1.0);
    return float4(ScanOutColor(color), 1.0);
}
