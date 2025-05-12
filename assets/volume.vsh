struct VSOut
{
    float4 Pos : SV_POSITION;
    float2 UV : TEX_COORD;
};

void main(in uint VertexID : SV_VertexID, out VSOut Out)
{
    float2 pos[6] =
    {
        float2(-1, -1), float2(-1, 1), float2(1, 1),
        float2(-1, -1), float2(1, 1), float2(1, -1)
    };

    Out.Pos = float4(pos[VertexID], 0.0, 1.0);
    Out.UV = Out.Pos.xy * 0.5 + 0.5; // [0,1] UV
}
