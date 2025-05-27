Texture3D<float3> VelocitySampler;
SamplerState VelocitySampler_sampler;
RWTexture3D<float> Divergence;

[numthreads(8, 8, 8)]
void main(uint3 id : SV_DispatchThreadID)
{
    // Get dimensions for boundary checking
    uint3 dims;
    uint numLevels;
    VelocitySampler.GetDimensions(0, dims.x, dims.y, dims.z, numLevels);
    
    // Special handling for boundaries - use wrap-around sampling
    int3 idL = int3(id) - int3(1, 0, 0);
    int3 idR = int3(id) + int3(1, 0, 0);
    int3 idD = int3(id) - int3(0, 1, 0);
    int3 idU = int3(id) + int3(0, 1, 0);
    int3 idB = int3(id) - int3(0, 0, 1);
    int3 idT = int3(id) + int3(0, 0, 1);
    
    // Wrap coordinates for periodic boundary
    if (idL.x < 0) idL.x += dims.x;
    if (idR.x >= dims.x) idR.x -= dims.x;
    if (idD.y < 0) idD.y += dims.y;
    if (idU.y >= dims.y) idU.y -= dims.y;
    if (idB.z < 0) idB.z += dims.z;
    if (idT.z >= dims.z) idT.z -= dims.z;
    
    // Sample velocities using wrapped indices
    float3 L = VelocitySampler.Load(int4(idL, 0));
    float3 R = VelocitySampler.Load(int4(idR, 0));
    float3 D = VelocitySampler.Load(int4(idD, 0));
    float3 U = VelocitySampler.Load(int4(idU, 0));
    float3 B = VelocitySampler.Load(int4(idB, 0));
    float3 T = VelocitySampler.Load(int4(idT, 0));

    // Calculate divergence using central differences
    float div = 0.5 * ((R.x - L.x) + (U.y - D.y) + (T.z - B.z));
    
    // Reduce divergence effect to allow more flow
    div *= 0.9;
    
    Divergence[id] = div;
}
