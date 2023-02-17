#include <appbase/application.hpp>

namespace appbase {

application& application::instance() {
   if (__builtin_expect(!!app_instance, 1))
      return *app_instance;
   app_instance.reset(new application);
   return *app_instance;
}

application& app() { return application::instance(); }

} // namespace appbase
