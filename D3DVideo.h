#ifndef D3DVIDEO_HPP__
#define D3DVIDEO_HPP__

#include "common.h"

#include <d3d9.h>
#include <d3dx9.h>
#include <d3dx9core.h>
#include <Cg/cg.h>
#include <Cg/cgD3D9.h>
#include <string>
#include <vector>
#include <memory>

class ConfigFile;
class RenderChain;

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

      struct
      {
         bool active;
         HMODULE lib;
         HRESULT (*WINAPI dwm_flush)();
      } dwm;
      void init_dwm();
      void deinit_dwm();

      WNDCLASSEX windowClass;
      HWND hWnd;
      IDirect3D9 *g_pD3D;
      IDirect3DDevice9 *dev;
      LPD3DXFONT font;

      void calculate_rect(unsigned width, unsigned height, bool keep, float aspect);
      void set_viewport(unsigned x, unsigned y, unsigned width, unsigned height);
      unsigned screen_width;
      unsigned screen_height;
      D3DVIEWPORT9 final_viewport;

      void process();

      void init(const ssnes_video_info_t &info);
      void init_base(const ssnes_video_info_t &info);
      void make_d3dpp(const ssnes_video_info_t &info, D3DPRESENT_PARAMETERS &d3dpp);
      void deinit();
      ssnes_video_info_t video_info;

      bool needs_restore;
      bool dwm_enabled;
      bool restore();

      CGcontext cgCtx;
      bool init_cg();
      void deinit_cg();

      void init_imports(ConfigFile &conf, const std::string &basedir);
      void init_luts(ConfigFile &conf, const std::string &basedir);
      void init_chain_singlepass(const ssnes_video_info_t &video_info);
      void init_chain_multipass(const ssnes_video_info_t &video_info);
      bool init_chain(const ssnes_video_info_t &video_info);
      std::unique_ptr<RenderChain> chain;
      void deinit_chain();

      bool init_font();
      void deinit_font();
      RECT font_rect;

      void update_title();
      std::wstring title;
      unsigned frames;
      LARGE_INTEGER last_time;
      LARGE_INTEGER freq;
};

#endif

