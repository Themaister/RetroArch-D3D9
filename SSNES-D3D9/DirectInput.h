#pragma once

#include <stdint.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

class DirectInput
{
	public:
		DirectInput(const int joypad_index[5], float axis_thres);
		~DirectInput();
		int state(const struct ssnes_keybind* bind, unsigned player);
		void poll();

		BOOL init_joypad(const DIDEVICEINSTANCE *instance);

	private:
		uint8_t di_state[256];
		IDirectInput8 *ctx;
		IDirectInputDevice8 *keyboard;

		int joypad_indices[5];
		IDirectInputDevice8 *joypad[5];
		unsigned joypad_cnt;

		DIJOYSTATE2 joy_state[5];

		float thres;
};

