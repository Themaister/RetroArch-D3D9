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
      StateTracker(const std::string &program, const std::string &py_class, const std::vector<std::string> &uniforms);
      ~StateTracker();

      std::vector<std::pair<std::string, float>> get_uniforms(unsigned frame_count);

   private:
      struct py_state;
      py_state *handle;
      py_state *(*pstate_new)(const char *, bool, const char *);
      void (*pstate_free)(py_state *);
      float (*pstate_get)(py_state *, const char *id, unsigned);

      std::vector<std::string> uniforms;
};

#endif

