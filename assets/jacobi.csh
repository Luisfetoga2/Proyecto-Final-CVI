Texture3D<float> PressureIn;
Texture3D<float> Divergence;
RWTexture3D<float> PressureOut;

[numthreads(8, 8, 8)]
void main(uint3 id : SV_DispatchThreadID)
{
    float pL = PressureIn[id - uint3(1,0,0)];
    float pR = PressureIn[id + uint3(1,0,0)];
    float pD = PressureIn[id - uint3(0,1,0)];
    float pU = PressureIn[id + uint3(0,1,0)];
    float pB = PressureIn[id - uint3(0,0,1)];
    float pT = PressureIn[id + uint3(0,0,1)];
    float div = Divergence[id];
    PressureOut[id] = (pL + pR + pD + pU + pB + pT - div) / 6.0;
}