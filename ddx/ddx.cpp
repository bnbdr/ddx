#define WIN32_LEAN_AND_MEAN    
#pragma warning( push )
#pragma warning( disable : 4201 )
#include <windows.h>
#include <dxgi1_2.h>
#include <d3d11.h>
#pragma warning( pop ) 

#include "ddx.h"

#ifdef NDEBUG
#define DBGPRINT(x)
#else
#define DBGPRINT(x) OutputDebugString(x)
#endif

// uses D3D11.lib, can'ut use pragma with NODEFAULTLIBS
#pragma comment(lib, "D3D11.lib")


BOOL WINAPI DllMain(HMODULE hModule,
	DWORD  dwReason,
	LPVOID lpReserved
)
{
	UNREFERENCED_PARAMETER(hModule);
	UNREFERENCED_PARAMETER(lpReserved);

	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

typedef struct RECORD_CONTEXT {
	ID3D11Device *pDevice;
	ID3D11DeviceContext *pImmCtx;
	IDXGIOutputDuplication *pOutputDup;
	ID3D11Texture2D *pGDIImage;
	ID3D11Texture2D *pDupImage;

	DXGI_OUTPUT_DESC OutoutDesc;
	DXGI_OUTDUPL_DESC DupDesc;
	D3D_FEATURE_LEVEL FeatureLevel;

} RECORD_CONTEXT, *PRECORD_CONTEXT;


D3D_FEATURE_LEVEL RequiredLevels[] =
{
	D3D_FEATURE_LEVEL_11_0,
	D3D_FEATURE_LEVEL_10_1,
	D3D_FEATURE_LEVEL_10_0,
	D3D_FEATURE_LEVEL_9_1
};

#define EXIT_IF(c) if((c)){err = __LINE__; goto exit;}
#define SAFE_RELEASE(p) if(p){p->Release(); p=NULL;}



// implementation

int __stdcall ddx_context_size()
{
	return sizeof(RECORD_CONTEXT);
}

int __stdcall ddx_init(PRECORD_CONTEXT pRc)
{
	int err = 0;
	HRESULT hr = 0;
	IDXGIDevice * pDxgiDevice = NULL;
	IDXGIAdapter* pDxgiAdapter = NULL;
	IDXGIOutput* pDxgiOutput = NULL;
	IDXGIOutput1* pDxgiOutput2 = NULL;

	UINT Output = 0;

	EXIT_IF(!pRc);

	hr = D3D11CreateDevice(
		NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		0,
		RequiredLevels,
		ARRAYSIZE(RequiredLevels),
		D3D11_SDK_VERSION,
		&pRc->pDevice,
		&pRc->FeatureLevel,
		&pRc->pImmCtx);

	EXIT_IF(FAILED(hr));
	EXIT_IF(NULL == pRc->pDevice);

	hr = pRc->pDevice->QueryInterface(IID_PPV_ARGS(&pDxgiDevice));
	EXIT_IF(FAILED(hr));

	hr = pDxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**)(&pDxgiAdapter));
	EXIT_IF(FAILED(hr));

	hr = pDxgiAdapter->EnumOutputs(Output, &pDxgiOutput);
	EXIT_IF(FAILED(hr));

	hr = pDxgiOutput->GetDesc(&pRc->OutoutDesc);
	EXIT_IF(FAILED(hr));

	hr = pDxgiOutput->QueryInterface(IID_PPV_ARGS(&pDxgiOutput2));
	EXIT_IF(FAILED(hr));

	hr = pDxgiOutput2->DuplicateOutput(pRc->pDevice, &pRc->pOutputDup);
	EXIT_IF(FAILED(hr));

	pRc->pOutputDup->GetDesc(&pRc->DupDesc);

	D3D11_TEXTURE2D_DESC desc;

	desc.Width = pRc->DupDesc.ModeDesc.Width;
	desc.Height = pRc->DupDesc.ModeDesc.Height;
	desc.Format = pRc->DupDesc.ModeDesc.Format;
	desc.ArraySize = 1;
	desc.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_RENDER_TARGET;
	desc.MiscFlags = D3D11_RESOURCE_MISC_GDI_COMPATIBLE;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.MipLevels = 1;
	desc.CPUAccessFlags = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;

	hr = pRc->pDevice->CreateTexture2D(&desc, NULL, &pRc->pGDIImage);

	EXIT_IF(FAILED(hr));
	EXIT_IF(NULL == pRc->pGDIImage);

	// texture for cpu buf
	desc.Width = pRc->DupDesc.ModeDesc.Width;
	desc.Height = pRc->DupDesc.ModeDesc.Height;
	desc.Format = pRc->DupDesc.ModeDesc.Format;
	desc.ArraySize = 1;
	desc.BindFlags = 0;
	desc.MiscFlags = 0;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.MipLevels = 1;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
	desc.Usage = D3D11_USAGE_STAGING;

	hr = pRc->pDevice->CreateTexture2D(&desc, NULL, &pRc->pDupImage);

	EXIT_IF(FAILED(hr));
	EXIT_IF(NULL == pRc->pDupImage);

exit:
	SAFE_RELEASE(pDxgiDevice);
	SAFE_RELEASE(pDxgiAdapter);
	SAFE_RELEASE(pDxgiOutput);
	SAFE_RELEASE(pDxgiOutput2);

	return err;
}

inline int no_changes_in_frame(DXGI_OUTDUPL_FRAME_INFO * frame_info)
{
	if (1 == frame_info->AccumulatedFrames)
		return TRUE;

	if (0 == frame_info->AccumulatedFrames &&
		0 == frame_info->TotalMetadataBufferSize &&
		0 == frame_info->LastPresentTime.QuadPart)
		return TRUE;

	return FALSE;
}

int __stdcall ddx_record(PRECORD_CONTEXT pRc, FrameCallbackType onFrame, void* opq)
{
	int err = 0;
	int cb_response = DDX_CONTINUE_RECORDING;

	IDXGIResource * pDesktopRes = NULL;
	ID3D11Texture2D * pAcquiredImage = NULL;
	DXGI_OUTDUPL_FRAME_INFO FrameInfo;
	D3D11_MAPPED_SUBRESOURCE OutResource;

	HRESULT hr = 0;
	UINT subresource = 0;

	EXIT_IF(!pRc);
	EXIT_IF(!onFrame);

	do
	{
		FRAME_DATA frame = { 0 };

		// Get new frame
		hr = pRc->pOutputDup->AcquireNextFrame(
			INFINITE,
			&FrameInfo,
			&pDesktopRes);
		EXIT_IF(FAILED(hr));

		if (no_changes_in_frame(&FrameInfo))
		{
			DBGPRINT("no change");
			cb_response = onFrame(NULL, opq); // ignores result
			SAFE_RELEASE(pDesktopRes);
			pRc->pOutputDup->ReleaseFrame(); // must do this the closes to acquire as possible

			if (DDX_CONTINUE_RECORDING != cb_response)
				break;

			continue;
		}

		hr = pDesktopRes->QueryInterface(IID_PPV_ARGS(&pAcquiredImage));
		EXIT_IF(FAILED(hr));
		EXIT_IF(NULL == pAcquiredImage);
		SAFE_RELEASE(pDesktopRes);

		pRc->pImmCtx->CopyResource(pRc->pGDIImage, pAcquiredImage);
		SAFE_RELEASE(pAcquiredImage);

		pRc->pImmCtx->CopyResource(pRc->pDupImage, pRc->pGDIImage);

		// copy cpu buf
		subresource = D3D11CalcSubresource(0, 0, 0);
		hr = pRc->pImmCtx->Map(pRc->pDupImage, subresource, D3D11_MAP_READ_WRITE, 0, &OutResource);
		EXIT_IF(FAILED(hr));

		DBGPRINT("got frame buffer, calling cb");
		frame.buffer = (unsigned char*)OutResource.pData;
		frame.dxgiFormat = pRc->DupDesc.ModeDesc.Format;
		frame.rowPitch = OutResource.RowPitch;
		frame.height = pRc->DupDesc.ModeDesc.Height;
		frame.width = pRc->DupDesc.ModeDesc.Width;

		cb_response = onFrame(&frame, opq); // callack should perform sleep if needed

		pRc->pImmCtx->Unmap(pRc->pDupImage, subresource);
		pRc->pOutputDup->ReleaseFrame(); // should be called the closes to acquire as possible
	} while (DDX_CONTINUE_RECORDING == cb_response);

	DBGPRINT("instructed to stop");

exit:
	SAFE_RELEASE(pDesktopRes);
	SAFE_RELEASE(pAcquiredImage);

	return err;
}

int __stdcall ddx_cleanup(PRECORD_CONTEXT pRc)
{
	int err = 0;
	EXIT_IF(!pRc);

	SAFE_RELEASE(pRc->pDupImage);
	SAFE_RELEASE(pRc->pGDIImage);
	SAFE_RELEASE(pRc->pOutputDup);
	SAFE_RELEASE(pRc->pImmCtx);
	SAFE_RELEASE(pRc->pDevice);

exit:
	return err;
}