#define MAX_GROUPS 4

Texture2D<float2> XRayMask : register(t0);   // R = hidden group / 255, G = visible

cbuffer XRayCompositeCB : register(b0){
    float4 GroupColor[MAX_GROUPS];    // rgb (exposure-compensated)
    float4 GroupParams[MAX_GROUPS];   // x fillAlpha, y outlineWidth (px)
    uint GroupCount;
    uint3 Pad;
};

static const int2 kDirs[8] = {
    int2( 1, 0), int2(-1, 0), int2(0,  1), int2( 0, -1),
    int2( 1, 1), int2(-1, 1), int2(1, -1), int2(-1, -1)
};
static const int kMaxOutlineSteps = 4;

bool IsOffCharacter(int2 p, int2 size){
    if (any(p < 0) || any(p >= size))
        return true;
    float2 m = XRayMask.Load(int3(p, 0));
    return m.x <= 0.0f && m.y <= 0.0f;
}

float4 main(float2 uv : TEXCOORD, float4 position : SV_Position) : SV_Target {
    int2 p = int2(position.xy);
    float2 m = XRayMask.Load(int3(p, 0));

    // Only pixels where a tagged mesh is hidden and nothing tagged is visible.
    if (m.x <= 0.0f || m.y > 0.0f)
        discard;

    uint group = (uint)round(m.x * 255.0f);
    if (group == 0 || group > GroupCount)
        discard;

    float3 color = GroupColor[group - 1].rgb;
    float fillAlpha = GroupParams[group - 1].x;
    float outlineWidth = GroupParams[group - 1].y;

    // Outline: distance (in px, along 8 directions) to the nearest pixel with no tagged surface at all.
    // Pixels bordering the visible part of the character are not "off character", so there is no halo
    // where the silhouette meets the character's visible body.
    float outline = 0.0f;
    if (outlineWidth > 0.0f){
        uint w, h;
        XRayMask.GetDimensions(w, h);
        int2 size = int2(w, h);
        int steps = min((int)ceil(outlineWidth), kMaxOutlineSteps);
        float nearest = 1e6f;
        [unroll] for (int d = 0; d < 8; ++d){
            float stepLen = (d < 4) ? 1.0f : 1.41421356f;
            for (int s = 1; s <= steps; ++s){
                if (IsOffCharacter(p + kDirs[d] * s, size)){
                    nearest = min(nearest, s * stepLen);
                    break;
                }
            }
        }
        outline = saturate(outlineWidth - nearest + 1.0f);
    }

    float3 outColor = lerp(color, color * 1.75f, outline);
    float alpha = lerp(fillAlpha, max(fillAlpha, 0.9f), outline);
    return float4(outColor, alpha);
}
