#ifndef RENDER_CHAIN_HPP__
#define RENDER_CHAIN_HPP__

#include "common.h"
#include "D3DVideo.h"

struct Vertex
{
   float x, y, z;
   float u, v;
};

struct LinkInfo
{
   enum ScaleType { Relative, Absolute, Viewport };

   unsigned tex_w, tex_h;
   
   float scale_x, scale_y;
   unsigned abs_x, abs_y;
   bool filter_linear;
   ScaleType scale_type_x, scale_type_y;

   std::string shader_path;
};

class RenderChain
{
   public:
      enum PixelFormat { RGB15, ARGB };

      RenderChain(IDirect3DDevice9 *dev, CGcontext cgCtx, const LinkInfo &info, PixelFormat fmt,
            const D3DVIEWPORT9 &final_viewport);

      void add_pass(const LinkInfo &info);

      bool render(const void *data,
            unsigned width, unsigned height, unsigned pitch);

      static void convert_geometry(const LinkInfo &info,
            unsigned &out_width, unsigned &out_height,
            unsigned width, unsigned height,
	    const D3DVIEWPORT9 &final_viewport);

      void clear();
      ~RenderChain();
   private:
      IDirect3DDevice9 *dev;
      CGcontext cgCtx;
      unsigned pixel_size;

      enum { Textures = 8, TexturesMask = Textures - 1 };
      struct
      {
         IDirect3DTexture9 *tex[Textures];
         IDirect3DVertexBuffer9 *vertex_buf[Textures];
         unsigned ptr;
         unsigned last_width[Textures];
         unsigned last_height[Textures];
      } prev;

      struct Pass
      {
         LinkInfo info;
         IDirect3DTexture9 *tex;
         IDirect3DVertexBuffer9 *vertex_buf;
         CGprogram vPrg, fPrg;
         unsigned last_width, last_height;
      };
      std::vector<Pass> passes;

      D3DVIEWPORT9 final_viewport;
      unsigned frame_count;

      void create_first_pass(const LinkInfo &info, PixelFormat fmt);
      void compile_shaders(Pass &pass, const std::string &shader);

      void set_vertices(Pass &pass,
            unsigned width, unsigned height,
            unsigned out_width, unsigned out_height,
            unsigned vp_width, unsigned vp_height);
      void set_viewport(const D3DVIEWPORT9 &vp);

      void set_shaders(Pass &pass);
      void set_cg_mvp(Pass &pass, const D3DXMATRIX &matrix);
      void set_cg_params(Pass &pass,
            unsigned input_w, unsigned input_h,
            unsigned tex_w, unsigned tex_h,
            unsigned vp_w, unsigned vp_h);

      void clear_texture(Pass &pass);

      void blit_to_texture(const void *data,
            unsigned width, unsigned height,
            unsigned pitch);

      void render_pass(Pass &pass);
      void log_info(const LinkInfo &info);

      void start_render();
      void end_render();

      std::vector<unsigned> bound_prev;
      std::vector<unsigned> bound_prev_vert;
      void bind_orig(Pass &pass);
      void bind_prev(Pass &pass);
      void unbind_prev();

      void init_fvf();
};

#endif

