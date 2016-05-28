//
// Globals
//

matrix WorldViewMatrix;
matrix WorldViewProjMatrix;

vector Color;
vector LightDirection;

//
// Structures
//

struct VS_INPUT
{
	vector position : POSITION;
	vector normal   : NORMAL;
	float2 uv : TEXCOORD;
};

struct VS_OUTPUT
{
	vector position : POSITION;
	float2 uvCoords : TEXCOORD;
	vector diffuse  : COLOR;
};


//
// Main
//

VS_OUTPUT Main(VS_INPUT input)
{
	// ��ʼ��output
	VS_OUTPUT output = (VS_OUTPUT)0;


	// �任����
	output.position = mul(input.position, WorldViewProjMatrix);


	vector tempDir = LightDirection;

	VS_INPUT tempInput;
	tempInput.position = input.position;
	tempInput.normal = input.normal;
	tempInput.normal.w = 0.0f;
	tempInput.normal = mul(tempInput.normal, WorldViewMatrix);

	// ͨ�����㷢�ֺ͹��շ����˵õ� ����ǿ������u
	float u = dot(tempDir, tempInput.normal);


	if (u < 0.0f)
		u = 0.0f;

	// 
	// v����ȡ0.5
	//
	float v = 0.5f;


	output.uvCoords.x = u;
	output.uvCoords.y = v;

	// save color
	output.diffuse = Color;

	return output;
}


