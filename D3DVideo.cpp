
#include "D3DVideo.h"
#include "render_chain.hpp"
#include "config_file.hpp"

#include <iostream>
#include <exception>
#include <stdexcept>
#include <cstring>
#include <stdio.h>
#include <stdint.h>
#include <iostream>
#include <cmath>

namespace Callback
{
   static bool quit = false;

   LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, 
         WPARAM wParam, LPARAM lParam)
   {
      switch (message)
      {
         case WM_SYSKEYDOWN:
            switch (wParam)
            {
               case VK_F10:
               case VK_RSHIFT:
                  return 0;
            }
            break;

         case WM_DESTROY:
            quit = true;
            return 0;

         default:
            return DefWindowProc(hWnd, message, wParam, lParam);
      }
      return DefWindowProc(hWnd, message, wParam, lParam);
   }
}

namespace Global
{
   static HWND hwnd = nullptr;
}

void D3DVideo::init_base(const rarch_video_info_t &info)
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

void D3DVideo::make_d3dpp(const rarch_video_info_t &info, D3DPRESENT_PARAMETERS &d3dpp)
{
   ZeroMemory(&d3dpp, sizeof(d3dpp));

   d3dpp.Windowed = true;

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

void D3DVideo::init(const rarch_video_info_t &info)
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

   calculate_rect(screen_width, screen_height, info.force_aspect, info.aspect_ratio);

   if (!init_cg())
      throw std::runtime_error("Failed to init Cg");
   if (!init_chain(info))
      throw std::runtime_error("Failed to init render chain");
   if (!init_font())
      throw std::runtime_error("Failed to init Font");
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

   font_rect.left = x + width * 0.08;
   font_rect.right = x + width;
   font_rect.top = y + 0.85 * height; 
   font_rect.bottom = height;

   font_rect_shifted = font_rect;
   font_rect_shifted.left -= 2;
   font_rect_shifted.right -= 2;
   font_rect_shifted.top += 2;
   font_rect_shifted.bottom += 2;

   final_viewport = viewport;
}

void D3DVideo::set_rotation(unsigned rot)
{
   rotation = rot;
}

void D3DVideo::viewport_size(unsigned &width, unsigned &height)
{
   width  = final_viewport.Width;
   height = final_viewport.Height;
}

bool D3DVideo::read_viewport(uint8_t *buffer)
{
   bool ret = true;
   IDirect3DSurface9 *target = nullptr;
   IDirect3DSurface9 *dest   = nullptr;

   if (FAILED(dev->GetRenderTarget(0, &target)))
   {
      ret = false;
      goto end;
   }

   if (FAILED(dev->CreateOffscreenPlainSurface(screen_width, screen_height,
        D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM,
        &dest, nullptr)))
   {
      ret = false;
      goto end;
   }

   if (FAILED(dev->GetRenderTargetData(target, dest)))
   {
      ret = false;
      goto end;
   }

   D3DLOCKED_RECT rect;
   if (SUCCEEDED(dest->LockRect(&rect, nullptr, D3DLOCK_READONLY)))
   {
      unsigned pitchpix = rect.Pitch / 4;
      const uint32_t *pixels = (const uint32_t*)rect.pBits;
      pixels += final_viewport.X;
      pixels += (final_viewport.Height - 1) * pitchpix;
      pixels -= final_viewport.Y * pitchpix;

      for (unsigned y = 0; y < final_viewport.Height; y++, pixels -= pitchpix)
      {
         for (unsigned x = 0; x < final_viewport.Width; x++)
         {
            *buffer++ = (pixels[x] >>  0) & 0xff;
            *buffer++ = (pixels[x] >>  8) & 0xff;
            *buffer++ = (pixels[x] >> 16) & 0xff;
         }
      }

      dest->UnlockRect();
   }
   else
   {
      ret = false;
      goto end;
   }

end:
   if (target)
      target->Release();
   if (dest)
      dest->Release();
   return ret;
}

void D3DVideo::calculate_rect(unsigned width, unsigned height,
   bool keep, float desired_aspect)
{
   if (!keep)
      set_viewport(0, 0, width, height);
   else
   {
      float device_aspect = static_cast<float>(width) / static_cast<float>(height);
      if (fabs(device_aspect - desired_aspect) < 0.001)
         set_viewport(0, 0, width, height);
      else if (device_aspect > desired_aspect)
      {
         float delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5;
         set_viewport(width * (0.5 - delta), 0, 2.0 * width * delta, height);
      }
      else
      {
         float delta = (device_aspect / desired_aspect - 1.0) / 2.0 + 0.5;
         set_viewport(0, height * (0.5 - delta), width, 2.0 * height * delta);
      }
   }
}

D3DVideo::D3DVideo(const rarch_video_info_t *info) : 
   g_pD3D(nullptr), dev(nullptr), rotation(0), needs_restore(false), frames(0)
{
   ZeroMemory(&windowClass, sizeof(windowClass));
   windowClass.cbSize = sizeof(windowClass);
   windowClass.style = CS_HREDRAW | CS_VREDRAW;
   windowClass.lpfnWndProc = Callback::WindowProc;
   windowClass.hInstance = nullptr;
   windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
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

   hWnd = CreateWindowEx(0, L"SSNESWindowClass", title.c_str(), 
         info->fullscreen ?
         (WS_EX_TOPMOST | WS_POPUP) : WS_OVERLAPPEDWINDOW & ~(WS_MAXIMIZEBOX | WS_THICKFRAME), 
         info->fullscreen ? 0 : CW_USEDEFAULT, info->fullscreen ? 0 : CW_USEDEFAULT, win_width, win_height,
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
   deinit_chain();
   deinit_cg();

   needs_restore = false;
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
   Global::hwnd = nullptr;
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
      return RARCH_ERROR;
   }

   if (!chain->render(frame, width, height, pitch, rotation))
      return RARCH_FALSE;

   if (msg && SUCCEEDED(dev->BeginScene()))
   {
      font->DrawTextA(nullptr,
            msg,
            -1,
            &font_rect_shifted,
            DT_LEFT,
            ((video_info.ttf_font_color >> 2) & 0x3f3f3f) | 0xff000000);

      font->DrawTextA(nullptr,
            msg,
            -1,
            &font_rect,
            DT_LEFT,
            video_info.ttf_font_color | 0xff000000);

      dev->EndScene();
   }

   if (dev->Present(nullptr, nullptr, nullptr, nullptr) != D3D_OK)
   {
      needs_restore = true;
      return RARCH_OK;
   }

   update_title();

   return RARCH_OK;
}

void D3DVideo::set_nonblock_state(int state)
{
   video_info.vsync = !state;
   restore();
}

int D3DVideo::alive()
{
   process();
   return !Callback::quit;
}

int D3DVideo::focus() const
{
   return GetFocus() == hwnd();
}

void D3DVideo::process()
{
   MSG msg;
   while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
   {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
   }
}

bool D3DVideo::init_cg()
{
   cgCtx = cgCreateContext();
   if (cgCtx == nullptr)
      return false;

   std::cerr << "[Direct3D Cg]: Created context ..." << std::endl; 

   HRESULT ret = cgD3D9SetDevice(dev);
   if (FAILED(ret))
      return false;

   return true;
}

void D3DVideo::deinit_cg()
{
   if (cgCtx)
   {
      cgD3D9UnloadAllPrograms();
      cgD3D9SetDevice(nullptr);
      cgDestroyContext(cgCtx);
      cgCtx = nullptr;
   }
}

void D3DVideo::init_chain_singlepass(const rarch_video_info_t &video_info)
{
   LinkInfo info = {0};
   info.shader_path = video_info.cg_shader ? video_info.cg_shader : "";
   info.scale_x = info.scale_y = 1.0f;
   info.filter_linear = video_info.smooth;
   info.tex_w = info.tex_h = 256 * video_info.input_scale;
   info.scale_type_x = info.scale_type_y = LinkInfo::Viewport;

   chain = std::unique_ptr<RenderChain>(new RenderChain(
               video_info,
               dev, cgCtx,
               info,
               video_info.color_format == RARCH_COLOR_FORMAT_XRGB1555 ?
               RenderChain::RGB15 : RenderChain::ARGB,
               final_viewport));
}

static inline uint32_t next_pot(uint32_t v)
{
   v--;
   v |= v >> 1;
   v |= v >> 2;
   v |= v >> 4;
   v |= v >> 8;
   v |= v >> 16;
   v++;
   return v;
}

static std::vector<std::string> tokenize(const std::string &str)
{
   std::vector<std::string> list;
   char *elem = strdup(str.c_str());
   const char *tex = strtok(elem, ";");
   while (tex)
   {
      list.push_back(tex);
      tex = strtok(nullptr, ";");
   }
   free(elem);

   return list;
}

void D3DVideo::init_imports(ConfigFile &conf, const std::string &basedir)
{
   std::string imports;
   if (!conf.get("imports", imports))
      return;

   std::vector<std::string> list = tokenize(imports);

   std::string path;
   if (!conf.get("import_script", path))
      throw std::runtime_error("Didn't find import_script!");

   std::string py_class;
   if (!conf.get("import_script_class", py_class))
      throw std::runtime_error("Didn't find import_script_class!");

   chain->add_state_tracker(basedir + path, py_class, list);
}

void D3DVideo::init_luts(ConfigFile &conf, const std::string &basedir)
{
   std::string textures;
   if (!conf.get("textures", textures))
      return;

   std::vector<std::string> list = tokenize(textures);

   for (unsigned i = 0; i < list.size(); i++)
   {
      const std::string &id = list[i];

      bool smooth = true;
      conf.get(id + "_filter", smooth);

      std::string path;
      if (!conf.get(id, path))
         throw std::runtime_error("Failed to get LUT texture path!");

      chain->add_lut(id, basedir + path, smooth);
   }
}

void D3DVideo::init_chain_multipass(const rarch_video_info_t &info)
{
   ConfigFile conf(info.cg_shader);

   int shaders;
   if (!conf.get("shaders", shaders))
      throw std::runtime_error("Couldn't find \"shaders\" in meta-shader");

   if (shaders < 1)
      throw std::runtime_error("Must have at least one shader!");

   std::cerr << "[Direct3D Meta-Cg] Found " << shaders << " shaders!" << std::endl;

   std::string basedir = info.cg_shader;
   size_t pos = basedir.rfind('/');
   if (pos == std::string::npos)
      pos = basedir.rfind('\\');
   if (pos != std::string::npos)
      basedir.replace(basedir.begin() + pos + 1, basedir.end(), "");

   bool use_extra_pass = false;
   bool use_first_pass_only = false;

   std::vector<std::string> shader_paths;
   std::vector<LinkInfo::ScaleType> scale_types_x;
   std::vector<LinkInfo::ScaleType> scale_types_y;
   std::vector<float> scales_x;
   std::vector<float> scales_y;
   std::vector<unsigned> abses_x;
   std::vector<unsigned> abses_y;
   std::vector<bool> filters;

   // Shader paths.
   for (int i = 0; i < shaders; i++)
   {
      char buf[256];
      snprintf(buf, sizeof(buf), "shader%d", i);

      std::string relpath;
      if (!conf.get(buf, relpath))
         throw std::runtime_error("Couldn't locate shader path in meta-shader");

      shader_paths.push_back(basedir);
      shader_paths.back() += relpath;
   }

   // Dimensions.
   for (int i = 0; i < shaders; i++)
   {
      char attr_type[64];
      char attr_type_x[64];
      char attr_type_y[64];
      char attr_scale[64];
      char attr_scale_x[64];
      char attr_scale_y[64];
      int abs_x = 256 * info.input_scale;
      int abs_y = 256 * info.input_scale;
      double scale_x = 1.0f;
      double scale_y = 1.0f;

      std::string attr   = "source";
      std::string attr_x = "source";
      std::string attr_y = "source";
      snprintf(attr_type,    sizeof(attr_type),    "scale_type%d", i);
      snprintf(attr_type_x,  sizeof(attr_type_x),  "scale_type_x%d", i);
      snprintf(attr_type_y,  sizeof(attr_type_x),  "scale_type_y%d", i);
      snprintf(attr_scale,    sizeof(attr_scale),   "scale%d", i);
      snprintf(attr_scale_x, sizeof(attr_scale_x), "scale_x%d", i);
      snprintf(attr_scale_y, sizeof(attr_scale_y), "scale_y%d", i);

      bool has_scale = false;

      if (conf.get(attr_type, attr))
      {
         attr_x = attr_y = attr;
         has_scale = true;
      }
      else
      {
         if (conf.get(attr_type_x, attr))
            has_scale = true;
         if (conf.get(attr_type_y, attr))
            has_scale = true;
      }

      if (attr_x == "source")
         scale_types_x.push_back(LinkInfo::Relative);
      else if (attr_x == "viewport")
         scale_types_x.push_back(LinkInfo::Viewport);
      else if (attr_x == "absolute")
         scale_types_x.push_back(LinkInfo::Absolute);
      else
         throw std::runtime_error("Invalid scale_type_x!");

      if (attr_y == "source")
         scale_types_y.push_back(LinkInfo::Relative);
      else if (attr_y == "viewport")
         scale_types_y.push_back(LinkInfo::Viewport);
      else if (attr_y == "absolute")
         scale_types_y.push_back(LinkInfo::Absolute);
      else
         throw std::runtime_error("Invalid scale_type_y!");
      
      double scale;
      if (conf.get(attr_scale, scale))
         scale_x = scale_y = scale;
      else
      {
         conf.get(attr_scale_x, scale_x);
         conf.get(attr_scale_y, scale_y);
      }

      int absolute;
      if (conf.get(attr_scale, absolute))
         abs_x = abs_y = absolute;
      else
      {
         conf.get(attr_scale_x, abs_x);
         conf.get(attr_scale_y, abs_y);
      }

      scales_x.push_back(scale_x);
      scales_y.push_back(scale_y);
      abses_x.push_back(abs_x);
      abses_y.push_back(abs_y);

      if (has_scale && i == shaders - 1)
         use_extra_pass = true;
      else if (!has_scale && i == 0)
         use_first_pass_only = true;
      else if (i > 0)
         use_first_pass_only = false;
   }

   // Filter options.
   for (int i = 0; i < shaders; i++)
   {
      char attr_filter[64];
      snprintf(attr_filter, sizeof(attr_filter), "filter_linear%d", i);
      bool filter = info.smooth;
      conf.get(attr_filter, filter);
      filters.push_back(filter);
   }

   // Setup information for first pass.
   LinkInfo link_info = {0};
   link_info.shader_path = shader_paths[0];

   if (use_first_pass_only)
   {
      link_info.scale_x = link_info.scale_y = 1.0f;
      link_info.scale_type_x = link_info.scale_type_y = LinkInfo::Viewport;
   }
   else
   {
      link_info.scale_x = scales_x[0];
      link_info.scale_y = scales_y[0];
      link_info.abs_x = abses_x[0];
      link_info.abs_y = abses_y[0];
      link_info.scale_type_x = scale_types_x[0];
      link_info.scale_type_y = scale_types_y[0];
   }

   link_info.filter_linear = filters[0];
   link_info.tex_w = link_info.tex_h = info.input_scale * 256;

   chain = std::unique_ptr<RenderChain>(
         new RenderChain(
            video_info,
            dev, cgCtx,
            link_info,
            info.color_format == RARCH_COLOR_FORMAT_XRGB1555 ?
            RenderChain::RGB15 : RenderChain::ARGB,
            final_viewport));

   unsigned current_width = link_info.tex_w;
   unsigned current_height = link_info.tex_h;
   unsigned out_width = 0;
   unsigned out_height = 0;

   for (int i = 1; i < shaders; i++)
   {
      RenderChain::convert_geometry(link_info,
            out_width, out_height,
            current_width, current_height, final_viewport);

      link_info.scale_x = scales_x[i];
      link_info.scale_y = scales_y[i];
      link_info.tex_w = next_pot(out_width);
      link_info.tex_h = next_pot(out_height);
      link_info.scale_type_x = scale_types_x[i];
      link_info.scale_type_y = scale_types_y[i];
      link_info.filter_linear = filters[i];
      link_info.shader_path = shader_paths[i];

      current_width = out_width;
      current_height = out_height;

      if (i == shaders - 1 && !use_extra_pass)
      {
         link_info.scale_x = link_info.scale_y = 1.0f;
         link_info.scale_type_x = link_info.scale_type_y = LinkInfo::Viewport;
      }

      chain->add_pass(link_info);
   }

   if (use_extra_pass)
   {
      RenderChain::convert_geometry(link_info,
            out_width, out_height,
            current_width, current_height, final_viewport);

      link_info.scale_x = link_info.scale_y = 1.0f;
      link_info.scale_type_x = link_info.scale_type_y = LinkInfo::Viewport;
      link_info.filter_linear = info.smooth;
      link_info.tex_w = next_pot(out_width);
      link_info.tex_h = next_pot(out_height);
      link_info.shader_path = "";
      chain->add_pass(link_info);
   }

   init_luts(conf, basedir);
   init_imports(conf, basedir);
}

bool D3DVideo::init_chain(const rarch_video_info_t &video_info)
{
   try
   {
      if (video_info.cg_shader && std::strstr(video_info.cg_shader, ".cgp"))
         init_chain_multipass(video_info);
      else
         init_chain_singlepass(video_info);
   }
   catch (const std::exception &e)
   {
      std::cerr << "[Direct3D]: Render chain error: " << e.what() << std::endl;
      return false;
   }

   return true;
}

void D3DVideo::deinit_chain()
{
   chain.reset();
}

bool D3DVideo::init_font()
{
   D3DXFONT_DESC desc = {
      static_cast<int>(video_info.ttf_font_size), 0, 400, 0,
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
   if (frames >= 180)
   {
      LARGE_INTEGER current;
      QueryPerformanceCounter(&current);
      double time = static_cast<double>(current.QuadPart - last_time.QuadPart) / freq.QuadPart;
      unsigned fps = (180 / time) + 0.5;
      last_time = current;

      std::wstring tmp = title;
      wchar_t buf[64];
      snwprintf(buf, 64, L"%u", fps);
      tmp += L" || FPS: ";
      tmp += buf;
      frames = 0;

      SetWindowText(hWnd, tmp.c_str());
   }
}

