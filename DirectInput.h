#pragma once

#include "common.h"
#include <stdint.h>
#include <dinput.h>
#include <vector>

class DirectInput
{
   public:
      DirectInput(const int joypad_index[8], float axis_thres);
      ~DirectInput();
      int state(const struct rarch_keybind* bind, unsigned player);
      void poll();

      BOOL init_joypad(const DIDEVICEINSTANCE *instance);

   private:
      uint8_t di_state[256];
      IDirectInput8 *ctx;
      IDirectInputDevice8 *keyboard;

      int joypad_indices[8];
      DIJOYSTATE2 joy_state[8];

      std::vector<IDirectInputDevice8*> joypad;

      float thres;
};

