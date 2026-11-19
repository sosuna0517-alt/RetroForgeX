// "Analog Channel Surf" transition, driven by Progress (0 -> 1).
//
//   0.00 - 0.30  Vertical CRT collapse: the OLD picture squeezes into a bright
//                horizontal line (chromatic aberration ramps up).
//   0.30 - 0.36  The line pinches into a dot and fades.
//   0.36 - 0.64  TV static with rolling bars (the frontend swaps cores/states here;
//                it holds Progress at 0.5 while content is still loading).
//   0.64 - 1.00  The NEW picture expands back vertically from a line, with
//                chromatic aberration and residual static decaying to zero.
//
// The frontend binds the frozen snapshot while Progress < 0.5 and the live
// frame afterwards. With Active = 0 this is a straight passthrough.
#include "Common.hlsli"

Texture2D    Source        : register(t0);
SamplerState LinearSampler : register(s0);

cbuffer SurfParams : register(b0)
{
    float  Progress;
    float  Time;
    float2 OutputSize;
    float  Active;
    float3 _pad0;
};

float3 SampleCA(float2 uv, float amount)
{
    float2 dir = (uv - 0.5) * amount;
    float r = Source.SampleLevel(LinearSampler, uv + dir, 0).r;
    float g = Source.SampleLevel(LinearSampler, uv, 0).g;
    float b = Source.SampleLevel(LinearSampler, uv - dir, 0).b;
    return float3(r, g, b);
}

float3 TvStatic(float2 pos, float t)
{
    // Grain at roughly 2x2 output pixels at 4K, re-rolled every frame.
    float2 cell = floor(pos / 2.0);
    float n = Hash12(cell + frac(t * 7.13) * 917.0);
    // Rolling horizontal bars.
    float bars = 0.75 + 0.25 * sin((pos.y / OutputSize.y) * 18.0 - t * 22.0);
    // Faint horizontal tearing.
    float tear = Hash12(float2(floor(pos.y / 6.0), floor(t * 30.0))) * 0.15;
    return saturate(n * bars + tear).xxx;
}

float4 main(VSOutput input) : SV_Target
{
    float2 uv = input.uv;
    if (Active < 0.5)
    {
        return float4(Source.SampleLevel(LinearSampler, uv, 0).rgb, 1.0);
    }

    float p = saturate(Progress);
    float3 color = 0.0;

    if (p < 0.36)
    {
        // Collapse old picture.
        float sy = max(1.0 - smoothstep(0.0, 0.30, p), 0.004);   // vertical scale
        float sx = max(1.0 - smoothstep(0.30, 0.36, p), 0.002);  // horizontal pinch
        float2 d = uv - 0.5;
        float2 src = float2(d.x / sx, d.y / sy) + 0.5;

        if (all(src >= 0.0) && all(src <= 1.0))
        {
            float ca = (1.0 - sy) * 0.02;
            color = SampleCA(src, ca);
            // Energy concentrates as the beam collapses.
            color *= 1.0 + (1.0 - sy) * 3.0;
        }

        // Glowing scan line / dot at the center.
        float lineGlow = exp(-pow(d.y * OutputSize.y / (2.0 + 40.0 * sy), 2.0));
        float dotFade = 1.0 - smoothstep(0.30, 0.36, p) * smoothstep(0.0, 0.5, abs(d.x) / max(sx, 0.002));
        color += lineGlow * smoothstep(0.15, 0.30, p) * dotFade * float3(0.9, 1.0, 0.95);
    }
    else if (p < 0.64)
    {
        // Pure static between channels.
        float fadeIn  = smoothstep(0.36, 0.42, p);
        float fadeOut = 1.0 - smoothstep(0.58, 0.64, p);
        color = TvStatic(input.position.xy, Time) * fadeIn * fadeOut * 0.85;
    }
    else
    {
        // Expand new picture.
        float e = smoothstep(0.64, 1.0, p);
        float sy = max(e, 0.004);
        float2 d = uv - 0.5;
        float2 src = float2(d.x, d.y / sy) + 0.5;

        if (src.y >= 0.0 && src.y <= 1.0)
        {
            float ca = (1.0 - e) * 0.03;
            color = SampleCA(src, ca);
            color *= 1.0 + (1.0 - e) * 1.5;
        }

        // Residual static and a soft line glow while the beam opens up.
        color = lerp(color, TvStatic(input.position.xy, Time), (1.0 - e) * 0.35);
        float lineGlow = exp(-pow(d.y * OutputSize.y / (2.0 + 400.0 * e), 2.0));
        color += lineGlow * (1.0 - e) * 0.8;
    }

    return float4(saturate(color), 1.0);
}
