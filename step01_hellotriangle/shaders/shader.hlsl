struct PSInput
{
    float4 position : SV_POSITION;
    [[vk::location(0)]] float4 color : COLOR0;
};

PSInput VSMain(uint vertexID : SV_VertexID)
{
    PSInput result;
    
    float2 positions[3] = 
    {
        float2(0.0f, -0.5f),
        float2(0.5f, 0.5f),
        float2(-0.5f, 0.5f)
    };
    
    float3 colors[3] = 
    {
        float3(1.0f, 0.0f, 0.0f),
        float3(0.0f, 1.0f, 0.0f),
        float3(0.0f, 0.0f, 1.0f)
    };
    
    result.position = float4(positions[vertexID], 0.0f, 1.0f);
    result.color = float4(colors[vertexID], 1.0f);
    
    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}