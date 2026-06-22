
Texture2D<float2> MinMax : register(t0);
RWStructuredBuffer<float4x4> VPOut : register(u0);

cbuffer LightMatrixCB : register(b0){
    float4x4 InvViewProj;
    float3 LightDir;
    float SunDistance;
};

float4x4 LookAtRH(float3 eye, float3 target, float3 up){
    float3 z = normalize(eye - target);
    float3 x = normalize(cross(up, z));
    float3 y = cross(z, x);
    return float4x4(
        x.x, y.x, z.x, 0,
        x.y, y.y, z.y, 0,
        x.z, y.z, z.z, 0,
        -dot(x, eye), -dot(y, eye), -dot(z, eye), 1);
}

float4x4 OrthoRH(float w, float h, float zn, float zf){
    return float4x4(
        2.0f / w, 0, 0, 0,
        0, 2.0f / h, 0, 0,
        0, 0, 1.0f / (zn - zf), 0,
        0, 0, zn / (zn - zf), 1);
}

[numthreads(1, 1, 1)]
void main(){
    float2 mm = MinMax.Load(int3(0, 0, 0));
    mm.y = max(mm.y, mm.x + 0.0001f);

    float3 corners[8];
    int i = 0;
    [unroll] for (int zi = 0; zi < 2; ++zi){
        float z = (zi == 0) ? mm.x : mm.y;
        [unroll] for (int yi = -1; yi <= 1; yi += 2)
            [unroll] for (int xi = -1; xi <= 1; xi += 2){
                float4 c = mul(float4(xi, yi, z, 1.0f), InvViewProj);
                corners[i++] = c.xyz / c.w;
            }
    }

    float3 center = 0.0f;
    [unroll] for (int a = 0; a < 8; ++a) center += corners[a];
    center /= 8.0f;
    float radius = 0.0f;
    [unroll] for (int b = 0; b < 8; ++b) radius = max(radius, length(corners[b] - center));

    float3 dir = normalize(LightDir);
    float3 up = (abs(dir.y) > 0.99f) ? float3(0, 0, 1) : float3(0, 1, 0);
    float3 eye = center - dir * (radius + SunDistance);

    float4x4 view = LookAtRH(eye, center, up);
    float4x4 proj = OrthoRH(radius * 2.0f, radius * 2.0f, 0.0f, radius * 2.0f + SunDistance);
    VPOut[0] = mul(view, proj);
}
