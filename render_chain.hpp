#ifndef RENDER_CHAIN_HPP__
#define RENDER_CHAIN_HPP__

#include "common.h"
#include "D3DVideo.h"
#include <map>
#include <utility>
#include "state_tracker.hpp"
#include <memory>

struct Vertex
{
   float x, y, z;
   float u, v;
   float lut_u, lut_v;
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

      RenderChain(const rarch_video_info_t &video_info,
            IDirect3DDevice9 *dev,
            CGcontext cgCtx,
            const LinkInfo &info,
            PixelFormat fmt,
            const D3DVIEWPORT9 &final_viewport);

      void add_pass(const LinkInfo &info);
      void add_lut(const std::string &id, const std::string &path, bool smooth);
      void add_state_tracker(const std::string &program,
            const std::string &py_class,
            const std::vector<std::string> &uniforms);

      bool render(const void *data,
            unsigned width, unsigned height, unsigned pitch, unsigned rotation);

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

      const rarch_video_info_t &video_info;

      std::unique_ptr<StateTracker> tracker;

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

         IDirect3DVertexDeclaration9 *vertex_decl;
         std::vector<unsigned> attrib_map;
      };
      std::vector<Pass> passes;

      struct lut_info
      {
         IDirect3DTexture9 *tex;
         std::string id;
         bool smooth;
      };
      std::vector<lut_info> luts;

      D3DVIEWPORT9 final_viewport;
      unsigned frame_count;

      void create_first_pass(const LinkInfo &info, PixelFormat fmt);
      void compile_shaders(Pass &pass, const std::string &shader);

      void set_vertices(Pass &pass,
            unsigned width, unsigned height,
            unsigned out_width, unsigned out_height,
            unsigned vp_width, unsigned vp_height,
            unsigned rotation);
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

      void render_pass(Pass &pass, unsigned pass_index);
      void log_info(const LinkInfo &info);

      void start_render();
      void end_render();

      std::vector<unsigned> bound_tex;
      std::vector<unsigned> bound_vert;
      void bind_luts(Pass &pass);
      void bind_orig(Pass &pass);
      void bind_prev(Pass &pass);
      void bind_pass(Pass &pass, unsigned pass_index);
      void bind_tracker(Pass &pass);
      void unbind_all();

      void init_fvf(Pass &pass);
};

#endif

