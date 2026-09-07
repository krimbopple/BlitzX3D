cbuffer VSParams : register(b0, space1)
{
	float4x4 viewProj;
};

struct VSIn
{
	float3 pos : POSITION;
	float3 normal : NORMAL;
	float4 color : COLOR;
	float2 uv : TEXCOORD0;
};

struct VSOut
{
	float4 pos : SV_Position;
	float4 color : COLOR0;
	float2 uv : TEXCOORD0;
};

VSOut VSMain(VSIn i)
{
	VSOut o;
	o.pos = mul(viewProj, float4(i.pos, 1.0));
	o.color = i.color.bgra;
	o.uv = i.uv;
	return o;
}

Texture2D MeshTex : register(t0, space2);
SamplerState MeshSamp : register(s0, space2);

float4 PSMain(VSOut i) : SV_Target0
{
	return MeshTex.Sample(MeshSamp, i.uv) * i.color;
}
