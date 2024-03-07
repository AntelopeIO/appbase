#pragma once

#include <appbase/abstract_plugin.hpp>
#include <appbase/channel.hpp>
#include <appbase/method.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/core/demangle.hpp>
#include <typeindex>
#include <exception>
#include <string_view>

namespace appbase {
namespace bpo = boost::program_options;
namespace bfs = boost::filesystem;

using config_comparison_f = std::function<bool(const boost::any& a, const boost::any& b)>;

struct priority {
   static constexpr int lowest      = std::numeric_limits<int>::min();
   static constexpr int low         = 10;
   static constexpr int medium_low  = 25;
   static constexpr int medium      = 50;
   static constexpr int medium_high = 75;
   static constexpr int high        = 100;
   static constexpr int highest     = std::numeric_limits<int>::max();
};

class application_base {
public:
   ~application_base();


   /** @brief Set version
    *
    * @param version Version output with -v/--version
    */
   void set_version(uint64_t version);
   /** @brief Get version
    *
    * @return Version output with -v/--version
    */
   uint64_t version() const;
   /** @brief Get version string; generated from git describe if available
    *
    * @return A string worthy of output with -v/--version, or "Unknown" if git not available
    */
   string version_string() const;
   /** @brief User provided version string for version_string() which overrides git describe value.
    */
   void set_version_string(std::string v);
   /** @brief Get full version string; same as version_string() unless set differently.
    *
    * @return A string worthy of output with -v/--version, or "Unknown" if git not available
    */
   string full_version_string() const;
   /** @brief User provided full version string for full_version_string()
    */
   void set_full_version_string(std::string v);
   /** @brief Set default data directory
    *
    * @param data_dir Default data directory to use if not specified
    *                 on the command line.
    */
   void set_default_data_dir(const bfs::path& data_dir = "data-dir");
   /** @brief Get data directory
    *
    * @return Data directory, possibly from command line
    */
   bfs::path data_dir() const;
   /** @brief Set default config directory
    *
    * @param config_dir Default configuration directory to use if not
    *                   specified on the command line.
    */
   void set_default_config_dir(const bfs::path& config_dir = "etc");
   /** @brief Get config directory
    *
    * @return Config directory, possibly from command line
    */
   bfs::path config_dir() const;
   /** @brief Get logging configuration path.
    *
    * @return Logging configuration location from command line
    */
   bfs::path get_logging_conf() const;
   /** @brief Get full config.ini path
    *
    * @return Config directory & config file name, possibly from command line. Only
    *         valid after initialize() has been called.
    */
   bfs::path full_config_file_path() const;
   /** @brief Set function pointer invoked on receipt of SIGHUP
    *
    * The provided function will be invoked on receipt of SIGHUP followed
    * by invoking handle_sighup() on all initialized plugins. Caller
    * is responsible for preserving an object if necessary.
    *
    * @param callback Function pointer that will be invoked when the process
    *                 receives the HUP (1) signal.
    */
   void set_sighup_callback(std::function<void()> callback);
   /**
    * @brief Looks for the --plugin commandline / config option and calls initialize on those plugins
    *
    * @tparam Plugin List of plugins to initalize even if not mentioned by configuration. For plugins started by
    * configuration settings or dependency resolution, this template has no effect.
    * @param initialize_logging Function pointer that will be invoked to initialize logging
    * @return true if the application and plugins were initialized, false or exception on error
    */
   template <typename... Plugin>
   bool initialize(int argc, char** argv, std::function<void()> initialize_logging={}) {
      for (const auto& f : plugin_registrations)
         f(*this);
      return initialize_impl(argc, argv, {find_plugin<Plugin>()...}, initialize_logging);
   }

   void startup(boost::asio::io_service& io_serv);
   void shutdown();

   /**
    *  Wait until quit(), SIGINT or SIGTERM and then shutdown.
    *  Should only be executed from one thread.
    */
   template <typename Executor>
   void exec(Executor& exec) {
      std::exception_ptr eptr = nullptr;
      {
         auto& io_serv{exec.get_io_service()};
         boost::asio::io_service::work work(io_serv);
         (void)work;
         bool more = true;

         while (more || io_serv.run_one()) {
            if (is_quiting())
               break;
            try {
               while (io_serv.poll_one()) {}
               // execute the highest priority item
               more = exec.execute_highest();
            } catch (...) {
               more = true; // so we exit the while loop without calling io_serv.run_one()
               quit();
               eptr = std::current_exception();
               handle_exception(eptr, "application loop");
            }
         }

         try {
            exec.clear(); // make sure the queue is empty
            shutdown();   // may rethrow exceptions
         } catch (...) {
            if (!eptr)
               eptr = std::current_exception();
         }
      }

      // if we caught an exception while in the application loop, rethrow it so that main()
      // can catch it and report the error
      if (eptr)
         std::rethrow_exception(eptr);
   }

   void quit();

   /**
    * If in long running process this flag can be checked to see if processing should be stoppped.
    * @return true if quit() has been called.
    */
   bool is_quiting() const;

   /**
    * Register a configuration type with appbase. most "plain" types are already registered in
    * application.cpp. Failure to register a type will cause initialization to fail.
    */
   template <typename T>
   void register_config_type() {
      register_config_type_comparison(typeid(T), [](const auto& a, const auto& b) {
         return boost::any_cast<const T&>(a) == boost::any_cast<const T&>(b);
      });
   }
   void register_config_type_comparison(std::type_index, config_comparison_f comp);

   abstract_plugin* find_plugin(const string& name) const;
   abstract_plugin& get_plugin(const string& name) const;

   template <typename Plugin>
   auto& _register_plugin() {
      auto existing = find_plugin<Plugin>();
      if (existing)
         return *existing;

      auto plug = new Plugin();
      plugins[plug->name()].reset(plug);
      plug->register_dependencies();
      return *plug;
   }

   template <typename Plugin>
   static auto& register_plugin() {
      static int bogus = 0;
      plugin_registrations.push_back([](application_base& app) -> void { app._register_plugin<Plugin>(); });
      return bogus;
   }

   template <typename Plugin>
   Plugin* find_plugin() const {
      string name = boost::core::demangle(typeid(Plugin).name());
      return dynamic_cast<Plugin*>(find_plugin(name));
   }

   template <typename Plugin>
   Plugin& get_plugin() const {
      auto ptr = find_plugin<Plugin>();
      return *ptr;
   }

   /**
    * Fetch a reference to the method declared by the passed in type.  This will construct the method
    * on first access.  This allows loose and deferred binding between plugins
    *
    * @tparam MethodDecl - @ref appbase::method_decl
    * @return reference to the method described by the declaration
    */
   template <typename MethodDecl>
   auto get_method() -> std::enable_if_t<is_method_decl<MethodDecl>::value, typename MethodDecl::method_type&> {
      using method_type = typename MethodDecl::method_type;
      auto key = std::type_index(typeid(MethodDecl));
      auto itr = methods.find(key);
      if (itr != methods.end()) {
         return *method_type::get_method(itr->second);
      } else {
         methods.emplace(std::make_pair(key, method_type::make_unique()));
         return *method_type::get_method(methods.at(key));
      }
   }

   /**
    * Fetch a reference to the channel declared by the passed in type.  This will construct the channel
    * on first access.  This allows loose and deferred binding between plugins
    *
    * @tparam ChannelDecl - @ref appbase::channel_decl
    * @return reference to the channel described by the declaration
    */
   template <typename ChannelDecl>
   auto get_channel() -> std::enable_if_t<is_channel_decl<ChannelDecl>::value, typename ChannelDecl::channel_type&> {
      using channel_type = typename ChannelDecl::channel_type;
      auto key = std::type_index(typeid(ChannelDecl));
      auto itr = channels.find(key);
      if (itr != channels.end()) {
         return *channel_type::get_channel(itr->second);
      } else {
         channels.emplace(std::make_pair(key, channel_type::make_unique()));
         return *channel_type::get_channel(channels.at(key));
      }
   }

   const bpo::variables_map& get_options() const;

   /**
    * Set the current thread schedule priority to maximum.
    * Works for pthreads.
    */
   void set_thread_priority_max();

   void set_stop_executor_cb(std::function<void()> cb) {
      stop_executor_cb = std::move(cb);
   }

   void set_post_cb(std::function<void(int, std::function<void()>)> cb) {
      post_cb = std::move(cb);
   }


protected:
   template <typename Impl>
   friend class plugin;

   bool initialize_impl(int argc, char** argv, vector<abstract_plugin*> autostart_plugins, std::function<void()> initialize_logging);

   /** these notifications get called from the plugin when their state changes so that
    * the application can call shutdown in the reverse order.
    */
   ///@{
   void plugin_initialized(abstract_plugin& plug) {
      initialized_plugins.push_back(&plug);
   }
   void plugin_started(abstract_plugin& plug) {
      running_plugins.push_back(&plug);
   }
   ///@}

   application_base(std::shared_ptr<void>&& e); ///< protected because application is a singleton that should be accessed via instance()

   /// !!! must be dtor'ed after plugins
   std::shared_ptr<void> executor_ptr;

private:
   // members are ordered taking into account that the last one is destructed first
   std::function<void()> sighup_callback;
   std::function<void()> stop_executor_cb;
   std::function<void(int, std::function<void()>)> post_cb;

   map<std::type_index, erased_method_ptr> methods;
   map<std::type_index, erased_channel_ptr> channels;

   std::unique_ptr<class application_impl> my;

   map<string, std::unique_ptr<abstract_plugin>> plugins; ///< all registered plugins
   vector<abstract_plugin*> initialized_plugins;          ///< stored in the order they were started running
   vector<abstract_plugin*> running_plugins;              ///< stored in the order they were started running

   inline static std::vector<std::function<void(application_base&)>> plugin_registrations;

   void start_sighup_handler(std::shared_ptr<boost::asio::signal_set> sighup_set);
   void set_program_options();
   void write_default_config(const bfs::path& cfg_file);
   void print_default_config(std::ostream& os);

   void wait_for_signal(std::shared_ptr<boost::asio::signal_set> ss);
   void setup_signal_handling_on_ios(boost::asio::io_service& ios, bool startup);

   void handle_exception(std::exception_ptr eptr, std::string_view origin);
};

// ------------------------------------------------------------------------------------------
template <class executor_t>
class application_t : public application_base {
public:
   static application_t& instance() {
      if (__builtin_expect(!!app_instance, 1))
         return *app_instance;
      app_instance.reset(new application_t);
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
    * -- deprecated: use app().executor().post()
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

   void startup() {
      application_base::startup(get_io_service());
   }

   application_t() : application_base(std::make_shared<executor_t>()), executor_(*static_cast<executor_t*>(executor_ptr.get())) {
      set_stop_executor_cb([&]() { get_io_service().stop(); });
      set_post_cb([&](int prio, std::function<void()> cb) { executor_.post(prio, std::move(cb)); });
   }

   executor_t& executor() {
      return executor_;
   }

private:
   inline static std::unique_ptr<application_t> app_instance;
   executor_t& executor_;
};


} // namespace appbase
