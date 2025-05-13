// Advección semi-lagrangiana
RWTexture3D<float4>  VelocityOut;
Texture3D<float4>    VelocityInSampler;
SamplerState VelocityInSampler_sampler;

cbuffer Constants
{
    float timestep;
    float3 gridSizeInv;
};

[numthreads(8, 8, 8)]
void main(uint3 id : SV_DispatchThreadID)
{
    float3 pos = float3(id);
    float4 vel = VelocityInSampler.Load(int4(id, 0));
    float3 prevPos = pos - timestep * vel.xyz;
    float3 uvw = (prevPos + 0.5) * gridSizeInv;

    uvw = clamp(uvw, 0.0, 1.0);

    VelocityOut[id] = VelocityInSampler.SampleLevel(VelocityInSampler_sampler, uvw, 0);
}