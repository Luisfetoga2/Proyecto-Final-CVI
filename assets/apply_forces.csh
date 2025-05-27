RWTexture3D<float3> Velocity;

cbuffer Constants
{
    float timestep;
    float3 forces;
};

[numthreads(8, 8, 8)]
void main(uint3 id : SV_DispatchThreadID)
{
    // Get dimensions for boundary checking
    uint3 dims;
    uint numLevels;
    Velocity.GetDimensions(dims.x, dims.y, dims.z);
    
    // Skip boundary cells
    if (id.x == 0 || id.x >= dims.x - 1 ||
        id.y == 0 || id.y >= dims.y - 1 ||
        id.z == 0 || id.z >= dims.z - 1)
        return;
        
    Velocity[id] += timestep * forces;
}
