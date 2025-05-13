cbuffer CameraCB
{
    float4x4 ViewProj;
};

struct VSOut
{
    float4 Pos : SV_POSITION;
    float3 WorldPos : TEX_COORD;
};

float3 positions[8] = {
    float3(-1, -1, -1), float3(1, -1, -1),
    float3(-1,  1, -1), float3(1,  1, -1),
    float3(-1, -1,  1), float3(1, -1,  1),
    float3(-1,  1,  1), float3(1,  1,  1)
};

uint indices[36] = {
    0,1,2,  1,3,2,
    4,6,5,  5,6,7,
    0,2,4,  4,2,6,
    1,5,3,  5,7,3,
    0,4,1,  1,4,5,
    2,3,6,  6,3,7
};


void main(uint VertexID : SV_VertexID, out VSOut Out)
{
    uint idx = indices[VertexID];
    float3 world = positions[idx];

    Out.Pos = mul(float4(world, 1.0), ViewProj);
    Out.WorldPos = world * 0.5 + 0.5;
}
