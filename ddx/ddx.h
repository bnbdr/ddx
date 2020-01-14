#pragma once

typedef struct RECORD_CONTEXT RECORD_CONTEXT, *PRECORD_CONTEXT;

typedef struct FRAME_DATA {
	unsigned char* buffer;
	unsigned long rowPitch;
	unsigned long dxgiFormat; // most likely DXGI_FORMAT_B8G8R8A8_*
	unsigned long width;
	unsigned long height;
} FRAME_DATA, *PFRAME_DATA;

#define DDX_CONTINUE_RECORDING		0
#define DDX_STOP_RECORDING			1

typedef int(__stdcall *FrameCallbackType)(PFRAME_DATA frame, void* opq);

extern "C" {
	// call this function to query the required size for record context
	int __stdcall ddx_context_size();

	// pass a pre-allocated RECORD_CONTEXT struct
	int __stdcall ddx_init(PRECORD_CONTEXT pRc);

	// will call onFrame callback whenever the frame got dirty(kinda), will sleep interval amount between check. onFramw will be called until it returns a non-zero value - signals the recording to stop
	int __stdcall ddx_record(PRECORD_CONTEXT pRc, FrameCallbackType onFrame, void* opq);

	// always call cleanup, even if init fails
	int __stdcall ddx_cleanup(PRECORD_CONTEXT pRc);
}