Texture3D<float> Pressure;
RWTexture3D<float3> Velocity;

[numthreads(8, 8, 8)]
void main(int3 id : SV_DispatchThreadID)
{
    uint3 dim;
    uint numLevels;
    Pressure.GetDimensions(0, dim.x, dim.y, dim.z, numLevels);
    
    // Check bounds
    if (any(id < 0) || any(id >= int3(dim)))
        return;
        
    float halfrdx = 0.5f; // Reciprocal of cell size

    // Sample pressure with wrap-around for periodic boundary conditions
    float pL = id.x > 0 ? Pressure[id - int3(1,0,0)] : Pressure[int3(dim.x-1, id.y, id.z)];
    float pR = id.x < dim.x - 1 ? Pressure[id + int3(1,0,0)] : Pressure[int3(0, id.y, id.z)];
    float pB = id.y > 0 ? Pressure[id - int3(0,1,0)] : Pressure[int3(id.x, dim.y-1, id.z)];
    float pT = id.y < dim.y - 1 ? Pressure[id + int3(0,1,0)] : Pressure[int3(id.x, 0, id.z)];
    float pD = id.z > 0 ? Pressure[id - int3(0,0,1)] : Pressure[int3(id.x, id.y, dim.z-1)];
    float pF = id.z < dim.z - 1 ? Pressure[id + int3(0,0,1)] : Pressure[int3(id.x, id.y, 0)];
    
    // Calculate pressure gradient
    float3 gradP = halfrdx * float3(
        pR - pL,
        pT - pB,
        pF - pD
    );
    
    // Update velocity by subtracting pressure gradient, with reduced effect
    float3 v = Velocity[id];
    v -= gradP * 0.8; // Reduced pressure effect to allow more flow
    
    // Only apply minimal damping at outermost boundaries
    if (id.x == 0 || id.x == dim.x - 1) v.x *= 0.95;
    if (id.y == 0 || id.y == dim.y - 1) v.y *= 0.95;
    if (id.z == 0 || id.z == dim.z - 1) v.z *= 0.95;
    
    Velocity[id] = v;
}