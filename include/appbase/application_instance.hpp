#pragma once

namespace appbase {

static application& app();

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
         app().plugin_initialized(this);
      }
      assert(_state == initialized); /// if initial state was not registered, final state cannot be initialized
   }

   virtual void handle_sighup() override {}

   virtual void startup() final {
      if (_state == initialized) {
         _state = started;
         static_cast<Impl*>(this)->plugin_requires([&](auto& plug) { plug.startup(); });
         app().plugin_started(this); // add to `running_plugins` before so it will be shutdown if we throw in `plugin_startup()`
         static_cast<Impl*>(this)->plugin_startup();
         // some plugins (such as producer_plugin) may call `app().quit()` during startup (see `producer_plugin_impl::start_block()`.
         // this is not cause for immediate termination.
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
      app().executor().post(priority, [this, data]() { _signal(data); });
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
