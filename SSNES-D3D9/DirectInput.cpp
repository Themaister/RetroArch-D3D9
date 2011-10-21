#include "StdAfx.h"
#include "DirectInput.h"

#include <algorithm>
#include <iostream>
#include "keysym.h"
#include "D3DVideo.h"
#include "ssnes_video.h"
#include <assert.h>

namespace Callback
{
	static BOOL CALLBACK EnumJoypad(const DIDEVICEINSTANCE *instance, void *p)
	{
		return reinterpret_cast<DirectInput*>(p)->init_joypad(instance);
	}

	static BOOL CALLBACK EnumAxes(const DIDEVICEOBJECTINSTANCE *instance, void *p)
	{
		IDirectInputDevice8 *joypad = reinterpret_cast<IDirectInputDevice8*>(p);
		
		DIPROPRANGE range;
		memset(&range, 0, sizeof(range));
		range.diph.dwSize = sizeof(DIPROPRANGE);
		range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
		range.diph.dwHow = DIPH_BYID;
		range.diph.dwObj = instance->dwType;
		range.lMin = -32768;
		range.lMax = 32767;
		joypad->SetProperty(DIPROP_RANGE, &range.diph);

		return DIENUM_CONTINUE;
	}
}

namespace Map
{
	struct Key 
	{
		int sdl;
		int di;
	};

	static int sdl_to_di_lut[1024];

	// Not exhaustive ... ;)
	static const Key sdl_to_di[] = {
		{ SK_a, DIK_A },
		{ SK_b, DIK_B },
		{ SK_c, DIK_C },
		{ SK_d, DIK_D },
		{ SK_e, DIK_E },
		{ SK_f, DIK_F },
		{ SK_g, DIK_G },
		{ SK_h, DIK_H },
		{ SK_i, DIK_I },
		{ SK_j, DIK_J },
		{ SK_k, DIK_K },
		{ SK_l, DIK_L },
		{ SK_m, DIK_M },
		{ SK_n, DIK_N },
		{ SK_o, DIK_O },
		{ SK_p, DIK_P },
		{ SK_q, DIK_Q },
		{ SK_r, DIK_R },
		{ SK_s, DIK_S },
		{ SK_t, DIK_T },
		{ SK_u, DIK_U },
		{ SK_v, DIK_V },
		{ SK_w, DIK_W },
		{ SK_x, DIK_X },
		{ SK_y, DIK_Y },
		{ SK_z, DIK_Z },
		{ SK_F1, DIK_F1 },
		{ SK_F2, DIK_F2 },
		{ SK_F3, DIK_F3 },
		{ SK_F4, DIK_F4 },
		{ SK_F5, DIK_F5 },
		{ SK_F6, DIK_F6 },
		{ SK_F7, DIK_F7 },
		{ SK_F8, DIK_F8 },
		{ SK_F9, DIK_F9 },
		{ SK_F10, DIK_F10 },
		{ SK_F11, DIK_F11 },
		{ SK_F12, DIK_F12 },
		{ SK_LEFT, DIK_LEFT },
		{ SK_RIGHT, DIK_RIGHT },
		{ SK_UP, DIK_UP },
		{ SK_DOWN, DIK_DOWN },
		{ SK_ESCAPE, DIK_ESCAPE },
		{ SK_RETURN, DIK_RETURN },
		{ SK_BACKSPACE, DIK_BACKSPACE },
		{ SK_SPACE, DIK_SPACE },

		{ SK_0, DIK_0 },
		{ SK_1, DIK_1 },
		{ SK_2, DIK_2 },
		{ SK_3, DIK_3 },
		{ SK_4, DIK_4 },
		{ SK_5, DIK_5 },
		{ SK_6, DIK_6 },
		{ SK_7, DIK_7 },
		{ SK_8, DIK_8 },
		{ SK_9, DIK_9 },
		{ SK_KP0, DIK_NUMPAD0 },
		{ SK_KP1, DIK_NUMPAD1 },
		{ SK_KP2, DIK_NUMPAD2 },
		{ SK_KP3, DIK_NUMPAD3 },
		{ SK_KP4, DIK_NUMPAD4 },
		{ SK_KP5, DIK_NUMPAD5 },
		{ SK_KP6, DIK_NUMPAD6 },
		{ SK_KP7, DIK_NUMPAD7 },
		{ SK_KP8, DIK_NUMPAD8 },
		{ SK_KP9, DIK_NUMPAD9 },

		{ SK_PAUSE, DIK_PAUSE },
		{ SK_BACKQUOTE, DIK_GRAVE },
		{ SK_KP_PLUS, DIK_ADD },
		{ SK_KP_MINUS, DIK_SUBTRACT },
	};
}

DirectInput::DirectInput(const int joypad_index[5], float threshold) :
	ctx(NULL), keyboard(NULL), joypad_cnt(0), thres(threshold)
{
	std::fill(di_state, di_state + 256, 0);
	keyboard = NULL;
	for (unsigned i = 0; i < 5; i++)
		joypad[i] = NULL;
	std::copy(joypad_index, joypad_index + 5, joypad_indices);

	if (FAILED(DirectInput8Create(
		GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8, 
		reinterpret_cast<void**>(&ctx), NULL)))
	{
		throw std::runtime_error("Failed to init DInput8");
	}

	if (FAILED(ctx->CreateDevice(GUID_SysKeyboard, &keyboard, NULL)))
	{
		throw std::runtime_error("Failed to init input device");
	}

	keyboard->SetDataFormat(&c_dfDIKeyboard);
	keyboard->SetCooperativeLevel(D3DVideo::hwnd(), DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);
	if (FAILED(keyboard->Acquire()))
	{
		throw std::runtime_error("Failed to aquire keyboard");
	}

	ctx->EnumDevices(DI8DEVCLASS_GAMECTRL, Callback::EnumJoypad, reinterpret_cast<void*>(this), DIEDFL_ATTACHEDONLY);

	assert(SK_LAST < (sizeof(Map::sdl_to_di_lut) / sizeof(Map::sdl_to_di_lut[0])));
	assert(DIK_Z < (sizeof(Map::sdl_to_di_lut) / sizeof(Map::sdl_to_di_lut[0])));

	// Set up direct LUT.
	for (unsigned i = 0; i < sizeof(Map::sdl_to_di) / sizeof(Map::sdl_to_di[0]); i++)
		Map::sdl_to_di_lut[Map::sdl_to_di[i].sdl] = Map::sdl_to_di[i].di;
}

BOOL DirectInput::init_joypad(const DIDEVICEINSTANCE *instance)
{
	unsigned active = 0;
	unsigned n;
	for (n = 0; n < 5; n++)
	{
		if (!joypad[n] && joypad_indices[n] == joypad_cnt)
			break;

		if (joypad[n])
			active++;
	}
	if (active == 5) return DIENUM_STOP;

	if (FAILED(ctx->CreateDevice(instance->guidInstance, &joypad[n], NULL)))
		return DIENUM_CONTINUE;

	joypad_cnt++;

	std::cerr << "Bound joypad ... " << joypad_cnt << std::endl;

	joypad[n]->SetDataFormat(&c_dfDIJoystick2);
	joypad[n]->SetCooperativeLevel(D3DVideo::hwnd(), 
		DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);

	joypad[n]->EnumObjects(Callback::EnumAxes, 
		reinterpret_cast<void*>(joypad[n]), DIDFT_ABSAXIS);

	return DIENUM_CONTINUE;
}

DirectInput::~DirectInput()
{
	if (keyboard)
	{
		keyboard->Unacquire();
		keyboard->Release();
	}

	for (unsigned i = 0; i < 5; i++)
	{
		if (joypad[i])
		{
			joypad[i]->Unacquire();
			joypad[i]->Release();
		}
	}
	
	if (ctx)
		ctx->Release();
}

int DirectInput::state(const struct ssnes_keybind* bind, unsigned player)
{
	int ret = di_state[Map::sdl_to_di_lut[bind->key]] & 0x80 ? 1 : 0;
	if (ret)
		return ret;

	if (!joypad[player - 1])
		return 0;


	if (bind->joykey != SSNES_NO_BTN)
	{
		if (!SSNES_GET_HAT_DIR(bind->joykey))
		{
			int ret = joy_state[player - 1].rgbButtons[bind->joykey] ? 1 : 0;
			if (ret)
				return ret;
		}
		else
		{
			unsigned hat = SSNES_GET_HAT(bind->joykey);
			unsigned pov = joy_state[player - 1].rgdwPOV[hat];
			if (pov < 36000)
			{
				bool retval = false;
				switch (SSNES_GET_HAT_DIR(bind->joykey))
				{
				case SSNES_HAT_UP_MASK:
					retval = (pov >= 31500) || (pov <= 4500);
					break;
				case SSNES_HAT_RIGHT_MASK:
					retval = (pov >= 4500) && (pov <= 13500);
					break;
				case SSNES_HAT_DOWN_MASK:
					retval = (pov >= 13500) && (pov <= 22500);
					break;
				case SSNES_HAT_LEFT_MASK:
					retval = (pov >= 22500) && (pov <= 31500);
					break;
				}

				if (retval)
					return true;
			}
		}
	}
	if (bind->joyaxis != SSNES_NO_AXIS)
	{
		int min = static_cast<int>(-32678 * thres);
		int max = static_cast<int>(32677 * thres);

		switch (SSNES_AXIS_NEG_GET(bind->joyaxis))
		{
		case 0:
			return joy_state[player - 1].lX <= min;
		case 1:
			return joy_state[player - 1].lY <= min;
		case 2:
			return joy_state[player - 1].lZ <= min;
		case 3:
			return joy_state[player - 1].lRx <= min;
		case 4:
			return joy_state[player - 1].lRy <= min;
		case 5:
			return joy_state[player - 1].lRz <= min;
		}

		switch (SSNES_AXIS_POS_GET(bind->joyaxis))
		{
		case 0:
			return joy_state[player - 1].lX >= max;
		case 1:
			return joy_state[player - 1].lY >= max;
		case 2:
			return joy_state[player - 1].lZ >= max;
		case 3:
			return joy_state[player - 1].lRx >= max;
		case 4:
			return joy_state[player - 1].lRy >= max;
		case 5:
			return joy_state[player - 1].lRz >= max;
		}
	}

	return 0; // Nothing is pressed :)
}

void DirectInput::poll()
{
	std::fill(di_state, di_state + 256, 0);
	if (FAILED(keyboard->GetDeviceState(sizeof(di_state), di_state)))
	{
		keyboard->Acquire();
		if (FAILED(keyboard->GetDeviceState(sizeof(di_state), di_state)))
			ZeroMemory(di_state, sizeof(di_state));
	}

	ZeroMemory(&joy_state, sizeof(joy_state));
	for (unsigned i = 0; i < 5; i++)
	{
		if (joypad[i])
		{
			if (FAILED(joypad[i]->Poll()))
			{
				joypad[i]->Acquire();
				continue;
			}
		
			joypad[i]->GetDeviceState(sizeof(DIJOYSTATE2), &joy_state[i]);
		}
	}
}
