// Advección semi-lagrangiana
RWTexture3D<float4> VelocityOut;
Texture3D<float4> VelocityInSampler;
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
    float4 vel4 = VelocityInSampler.Load(int4(id, 0));
    float3 vel = vel4.xyz;
    float3 pos_prev = pos - timestep * vel;
    float3 uvw = pos_prev * gridSizeInv;

    float4 advected = VelocityInSampler.SampleLevel(VelocityInSampler_sampler, uvw, 0);
    advected.w = 1.0; // or preserve vel4.w if needed
    VelocityOut[id] = advected;
}