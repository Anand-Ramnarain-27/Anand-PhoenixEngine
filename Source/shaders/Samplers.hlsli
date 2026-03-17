#ifndef _SAMPLERS_HLSLI_
#define _SAMPLERS_HLSLI_

SamplerState BilinearWrap  : register(s0);
SamplerState PointWrap     : register(s1);
SamplerState BilinearClamp : register(s2);
SamplerState PointClamp    : register(s3);

#endif // _SAMPLERS_HLSLI_
