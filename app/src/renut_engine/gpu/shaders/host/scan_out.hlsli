// shh-nx (2026-10-31): the scan-out's colour steps, shared by present_ps and fsr_rcas_ps (the last pass of FSR 1).
// param1's bits, when not 0, = the bindless slot of the game's display gamma ramp; param0 = the output gamma exponent.
#include "utility.hlsli"

float3 ScanOutColor(float3 color)
{
    // sa2-nx (2026-10-20): param1's bits, when not 0, = the bindless slot of the game's display gamma ramp
    // (SetGammaRamp; the Xbox 360's display controller applies it at scan-out): 256 texels of red, green, blue in 0-1.
    // The same lookup as the emulated GPU's and sa2-nx's native renderer's output pass: each channel's 8-bit value picks
    // its entry. The slot is read as it is, with no arithmetic: an emulator (Ryujinx) follows a bindless handle only
    // back to a constant buffer load, and a computed one read zero (a black picture).
    const uint ramp = asuint(g_Push.param1);
    if (ramp != 0u)
    {
        const uint3 i = uint3(saturate(color) * 255.0 + 0.5);
        color = float3(g_Textures[NonUniformResourceIndex(ramp)].Load(int3(i.x, 0, 0)).x,
                       g_Textures[NonUniformResourceIndex(ramp)].Load(int3(i.y, 0, 0)).y,
                       g_Textures[NonUniformResourceIndex(ramp)].Load(int3(i.z, 0, 0)).z);
    }
    // param0 = output gamma exponent (1 = passthrough). sa2-nx: skipped at 1, where pow's exp2(log2) could move a
    // value by an ulp.
    if (g_Push.param0 != 1.0)
        color = pow(saturate(color), g_Push.param0);
    // shh-nx (2026-11-11): param2 = the OLED black threshold (0 = none). The game's near-blacks, lifted further by the
    // brightness exponent above, glow on a screen whose black is off.
    // shh-nx (2026-11-12, the user: too dark even on light): the first curve subtracted the floor from every value and
    // stretched the rest, which took all the shadows down (some 4 of 255 at light, on the PC). Now only what is under
    // the threshold moves, eased to black (x * smoothstep(0, t, x): 0 at 0, half at t/2, itself from t up); the
    // picture above the threshold is exactly the game's.
    if (g_Push.param2 > 0.0)
        color *= smoothstep(0.0, g_Push.param2, color);
    return color;
}
