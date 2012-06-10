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
      D3DVideo(const rarch_video_info_t* info);
      int frame(const void* frame, 
            unsigned width, unsigned height, unsigned pitch,
            const char *msg);
      ~D3DVideo();

      int alive();
      int focus() const;
      void set_nonblock_state(int state);
      void set_rotation(unsigned rot);
      void viewport_size(unsigned &width, unsigned &height);
      bool read_viewport(uint8_t *buffer);

      static HWND hwnd();

   private:

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

      void init(const rarch_video_info_t &info);
      void init_base(const rarch_video_info_t &info);
      void make_d3dpp(const rarch_video_info_t &info, D3DPRESENT_PARAMETERS &d3dpp);
      void deinit();
      rarch_video_info_t video_info;

      bool needs_restore;
      bool restore();

      CGcontext cgCtx;
      bool init_cg();
      void deinit_cg();

      void init_imports(ConfigFile &conf, const std::string &basedir);
      void init_luts(ConfigFile &conf, const std::string &basedir);
      void init_chain_singlepass(const rarch_video_info_t &video_info);
      void init_chain_multipass(const rarch_video_info_t &video_info);
      bool init_chain(const rarch_video_info_t &video_info);
      std::unique_ptr<RenderChain> chain;
      void deinit_chain();

      bool init_font();
      void deinit_font();
      RECT font_rect;
      RECT font_rect_shifted;

      void update_title();
      std::wstring title;
      unsigned frames;
      LARGE_INTEGER last_time;
      LARGE_INTEGER freq;
};

#endif

