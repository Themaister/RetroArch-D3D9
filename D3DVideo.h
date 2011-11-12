#pragma once

#include "common.h"

#include <d3d9.h>
#include <d3dx9.h>
#include <d3dx9core.h>
#include <Cg/cg.h>
#include <Cg/cgD3D9.h>
#include <string>
#include <vector>

#define FVF (D3DFVF_XYZ | D3DFVF_TEX1)

class D3DVideo
{
   public:
      D3DVideo(const ssnes_video_info_t* info);
      int frame(const void* frame, 
            unsigned width, unsigned height, unsigned pitch,
            const char *msg);
      ~D3DVideo();

      int alive();
      int focus() const;
      void set_nonblock_state(int state);

      static HWND hwnd();

   private:

      struct Vertex
      {
         float x, y, z;
         float u, v;
      };

      WNDCLASSEX windowClass;
      HWND hWnd;
      IDirect3D9 *g_pD3D;
      IDirect3DDevice9 *dev;
      IDirect3DVertexBuffer9 *vertex_buf;
      IDirect3DTexture9 *tex;
      LPD3DXFONT font;

      unsigned pixel_size;
      unsigned tex_w;
      unsigned tex_h;
      unsigned last_width;
      unsigned last_height;

      void calculate_rect(unsigned width, unsigned height, bool keep, float aspect);
      void set_viewport(unsigned x, unsigned y, unsigned width, unsigned height);
      unsigned screen_width;
      unsigned screen_height;
      float screen_left;
      float screen_right;
      float screen_top;
      float screen_bottom;
      unsigned vp_width;
      unsigned vp_height;

      void process();

      void init(const ssnes_video_info_t &info);
      void init_base(const ssnes_video_info_t &info);
      void make_d3dpp(const ssnes_video_info_t &info, D3DPRESENT_PARAMETERS &d3dpp);
      void deinit();
      ssnes_video_info_t video_info;

      void update_coord(unsigned width, unsigned height);

      bool nonblock;
      bool needs_restore;
      bool dwm_enabled;
      void clear_texture();
      bool restore();

      unsigned frame_count;
      CGcontext cgCtx;
      CGprogram fPrg, vPrg;
      bool init_cg(const char *path);
      void set_cg_params(unsigned video_w, unsigned video_h,
            unsigned tex_w, unsigned tex_h,
            unsigned viewport_w, unsigned viewport_h);
      void set_cg_mvp(const D3DXMATRIX &matrix);

      template <class T>
         void set_cg_param(CGprogram prog, const char *name, const T& vals);

      void deinit_cg();

      bool init_font();
      void deinit_font();
      RECT font_rect;

      void update_title();
      std::wstring title;
      unsigned frames;
      LARGE_INTEGER last_time;
      LARGE_INTEGER freq;


      struct RenderPass
      {
         enum ScaleType { Scale, Viewport, Absolute } type;

         RenderPass(IDirect3DDevice9 *dev, unsigned tex_w, unsigned tex_h, ScaleType type);
         ~RenderPass();
         RenderPass(RenderPass &&in);
         RenderPass& operator=(RenderPass &&in);

         float scale[2];
         unsigned abs_scale[2];

         IDirect3DTexture9 *tex;
         unsigned tex_w;
         unsigned tex_h;

         private:
            RenderPass(const RenderPass&);
            void operator=(const RenderPass&);
      };

      std::vector<RenderPass> passes;
};


