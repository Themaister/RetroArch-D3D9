
#include "common.h"

#include <iostream>
#include <stdexcept>

static rarch_video_driver_t video_driver;
static rarch_input_driver_t input_driver;

static void* video_init(const rarch_video_info_t *info, const rarch_input_driver_t **input)
{
   try 
   {
      *input = &input_driver;
      return new D3DVideo(info);
   }
   catch (const std::exception& e)
   {
      std::cerr << "[Direct3D Error]: " << e.what() << std::endl;
      return nullptr;
   }
}

static void video_free(void *data)
{
   delete reinterpret_cast<D3DVideo*>(data);
}

static int video_alive(void *data)
{
   return reinterpret_cast<D3DVideo*>(data)->alive();
}

static int video_focus(void *data)
{
   return reinterpret_cast<D3DVideo*>(data)->focus();
}

static int video_frame(void *data, const void *frame, 
      unsigned width, unsigned height, unsigned pitch,
      const char *msg)
{
   return reinterpret_cast<D3DVideo*>(data)->frame(frame, width, height, pitch, msg);
}

static void video_set_nonblock_state(void *data, int state)
{
   reinterpret_cast<D3DVideo*>(data)->set_nonblock_state(state);
}

static void video_set_rotation(void *data, unsigned rot)
{
   reinterpret_cast<D3DVideo*>(data)->set_rotation(rot);
}

static void video_viewport_size(void *data, unsigned *width, unsigned *height)
{
   reinterpret_cast<D3DVideo*>(data)->viewport_size(*width, *height);
}

static int video_read_viewport(void *data, unsigned char *buffer)
{
   return reinterpret_cast<D3DVideo*>(data)->read_viewport(buffer);
}

static void* input_init(const int joypad_index[5], float axis_thres)
{
   try 
   {
      return new DirectInput(joypad_index, axis_thres);
   }
   catch (const std::exception& e) 
   {
      std::cerr << e.what() << std::endl;
      return nullptr;
   }
}

static void input_free(void *data)
{
   delete reinterpret_cast<DirectInput*>(data);
}

static int input_state(void *data, const struct rarch_keybind *bind, unsigned player)
{
   return reinterpret_cast<DirectInput*>(data)->state(bind, player);
}

static void input_poll(void *data)
{
   reinterpret_cast<DirectInput*>(data)->poll();
}

RARCH_API_EXPORT const rarch_video_driver_t* RARCH_API_CALLTYPE rarch_video_init(void)
{
   video_driver.init = video_init;
   video_driver.free = video_free;
   video_driver.alive = video_alive;
   video_driver.focus = video_focus;
   video_driver.frame = video_frame;
   video_driver.set_nonblock_state = video_set_nonblock_state;
   video_driver.ident = "Direct3D";
   video_driver.set_rotation = video_set_rotation;
   video_driver.viewport_size = video_viewport_size;
   video_driver.read_viewport = video_read_viewport;

   input_driver.init = input_init;
   input_driver.free = input_free;
   input_driver.input_state = input_state;
   input_driver.poll = input_poll;
   input_driver.ident = "DirectInput";
   video_driver.api_version = RARCH_GRAPHICS_API_VERSION;

   return &video_driver;
}

