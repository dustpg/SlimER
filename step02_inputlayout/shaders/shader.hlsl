struct VSInput
{
    [[vk::location(0)]] float2 pos : POSITION0;
    [[vk::location(1)]] float3 color : COLOR0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    [[vk::location(0)]] float4 color : COLOR0;
};

PSInput VSMain(VSInput input)
{
    PSInput result;
    
    result.position = float4(input.pos, 0.0f, 1.0f);
    result.color = float4(input.color, 1.0f);
    
    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}