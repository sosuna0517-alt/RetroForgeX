// Fullscreen triangle generated from SV_VertexID (draw 3 vertices, no buffers).
// The viewport decides where the triangle lands (letterboxed game area or full screen).
#include "Common.hlsli"

VSOutput main(uint id : SV_VertexID)
{
    VSOutput o;
    float2 uv = float2((id << 1) & 2, id & 2);
    o.uv = uv;
    o.position = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return o;
}
