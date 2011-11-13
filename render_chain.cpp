#include "render_chain.hpp"
#include <utility>

#include <stdexcept>
#include <cstring>
#include <iostream>
#include <cstdio>

namespace Global
{
   static const char *stock_program =
      "void main_vertex(out float4 oPos : POSITION, float4 pos : POSITION,\n"
      "                 out float2 oTex : TEXCOORD0, float2 tex : TEXCOORD0,\n"
      "                 uniform float4x4 modelViewProj)\n"
      "{\n"
      "  oPos = mul(modelViewProj, pos);\n"
      "  oTex = tex;\n"
      "}\n"

      "float4 main_fragment(uniform sampler2D samp, float2 tex : TEXCOORD0) : COLOR\n"
      "{\n"
      "  return tex2D(samp, tex);\n"
      "}";
}

#define FVF (D3DFVF_XYZ | D3DFVF_TEX1)

RenderChain::~RenderChain()
{
   clear();
}

RenderChain::RenderChain(IDirect3DDevice9 *dev_, CGcontext cgCtx_,
      const LinkInfo &info, PixelFormat fmt,
      const D3DVIEWPORT9 &final_viewport_)
      : dev(dev_), cgCtx(cgCtx_), final_viewport(final_viewport_), frame_count(0)
{
   pixel_size = fmt == RGB15 ? 2 : 4;
   create_first_pass(info, fmt);
   init_fvf();
   log_info(info);
}

void RenderChain::clear()
{
   for (unsigned i = 0; i < Textures; i++)
   {
      if (prev.tex[i])
         prev.tex[i]->Release();
      if (prev.vertex_buf[i])
         prev.vertex_buf[i]->Release();
   }

   for (unsigned i = 1; i < passes.size(); i++)
   {
      if (passes[i].tex)
         passes[i].tex->Release();
      if (passes[i].vertex_buf)
         passes[i].vertex_buf->Release();
   }

   passes.clear();
}

void RenderChain::add_pass(const LinkInfo &info)
{
   Pass pass;
   pass.info = info;
   pass.last_width = 0;
   pass.last_height = 0;

   compile_shaders(pass, info.shader_path);

   if (FAILED(dev->CreateVertexBuffer(
               4 * sizeof(Vertex),
               0,
               FVF,
               D3DPOOL_DEFAULT,
               &pass.vertex_buf,
               nullptr)))
   {
      throw std::runtime_error("Failed to create Vertex buf ...");
   }

   if (FAILED(dev->CreateTexture(info.tex_w, info.tex_h, 1,
               D3DUSAGE_RENDERTARGET,
               D3DFMT_X8R8G8B8,
               D3DPOOL_DEFAULT,
               &pass.tex, nullptr)))
   {
      throw std::runtime_error("Failed to create texture ...");
   }

   dev->SetTexture(0, pass.tex);
   dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
   dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
   dev->SetTexture(0, nullptr);

   passes.push_back(pass);

   log_info(info);
}

void RenderChain::start_render()
{
   passes[0].tex = prev.tex[prev.ptr];
   passes[0].vertex_buf = prev.vertex_buf[prev.ptr];
   passes[0].last_width = prev.last_width[prev.ptr];
   passes[0].last_height = prev.last_height[prev.ptr];
}

void RenderChain::end_render()
{
   prev.last_width[prev.ptr] = passes[0].last_width;
   prev.last_height[prev.ptr] = passes[0].last_height;
   prev.ptr = (prev.ptr + 1) & TexturesMask;
}

bool RenderChain::render(const void *data,
      unsigned width, unsigned height, unsigned pitch)
{
   start_render();

   unsigned current_width = width, current_height = height;
   unsigned out_width, out_height;
   convert_geometry(passes[0].info, out_width, out_height,
         current_width, current_height, final_viewport);

   blit_to_texture(data, width, height, pitch);

   // Grab back buffer.
   IDirect3DSurface9 *back_buffer;
   dev->GetRenderTarget(0, &back_buffer);

   // In-between render target passes.
   for (unsigned i = 0; i < passes.size() - 1; i++)
   {

      Pass &from_pass = passes[i];
      Pass &to_pass = passes[i + 1];

      bind_orig(from_pass);
      IDirect3DSurface9 *target;
      to_pass.tex->GetSurfaceLevel(0, &target);
      dev->SetRenderTarget(0, target);

      convert_geometry(from_pass.info,
            out_width, out_height,
            current_width, current_height, final_viewport);

      D3DVIEWPORT9 viewport = {0};
      viewport.X = 0;
      viewport.Y = 0;
      viewport.Width = out_width;
      viewport.Height = out_height;
      viewport.MinZ = 0.0f;
      viewport.MaxZ = 1.0f;
      set_viewport(viewport);

      set_vertices(from_pass,
            current_width, current_height,
            out_width, out_height,
            out_width, out_height);
      render_pass(from_pass);

      current_width = out_width;
      current_height = out_height;
      target->Release();
   }

   // Final pass
   dev->SetRenderTarget(0, back_buffer);
   Pass &last_pass = passes.back();

   convert_geometry(last_pass.info,
         out_width, out_height,
         current_width, current_height, final_viewport);
   set_viewport(final_viewport);
   set_vertices(last_pass,
            current_width, current_height,
            out_width, out_height,
            final_viewport.Width, final_viewport.Height);
   bind_orig(last_pass);
   render_pass(last_pass);

   frame_count++;

   back_buffer->Release();

   end_render();
   return true;
}

void RenderChain::create_first_pass(const LinkInfo &info, PixelFormat fmt)
{
   D3DXMATRIX ident;
   D3DXMatrixIdentity(&ident);
   dev->SetTransform(D3DTS_WORLD, &ident);
   dev->SetTransform(D3DTS_VIEW, &ident);

   Pass pass;
   pass.info = info;
   pass.last_width = 0;
   pass.last_height = 0;

   prev.ptr = 0;
   for (unsigned i = 0; i < Textures; i++)
   {
      prev.last_width[i] = 0;
      prev.last_height[i] = 0;

      if (FAILED(dev->CreateVertexBuffer(
                  4 * sizeof(Vertex),
                  0,
                  FVF,
                  D3DPOOL_DEFAULT,
                  &prev.vertex_buf[i],
                  nullptr)))
      {
         throw std::runtime_error("Failed to create Vertex buf ...");
      }

      if (FAILED(dev->CreateTexture(info.tex_w, info.tex_h, 1, 0,
                  fmt == RGB15 ? D3DFMT_X1R5G5B5 : D3DFMT_X8R8G8B8,
                  D3DPOOL_MANAGED,
                  &prev.tex[i], nullptr)))
      {
         throw std::runtime_error("Failed to create texture ...");
      }

      dev->SetTexture(0, prev.tex[i]);
      dev->SetSamplerState(0, D3DSAMP_MINFILTER,
            info.filter_linear ? D3DTEXF_LINEAR : D3DTEXF_POINT);
      dev->SetSamplerState(0, D3DSAMP_MAGFILTER,
            info.filter_linear ? D3DTEXF_LINEAR : D3DTEXF_POINT);
      dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
      dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
      dev->SetRenderState(D3DRS_LIGHTING, FALSE);
      dev->SetTexture(0, nullptr);
   }

   compile_shaders(pass, info.shader_path);
   passes.push_back(pass);
}

void RenderChain::compile_shaders(Pass &pass, const std::string &shader)
{
   CGprofile fragment_profile = cgD3D9GetLatestPixelProfile();
   CGprofile vertex_profile = cgD3D9GetLatestVertexProfile();
   const char **fragment_opts = cgD3D9GetOptimalOptions(fragment_profile);
   const char **vertex_opts = cgD3D9GetOptimalOptions(vertex_profile);

   if (shader.length() > 0)
   {
      std::cerr << "[Direct3D Cg]: Compiling shader: " << shader << std::endl;
      pass.fPrg = cgCreateProgramFromFile(cgCtx, CG_SOURCE,
            shader.c_str(), fragment_profile, "main_fragment", fragment_opts);

      if (cgGetLastListing(cgCtx))
         std::cerr << "Fragment error: " << std::endl <<
            cgGetLastListing(cgCtx) << std::endl;

      pass.vPrg = cgCreateProgramFromFile(cgCtx, CG_SOURCE,
            shader.c_str(), vertex_profile, "main_vertex", vertex_opts);

      if (cgGetLastListing(cgCtx))
         std::cerr << "Vertex error: " << std::endl <<
            cgGetLastListing(cgCtx) << std::endl;
   }
   else
   {
      std::cerr << "[Direct3D Cg]: Compiling stock shader" << std::endl;

      pass.fPrg = cgCreateProgram(cgCtx, CG_SOURCE, Global::stock_program,
            fragment_profile, "main_fragment", fragment_opts);
      if (cgGetLastListing(cgCtx))
         std::cerr << "Fragment error: " << std::endl <<
            cgGetLastListing(cgCtx) << std::endl;
      pass.vPrg = cgCreateProgram(cgCtx, CG_SOURCE, Global::stock_program,
            vertex_profile, "main_vertex", vertex_opts);
      if (cgGetLastListing(cgCtx))
         std::cerr << "Vertex error: " << std::endl <<
            cgGetLastListing(cgCtx) << std::endl;
   }

   if (!pass.fPrg || !pass.vPrg)
      throw std::runtime_error("Failed to compile shaders!");

   cgD3D9LoadProgram(pass.fPrg, true, 0);
   cgD3D9LoadProgram(pass.vPrg, true, 0);
}

void RenderChain::set_shaders(Pass &pass)
{
   cgD3D9BindProgram(pass.fPrg);
   cgD3D9BindProgram(pass.vPrg);
}

void RenderChain::set_vertices(Pass &pass,
      unsigned width, unsigned height,
      unsigned out_width, unsigned out_height,
      unsigned vp_width, unsigned vp_height)
{
   const LinkInfo &info = pass.info;

   if (pass.last_width != width || pass.last_height != height)
   {
      pass.last_width = width;
      pass.last_height = height;

      float _u = static_cast<float>(width) / info.tex_w;
      float _v = static_cast<float>(height) / info.tex_h;
      Vertex vert[4];
      for (unsigned i = 0; i < 4; i++)
         vert[i].z = 0.5f;

      vert[0].x = 0.0f;
      vert[1].x = out_width - 1.0f;
      vert[2].x = 0.0f;
      vert[3].x = out_width - 1.0f;
      vert[0].y = out_height - 1.0f;
      vert[1].y = out_height - 1.0f;
      vert[2].y = 0.0f;
      vert[3].y = 0.0f;

      vert[0].u = 0.0f;
      vert[1].u = _u;
      vert[2].u = 0.0f;
      vert[3].u = _u;
      vert[0].v = 0;
      vert[1].v = 0;
      vert[2].v = _v;
      vert[3].v = _v;

      // Align texels and vertices.
      for (unsigned i = 0; i < 4; i++)
      {
         vert[i].x -= 0.5f;
         vert[i].y += 0.5f;
      }

      void *verts;
      pass.vertex_buf->Lock(0, sizeof(vert), &verts, 0);
      std::memcpy(verts, vert, sizeof(vert));
      pass.vertex_buf->Unlock();
   }

   D3DXMATRIX proj;
   D3DXMatrixOrthoOffCenterLH(&proj, 0, vp_width - 1, 0, vp_height - 1, 0, 1);
   dev->SetTransform(D3DTS_PROJECTION, &proj);
   set_cg_mvp(pass, proj);

   set_cg_params(pass,
         width, height,
         info.tex_w, info.tex_h,
         vp_width, vp_height);
}

void RenderChain::set_viewport(const D3DVIEWPORT9 &vp)
{
   dev->SetViewport(&vp);
}

void RenderChain::set_cg_mvp(Pass &pass, const D3DXMATRIX &matrix)
{
   D3DXMATRIX tmp;
   D3DXMatrixTranspose(&tmp, &matrix);
   CGparameter cgpModelViewProj = cgGetNamedParameter(pass.vPrg, "modelViewProj");
   if (cgpModelViewProj)
      cgD3D9SetUniformMatrix(cgpModelViewProj, &tmp);
}

template <class T>
static void set_cg_param(CGprogram prog, const char *param,
      const T& val)
{
   CGparameter cgp = cgGetNamedParameter(prog, param);
   if (cgp)
      cgD3D9SetUniform(cgp, &val);
}

void RenderChain::set_cg_params(Pass &pass,
            unsigned video_w, unsigned video_h,
            unsigned tex_w, unsigned tex_h,
            unsigned viewport_w, unsigned viewport_h)
{
   D3DXVECTOR2 video_size;
   video_size.x = video_w;
   video_size.y = video_h;
   D3DXVECTOR2 texture_size;
   texture_size.x = tex_w;
   texture_size.y = tex_h;
   D3DXVECTOR2 output_size;
   output_size.x = viewport_w;
   output_size.y = viewport_h;

   set_cg_param(pass.vPrg, "IN.video_size", video_size);
   set_cg_param(pass.fPrg, "IN.video_size", video_size);
   set_cg_param(pass.vPrg, "IN.texture_size", texture_size);
   set_cg_param(pass.fPrg, "IN.texture_size", texture_size);
   set_cg_param(pass.vPrg, "IN.output_size", output_size);
   set_cg_param(pass.fPrg, "IN.output_size", output_size);

   float frame_cnt = frame_count;
   set_cg_param(pass.fPrg, "IN.frame_count", frame_cnt);
   set_cg_param(pass.vPrg, "IN.frame_count", frame_cnt);
}

void RenderChain::clear_texture(Pass &pass)
{
   D3DLOCKED_RECT d3dlr;
   if (SUCCEEDED(pass.tex->LockRect(0, &d3dlr, NULL, D3DLOCK_NOSYSLOCK)))
   {
      std::memset(d3dlr.pBits, 0, pass.info.tex_h * d3dlr.Pitch);
      pass.tex->UnlockRect(0);
   }
}

void RenderChain::convert_geometry(const LinkInfo &info,
      unsigned &out_width, unsigned &out_height,
      unsigned width, unsigned height,
      const D3DVIEWPORT9 &final_viewport)
{
   switch (info.scale_type_x)
   {
      case LinkInfo::Viewport:
         out_width = info.scale_x * final_viewport.Width;
         break;

      case LinkInfo::Absolute:
         out_width = info.abs_x;
         break;

      case LinkInfo::Relative:
         out_width = info.scale_x * width;
         break;
   }

   switch (info.scale_type_y)
   {
      case LinkInfo::Viewport:
         out_height = info.scale_y * final_viewport.Height;
         break;

      case LinkInfo::Absolute:
         out_height = info.abs_y;
         break;

      case LinkInfo::Relative:
         out_height = info.scale_y * height;
         break;
   }
}

void RenderChain::blit_to_texture(const void *frame,
      unsigned width, unsigned height,
      unsigned pitch)
{
   Pass &first = passes[0];
   if (first.last_width != width || first.last_height != height)
      clear_texture(first);

   D3DLOCKED_RECT d3dlr;
   if (SUCCEEDED(first.tex->LockRect(0, &d3dlr, nullptr, D3DLOCK_NOSYSLOCK)))
   {
      for (unsigned y = 0; y < height; y++)
      {
         const uint8_t *in = reinterpret_cast<const uint8_t*>(frame) + y * pitch;
         uint8_t *out = reinterpret_cast<uint8_t*>(d3dlr.pBits) + y * d3dlr.Pitch;
         std::memcpy(out, in, width * pixel_size);
      }

      first.tex->UnlockRect(0);
   }
}

void RenderChain::render_pass(Pass &pass)
{
   set_shaders(pass);
   dev->SetTexture(0, pass.tex);
   dev->SetSamplerState(0, D3DSAMP_MINFILTER,
         pass.info.filter_linear ? D3DTEXF_LINEAR : D3DTEXF_POINT);
   dev->SetSamplerState(0, D3DSAMP_MAGFILTER,
         pass.info.filter_linear ? D3DTEXF_LINEAR : D3DTEXF_POINT);

   dev->SetStreamSource(0, pass.vertex_buf, 0, sizeof(Vertex));

   bind_prev(pass);

   dev->Clear(0, 0, D3DCLEAR_TARGET, 0, 1, 0);
   if (SUCCEEDED(dev->BeginScene()))
   {
      dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
      dev->EndScene();
   }

   // So we don't render with linear filter into render targets,
   // which apparently looked odd (too blurry).
   dev->SetSamplerState(0, D3DSAMP_MINFILTER,
         D3DTEXF_NONE);
   dev->SetSamplerState(0, D3DSAMP_MAGFILTER,
         D3DTEXF_NONE);

   unbind_prev();
}

void RenderChain::log_info(const LinkInfo &info)
{
   std::cerr << "[Direct3D Cg] Render pass info:" << std::endl;
   std::cerr << "\tTexture width: " << info.tex_w << std::endl;
   std::cerr << "\tTexture height: " << info.tex_h << std::endl;

   std::cerr << "\tScale type (X): ";
   switch (info.scale_type_x)
   {
      case LinkInfo::Relative:
         std::cerr << "Relative @ " << info.scale_x << "x" << std::endl;
         break;

      case LinkInfo::Viewport:
         std::cerr << "Viewport @ " << info.scale_x << "x" << std::endl;
         break;

      case LinkInfo::Absolute:
         std::cerr << "Absolute @ " << info.abs_x << "px" << std::endl;
         break;
   }

   std::cerr << "\tScale type (Y): ";
   switch (info.scale_type_y)
   {
      case LinkInfo::Relative:
         std::cerr << "Relative @ " << info.scale_y << "x" << std::endl;
         break;

      case LinkInfo::Viewport:
         std::cerr << "Viewport @ " << info.scale_y << "x" << std::endl;
         break;

      case LinkInfo::Absolute:
         std::cerr << "Absolute @ " << info.abs_y << "px" << std::endl;
         break;
   }

   std::cerr << "\tBilinear filter: " << std::boolalpha << info.filter_linear << std::endl;
}

void RenderChain::bind_orig(Pass &pass)
{
   D3DXVECTOR2 video_size;
   video_size.x = passes[0].last_width;
   video_size.y = passes[0].last_height;

   D3DXVECTOR2 texture_size;
   texture_size.x = passes[0].info.tex_w;
   texture_size.y = passes[0].info.tex_h;

   set_cg_param(pass.vPrg, "ORIG.video_size", video_size);
   set_cg_param(pass.fPrg, "ORIG.video_size", video_size);
   set_cg_param(pass.vPrg, "ORIG.texture_size", texture_size);
   set_cg_param(pass.fPrg, "ORIG.texture_size", texture_size);

   CGparameter param = cgGetNamedParameter(pass.fPrg, "ORIG.texture");
   if (param)
   {
      unsigned index = cgGetParameterResourceIndex(param);
      dev->SetTexture(index, passes[0].tex);
      dev->SetSamplerState(index, D3DSAMP_MAGFILTER,
            passes[0].info.filter_linear ? D3DTEXF_LINEAR : D3DTEXF_POINT);
      dev->SetSamplerState(index, D3DSAMP_MINFILTER,
            passes[0].info.filter_linear ? D3DTEXF_LINEAR : D3DTEXF_POINT);
   }

   param = cgGetNamedParameter(pass.vPrg, "ORIG.tex_coord");
   if (param)
   {
      unsigned index = cgGetParameterResourceIndex(param);
      dev->SetStreamSource(index, passes[0].vertex_buf, 0, sizeof(Vertex));
   }
}

void RenderChain::bind_prev(Pass &pass)
{
   static const char *prev_names[] = {
      "PREV",
      "PREV1",
      "PREV2",
      "PREV3",
      "PREV4",
      "PREV5",
      "PREV6",
   };

   char attr_texture[64];
   char attr_input_size[64];
   char attr_tex_size[64];
   char attr_coord[64];

   D3DXVECTOR2 texture_size;
   texture_size.x = passes[0].info.tex_w;
   texture_size.y = passes[0].info.tex_h;

   for (unsigned i = 0; i < Textures - 1; i++)
   {
      std::snprintf(attr_texture,    sizeof(attr_texture),    "%s.texture",      prev_names[i]);
      std::snprintf(attr_input_size, sizeof(attr_input_size), "%s.video_size",   prev_names[i]);
      std::snprintf(attr_tex_size,   sizeof(attr_tex_size),   "%s.texture_size", prev_names[i]);
      std::snprintf(attr_coord,      sizeof(attr_coord),      "%s.tex_coord",    prev_names[i]);

      D3DXVECTOR2 video_size;
      video_size.x = prev.last_width[(prev.ptr - (i + 1)) & TexturesMask];
      video_size.y = prev.last_height[(prev.ptr - (i + 1)) & TexturesMask];

      set_cg_param(pass.vPrg, attr_input_size, video_size);
      set_cg_param(pass.fPrg, attr_input_size, video_size);
      set_cg_param(pass.vPrg, attr_tex_size, texture_size);
      set_cg_param(pass.fPrg, attr_tex_size, texture_size);

      CGparameter param = cgGetNamedParameter(pass.fPrg, attr_texture);
      if (param)
      {
         unsigned index = cgGetParameterResourceIndex(param);

         IDirect3DTexture9 *tex = prev.tex[(prev.ptr - (i + 1)) & TexturesMask];
         dev->SetTexture(index, tex);
         bound_prev.push_back(index);

         dev->SetSamplerState(index, D3DSAMP_MAGFILTER,
               passes[0].info.filter_linear ? D3DTEXF_LINEAR : D3DTEXF_POINT);
         dev->SetSamplerState(index, D3DSAMP_MINFILTER,
               passes[0].info.filter_linear ? D3DTEXF_LINEAR : D3DTEXF_POINT);
      }

      param = cgGetNamedParameter(pass.vPrg, attr_coord);
      if (param)
      {
         unsigned index = cgGetParameterResourceIndex(param);
         IDirect3DVertexBuffer9 *vert_buf = prev.vertex_buf[(prev.ptr - (i + 1)) & TexturesMask];
         bound_prev_vert.push_back(index);

         dev->SetStreamSource(index, vert_buf, 0, sizeof(Vertex));
      }
   }
}

void RenderChain::unbind_prev()
{
   for (unsigned i = 0; i < bound_prev.size(); i++)
      dev->SetTexture(bound_prev[i], nullptr);
   bound_prev.clear();

   for (unsigned i = 0; i < bound_prev_vert.size(); i++)
      dev->SetStreamSource(bound_prev_vert[i], nullptr, 0, 0);
   bound_prev_vert.clear();
}

#define DECL_FVF(index) \
   { index, 3 * sizeof(float), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, \
      D3DDECLUSAGE_TEXCOORD, index },

void RenderChain::init_fvf()
{
   const D3DVERTEXELEMENT9 fvfs[] = 
   {
      { 0, 0 * sizeof(float), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,
         D3DDECLUSAGE_POSITION, 0 },
      DECL_FVF(0)
      DECL_FVF(1)
      DECL_FVF(2)
      DECL_FVF(3)
      DECL_FVF(4)
      DECL_FVF(5)
      DECL_FVF(6)
      DECL_FVF(7)
      DECL_FVF(8)
      DECL_FVF(9)
      DECL_FVF(10)
      DECL_FVF(11)
      DECL_FVF(12)
      DECL_FVF(13)
      DECL_FVF(14)
      DECL_FVF(15)

      D3DDECL_END()
   };

   IDirect3DVertexDeclaration9 *decl;
   if (FAILED(dev->CreateVertexDeclaration(fvfs, &decl)))
      throw std::runtime_error("Failed to set up FVF!");

   if (FAILED(dev->SetVertexDeclaration(decl)))
      throw std::runtime_error("Failed to set FVF!");
   decl->Release();
}

