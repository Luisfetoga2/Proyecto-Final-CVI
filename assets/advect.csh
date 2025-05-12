// Advección semi-lagrangiana
RWTexture3D<float3> VelocityOut;
Texture3D<float3> VelocityInSampler;
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
    float3 vel = VelocityInSampler.Load(int4(id, 0));
    float3 prevPos = pos - timestep * vel;
    float3 uvw = prevPos * gridSizeInv;

    VelocityOut[id] = VelocityInSampler.SampleLevel(VelocityInSampler_sampler, uvw, 0);
}