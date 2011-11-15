#ifndef STATE_TRACKER_HPP__
#define STATE_TRACKER_HPP__

#include "common.h"
#include "config_file.hpp"
#include <vector>
#include <utility>
#include <string>

class StateTracker
{
   public:
      StateTracker(const std::string &program, const std::string &py_class, const std::vector<std::string> &uniforms,
      const ssnes_video_info_t &info);
      ~StateTracker();

      std::vector<std::pair<std::string, float>> get_uniforms(unsigned frame_count);

   private:
      py_state_t *handle;
      const ssnes_video_info_t &info;

      std::vector<std::string> uniforms;
};

#endif

