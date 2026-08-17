#ifndef _POSTPROCESSEFFECT_HLSLI_
#define _POSTPROCESSEFFECT_HLSLI_

cbuffer PostProcessParams : register(b0){
    float4 Params0;
    float4 Params1;
};

Texture2D InputTexture : register(t0);
Texture2D AuxTexture : register(t1);

#include "Samplers.hlsli"

#endif
