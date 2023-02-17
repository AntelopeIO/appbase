#pragma once

namespace appbase {

   using executor = appbase_executor;

   class application : private executor, public application_base {
   public:
      static application&  instance() {
         if (__builtin_expect(!!app_instance, 1))
            return *app_instance;
         app_instance.reset(new application);
         return *app_instance;
      }

      static void reset_app_singleton() { app_instance.reset(); }
      
      static bool null_app_singleton()  { return !app_instance; }

      template <typename Func>
      auto post( int priority, Func&& func ) {
         return application_base::post(*static_cast<executor*>(this), priority, std::forward<Func>(func));
      }

      void exec() {
         application_base::exec(*static_cast<executor*>(this));
      }

      void startup() {
         application_base::startup(get_io_service());
      }

      application() {
         set_stop_executor_cb([&]() { get_io_service().stop(); });
         set_post_cb([&](int prio, std::function<void()> cb) { this->post(prio, std::move(cb)); });
      }

  private:
      inline static std::unique_ptr<application> app_instance;      
   };
}

#include <appbase/plugin.hpp>

namespace appbase {

   template<typename Data, typename DispatchPolicy>
   void channel<Data,DispatchPolicy>::publish(int priority, const Data& data) {
      if (has_subscribers()) {
         // this will copy data into the lambda
         app().post( priority, [this, data]() {
            _signal(data);
         });
      }
   }

   class scoped_app {
   public:
      explicit scoped_app()  { assert(application::null_app_singleton()); app_ = &app(); }
      ~scoped_app() { application::reset_app_singleton(); } // destroy app instance so next instance gets a clean one

      scoped_app(const scoped_app&) = delete;
      scoped_app& operator=(const scoped_app&) = delete;

      // access methods
      application*       operator->()       { return app_; }
      const application* operator->() const { return app_; }

   private:
      application* app_;
   };

   static application& app() { return application::instance(); }
   
}

