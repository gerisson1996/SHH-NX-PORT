// shh-nx (2026-10-31, shh_fsr): AMD FidelityFX Super Resolution 1.0, its upscaling pass (EASU: edge adaptive spatial
// upsampling), from the game's frame at the render resolution to the output's size. After AMD's ffx_fsr1.h (MIT,
// Copyright (c) 2021 Advanced Micro Devices, Inc.; THIRD_PARTY_NOTICES.md), its 32-bit path: the same 12-tap kernel,
// with the approximate reciprocals as exact ones, and the 12 taps read with Load (one fetch each) instead of 12
// gathers (four per channel).
//
// present_vs's uv is the source texture's coordinate of this pixel's centre (scale / translate as the plain scan-out's);
// param0 / param1 = 1 / the source texture's width / height. The output is the frame as the game made it (gamma
// space), before the display ramp: fsr_rcas_ps applies that.
#include "utility.hlsli"

struct PSInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float3 Tap(int2 p, int2 hi)
{
    return g_Textures[NonUniformResourceIndex(g_Push.textureIndex)].Load(int3(clamp(p, int2(0, 0), hi), 0)).rgb;
}

// Luma times 2, in two multiply-adds.
float Luma(float3 c)
{
    return c.b * 0.5 + (c.r * 0.5 + c.g);
}

// One of the four bilinear corners' contribution to the direction and the edge length.
void SetF(inout float2 dir, inout float len, float w, float lA, float lB, float lC, float lD, float lE)
{
    // Horizontal: B C D.
    const float dc = lD - lC;
    const float cb = lC - lB;
    float lenX = max(abs(dc), abs(cb));
    lenX = lenX > 0.0 ? 1.0 / lenX : 0.0;
    const float dirX = lD - lB;
    dir.x += dirX * w;
    lenX = saturate(abs(dirX) * lenX);
    lenX *= lenX;
    len += lenX * w;
    // Vertical: A C E.
    const float ec = lE - lC;
    const float ca = lC - lA;
    float lenY = max(abs(ec), abs(ca));
    lenY = lenY > 0.0 ? 1.0 / lenY : 0.0;
    const float dirY = lE - lA;
    dir.y += dirY * w;
    lenY = saturate(abs(dirY) * lenY);
    lenY *= lenY;
    len += lenY * w;
}

// One tap of the kernel: a Lanczos-2 approximation, rotated to the edge and stretched along it.
void TapF(inout float3 aC, inout float aW, float2 off, float2 dir, float2 len, float lob, float clp, float3 c)
{
    float2 v;
    v.x = off.x * dir.x + off.y * dir.y;
    v.y = off.x * -dir.y + off.y * dir.x;
    v *= len;
    float d2 = v.x * v.x + v.y * v.y;
    d2 = min(d2, clp);
    // (25/16 * (2/5 * x^2 - 1)^2 - (25/16 - 1)) * (lob * x^2 - 1)^2
    float wB = (2.0 / 5.0) * d2 - 1.0;
    float wA = lob * d2 - 1.0;
    wB *= wB;
    wA *= wA;
    wB = (25.0 / 16.0) * wB - (25.0 / 16.0 - 1.0);
    const float w = wB * wA;
    aC += c * w;
    aW += w;
}

float4 main(PSInput input) : SV_Target0
{
    const float2 size = float2(1.0 / g_Push.param0, 1.0 / g_Push.param1);
    const int2 hi = int2(size) - 1;
    // This pixel's centre in source texels, less half a texel: f below is the texel at its top left.
    float2 pp = input.uv * size - 0.5;
    const float2 fp = floor(pp);
    pp -= fp;
    const int2 f0 = int2(fp);
    // The 12 taps:
    //     b c
    //   e f g h
    //   i j k l
    //     n o
    const float3 b = Tap(f0 + int2(0, -1), hi);
    const float3 c = Tap(f0 + int2(1, -1), hi);
    const float3 e = Tap(f0 + int2(-1, 0), hi);
    const float3 f = Tap(f0 + int2(0, 0), hi);
    const float3 g = Tap(f0 + int2(1, 0), hi);
    const float3 h = Tap(f0 + int2(2, 0), hi);
    const float3 i = Tap(f0 + int2(-1, 1), hi);
    const float3 j = Tap(f0 + int2(0, 1), hi);
    const float3 k = Tap(f0 + int2(1, 1), hi);
    const float3 l = Tap(f0 + int2(2, 1), hi);
    const float3 n = Tap(f0 + int2(0, 2), hi);
    const float3 o = Tap(f0 + int2(1, 2), hi);
    const float bL = Luma(b), cL = Luma(c), eL = Luma(e), fL = Luma(f), gL = Luma(g), hL = Luma(h);
    const float iL = Luma(i), jL = Luma(j), kL = Luma(k), lL = Luma(l), nL = Luma(n), oL = Luma(o);

    // The direction and the edge length, bilinearly from the four nearest texels' own.
    float2 dir = float2(0.0, 0.0);
    float len = 0.0;
    SetF(dir, len, (1.0 - pp.x) * (1.0 - pp.y), bL, eL, fL, gL, jL);
    SetF(dir, len, pp.x * (1.0 - pp.y), cL, fL, gL, hL, kL);
    SetF(dir, len, (1.0 - pp.x) * pp.y, fL, iL, jL, kL, nL);
    SetF(dir, len, pp.x * pp.y, gL, jL, kL, lL, oL);

    // Normalized, with no direction (flat) as horizontal.
    const float2 dir2 = dir * dir;
    float dirR = dir2.x + dir2.y;
    const bool zro = dirR < 1.0 / 32768.0;
    dirR = zro ? 1.0 : rsqrt(dirR);
    dir.x = zro ? 1.0 : dir.x;
    dir *= dirR;
    // 0-2 to 0-1, shaped with a square.
    len = len * 0.5;
    len *= len;
    // The kernel's stretch: 1 on horizontal and vertical edges, sqrt(2) on diagonals.
    const float stretch = (dir.x * dir.x + dir.y * dir.y) / max(abs(dir.x), abs(dir.y));
    // The anisotropic length after the rotation: x from 1 to 'stretch' along edges, y from 1 to 0.5 across them.
    const float2 len2 = float2(1.0 + (stretch - 1.0) * len, 1.0 - 0.5 * len);
    // The window: +/- sqrt(2) on flat areas to slightly beyond 2 on edges.
    const float lob = 0.5 + ((1.0 / 4.0 - 0.04) - 0.5) * len;
    const float clp = 1.0 / lob;

    float3 aC = float3(0.0, 0.0, 0.0);
    float aW = 0.0;
    TapF(aC, aW, float2(0.0, -1.0) - pp, dir, len2, lob, clp, b);
    TapF(aC, aW, float2(1.0, -1.0) - pp, dir, len2, lob, clp, c);
    TapF(aC, aW, float2(-1.0, 1.0) - pp, dir, len2, lob, clp, i);
    TapF(aC, aW, float2(0.0, 1.0) - pp, dir, len2, lob, clp, j);
    TapF(aC, aW, float2(0.0, 0.0) - pp, dir, len2, lob, clp, f);
    TapF(aC, aW, float2(-1.0, 0.0) - pp, dir, len2, lob, clp, e);
    TapF(aC, aW, float2(1.0, 1.0) - pp, dir, len2, lob, clp, k);
    TapF(aC, aW, float2(2.0, 1.0) - pp, dir, len2, lob, clp, l);
    TapF(aC, aW, float2(2.0, 0.0) - pp, dir, len2, lob, clp, h);
    TapF(aC, aW, float2(1.0, 0.0) - pp, dir, len2, lob, clp, g);
    TapF(aC, aW, float2(1.0, 2.0) - pp, dir, len2, lob, clp, o);
    TapF(aC, aW, float2(0.0, 2.0) - pp, dir, len2, lob, clp, n);

    // Normalized and deringed: within the four nearest texels' range.
    const float3 min4 = min(min(f, g), min(j, k));
    const float3 max4 = max(max(f, g), max(j, k));
    return float4(min(max4, max(min4, aC / aW)), 1.0);
}
