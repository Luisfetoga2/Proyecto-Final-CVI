Texture3D<float> PressureIn;
Texture3D<float> Divergence;
RWTexture3D<float> PressureOut;

[numthreads(8, 8, 8)]
void main(uint3 id : SV_DispatchThreadID)
{
    // Get dimensions for boundary checking
    uint3 dims;
    uint numLevels;
    PressureIn.GetDimensions(0, dims.x, dims.y, dims.z, numLevels);
    
    // Use periodic boundary conditions (wrap-around)
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
    
    // For interior cells, perform regular Jacobi iteration with wrapped boundaries
    float pL = PressureIn[idL];
    float pR = PressureIn[idR];
    float pD = PressureIn[idD];
    float pU = PressureIn[idU];
    float pB = PressureIn[idB];
    float pT = PressureIn[idT];
    float div = Divergence[id];
    
    // Reduced weight on divergence to allow more flow
    float alpha = 0.8;
    float beta = 1.0 / 6.0;
    
    // Modified Jacobi iteration with reduced divergence influence
    PressureOut[id] = (pL + pR + pD + pU + pB + pT - alpha * div) * beta;
}