// Shared HLSL definitions (Shader Model 5.0, D3D11).
#ifndef RFX_COMMON_HLSLI
#define RFX_COMMON_HLSLI

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

float Hash12(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float Luma(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

#endif
