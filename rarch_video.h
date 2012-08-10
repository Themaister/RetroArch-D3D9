/////
// API header for external RetroArch video and input plugins.
//
//

#ifndef __RARCH_VIDEO_DRIVER_H
#define __RARCH_VIDEO_DRIVER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#ifdef RARCH_DLL_IMPORT
#define RARCH_API_EXPORT __declspec(dllimport) 
#else
#define RARCH_API_EXPORT __declspec(dllexport) 
#endif
#define RARCH_API_CALLTYPE __cdecl
#else
#define RARCH_API_EXPORT
#define RARCH_API_CALLTYPE
#endif

#define RARCH_GRAPHICS_API_VERSION 4

// Since we don't want to rely on C++ or C99 for a proper boolean type,
// make sure return semantics are perfectly clear ... ;)

#ifndef RARCH_OK
#define RARCH_OK 1
#endif

#ifndef RARCH_ERROR
#define RARCH_ERROR 0
#endif

#ifndef RARCH_TRUE
#define RARCH_TRUE 1
#endif

#ifndef RARCH_FALSE
#define RARCH_FALSE 0
#endif

#define RARCH_COLOR_FORMAT_XRGB1555 0
#define RARCH_COLOR_FORMAT_ARGB8888 1

#define RARCH_INPUT_SCALE_BASE 256

typedef struct py_state py_state_t;

// Create a new runtime for Python.
// py_script: The python script to be loaded. If is_file is true, this will be the full path to a file.
// If false, it will be an UTF-8 encoded string of the script.
// is_file: Tells if py_script is the path to a script, or a script itself.
// py_class: name of the class being instantiated. 
typedef py_state_t *(*python_state_new_cb)(const char *py_script, unsigned is_file, const char *py_class);
// Grabs a value from the Python runtime.
// id: The uniform (class method) to be called.
// frame_count: Passes frame_count as an argument to the script.
typedef float (*python_state_get_cb)(py_state_t *handle, const char *id, unsigned frame_count);
// Frees the runtime.
typedef void (*python_state_free_cb)(py_state_t *handle);

typedef struct rarch_video_info
{ 
   // Width of window. 
   // If fullscreen mode is requested, 
   // a width of 0 means the resolution of the desktop should be used.
   unsigned width;

   // Height of window. 
   // If fullscreen mode is requested, 
   // a height of 0 means the resolutiof the desktop should be used.
   unsigned height;

   // If true, start the window in fullscreen mode.
   int fullscreen;

   // If true, VSync should be enabled.
   int vsync;

   // If true, the output image should have the aspect ratio 
   // as set in aspect_ratio.
   int force_aspect;

   // Aspect ratio. Only takes effect if force_aspect is enabled.
   float aspect_ratio;

   // Requests that the image is smoothed,
   // using bilinear filtering or otherwise.
   // If this cannot be implemented efficiently, this can be disregarded.
   // If smooth is false, nearest-neighbor scaling is requested.
   int smooth;

   // input_scale defines the maximum size of the picture that will
   // ever be used with the frame callback.
   // The maximum resolution is a multiple of 256x256 size (RARCH_INPUT_SCALE_BASE),
   // so an input scale of 2
   // means you should allocate a texture or of 512x512.
   unsigned input_scale;

   // Defines the coloring format used of the input frame.
   // XRGB1555 format is 16-bit and has byte ordering: 0RRRRRGGGGGBBBBB,
   // in native endian.
   // ARGB8888 is AAAAAAAARRRRRRRRGGGGGGGGBBBBBBBB, native endian.
   // Alpha channel should be disregarded.
   int color_format;

   // If non-NULL, requests the use of an XML shader.
   // Can be disregarded.
   const char *xml_shader;
   // If non-NULL, requests the use of a Cg shader.
   // Can be disregarded.
   // If both are non-NULL,
   // Cg or XML could be used at the discretion of the plugin.
   const char *cg_shader;

   // Requestes that a certain
   // TTF font is used for rendering messages to the screen.
   // Can be disregarded.
   const char *ttf_font;
   unsigned ttf_font_size;
   unsigned ttf_font_color; // Font color, in format ARGB8888. Alpha should be disregarded.

   // A title that should be displayed in the title bar of the window.
   const char *title_hint;

   // Functions to peek into the python runtime for shaders.
   // Check typedefs above for explanation.
   // These may be NULL if RetroArch is not built with Python support.
   python_state_new_cb python_state_new;
   python_state_get_cb python_state_get;
   python_state_free_cb python_state_free;
} rarch_video_info_t;

// Some convenience macros.
// Extract which axes to test for in negative or positive direction.
// May be equal to RARCH_NO_AXIS, which means testing should not occur.
#define RARCH_AXIS_NEG_GET(x) (((unsigned)(x) >> 16) & 0xFFFFU)
#define RARCH_AXIS_POS_GET(x) ((unsigned)(x) & 0xFFFFU)

// I hope no joypad will ever have this many buttons or axes ... ;)
// If joykey is this value, do not check that button.
#define RARCH_NO_AXIS (0xFFFFFFFFU)
#define RARCH_NO_BTN ((unsigned short)0xFFFFU)

// Masks to test on joykey which hat direction is to be tested for.
#define RARCH_HAT_UP_MASK (1 << 15)
#define RARCH_HAT_DOWN_MASK (1 << 14)
#define RARCH_HAT_LEFT_MASK (1 << 13)
#define RARCH_HAT_RIGHT_MASK (1 << 12)
#define RARCH_HAT_MAP(x, hat) ((x & ((1 << 12) - 1)) | hat)

#define RARCH_HAT_MASK (RARCH_HAT_UP_MASK | RARCH_HAT_DOWN_MASK | \
      RARCH_HAT_LEFT_MASK | RARCH_HAT_RIGHT_MASK)

// Test this on the joykey. If true, we want to test for a joypad hat
// rather than a button.
#define RARCH_GET_HAT_DIR(x) (x & RARCH_HAT_MASK)

// Gets the joypad hat to be tested for. 
// Only valid when RARCH_GET_HAT_DIR() returns true.
#define RARCH_GET_HAT(x) (x & (~RARCH_HAT_MASK))

// key, joykey and joyaxis are all checked at the same time.
// If any one of these are pressed, return 1 in state callback.
struct rarch_keybind
{
   // Keyboard key. The key values use the SDL 1.2 keysyms, 
   // which probably need to be transformed to the native format.
   // The actual keysyms RetroArch uses are found in libretro.h.
   unsigned short key;

   // Joypad key. Joypad POV (hats) are embedded into this key as well.
   unsigned short joykey;

   // Joypad axis. Negative and positive axes are embedded into this variable.
   unsigned joyaxis;
};

typedef struct rarch_input_driver
{
   // Inits input driver. 
   // Joypad index denotes which joypads are desired for the various players.
   // Should an entry be negative,
   // do not open joypad for that player.
   // Threshold states the minimum offset that a joypad axis 
   // has to be held for it to be registered.
   // The range of this is [0, 1],
   // where 0 means any displacement will register,
   // and 1 means the axis has to be pressed all the way to register.
   void *(*init)(const int joypad_index[8], float axis_threshold);

   // Polls input. Called once every frame.
   void (*poll)(void *data);

   // Queries input state for a certain key on a certain player.
   // Players are 1 - 8.
   // For digital inputs, pressed key is 1, not pressed key is 0.
   // Analog values have same range as a signed 16-bit integer.
   short (*input_state)(void *data, const struct rarch_keybind *bind,
         unsigned player);

   // Queries analog input state for a certain key on a certain player in the range of [-0x8000, 0x7fff].
   // Only the direction of the bind should be returned.
   //
   // E.g. if the bind has bind for negative axis 5, and the axis is pressed to the positive
   // axis, the returned value should be 0.
   short (*input_state_analog)(void *data, unsigned joyaxis,
         unsigned player);

   // Frees the input struct.
   void (*free)(void *data);

   // Human readable indentification string.
   const char *ident;
} rarch_input_driver_t;

typedef struct rarch_video_driver
{
   // Inits the video driver. Returns an opaque handle pointer to the driver.
   // Returns NULL on error.
   //
   // Should the video driver request that a certain input driver is used,
   // it is possible to set the driver to *input.
   // If no certain driver is desired, set *input to NULL.
   void *(*init)(const rarch_video_info_t *video, 
         const rarch_input_driver_t **input); 

   // Updates frame on the screen. 
   // Frame can be either XRGB1555 or ARGB32 format
   // depending on rgb32 setting in rarch_video_info_t. 
   // Pitch is the distance in bytes between two scanlines in memory. 
   // 
   // When msg is non-NULL, 
   // it's a message that should be displayed to the user.
   int (*frame)(void *data, const void *frame, 
         unsigned width, unsigned height, unsigned pitch, const char *msg);

   // Requests nonblocking operation. 
   // True = VSync is turned off. 
   // False = VSync is turned on.
   void (*set_nonblock_state)(void *data, int toggle);

   // This must return false when the user exits the emulator.
   int (*alive)(void *data);

   // Does the window have focus?
   int (*focus)(void *data);

   // Frees the video driver.
   void (*free)(void *data);

   // A human-readable identification of the video driver.
   const char *ident;

   // Needs to be defined to RARCH_GRAPHICS_API_VERSION. 
   // This is used to detect API/ABI mismatches.
   int api_version;

   // The final image should be rotated counter-clockwise by rot * 90 degrees.
   void (*set_rotation)(void *data, unsigned rot);

   // Gets the current width/height of the viewport.
   void (*viewport_size)(void *data, unsigned *width, unsigned *height);

   // Reads the content of the viewport into memory.
   // Tightly packed BGR888. Pitch is width * 3.
   int (*read_viewport)(void *data, unsigned char *buffer);
} rarch_video_driver_t;

// Called by RetroArch on startup to get a driver handle.
// This is NOT dynamically allocated.
RARCH_API_EXPORT const rarch_video_driver_t* RARCH_API_CALLTYPE
   rarch_video_init(void);

#ifdef __cplusplus
}
#endif

#endif
