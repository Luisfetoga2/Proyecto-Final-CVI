Texture3D<float> Pressure;
RWTexture3D<float4> Velocity;

[numthreads(8, 8, 8)]
void main(int3 id : SV_DispatchThreadID)
{
    // Make sure we don't access out-of-bounds indices
    int3 dim;
    Pressure.GetDimensions(dim.x, dim.y, dim.z);

    if (all(id > int3(0,0,0)) && all(id < dim - int3(1,1,1)))
    {
        float pL = Pressure[id - int3(1,0,0)];
        float pR = Pressure[id + int3(1,0,0)];
        float pD = Pressure[id - int3(0,1,0)];
        float pU = Pressure[id + int3(0,1,0)];
        float pB = Pressure[id - int3(0,0,1)];
        float pT = Pressure[id + int3(0,0,1)];
        float4 v = Velocity[id];
        v.x -= 0.5 * (pR - pL);
        v.y -= 0.5 * (pU - pD);
        v.z -= 0.5 * (pT - pB);
        Velocity[id] = v;
    }
}