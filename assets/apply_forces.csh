RWTexture3D<float3> Velocity;
cbuffer Constants { float timestep; float3 forces; };

[numthreads(8, 8, 8)]
void main(uint3 id : SV_DispatchThreadID)
{
    Velocity[id] += timestep * forces;
}
