// test.cpp : Defines the entry point for the console application.
//

#include <windows.h>
#include <malloc.h>
#include <stdlib.h>
#include <iostream>

#include "..\ddx\ddx.h"


typedef struct
{
	const wchar_t* root_dir;
	DWORD frame_count;
	DWORD max_frames;

}WRITER_DESC;


int write_bitmap(FRAME_DATA* frame, WRITER_DESC* d)
{

	FILE* lfile = nullptr;
	wchar_t path[MAX_PATH] = { 0 };

	// BMP 32 bpp
	BITMAPINFO	lBmpInfo = { 0 };
	BITMAPFILEHEADER bmpFileHeader = { 0 };

	lBmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	lBmpInfo.bmiHeader.biBitCount = 32;
	lBmpInfo.bmiHeader.biCompression = BI_RGB;
	lBmpInfo.bmiHeader.biWidth = frame->width;
	lBmpInfo.bmiHeader.biHeight = frame->height; // TODO negative height?
	lBmpInfo.bmiHeader.biPlanes = 1;
	lBmpInfo.bmiHeader.biSizeImage = lBmpInfo.bmiHeader.biWidth* lBmpInfo.bmiHeader.biHeight * 4;


	bmpFileHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + lBmpInfo.bmiHeader.biSizeImage;
	bmpFileHeader.bfType = 'MB';
	bmpFileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);


	swprintf_s(path, MAX_PATH, L"%s\\%d.bmp", d->root_dir, d->frame_count);

	errno_t lerr = _wfopen_s(&lfile, path, L"wb");

	if (lerr != 0) {
		return __LINE__;
	}

	if (lfile != nullptr)
	{
		unsigned int s = lBmpInfo.bmiHeader.biSizeImage;
		unsigned int w = lBmpInfo.bmiHeader.biWidth * 4;
		unsigned h = lBmpInfo.bmiHeader.biHeight;

		// write upside down, otherwise:
		fwrite(&bmpFileHeader, sizeof(BITMAPFILEHEADER), 1, lfile);
		fwrite(&lBmpInfo.bmiHeader, sizeof(BITMAPINFOHEADER), 1, lfile);
		//		fwrite(buf, lBmpInfo.bmiHeader.biSizeImage, 1, lfile);
		BYTE* sptr = frame->buffer + s - w;
		for (size_t ch = 0; ch < h; ++ch)
		{
			fwrite(sptr, w, 1, lfile);
			sptr -= w;
		}

		fclose(lfile);
	}

	return DDX_CONTINUE_RECORDING;
}

int __stdcall frame_handler(FRAME_DATA* frame, void* opq)
{
	WRITER_DESC* d = (WRITER_DESC*)opq;
	if (!opq)
	{
		// should not happen
		return 2;
	}

	if (!frame)
	{
		// same frame
		Sleep(100);
		return DDX_CONTINUE_RECORDING;
	}

	d->frame_count++;

	if (d->max_frames && d->max_frames < d->frame_count)
	{
		return 1; // stop
	}

	int res = write_bitmap(frame, d);
	if (!res)
	{
		Sleep(100); // till next frame
	}

	return res;
}

int main()
{
	int r = 0;
	PRECORD_CONTEXT pRc = (PRECORD_CONTEXT)malloc(ddx_context_size());
	WRITER_DESC d = { 0 };

	d.root_dir = L"C:\\windows\\temp\\ddx";
	d.max_frames = 200;

	r = ddx_init(pRc);
	if (0 != r)
	{
		printf("failed init %d", r);
		goto exit;
	}
	r = ddx_record(pRc, frame_handler, &d);
	if (0 != r)
	{
		printf("failed recording");
	}
	else
		printf("done recording");
exit:
	r = ddx_cleanup(pRc);
	if (0 != r)
		printf("failed cleanup %d", r);

	return 0;
}

