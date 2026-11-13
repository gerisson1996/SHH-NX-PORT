#include "scan_out.hlsli"

struct PSInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target0
{
    const float4 color = SampleBindless(g_Push.textureIndex, g_Push.samplerIndex, input.uv);
    return float4(ScanOutColor(color.rgb), 1.0);
}
