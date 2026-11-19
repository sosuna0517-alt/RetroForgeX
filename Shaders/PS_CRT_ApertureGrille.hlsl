// 4K CRT / aperture-grille filter.
//
// - Gaussian scanline beam whose width grows with brightness (bright lines bloom
//   into the dark gaps like a real tube).
// - Trinitron-style vertical RGB phosphor stripes, sized in OUTPUT pixels so the
//   triad stays crisp at 3840x2160 (MaskPitch = 3 -> 1px per phosphor).
// - Light halation, optional barrel curvature, corner vignette, gamma in/out.
#include "Common.hlsli"

Texture2D    Source        : register(t0);
SamplerState LinearSampler : register(s0);

cbuffer CrtParams : register(b0)
{
    float2 SourceSize;       // core frame size in texels (w, h)
    float2 SourceUvScale;    // frame size / texture size (frame may not fill the texture)
    float2 OutputSize;       // viewport size in pixels
    float2 ViewportOrigin;   // viewport top-left in render-target pixels
    float  ScanlineStrength; // 0..1
    float  MaskStrength;     // 0..1
    float  Bloom;            // 0..1
    float  Curvature;        // 0..0.25
    float  GammaIn;          // ~2.4
    float  GammaOut;         // ~2.2
    float  MaskPitch;        // output pixels per RGB triad (3 = 1px stripes)
    float  Enabled;          // 0 = plain bilinear blit
};

float2 Warp(float2 uv)
{
    float2 c = uv * 2.0 - 1.0;
    c *= 1.0 + Curvature * float2(c.y * c.y, c.x * c.x);
    return c * 0.5 + 0.5;
}

float3 SampleLinear(float2 uv)
{
    float3 c = Source.SampleLevel(LinearSampler, uv * SourceUvScale, 0).rgb;
    return pow(max(c, 0.0), GammaIn);
}

// Beam profile: distance d in scanline units (0 = line center).
float3 Beam(float d, float3 color)
{
    float3 width = lerp(0.30, 0.55, saturate(color));
    float3 w = d / width;
    return exp(-w * w);
}

float4 main(VSOutput input) : SV_Target
{
    if (Enabled < 0.5)
    {
        return float4(Source.SampleLevel(LinearSampler, input.uv * SourceUvScale, 0).rgb, 1.0);
    }

    float2 uv = Warp(input.uv);
    if (any(uv < 0.0) || any(uv > 1.0))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    // Horizontal: slightly sharpened bilinear. Vertical: two nearest scanlines.
    float2 texel = uv * SourceSize;
    float  y     = texel.y - 0.5;
    float  yBase = floor(y);
    float  yFrac = y - yBase;

    float  sharpX = (floor(texel.x - 0.5) + 0.5 + smoothstep(0.2, 0.8, frac(texel.x - 0.5))) / SourceSize.x;
    float2 uvA = float2(sharpX, (yBase + 0.5) / SourceSize.y);
    float2 uvB = float2(sharpX, (yBase + 1.5) / SourceSize.y);

    float3 a = SampleLinear(uvA);
    float3 b = SampleLinear(uvB);

    float3 scan = a * Beam(yFrac, a) + b * Beam(1.0 - yFrac, b);
    float3 flat = SampleLinear(float2(sharpX, uv.y));
    float3 color = lerp(flat, scan * 1.35, ScanlineStrength);

    // Halation: cheap 4-tap neighborhood glow.
    float2 px = 1.5 / SourceSize;
    float3 glow = SampleLinear(uv + float2(px.x, 0)) + SampleLinear(uv - float2(px.x, 0))
                + SampleLinear(uv + float2(0, px.y)) + SampleLinear(uv - float2(0, px.y));
    color += glow * 0.25 * Bloom * 0.35;

    // Aperture grille: vertical RGB stripes in output-pixel space.
    float pitch  = max(MaskPitch, 3.0);
    float stripe = frac((input.position.x - ViewportOrigin.x) / pitch) * 3.0;
    float3 mask  = float3(stripe < 1.0, stripe >= 1.0 && stripe < 2.0, stripe >= 2.0);
    mask = lerp(1.0.xxx, mask * 2.2 + 0.25, MaskStrength);
    color *= mask;

    // Brightness compensation for the mask's energy loss.
    color *= lerp(1.0, 1.35, MaskStrength);

    // Corner vignette (follows the curvature).
    float2 v = uv * (1.0 - uv.yx);
    color *= saturate(pow(max(v.x * v.y * 15.0, 0.0), 0.25 + Curvature));

    color = pow(saturate(color), 1.0 / GammaOut);
    return float4(color, 1.0);
}
