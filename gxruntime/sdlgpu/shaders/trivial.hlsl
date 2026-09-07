struct VSOut
{
	float4 pos : SV_Position;
	float4 color : COLOR0;
};

VSOut VSMain(uint vid : SV_VertexID)
{
	float2 pts[3];
	pts[0] = float2(0.0, 0.5);
	pts[1] = float2(-0.5, -0.5);
	pts[2] = float2(0.5, -0.5);

	float4 cols[3];
	cols[0] = float4(1.0, 0.3, 0.1, 1.0);
	cols[1] = float4(0.1, 1.0, 0.3, 1.0);
	cols[2] = float4(0.2, 0.4, 1.0, 1.0);

	VSOut o;
	o.pos = float4(pts[vid], 0.0, 1.0);
	o.color = cols[vid];
	return o;
}

float4 PSMain(VSOut i) : SV_Target0
{
	return i.color;
}
