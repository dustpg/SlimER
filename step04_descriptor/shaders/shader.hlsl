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


[[vk::binding(1)]]
cbuffer BaseMatrix : register(b1)
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

PSInput VSMain(VSInput input)
{
    PSInput result;
    
    float4 localPosition = float4(input.pos, 0.0f, 1.0f);
    float4 worldPosition = mul(model, localPosition);
    float4 viewPosition = mul(view, worldPosition);
    result.position = mul(proj, viewPosition);
    result.color = float4(input.color, 1.0f);
    
    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}
