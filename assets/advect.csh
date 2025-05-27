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
    // Get dimensions for boundary checking
    uint3 dims;
    uint numLevels;
    VelocityInSampler.GetDimensions(0, dims.x, dims.y, dims.z, numLevels);
    
    // Check if we're within bounds
    if (any(id >= dims))
        return;

    float3 pos = float3(id);
    float4 vel4 = VelocityInSampler.Load(int4(id, 0));
    float3 vel = vel4.xyz;
      // Use higher timestep to allow fluid to move more noticeably
    float effectiveTimestep = timestep * 0.5; // Significantly increased for more obvious movement
    
    // Calculate the previous position with backtracking
    float3 pos_prev = pos - effectiveTimestep * vel;
    
    // Create wrap-around effect for boundaries (toroidal domain)
    float3 grid_size = float3(dims);
    pos_prev = fmod(pos_prev + grid_size, grid_size);
    
    // Convert to texture coordinates [0,1]
    float3 uvw = pos_prev / float3(dims - 1);
    
    // Sample with boundary clamping
    float4 advected = VelocityInSampler.SampleLevel(VelocityInSampler_sampler, uvw, 0);
    
    // Apply almost no dissipation to prevent velocity from disappearing
    float dissipation = 0.999; // Changed to 0.999 for much less dissipation
    advected.xyz *= dissipation;
    
    // Preserve w component
    advected.w = 1.0;
    
    // Reduced boundary restrictions - only dampen at boundaries, don't zero out
    if (id.x <= 1 || id.x >= dims.x - 2)
        advected.x *= 0.95;
        
    if (id.y <= 1 || id.y >= dims.y - 2)
        advected.y *= 0.95;
        
    if (id.z <= 1 || id.z >= dims.z - 2)
        advected.z *= 0.95;
    
    VelocityOut[id] = advected;
}