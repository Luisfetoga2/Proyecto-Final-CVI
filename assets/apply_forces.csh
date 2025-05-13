RWTexture3D<float4> Velocity;
cbuffer Constants { float timestep; float3 forces; };


[numthreads(8, 8, 8)]
void main(uint3 id : SV_DispatchThreadID)
{
    float4 v = Velocity[id];
    v.xyz += timestep * forces;
    Velocity[id] = v;
}
