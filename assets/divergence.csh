Texture3D<float4> VelocitySampler;
SamplerState VelocitySampler_sampler;
RWTexture3D<float> Divergence;

[numthreads(8, 8, 8)]
void main(uint3 id : SV_DispatchThreadID)
{
    float4 L = VelocitySampler.Load(int4(id - uint3(1, 0, 0), 0));
    float4 R = VelocitySampler.Load(int4(id + uint3(1, 0, 0), 0));
    float4 D = VelocitySampler.Load(int4(id - uint3(0, 1, 0), 0));
    float4 U = VelocitySampler.Load(int4(id + uint3(0, 1, 0), 0));
    float4 B = VelocitySampler.Load(int4(id - uint3(0, 0, 1), 0));
    float4 T = VelocitySampler.Load(int4(id + uint3(0, 0, 1), 0));

    Divergence[id] = 0.5 * ((R.x - L.x) + (U.y - D.y) + (T.z - B.z));
}
