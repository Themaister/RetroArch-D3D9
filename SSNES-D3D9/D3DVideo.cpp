#include "StdAfx.h"
#include "D3DVideo.h"
#include <iostream>
#include <exception>
#include <cstring>
#include <cstdio>
#include <stdint.h>
#include <iostream>
#include <cmath>

#ifdef _MSC_VER
#pragma warning(disable: 4244)
#pragma warning(disable: 4800)
#pragma warning(disable: 4996)
#endif

namespace Callback
{
	static bool quit = false;

	LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, 
		WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_QUIT:
		case WM_DESTROY:
			PostQuitMessage(0);
			quit = true;
			return 0;
		
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
}

namespace Global
{
	static HWND hwnd = NULL;
}

void D3DVideo::init_base(const ssnes_video_info_t &info)
{
	D3DPRESENT_PARAMETERS d3dpp;
	make_d3dpp(info, d3dpp);

	g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
	if (!g_pD3D)
		throw std::runtime_error("Failed to create D3D9 interface!");

	if (FAILED(g_pD3D->CreateDevice(
			D3DADAPTER_DEFAULT,
			D3DDEVTYPE_HAL,
			hWnd,
			D3DCREATE_HARDWARE_VERTEXPROCESSING,
			&d3dpp,
			&dev)))
	{
		throw std::runtime_error("Failed to init device");
	}
}

void D3DVideo::make_d3dpp(const ssnes_video_info_t &info, D3DPRESENT_PARAMETERS &d3dpp)
{
	ZeroMemory(&d3dpp, sizeof(d3dpp));

#if 0
	if (std::getenv("SSNES_WINDOWED_FULLSCREEN"))
	{
		std::cerr << "[Direct3D]: Windowed fullscreen env var detected!" << std::endl;
		d3dpp.Windowed = true;
	}
	else
		d3dpp.Windowed = !info.fullscreen;
#else
	d3dpp.Windowed = true;
#endif
	
	d3dpp.PresentationInterval = info.vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.hDeviceWindow = hWnd;
	d3dpp.BackBufferCount = 2;
	d3dpp.BackBufferFormat = info.fullscreen ? D3DFMT_X8R8G8B8 : D3DFMT_UNKNOWN;

	if (info.fullscreen)
	{
		d3dpp.BackBufferWidth = screen_width;
		d3dpp.BackBufferHeight = screen_height;
	}
}

void D3DVideo::init(const ssnes_video_info_t &info)
{
	if (!g_pD3D)
		init_base(info);
	else if (needs_restore)
	{
		D3DPRESENT_PARAMETERS d3dpp;
		make_d3dpp(info, d3dpp);
		if (dev->Reset(&d3dpp) != D3D_OK)
			throw std::runtime_error("Failed to reset device ...");
	}
	
	if (!init_cg(info.cg_shader))
		throw std::runtime_error("Failed to init Cg");
	if (!init_font())
		throw std::runtime_error("Failed to init Font");

	D3DXMATRIX ident;
	D3DXMatrixIdentity(&ident);
	dev->SetTransform(D3DTS_WORLD, &ident);
	dev->SetTransform(D3DTS_VIEW, &ident);
	
	if (FAILED(dev->CreateVertexBuffer(
		4 * sizeof(Vertex),
		NULL,
		FVF,
		D3DPOOL_DEFAULT,
		&vertex_buf,
		NULL)))
	{
		throw std::runtime_error("Failed to create Vertex buf ...");
	}

	D3DFORMAT tex_format;
	if (info.color_format == SSNES_COLOR_FORMAT_ARGB8888)
	{
		pixel_size = 4;
		tex_format = D3DFMT_X8R8G8B8;
	}
	else
	{
		pixel_size = 2;
		tex_format = D3DFMT_X1R5G5B5;
	}

	tex_w = info.input_scale * 256;
	tex_h = info.input_scale * 256;
	last_width = tex_w;
	last_height = tex_h;

	update_coord(tex_w, tex_h);

	calculate_rect(screen_width, screen_height, info.force_aspect, info.aspect_ratio);

	if (FAILED(dev->CreateTexture(tex_w, tex_h, 1, 0, 
		tex_format, D3DPOOL_MANAGED,
		&tex, NULL)))
	{
		throw std::runtime_error("Failed to create texture ...");
	}

	dev->SetTexture(0, tex);
	dev->SetSamplerState(0, D3DSAMP_MINFILTER, info.smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
	dev->SetSamplerState(0, D3DSAMP_MAGFILTER, info.smooth ? D3DTEXF_LINEAR : D3DTEXF_POINT);
	dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
	dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
	dev->SetRenderState(D3DRS_LIGHTING, FALSE);

	nonblock = false;

	dev->SetFVF(FVF);
	dev->SetStreamSource(0, vertex_buf, 0, sizeof(Vertex));
}

void D3DVideo::set_viewport(unsigned x, unsigned y, unsigned width, unsigned height)
{
	D3DVIEWPORT9 viewport;
	viewport.X = x;
	viewport.Y = y;
	viewport.Width = width;
	viewport.Height = height;
	viewport.MinZ = 0.0f;
	viewport.MaxZ = 1.0f;

	vp_width = width;
	vp_height = height;

	dev->SetViewport(&viewport);

	font_rect.left = x + width * 0.05;
	font_rect.right = x + width;
	font_rect.top = y + 0.90 * height; 
	font_rect.bottom = height;
}

void D3DVideo::calculate_rect(unsigned width, unsigned height, bool keep, float desired_aspect)
{
	if (!keep)
	{
		set_viewport(0, 0, width, height);
	}
	else
	{
		float device_aspect = static_cast<float>(width) / static_cast<float>(height);
		if (fabs(device_aspect - desired_aspect) < 0.001)
		{
			set_viewport(0, 0, width, height);
		}
		else if (device_aspect > desired_aspect)
		{
			float delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5;
			set_viewport(width * (0.5 - delta), 0, 2.0 * width * delta, height);
			width = 2.0 * width * delta;
		}
		else
		{
			float delta = (device_aspect / desired_aspect - 1.0) / 2.0 + 0.5;
			set_viewport(0, height * (0.5 - delta), width, 2.0 * height * delta);
			height = 2.0 * height * delta;
		}
	}
}

void D3DVideo::update_coord(unsigned width, unsigned height)
{
	float tex_x = static_cast<float>(width) / tex_w;
	float tex_y = static_cast<float>(height) / tex_h;

	Vertex vert[] = {
		{0,					screen_height - 1,	0.5, 0,			0},
		{screen_width - 1,  screen_height - 1,  0.5, tex_x,		0},
		{0,					0,					0.5, 0,		tex_y},
		{screen_width - 1,	0,					0.5, tex_x, tex_y}
	};

	// Align vertex coords with texels (Direct3D quirk).
	for (unsigned i = 0; i < 4; i++)
	{
		vert[i].x -= 0.5f;
		vert[i].y += 0.5f;
	}

	void *verts;
	vertex_buf->Lock(0, sizeof(vert), &verts, 0);
	std::memcpy(verts, &vert, sizeof(vert));
	vertex_buf->Unlock();

	D3DXMATRIX proj;
	D3DXMatrixOrthoOffCenterLH(&proj, 0, screen_width - 1, 0, screen_height - 1, 0, 1);
	dev->SetTransform(D3DTS_PROJECTION, &proj);
	set_cg_mvp(proj);
}

D3DVideo::D3DVideo(const ssnes_video_info_t *info) : 
	g_pD3D(nullptr), dev(nullptr), vertex_buf(nullptr), tex(nullptr), needs_restore(false), frames(0)
{
	ZeroMemory(&windowClass, sizeof(windowClass));
	windowClass.cbSize = sizeof(windowClass);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = Callback::WindowProc;
	windowClass.hInstance = NULL;
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClass.lpszClassName = L"SSNESWindowClass";
	if (!info->fullscreen)
		windowClass.hbrBackground = (HBRUSH)COLOR_WINDOW;

	RegisterClassEx(&windowClass);

	RECT rect;
	GetClientRect(GetDesktopWindow(), &rect);
	unsigned full_x = info->width == 0 ? (rect.right - rect.left) : info->width;
	unsigned full_y = info->height == 0 ? (rect.bottom - rect.top) : info->height;
	std::cerr << "[Direct3D]: Desktop size: " << rect.right << "x" << rect.bottom << std::endl;

	screen_width = info->fullscreen ? full_x : info->width;
	screen_height = info->fullscreen ? full_y : info->height;

	unsigned win_width = screen_width;
	unsigned win_height = screen_height;

	if (!info->fullscreen)
	{
		RECT rect = {0};
		rect.right = screen_width;
		rect.bottom = screen_height;
		AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW & ~(WS_MAXIMIZEBOX | WS_THICKFRAME), FALSE);
		win_width = rect.right - rect.left;
		win_height = rect.bottom - rect.top;
	}

	wchar_t title_buf[1024];
	MultiByteToWideChar(CP_UTF8, 0, info->title_hint, -1, title_buf, 1024);
	title = title_buf;
	title += L" || Direct3D9";

	hWnd = CreateWindowEx(NULL, L"SSNESWindowClass", title.c_str(), 
		info->fullscreen ?
			(WS_EX_TOPMOST | WS_POPUP) : WS_OVERLAPPEDWINDOW & ~(WS_MAXIMIZEBOX | WS_THICKFRAME), 
		info->fullscreen ? 0 : 200, info->fullscreen ? 0 : 200, win_width, win_height,
		nullptr, nullptr, nullptr, nullptr);

	Global::hwnd = hWnd;

	if (info->fullscreen)
		ShowCursor(FALSE);

	Callback::quit = false;

	ShowWindow(hWnd, SW_RESTORE);
	UpdateWindow(hWnd);
	SetForegroundWindow(hWnd);
	SetFocus(hWnd);

	video_info = *info;
	init(video_info);

	clear();

	std::cerr << "[Direct3D]: Good to go!" << std::endl;

	// Init FPS count.
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&last_time);
}

HWND D3DVideo::hwnd()
{
	return Global::hwnd;
}

void D3DVideo::deinit()
{
	deinit_font();
	deinit_cg();
	
	if (vertex_buf)
		vertex_buf->Release();
	if (tex)
		tex->Release();
	
	needs_restore = false;
	vertex_buf = NULL;
	tex = 0;
}

D3DVideo::~D3DVideo()
{
	deinit();
	if (dev)
		dev->Release();
	if (g_pD3D)
		g_pD3D->Release();

	DestroyWindow(hWnd);
	UnregisterClass(L"SSNESWindowClass", GetModuleHandle(nullptr));
	Global::hwnd = NULL;
}

#define BLACK D3DCOLOR_XRGB(0, 0, 0)

bool D3DVideo::restore()
{
	deinit();
	try
	{
		needs_restore = true;
		init(video_info);
		needs_restore = false;
	}
	catch (const std::exception &e)
	{
		std::cerr << "[Direct3D]: Restore error: " << e.what() << std::endl;
		needs_restore = true;
	}

	return !needs_restore;
}

int D3DVideo::frame(const void *frame, 
	unsigned width, unsigned height, unsigned pitch,
	const char *msg)
{
	if (needs_restore && !restore())
	{
		std::cerr << "[Direct3D]: Restore failed!" << std::endl;
		return SSNES_ERROR;
	}
		

	if (width != last_width || height != last_height)
	{
		clear_texture();
		last_width = width;
		last_height = height;
		update_coord(width, height);
	}

	dev->Clear(0, 0, D3DCLEAR_TARGET, BLACK,
		1.0f, 0);

	if (SUCCEEDED(dev->BeginScene()))
	{
		if (fPrg && vPrg)
		{
			cgD3D9BindProgram(fPrg);
			cgD3D9BindProgram(vPrg);
		}
		dev->SetTexture(0, tex);
		set_cg_params(width, height, tex_w, tex_h, vp_width, vp_height);
		dev->SetFVF(FVF);
		dev->SetStreamSource(0, vertex_buf, 0, sizeof(Vertex));

		D3DLOCKED_RECT d3dlr;
		if (SUCCEEDED(tex->LockRect(0, &d3dlr, nullptr, D3DLOCK_NOSYSLOCK)))
		{
			for (unsigned y = 0; y < height; y++)
			{
				const uint8_t *in = reinterpret_cast<const uint8_t*>(frame) + y * pitch;
				uint8_t *out = reinterpret_cast<uint8_t*>(d3dlr.pBits) + y * d3dlr.Pitch;
				std::memcpy(out, in, width * pixel_size);
			}

			tex->UnlockRect(0);
		}
		else
		{
			std::cerr << "[Direct3D]: Locking surface failed ..." << std::endl;
			return SSNES_ERROR;
		}

		dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

		if (msg)
		{
			font->DrawTextA(nullptr,
				msg,
				-1,
				&font_rect,
				DT_LEFT,
				video_info.ttf_font_color | 0xff000000);
		}

		dev->EndScene();
	}
	else
		return SSNES_ERROR;

	if (dev->Present(nullptr, nullptr, nullptr, nullptr) != D3D_OK)
	{
		needs_restore = true;
		return SSNES_OK;
	}

	frame_count++;
	update_title();
	return SSNES_OK;
}

void D3DVideo::set_nonblock_state(int state)
{
	video_info.vsync = !state;

	// Very heavy way to set VSync, but hey ;)
	restore();
}

int D3DVideo::alive()
{
	process();
	return !Callback::quit;
}

int D3DVideo::focus() const
{
	return !Callback::quit;
}

void D3DVideo::process()
{
	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

void D3DVideo::clear_texture()
{
	D3DLOCKED_RECT d3dlr;
	if (SUCCEEDED(tex->LockRect(0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK)))
	{
		std::memset(d3dlr.pBits, 0, tex_h * d3dlr.Pitch);
		tex->UnlockRect(0);
	}
}

void D3DVideo::clear()
{
	clear_texture();
	dev->Clear(0, 0, D3DCLEAR_TARGET, BLACK, 1.0f, 0);
	dev->Present(0, 0, 0, 0);
}

bool D3DVideo::init_cg(const char *path)
{
	cgCtx = 0;
	fPrg = 0;
	vPrg = 0;
	frame_count = 0;

	if (!path)
		return true;

	std::cerr << "[Direct3D Cg]: Loading shader: " << path << std::endl;

	cgCtx = cgCreateContext();
	std::cerr << "[Direct3D Cg]: Created context ..." << std::endl; 

	HRESULT ret = cgD3D9SetDevice(dev);
	if (FAILED(ret))
		return false;

	CGprofile fragment_profile = cgD3D9GetLatestPixelProfile();
	CGprofile vertex_profile = cgD3D9GetLatestVertexProfile();
	const char **fragment_opts = cgD3D9GetOptimalOptions(fragment_profile);
	const char **vertex_opts = cgD3D9GetOptimalOptions(vertex_profile);

	fPrg = cgCreateProgramFromFile(cgCtx, CG_SOURCE,
		path, fragment_profile, "main_fragment", fragment_opts);
	vPrg = cgCreateProgramFromFile(cgCtx, CG_SOURCE,
		path, vertex_profile, "main_vertex", vertex_opts);
	if (!fPrg || !vPrg)
	{
		std::cerr << "[Direct3D Cg]: Program compilation failed!" << std::endl;
		return false;
	}

	if (FAILED(cgD3D9LoadProgram(fPrg, true, 0)))
		return false;
	cgD3D9BindProgram(fPrg);

	if (FAILED(cgD3D9LoadProgram(vPrg, true, 0)))
		return false;
	cgD3D9BindProgram(vPrg);
	
	std::cerr << "[Direct3D Cg]: Inited Cg shader!" << std::endl;

	return true;
}

void D3DVideo::set_cg_mvp(const D3DXMATRIX &matrix)
{
	if (!vPrg)
		return;

	D3DXMATRIX tmp;
	D3DXMatrixTranspose(&tmp, &matrix);
	CGparameter cgpModelViewProj = cgGetNamedParameter(vPrg, "modelViewProj");
	if (cgpModelViewProj)
		cgD3D9SetUniformMatrix(cgpModelViewProj, &tmp);
}

void D3DVideo::deinit_cg()
{
	if (cgCtx)
	{
		cgDestroyContext(cgCtx);
		cgD3D9SetDevice(0);
		cgCtx = 0;
	}
}

template <class T>
void D3DVideo::set_cg_param(CGprogram prog, const char *param, const T& val)
{
	CGparameter cgp = cgGetNamedParameter(prog, param);
	if (cgp)
		cgD3D9SetUniform(cgp, &val);
}

void D3DVideo::set_cg_params(unsigned video_w, unsigned video_h,
	unsigned tex_w, unsigned tex_h,
	unsigned viewport_w, unsigned viewport_h)
{
	if (!vPrg || !fPrg)
		return;

	D3DXVECTOR2 video_size;
	video_size.x = video_w;
	video_size.y = video_h;
	D3DXVECTOR2 texture_size;
	texture_size.x = tex_w;
	texture_size.y = tex_h;
	D3DXVECTOR2 output_size;
	output_size.x = viewport_w;
	output_size.y = viewport_h;

	set_cg_param(vPrg, "IN.video_size", video_size);
	set_cg_param(fPrg, "IN.video_size", video_size);
	set_cg_param(vPrg, "IN.texture_size", texture_size);
	set_cg_param(fPrg, "IN.texture_size", texture_size);
	set_cg_param(vPrg, "IN.output_size", output_size);
	set_cg_param(fPrg, "IN.output_size", output_size);
	float frame_cnt = frame_count;
	set_cg_param(fPrg, "IN.frame_count", frame_cnt);
	set_cg_param(vPrg, "IN.frame_count", frame_cnt);
}

bool D3DVideo::init_font()
{
	D3DXFONT_DESC desc = {
		video_info.ttf_font_size, 0, 400, 0,
		false, DEFAULT_CHARSET,
		OUT_TT_PRECIS,
		CLIP_DEFAULT_PRECIS,
		DEFAULT_PITCH,
		L"Verdana" // Hardcode ftl :(
	};

	return SUCCEEDED(D3DXCreateFontIndirect(dev, &desc, &font));
}

void D3DVideo::deinit_font()
{
	if (font)
		font->Release();
}

void D3DVideo::update_title()
{
	frames++;
	if (frames == 180)
	{
		LARGE_INTEGER current;
		QueryPerformanceCounter(&current);
		double time = static_cast<double>(current.QuadPart - last_time.QuadPart) / freq.QuadPart;
		unsigned fps = (180 / time) + 0.5;
		last_time = current;
		
		std::wstring tmp = title;
		wchar_t buf[16];
		std::swprintf(buf, sizeof(buf), L"%u", fps);
		tmp += L" || FPS: ";
		tmp += buf;
		frames = 0;

		SetWindowText(hWnd, tmp.c_str());
	}
}


D3DVideo::RenderPass::RenderPass(IDirect3DDevice9 *dev, unsigned width, unsigned height,
	D3DVideo::RenderPass::ScaleType type)
	: tex(nullptr), tex_w(width), tex_h(height), type(type)
{
	if (FAILED(dev->CreateTexture(width, height,
		1, D3DUSAGE_RENDERTARGET,
		D3DFMT_X8R8G8B8,
		D3DPOOL_DEFAULT,
		&tex, nullptr)))
	{
		throw std::runtime_error("Failed to create render target!\n");
	}
}

D3DVideo::RenderPass::~RenderPass()
{
	if (tex)
		tex->Release();
}

D3DVideo::RenderPass& D3DVideo::RenderPass::operator=(D3DVideo::RenderPass &&in)
{
	if (tex)
		tex->Release();

	tex = in.tex;
	in.tex = nullptr;
	tex_w = in.tex_w;
	tex_h = in.tex_h;
	type = in.type;
	scale[0] = in.scale[0];
	scale[1] = in.scale[1];
	abs_scale[0] = in.abs_scale[0];
	abs_scale[1] = in.abs_scale[1];

	return *this;
}

D3DVideo::RenderPass::RenderPass(D3DVideo::RenderPass &&in)
{
	*this = std::move(in);
}
