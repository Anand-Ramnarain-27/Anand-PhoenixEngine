
#include "Decal.hlsli"

struct VS_OUTPUT {
    float4 svPos : SV_POSITION;
};

// The pixel shader finds its screen UV from SV_POSITION: interpolating clip.xyz / clip.w from the vertices is
// wrong across a face in perspective and breaks once a box corner is behind the camera.
VS_OUTPUT main(float3 position : POSITION){
    VS_OUTPUT o;
    o.svPos = mul(float4(position, 1.0f), MVP);
    return o;
}
