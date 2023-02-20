#pragma once

// ---------------------------------------------------------------------------------------------
// to use a different executor:
//
//   1. create a copy of this file in your own application
//
//   2. create your own executor type (see appbase/default_executor.hpp)
//      and define an `application` type templated with it.
//
//   3. use as below
//
// ---------------------------------------------------------------------------------------------

#include <appbase/application_base.hpp>

#include <appbase/default_executor.hpp>

namespace appbase {
using application = application_t<default_executor>;
}

#include <appbase/application_instance.hpp>
