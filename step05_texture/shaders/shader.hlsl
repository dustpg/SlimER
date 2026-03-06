struct VSInput
{
    [[vk::location(0)]] float2 pos : POSITION0;
    [[vk::location(1)]] float2 tex : TEXCOORD0;
    [[vk::location(2)]] float3 color : COLOR0;
    
};

struct PSInput
{
    float4 position : SV_POSITION;
    [[vk::location(0)]] float4 color : COLOR0;
    [[vk::location(1)]] float2 tex : TEXCOORD0;
};


[[vk::binding(1)]]
cbuffer BaseMatrix : register(b1)
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

[[vk::binding(0, 1)]]
Texture2D texture2D  : register(t0, space1);

[[vk::binding(1, 1)]]
SamplerState currentSampler : register(s1, space1);
[[vk::binding(3, 1)]]
SamplerState currentSampler2 : register(s3, space1);

PSInput VSMain(VSInput input)
{
    PSInput result;
    
    float4 localPosition = float4(input.pos, 0.0f, 1.0f);
    float4 worldPosition = mul(model, localPosition);
    float4 viewPosition = mul(view, worldPosition);
    result.position = mul(proj, viewPosition);
    result.color = float4(input.color, 1.0f);
    result.tex = input.tex;
    
    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float4 sampled = texture2D.Sample(currentSampler2, input.tex);
    return sampled;
    //return sampled * input.color;
}
