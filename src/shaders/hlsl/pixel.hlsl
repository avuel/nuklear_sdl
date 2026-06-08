Texture2D<float4> Texture : register(t0, space2);
SamplerState Sampler : register(s0, space2);

struct output_ps
{
    float4 color : SV_Target;
};

output_ps main_ps(float2 uv : TEXCOORD0, float4 color : TEXCOORD1)
{
    output_ps result;

    result.color = color * Texture.Sample(Sampler, uv.rg);

    return result;
}
