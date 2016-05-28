//--------------------------------------------------------------------------------------
// File: EmptyProject.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "resource.h"
#include "DXUTcamera.h"
#include "silhouetteEdges.h"
#include <vector>
#include <fstream>
#include "opencv2/opencv.hpp"
using namespace cv;

IDirect3DVertexShader9* ToonShader = 0;
ID3DXConstantTable* ToonConstTable = 0;

ID3DXMesh*  Mesh = 0;
D3DXVECTOR4 MeshColor;

D3DXMATRIX ProjMatrix;

IDirect3DTexture9* ShadeTex = 0;

/* Material */
std::vector<D3DMATERIAL9> g_Mtrls(0);
/* Texture Of Mesh */
std::vector<IDirect3DTexture9*> g_Textures(0);

// Toon
D3DXHANDLE ToonWorldViewHandle = 0;
D3DXHANDLE ToonWorldViewProjHandle = 0;
D3DXHANDLE ToonColorHandle = 0;
D3DXHANDLE ToonLightDirHandle = 0;

// Outline
SilhouetteEdges* MeshOutline = 0;
IDirect3DVertexShader9* OutlineShader = 0;
ID3DXConstantTable* OutlineConstTable = 0;


D3DXHANDLE OutlineWorldViewHandle = 0;
D3DXHANDLE OutlineProjHandle = 0;


CModelViewerCamera modelCamera;

// processing image(TEXTURE) by OpenCV

IDirect3DTexture9* GetTextureAfterImageProcessing(IDirect3DDevice9* pd3dDevice, LPSTR fileName)
{
	Mat srcImage = imread(fileName);
	Mat dstImage;

	// 这里才用闭运算  先膨胀再腐蚀

	Mat element = getStructuringElement(MORPH_RECT, Size(10, 10));
	morphologyEx(
		srcImage,
		dstImage,
		MORPH_CLOSE,
		element
		);
	imwrite("AfterProcessing.bmp", dstImage);
	IDirect3DTexture9* tex = NULL;
	D3DXCreateTextureFromFileA(
		pd3dDevice,
		"AfterProcessing.bmp",
		&tex
		);
	return tex;
}


//--------------------------------------------------------------------------------------
// Rejects any D3D9 devices that aren't acceptable to the app by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D9DeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat,
                                      bool bWindowed, void* pUserContext )
{
    // Typically want to skip back buffer formats that don't support alpha blending
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                                         AdapterFormat, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
                                         D3DRTYPE_TEXTURE, BackBufferFormat ) ) )
        return false;

    return true;
}


//--------------------------------------------------------------------------------------
// Before a device is created, modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Create any D3D9 resources that will live through a device reset (D3DPOOL_MANAGED)
// and aren't tied to the back buffer size
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D9CreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext )
{
	D3DXVECTOR3 eyePt(0.0f, 0.0f, -250.0f);
	D3DXVECTOR3 lookAt(0.0f, 0.0f, 0.0f);
	modelCamera.SetViewParams(&eyePt, &lookAt);
	
	HRESULT hr = 0;

	//
	// Create geometry and compute corresponding world matrix and color
	// for each mesh.
	//


	// 创建模型
	ID3DXBuffer* adjBuffer = NULL;
	ID3DXBuffer* mtrlsBuffer = NULL;
	DWORD numMtrls = 0;

	V(D3DXLoadMeshFromX(
		L"cat.X",
		D3DXMESH_MANAGED,
		pd3dDevice,
		&adjBuffer,
		&mtrlsBuffer,
		0,
		&numMtrls,
		&Mesh
		)
		);

	// 优化网格
	DWORD* adjInfoBefore = new DWORD[Mesh->GetNumFaces() * 3];
	Mesh->GenerateAdjacency(0.001f, adjInfoBefore);

	DWORD* adjInfoAfter = new DWORD[Mesh->GetNumFaces() * 3];

	Mesh->OptimizeInplace(
		D3DXMESHOPT_ATTRSORT | D3DXMESHOPT_COMPACT,
		adjInfoBefore,
		adjInfoAfter,
		0,
		0
		);

	//ID3DXBuffer* TEAadjBuffer = 0;

	//D3DXCreateTeapot(pd3dDevice, &Mesh, &TEAadjBuffer);

	//DWORD* teapotAdjInfo = (DWORD*)TEAadjBuffer->GetBufferPointer();
	


	// load Materials and textures
	if (mtrlsBuffer != NULL && numMtrls != 0)
	{
		D3DXMATERIAL* mtrls = (D3DXMATERIAL*)mtrlsBuffer->GetBufferPointer();
		for (int i = 0; i < numMtrls; i++)
		{
			mtrls[i].MatD3D.Ambient = mtrls[i].MatD3D.Diffuse;

			g_Mtrls.push_back(mtrls[i].MatD3D);

			if (mtrls[i].pTextureFilename != NULL)
			{
				IDirect3DTexture9* tex = NULL;
				D3DXCreateTextureFromFileA(
					pd3dDevice,
					mtrls[i].pTextureFilename,
					&tex
					);
				g_Textures.push_back(tex);
			}
			else
			{
				g_Textures.push_back(0);
			}
		}

		mtrlsBuffer->Release();
	}


	MeshColor = D3DXVECTOR4(1.0f, 1.0f, 1.0f, 1.0f);

	
	MeshOutline = new SilhouetteEdges(pd3dDevice, Mesh, adjInfoAfter);


	ID3DXBuffer* toonCompiledCode = 0;
	ID3DXBuffer* toonErrorBuffer = 0;

	hr = D3DXCompileShaderFromFile(
		L"toon.hlsl",
		0,
		0,
		"Main", // entry point function name
		"vs_3_0",
		D3DXSHADER_ENABLE_BACKWARDS_COMPATIBILITY,
		&toonCompiledCode,
		&toonErrorBuffer,
		&ToonConstTable);

	// output any error messages
	if (toonErrorBuffer)
	{
		::MessageBox(0, (LPCWSTR)toonErrorBuffer->GetBufferPointer(), 0, 0);
	}

	if (FAILED(hr))
	{
		::MessageBox(0, L"D3DXCompileShaderFromFile() - FAILED", 0, 0);
		return false;
	}

	hr = pd3dDevice->CreateVertexShader(
		(DWORD*)toonCompiledCode->GetBufferPointer(),
		&ToonShader);

	if (FAILED(hr))
	{
		::MessageBox(0, L"CreateVertexShader - FAILED", 0, 0);
		return false;
	}


	// Outline Shader
	ID3DXBuffer* outlineCompiledCode = 0;
	ID3DXBuffer* outlineErrorBuffer = 0;

	hr = D3DXCompileShaderFromFile(
		L"outline.hlsl",
		0,
		0,
		"Main", // entry point function name
		"vs_3_0",
		D3DXSHADER_DEBUG,
		&outlineCompiledCode,
		&outlineErrorBuffer,
		&OutlineConstTable);

	// output any error messages
	if (outlineErrorBuffer)
	{
		::MessageBox(0, (LPCWSTR)outlineErrorBuffer->GetBufferPointer(), 0, 0);
		outlineErrorBuffer->Release();
	}

	if (FAILED(hr))
	{
		::MessageBox(0, L"D3DXCompileOutlineShaderFromFile() - FAILED", 0, 0);
		return false;
	}

	hr = pd3dDevice->CreateVertexShader(
		(DWORD*)outlineCompiledCode->GetBufferPointer(),
		&OutlineShader);

	if (FAILED(hr))
	{
		::MessageBox(0, L"CreateVertexShader - FAILED", 0, 0);
		return false;
	}

	// 
	// Get Handles
	//

	ToonWorldViewHandle = ToonConstTable->GetConstantByName(0, "WorldViewMatrix");
	ToonWorldViewProjHandle = ToonConstTable->GetConstantByName(0, "WorldViewProjMatrix");
	ToonColorHandle = ToonConstTable->GetConstantByName(0, "Color");
	ToonLightDirHandle = ToonConstTable->GetConstantByName(0, "LightDirection");

	OutlineWorldViewHandle = OutlineConstTable->GetConstantByName(0, "WorldViewMatrix");
	OutlineProjHandle = OutlineConstTable->GetConstantByName(0, "ProjMatrix");

	//
	// Load textures.
	//

	D3DXCreateTextureFromFile(pd3dDevice, L"toonshade.bmp", &ShadeTex);


	ToonConstTable->SetDefaults(pd3dDevice);

	OutlineConstTable->SetDefaults(pd3dDevice);


    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D9 resources that won't live through a device reset (D3DPOOL_DEFAULT) 
// or that are tied to the back buffer size 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D9ResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                    void* pUserContext )
{
	float fAspectRatio = pBackBufferSurfaceDesc->Width / (FLOAT)pBackBufferSurfaceDesc->Height;
	modelCamera.SetProjParams(D3DX_PI / 4, fAspectRatio, 0.1f, 1000.0f);
	modelCamera.SetWindow(pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height);

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
	modelCamera.FrameMove(fElapsedTime);
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D9 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9FrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
	D3DXMATRIX world = *modelCamera.GetWorldMatrix();
	D3DXMATRIX view = *modelCamera.GetViewMatrix();
    D3DXMATRIX proj = *modelCamera.GetProjMatrix();

	
	HRESULT hr;

    // Clear the render target and the zbuffer 
    V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_ARGB( 0, 0, 0, 255 ), 1.0f, 0 ) );

    // Render the scene
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {

		// Draw Cartoon Color
		
		pd3dDevice->SetTexture(0, ShadeTex);

		D3DXMATRIX worldView;
		D3DXMATRIX worldViewProj;
	
		worldView = world* view;
		worldViewProj = world * view * proj;

		ToonConstTable->SetMatrix(
			pd3dDevice,
			ToonWorldViewHandle,
			&worldView);

		ToonConstTable->SetMatrix(
			pd3dDevice,
			ToonWorldViewProjHandle,
			&worldViewProj);

		ToonConstTable->SetVector(
			pd3dDevice,
			ToonColorHandle,
			&MeshColor);

		// Light direction:
		D3DXVECTOR4 directionToLight(-0.57f, 0.57f, -0.57f, 0.0f);

		ToonConstTable->SetVector(
			pd3dDevice,
			ToonLightDirHandle,
			&directionToLight);

		//Mesh->DrawSubset(0);

		//{
		//	pd3dDevice->SetFVF(Mesh->GetFVF());

		//	IDirect3DVertexBuffer9* vertexBuffer;
		//	IDirect3DIndexBuffer9* indexBuffer;

		//	Mesh->GetVertexBuffer(&vertexBuffer);
		//	Mesh->GetIndexBuffer(&indexBuffer);

		//	pd3dDevice->SetStreamSource(0, vertexBuffer, 0, Mesh->GetNumBytesPerVertex());
		//	pd3dDevice->SetIndices(indexBuffer);

		//	pd3dDevice->DrawIndexedPrimitive(
		//		D3DPT_TRIANGLELIST, 0, 0, 4415, 0, 6000);
		//}

		for (int i = 0; i < g_Mtrls.size(); i++)
		{
			pd3dDevice->SetVertexShader(ToonShader);
			Mesh->DrawSubset(i);
		}

		// Draw Mesh Outline
		pd3dDevice->SetVertexShader(OutlineShader);
		pd3dDevice->SetTexture(0, 0);

		pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);


		worldView = world * view;

		OutlineConstTable->SetMatrix(
			pd3dDevice,
			OutlineWorldViewHandle,
			&worldView);

		OutlineConstTable->SetMatrix(
			pd3dDevice,
			OutlineProjHandle,
			&proj);

		MeshOutline->render();


		pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);

        V( pd3dDevice->EndScene() );
		pd3dDevice->Present(0, 0, 0, 0);
	}
}


//--------------------------------------------------------------------------------------
// Handle messages to the application 
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                          bool* pbNoFurtherProcessing, void* pUserContext )
{
	modelCamera.HandleMessages(hWnd, uMsg, wParam, lParam);
    return 0;
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9ResetDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9LostDevice( void* pUserContext )
{
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9CreateDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D9DestroyDevice(void* pUserContext)
{
	if (ToonShader != NULL)
	{
		ToonShader->Release();
	}

	if (ToonConstTable != NULL)
	{
		ToonConstTable->Release();
	}

	if (Mesh != NULL)
	{
		Mesh->Release();
	}

	if (ShadeTex != NULL)
	{
		ShadeTex->Release();
	}


	if (MeshOutline != NULL)
	{
		delete MeshOutline;
	}

	if (OutlineShader != NULL)
	{
		OutlineShader->Release();
	}

	if (OutlineConstTable != NULL)
	{
		OutlineConstTable->Release();
	}

	if (!g_Textures.empty())
	{
		for (std::vector<IDirect3DTexture9*>::iterator it = g_Textures.begin(); it != g_Textures.end(); ++it)
		{
			(*it)->Release();
		}
	}

}


//--------------------------------------------------------------------------------------
// Initialize everything and go into a render loop
//--------------------------------------------------------------------------------------
INT WINAPI wWinMain( HINSTANCE, HINSTANCE, LPWSTR, int )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // Set the callback functions
    DXUTSetCallbackD3D9DeviceAcceptable( IsD3D9DeviceAcceptable );
    DXUTSetCallbackD3D9DeviceCreated( OnD3D9CreateDevice );
    DXUTSetCallbackD3D9DeviceReset( OnD3D9ResetDevice );
    DXUTSetCallbackD3D9FrameRender( OnD3D9FrameRender );
    DXUTSetCallbackD3D9DeviceLost( OnD3D9LostDevice );
    DXUTSetCallbackD3D9DeviceDestroyed( OnD3D9DestroyDevice );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackFrameMove( OnFrameMove );

    // TODO: Perform any application-level initialization here

    // Initialize DXUT and create the desired Win32 window and Direct3D device for the application
    DXUTInit( true, true ); // Parse the command line and show msgboxes
    DXUTSetHotkeyHandling( true, true, true );  // handle the default hotkeys
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"EmptyProject" );
    DXUTCreateDevice( true, 640, 480 );

    // Start the render loop
    DXUTMainLoop();

    // TODO: Perform any application-level cleanup here

    return DXUTGetExitCode();
}


