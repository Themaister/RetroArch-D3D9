#include "state_tracker.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>

// TODO: Implement properly ...

StateTracker::StateTracker(const std::string &program,
      const std::string &py_class, const std::vector<std::string> &uniforms)
      : handle(nullptr), pstate_new(nullptr), pstate_free(nullptr), pstate_get(nullptr),
         uniforms(uniforms)
{
   pstate_new = (py_state *(*)(const char *, bool, const char *))
         GetProcAddress(GetModuleHandle(nullptr), "py_state_new");
   pstate_get = (float (*)(py_state*, const char *, unsigned))
         GetProcAddress(GetModuleHandle(nullptr), "py_state_get");
   pstate_free = (void (*)(py_state*))
         GetProcAddress(GetModuleHandle(nullptr), "py_state_free");

   if (!pstate_new || !pstate_get || !pstate_free)
   {
      throw std::runtime_error("Failed to find state tracker symbols!");
      return;
   }

   handle = pstate_new(program.c_str(), true, py_class.c_str());
   if (!handle)
      throw std::runtime_error("Failed to hook into state tracker!");
}

StateTracker::~StateTracker()
{
   if (handle && pstate_free)
      pstate_free(handle);
}

std::vector<std::pair<std::string, float>> StateTracker::get_uniforms(unsigned frame_count)
{
   if (!handle)
      return std::vector<std::pair<std::string, float>>();

   std::vector<std::pair<std::string, float>> ret;
   for (unsigned i = 0; i < uniforms.size(); i++)
   {
      ret.push_back(std::pair<std::string, float>(uniforms[i],
               pstate_get(handle, uniforms[i].c_str(), frame_count)));
   }

   return ret;
}

