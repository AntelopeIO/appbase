#pragma once

// --
// To use a different executor:
//
// 1. Create your own executor type  (see executor types already defined as examples).
// 2. Add a using statement indicating your application and include <appbase/application_instance.hpp>
//          namespace appbase {
//             using application = application_t<default_executor>;
//          }
//          #include <appbase/application_instance.hpp>
// --

#include <appbase/application_base.hpp>

#include <appbase/default_executor.hpp>

namespace appbase {
using application = application_t<default_executor>;
}

#include <appbase/application_instance.hpp>
