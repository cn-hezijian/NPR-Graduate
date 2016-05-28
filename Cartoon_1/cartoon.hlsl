//
// Globals

sampler MySampler;
matrix WorldViewMatrix;
vector LightDirection;




struct PS_INPUT
{
	vector normal : COLOR;
	float2 uv : TEXCOORD;
};

struct PS_OUTPUT
{
	vector diffuse : COLOR0;
};

PS_OUTPUT Main(PS_INPUT input)
{

	PS_OUTPUT output = (PS_OUTPUT)0;

	vector textureColor = tex2D(MySampler, input.uv);

	PS_INPUT tempInput;
	tempInput.normal = input.normal;
	tempInput.normal.w = 0.0f;
	tempInput.normal = mul(tempInput.normal, WorldViewMatrix);

	float angle = dot(tempInput.normal, LightDirection);
	float angleEnhance = 0.0f;
	
	if (angle < 0.1f)
	{
		angleEnhance = 0.5f;
	}
	else if (angle < 0.7f)
	{
		angleEnhance = 0.8f;
	}
	else if (angle < 1.0f)
	{
		angleEnhance = 1.0f;
	}


	vector color = angleEnhance * textureColor;

	output.diffuse = color;

	return output;
}