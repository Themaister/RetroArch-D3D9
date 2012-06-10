#include "state_tracker.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>

StateTracker::StateTracker(const std::string &program,
      const std::string &py_class,
      const std::vector<std::string> &uniforms,
      const rarch_video_info_t &info)
      : handle(nullptr), info(info), uniforms(uniforms)
{
   if (!info.python_state_new)
      throw std::runtime_error("Failed to find state tracker symbols!");

   handle = info.python_state_new(program.c_str(), true, py_class.c_str());
   if (!handle)
      throw std::runtime_error("Failed to hook into state tracker!");
}

StateTracker::~StateTracker()
{
   if (handle && info.python_state_free)
      info.python_state_free(handle);
}

std::vector<std::pair<std::string, float>> StateTracker::get_uniforms(unsigned frame_count)
{
   if (!handle)
      return std::vector<std::pair<std::string, float>>();

   std::vector<std::pair<std::string, float>> ret;
   for (unsigned i = 0; i < uniforms.size(); i++)
   {
      ret.push_back(std::pair<std::string, float>(uniforms[i],
               info.python_state_get(handle, uniforms[i].c_str(), frame_count)));
   }

   return ret;
}

