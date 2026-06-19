#include "Lights.hlsli"

#define TILE_SIZE       16
#define MAX_LIGHTS_PER_TILE 64

cbuffer CbCulling : register(b0){
    uint NumPointLights;
    uint NumSpotLights;
    uint ViewportWidth;
    uint ViewportHeight;
    float4x4 Projection;
    float4x4 View;
};

Texture2D<float> DepthBuffer : register(t0);
StructuredBuffer<PointLight> PointLights : register(t1);
StructuredBuffer<SpotLight> SpotLights : register(t2);

RWStructuredBuffer<int> PointLightList : register(u0);
RWStructuredBuffer<int> SpotLightList : register(u1);

groupshared uint gsMinDepthUint;
groupshared uint gsMaxDepthUint;
groupshared uint gsPointCount;
groupshared uint gsSpotCount;

float depthToViewZ(float depth){
    float c = Projection[2][2];
    float d = Projection[3][2];
    return -d / (depth + c);
}

float3 screenToView(float2 screenPos, float viewZ){
    float ndcX = (screenPos.x / float(ViewportWidth)) * 2.0 - 1.0;
    float ndcY = -(screenPos.y / float(ViewportHeight)) * 2.0 + 1.0;
    float viewX = ndcX * (-viewZ) / Projection[0][0];
    float viewY = ndcY * (-viewZ) / Projection[1][1];
    return float3(viewX, viewY, viewZ);
}

float distFromPlane(float3 planeNormal, float3 pos){
    return dot(planeNormal, pos);
}

uint tileIndex(uint2 tile){
    uint numTilesX = (ViewportWidth + TILE_SIZE - 1) / TILE_SIZE;
    return tile.y * numTilesX + tile.x;
}

[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void main(uint2 globalIdx : SV_DispatchThreadID,
          uint localIdx : SV_GroupIndex,
          uint2 groupIdx : SV_GroupID){
    if (localIdx == 0){
        gsMinDepthUint = 0xFFFFFFFF;
        gsMaxDepthUint = 0;
        gsPointCount = 0;
        gsSpotCount = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    uint2 coord = min(globalIdx, uint2(ViewportWidth - 1, ViewportHeight - 1));
    float d = DepthBuffer.Load(int3(coord, 0));

    if (d < 1.0f){
        InterlockedMin(gsMinDepthUint, asuint(d));
        InterlockedMax(gsMaxDepthUint, asuint(d));
    }
    GroupMemoryBarrierWithGroupSync();

    float refDepth = 0.0f;
    float refViewZ = depthToViewZ(refDepth);

    float2 corners[4];
    corners[0] = float2( groupIdx.x * TILE_SIZE, groupIdx.y * TILE_SIZE);
    corners[1] = float2((groupIdx.x + 1) * TILE_SIZE, groupIdx.y * TILE_SIZE);
    corners[2] = float2((groupIdx.x + 1) * TILE_SIZE, (groupIdx.y + 1) * TILE_SIZE);
    corners[3] = float2( groupIdx.x * TILE_SIZE, (groupIdx.y + 1) * TILE_SIZE);

    float3 viewCorners[4];
    for (int ci = 0; ci < 4; ++ci)
        viewCorners[ci] = screenToView(corners[ci], refViewZ);

    float3 planes[4];
    for (int pi = 0; pi < 4; ++pi)
        planes[pi] = normalize(cross(viewCorners[pi], viewCorners[(pi + 1) & 3]));

    float viewMinZ = depthToViewZ(asfloat(gsMaxDepthUint));
    float viewMaxZ = depthToViewZ(asfloat(gsMinDepthUint));

    uint threadIdx = localIdx;
    for (uint i = threadIdx; i < NumPointLights; i += TILE_SIZE * TILE_SIZE){
        PointLight pl = PointLights[i];
        float radius = sqrt(pl.SquaredRadius);
        float3 vPos = mul(float4(pl.Position, 1.0f), View).xyz;

        bool inside =
            distFromPlane(planes[0], vPos) < radius &&
            distFromPlane(planes[1], vPos) < radius &&
            distFromPlane(planes[2], vPos) < radius &&
            distFromPlane(planes[3], vPos) < radius &&
            (viewMinZ - vPos.z) < radius &&
            (vPos.z - viewMaxZ) < radius;

        if (inside){
            uint slot;
            InterlockedAdd(gsPointCount, 1u, slot);
            if (slot < MAX_LIGHTS_PER_TILE)
                PointLightList[tileIndex(groupIdx) * MAX_LIGHTS_PER_TILE + slot] = int(i);
        }
    }

    for (uint j = threadIdx; j < NumSpotLights; j += TILE_SIZE * TILE_SIZE){
        SpotLight sl = SpotLights[j];
        float spotR = sqrt(sl.SquaredRadius);
        float cosOuter = sl.OuterAngle;

        float sphereR;
        float3 sphereC;
        static const float kCosQuarterPi = 0.70710678f;

        if (cosOuter < kCosQuarterPi){
            float sinA = sqrt(1.0f - cosOuter * cosOuter);
            float tanA = sinA / cosOuter;
            sphereR = spotR * tanA;
            sphereC = sl.Position + sl.Direction * spotR;
        }
        else
        {
            sphereR = spotR * 0.5f / (cosOuter * cosOuter);
            sphereC = sl.Position + sl.Direction * sphereR;
        }

        float3 vPos = mul(float4(sphereC, 1.0f), View).xyz;

        bool inside =
            distFromPlane(planes[0], vPos) < sphereR &&
            distFromPlane(planes[1], vPos) < sphereR &&
            distFromPlane(planes[2], vPos) < sphereR &&
            distFromPlane(planes[3], vPos) < sphereR &&
            (viewMinZ - vPos.z) < sphereR &&
            (vPos.z - viewMaxZ) < sphereR;

        if (inside){
            uint slot;
            InterlockedAdd(gsSpotCount, 1u, slot);
            if (slot < MAX_LIGHTS_PER_TILE)
                SpotLightList[tileIndex(groupIdx) * MAX_LIGHTS_PER_TILE + slot] = int(j);
        }
    }

    GroupMemoryBarrierWithGroupSync();

    if (localIdx == 0){
        uint ti = tileIndex(groupIdx);
        if (gsPointCount < MAX_LIGHTS_PER_TILE)
            PointLightList[ti * MAX_LIGHTS_PER_TILE + gsPointCount] = -1;
        if (gsSpotCount < MAX_LIGHTS_PER_TILE)
            SpotLightList[ti * MAX_LIGHTS_PER_TILE + gsSpotCount] = -1;
    }
}
