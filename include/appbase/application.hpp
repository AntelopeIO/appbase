// ---------------------------------------------------------------------------------------------
// to use a different executor:
//
//   1. create a copy of this file in your own application
//
//   2. create your own version of appbase/default_executor.hpp
//           (it needs to define a type `appbase_executor`)
//      and include this file instead of <appbase/default_executor.hpp>
//
//   3. include your own `application.hpp` in your project
//
// ---------------------------------------------------------------------------------------------



#pragma once

#include <appbase/application_base.hpp>

#include <appbase/default_executor.hpp>

#include <appbase/application_instance.hpp>


