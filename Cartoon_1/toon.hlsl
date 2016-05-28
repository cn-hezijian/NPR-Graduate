////////////////////////////////////////////////////////////////////////////
// 
// File: toon.txt
// 
// Author: Frank Luna (C) All Rights Reserved
//
// System: AMD Athlon 1800+ XP, 512 DDR, Geforce 3, Windows XP, MSVC++ 7.0 
//
// Desc: Vertex shader that lights geometry such it appears to be
//       drawn in a cartoon style.
//          
////////////////////////////////////////////////////////////////////////////

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
    // zero out each member in output
    VS_OUTPUT output = (VS_OUTPUT)0;


    // transform vertex position to homogenous clip space
     output.position = mul(input.position, WorldViewProjMatrix);

    //
    // Transform lights and normals to view space.  Set w
    // components to zero since we're transforming vectors.
    // Assume there are no scalings in the world
    // matrix as well.
    //
    //LightDirection.w = 0.0f;
    //input.normal.w   = 0.0f;
    //LightDirection   = mul(LightDirection, WorldViewMatrix);
    //input.normal     = mul(input.normal, WorldViewMatrix);

	vector tempDir = LightDirection;
	//tempDir.w = 0.0f;
	//tempDir = mul(tempDir, WorldViewMatrix);
	VS_INPUT tempInput;
	tempInput.position = input.position;
	tempInput.normal = input.normal;
	tempInput.normal.w = 0.0f;
	tempInput.normal = mul(tempInput.normal, WorldViewMatrix);
	
    //
    // Compute the 1D texture coordinate for toon rendering.
    //
    float u = dot(tempDir, tempInput.normal);

    //
    // Clamp to zero if u is negative because u
    // negative implies the angle between the light
    // and normal is greater than 90 degrees.  And
    // if that is true then the surface receives
    // no light.
    //
    if( u < 0.0f )
        u = 0.0f;

    // 
    // Set other tex coord to middle.
    //
    float v = 0.5f;


    output.uvCoords.x = input.uv.x;
    output.uvCoords.y = input.uv.y;

    // save color
    output.diffuse = tempInput.normal;
    
    return output;
}





