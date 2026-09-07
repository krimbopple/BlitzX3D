struct VSOut
{
	float4 pos : SV_Position;
	float2 uv : TEXCOORD0;
};

VSOut VSMain(uint vid : SV_VertexID)
{
	float2 pos[3];
	pos[0] = float2(-1.0, -1.0);
	pos[1] = float2(3.0, -1.0);
	pos[2] = float2(-1.0, 3.0);

	float2 uv[3];
	uv[0] = float2(0.0, 1.0);
	uv[1] = float2(2.0, 1.0);
	uv[2] = float2(0.0, -1.0);

	VSOut o;
	o.pos = float4(pos[vid], 0.0, 1.0);
	o.uv = uv[vid];
	return o;
}

Texture2D BlitTex : register(t0, space2);
SamplerState BlitSamp : register(s0, space2);

float4 PSMain(VSOut i) : SV_Target0
{
	return BlitTex.Sample(BlitSamp, i.uv);
}
