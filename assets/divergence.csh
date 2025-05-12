Texture3D<float3> VelocitySampler;
SamplerState VelocitySampler_sampler;
RWTexture3D<float> Divergence;

[numthreads(8, 8, 8)]
void main(uint3 id : SV_DispatchThreadID)
{
    float3 L = VelocitySampler.Load(int4(id - uint3(1, 0, 0), 0));
    float3 R = VelocitySampler.Load(int4(id + uint3(1, 0, 0), 0));
    float3 D = VelocitySampler.Load(int4(id - uint3(0, 1, 0), 0));
    float3 U = VelocitySampler.Load(int4(id + uint3(0, 1, 0), 0));
    float3 B = VelocitySampler.Load(int4(id - uint3(0, 0, 1), 0));
    float3 T = VelocitySampler.Load(int4(id + uint3(0, 0, 1), 0));

    Divergence[id] = 0.5 * ((R.x - L.x) + (U.y - D.y) + (T.z - B.z));
}
