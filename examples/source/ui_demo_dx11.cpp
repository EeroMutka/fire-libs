#ifdef UI_DEMO_DX11

// For debug mode, uncomment this line:
//#define UI_DX11_DEBUG_MODE

#define _CRT_SECURE_NO_WARNINGS
#pragma comment (lib, "d3d11")
#pragma comment (lib, "d3dcompiler")

#include <d3d11.h>
#include <d3dcompiler.h>
#ifdef UI_DX11_DEBUG_MODE
#include <dxgidebug.h>
#endif

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "fire_ds.h"

#define STR_USE_FIRE_DS_ARENA
#include "fire_string.h"

#define FIRE_OS_WINDOW_IMPLEMENTATION
#include "fire_os_window.h"

#define FIRE_OS_CLIPBOARD_IMPLEMENTATION
#include "fire_os_clipboard.h"

#define UI_API static
#define UI_EXTERN extern
#include "fire_ui/fire_ui.h"
#include "fire_ui/fire_ui_backend_stb_truetype.h"
#include "fire_ui/fire_ui_backend_dx11.h"
#include "fire_ui/fire_ui_backend_fire_os.h"

// Build everything in this translation unit
#include "fire_ui/fire_ui.c"
#include "fire_ui/fire_ui_color_pickers.c"
#include "fire_ui/fire_ui_extras.c"

#include "ui_demo_window.h"

//// Globals ///////////////////////////////////////////////

static DS_BasicMemConfig g_mem;
static DS_Arena g_persist_arena;
static UI_Font g_base_font, g_icons_font;

static ID3D11Device* g_d3d11_device;
static ID3D11DeviceContext* g_d3d11_device_context;
static IDXGISwapChain* g_d3d11_swapchain;
static ID3D11RenderTargetView* g_d3d11_framebuffer_rtv;

static UI_Vec2 g_window_size = {900, 720};
static OS_Window g_window;

static UI_Inputs g_ui_inputs;

static UIDemoState g_demo_state;

////////////////////////////////////////////////////////////

static STR_View ReadEntireFile(DS_Arena* arena, const char* file) {
	FILE* f = fopen(file, "rb");
	assert(f);

	fseek(f, 0, SEEK_END);
	size_t fsize = ftell(f);
	fseek(f, 0, SEEK_SET);

	char* data = DS_ArenaPush(arena, fsize);
	fread(data, fsize, 1, f);

	fclose(f);
	STR_View result = {data, fsize};
	return result;
}

static void UpdateAndRender() {
	UI_BeginFrame(&g_ui_inputs, g_base_font, g_icons_font);

	UIDemoBuild(&g_demo_state, g_window_size);

	UI_Outputs ui_outputs;
	UI_EndFrame(&ui_outputs);
	
	FLOAT clear_color[4] = { 0.15f, 0.15f, 0.15f, 1.f };
	g_d3d11_device_context->ClearRenderTargetView(g_d3d11_framebuffer_rtv, clear_color);

	UI_DX11_Draw(&ui_outputs, g_window_size, g_d3d11_framebuffer_rtv);
	UI_OS_ApplyOutputs(&g_window, &ui_outputs);
	
	g_d3d11_swapchain->Present(1, 0);
}

static void OnResizeWindow(uint32_t width, uint32_t height, void* user_ptr) {
	g_window_size.x = (float)width;
	g_window_size.y = (float)height;

	// Recreate swapchain

	g_d3d11_framebuffer_rtv->Release();

	g_d3d11_swapchain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
	
	ID3D11Texture2D* framebuffer;
	g_d3d11_swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&framebuffer); // grab framebuffer from swapchain

	D3D11_RENDER_TARGET_VIEW_DESC framebuffer_rtv_desc{};
	framebuffer_rtv_desc.Format        = DXGI_FORMAT_B8G8R8A8_UNORM;
	framebuffer_rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
	g_d3d11_device->CreateRenderTargetView(framebuffer, &framebuffer_rtv_desc, &g_d3d11_framebuffer_rtv);
	
	framebuffer->Release(); // We don't need this handle anymore

	UpdateAndRender();
}

static void AppInit() {
	DS_InitBasicMemConfig(&g_mem);
	DS_ArenaInit(&g_persist_arena, 4096, g_mem.heap);

	UIDemoInit(&g_demo_state, &g_persist_arena);

	g_window = OS_CreateWindow((uint32_t)g_window_size.x, (uint32_t)g_window_size.y, "UI demo (DX11)");

	D3D_FEATURE_LEVEL dx_feature_levels[] = { D3D_FEATURE_LEVEL_11_0 };

	DXGI_SWAP_CHAIN_DESC swapchain_desc = {0};
	swapchain_desc.BufferDesc.Width  = 0; // use window width
	swapchain_desc.BufferDesc.Height = 0; // use window height
	swapchain_desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	swapchain_desc.SampleDesc.Count  = 8;
	swapchain_desc.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapchain_desc.BufferCount       = 2;
	swapchain_desc.OutputWindow      = (HWND)g_window.handle;
	swapchain_desc.Windowed          = TRUE;
	swapchain_desc.SwapEffect        = DXGI_SWAP_EFFECT_DISCARD;

	uint32_t create_device_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef UI_DX11_DEBUG_MODE
	create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	HRESULT res = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
		create_device_flags, dx_feature_levels, ARRAYSIZE(dx_feature_levels), D3D11_SDK_VERSION,
		&swapchain_desc, &g_d3d11_swapchain, &g_d3d11_device, NULL, &g_d3d11_device_context);
	assert(res == S_OK);

	g_d3d11_swapchain->GetDesc(&swapchain_desc); // Update swapchain_desc with actual window size

	///////////////////////////////////////////////////////////////////////////////////////////////

	ID3D11Texture2D* framebuffer;
	g_d3d11_swapchain->GetBuffer(0, _uuidof(ID3D11Texture2D), (void**)&framebuffer); // grab framebuffer from swapchain

	D3D11_RENDER_TARGET_VIEW_DESC framebuffer_rtv_desc = {};
	framebuffer_rtv_desc.Format        = DXGI_FORMAT_B8G8R8A8_UNORM;
	framebuffer_rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
	g_d3d11_device->CreateRenderTargetView(framebuffer, &framebuffer_rtv_desc, &g_d3d11_framebuffer_rtv);

	framebuffer->Release(); // We don't need this handle anymore

	///////////////////////////////////////////////////////////////////////////////////////////////

	UI_Init(g_mem.heap);
	UI_DX11_Init(g_d3d11_device, g_d3d11_device_context);
	UI_STBTT_Init(UI_DX11_CreateAtlas, UI_DX11_MapAtlas);

	// NOTE: the font data must remain alive across the whole program lifetime!
	STR_View roboto_mono_ttf = ReadEntireFile(&g_persist_arena, "../../fire_ui/resources/roboto_mono.ttf");
	STR_View icons_ttf = ReadEntireFile(&g_persist_arena, "../../fire_ui/resources/fontello/font/fontello.ttf");

	g_base_font = { UI_STBTT_FontInit(roboto_mono_ttf.data, -4.f), 18 };
	g_icons_font = { UI_STBTT_FontInit(icons_ttf.data, -2.f), 18 };
}

static void AppDeinit() {
	UI_STBTT_FontDeinit(g_base_font.id);
	UI_STBTT_FontDeinit(g_icons_font.id);

	UI_Deinit();
	UI_DX11_Deinit();
	UI_STBTT_Deinit();

	g_d3d11_framebuffer_rtv->Release();
	g_d3d11_swapchain->Release();
	g_d3d11_device->Release();
	g_d3d11_device_context->Release();

	DS_ArenaDeinit(&g_persist_arena);
	DS_DeinitBasicMemConfig(&g_mem);
}

int main() {
	AppInit();
	
	while (!OS_WindowShouldClose(&g_window)) {
		UI_OS_ResetFrameInputs(&g_window, &g_ui_inputs);
		
		OS_Event event; 
		while (OS_PollEvent(&g_window, &event, OnResizeWindow, NULL)) {
			UI_OS_RegisterInputEvent(&g_ui_inputs, &event);
		}

		UpdateAndRender();
	}

	AppDeinit();
}

#endif // UI_DEMO_DX11