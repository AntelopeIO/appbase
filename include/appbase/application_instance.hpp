#pragma once

namespace appbase {

using executor_t = appbase_executor;

// ------------------------------------------------------------------------------------------
class application : public application_base {
public:
   static application& instance() {
      if (__builtin_expect(!!app_instance, 1))
         return *app_instance;
      app_instance.reset(new application);
      return *app_instance;
   }

   static void reset_app_singleton() {
      app_instance.reset();
   }

   static bool null_app_singleton() {
      return !app_instance;
   }

   /**
    * Post func to run on io_service with given priority.
    *
    * @param priority can be appbase::priority::* constants or any int, larger ints run first
    * @param func function to run on io_service
    * @return result of boost::asio::post
    */
   template <typename Func>
   auto post(int priority, Func&& func) {
      return executor_.post(priority, std::forward<Func>(func));
   }

   /**
    *  Wait until quit(), SIGINT or SIGTERM and then shutdown.
    *  Should only be executed from one thread.
    */
   void exec() {
      application_base::exec(executor_);
   }

   boost::asio::io_service& get_io_service() {
      return executor_.get_io_service();
   }

   auto& get_priority_queue() {
      return executor_.get_priority_queue();
   }

   void startup() {
      application_base::startup(get_io_service());
   }

   application() {
      set_stop_executor_cb([&]() { get_io_service().stop(); });
      set_post_cb([&](int prio, std::function<void()> cb) { this->post(prio, std::move(cb)); });
   }

   executor_t& executor() {
      return executor_;
   }

private:
   inline static std::unique_ptr<application> app_instance;
   executor_t executor_;
};

// ------------------------------------------------------------------------------------------
template <typename Impl>
class plugin : public abstract_plugin {
public:
   plugin() : _name(boost::core::demangle(typeid(Impl).name())) {}
   virtual ~plugin() {}

   virtual state get_state() const final {
      return _state;
   }
   virtual const std::string& name() const final {
      return _name;
   }

   virtual void register_dependencies() {
      static_cast<Impl*>(this)->plugin_requires([&](auto& plug) {});
   }

   virtual void initialize(const variables_map& options) final {
      if (_state == registered) {
         _state = initialized;
         static_cast<Impl*>(this)->plugin_requires([&](auto& plug) { plug.initialize(options); });
         static_cast<Impl*>(this)->plugin_initialize(options);
         // ilog( "initializing plugin ${name}", ("name",name()) );
         app().plugin_initialized(*this);
      }
      assert(_state == initialized); /// if initial state was not registered, final state cannot be initialized
   }

   virtual void handle_sighup() override {}

   virtual void startup() final {
      if (_state == initialized) {
         _state = started;
         static_cast<Impl*>(this)->plugin_requires([&](auto& plug) { plug.startup(); });
         static_cast<Impl*>(this)->plugin_startup();
         app().plugin_started(*this);
      }
      assert(_state == started); // if initial state was not initialized, final state cannot be started
   }

   virtual void shutdown() final {
      if (_state == started) {
         _state = stopped;
         // ilog( "shutting down plugin ${name}", ("name",name()) );
         static_cast<Impl*>(this)->plugin_shutdown();
      }
   }

protected:
   plugin(const string& name) : _name(name) {}

private:
   state _state = abstract_plugin::registered;
   std::string _name;
};

// ------------------------------------------------------------------------------------------
template <typename Data, typename DispatchPolicy>
void channel<Data, DispatchPolicy>::publish(int priority, const Data& data) {
   if (has_subscribers()) {
      // this will copy data into the lambda
      app().post(priority, [this, data]() { _signal(data); });
   }
}

// ------------------------------------------------------------------------------------------
class scoped_app {
public:
   explicit scoped_app() {
      assert(application::null_app_singleton());
      app_ = &app();
   }
   ~scoped_app() {
      application::reset_app_singleton();
   } // destroy app instance so next instance gets a clean one

   scoped_app(const scoped_app&) = delete;
   scoped_app& operator=(const scoped_app&) = delete;

   // access methods
   application* operator->() {
      return app_;
   }
   const application* operator->() const {
      return app_;
   }

private:
   application* app_;
};

static application& app() {
   return application::instance();
}

} // namespace appbase
