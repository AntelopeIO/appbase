#include <appbase/application.hpp>
#include <iostream>
#include <string_view>
#include <thread>
#include <future>
#include <boost/exception/diagnostic_information.hpp>


#define BOOST_TEST_MODULE Basic Tests
#include <boost/test/included/unit_test.hpp>

namespace bu = boost::unit_test;
namespace bpo = boost::program_options;

using bpo::options_description;
using bpo::variables_map;
using std::string;
using std::vector;

class pluginA : public appbase::plugin<pluginA>
{
public:
   APPBASE_PLUGIN_REQUIRES();

   virtual void set_program_options( options_description& cli, options_description& cfg ) override {
      cli.add_options()
         ("readonly", "open db in read only mode")
         ("dbsize", bpo::value<uint64_t>()->default_value( 8*1024 ), "Minimum size MB of database shared memory file")
         ("replay", "clear db and replay all blocks" )
         ("throw_during_startup", "throw an exception in plugin_startup()" )
         ("quit_during_startup", "calls app().quit() plugin_startup()" )
         ("log", "log messages" );
   }

   void plugin_initialize( const variables_map& options ) {
      readonly_ = !!options.count("readonly");
      replay_   = !!options.count("replay");
      log_      = !!options.count("log");
      throw_during_startup_ = !!options.count("throw_during_startup");
      quit_during_startup_ = !!options.count("quit_during_startup");
      dbsize_   = options.at("dbsize").as<uint64_t>();
      log("initialize pluginA");
   }
   
   void plugin_startup()  {
      log("starting pluginA");
      if (throw_during_startup_)
         do_throw("throwing as requested");
      if (quit_during_startup_)
         appbase::app().quit();
   }
   
   void plugin_shutdown() {
      log("shutdown pluginA");
      if (shutdown_counter)
         ++(*shutdown_counter);
   }
   
   uint64_t dbsize() const { return dbsize_; }
   bool     readonly() const { return readonly_; }
   
   void     do_throw(const std::string& msg) { throw std::runtime_error(msg); }
   void     set_shutdown_counter(uint32_t &c) { shutdown_counter = &c; }
   
   void     log(std::string_view s) const {
      if (log_)
         std::cout << s << "\n";
   }

private:
   bool      readonly_ {false};
   bool      replay_ {false};
   bool      throw_during_startup_ {false};
   bool      quit_during_startup_ {false};
   bool      log_ {false};
   uint64_t  dbsize_ {0};
   uint32_t* shutdown_counter { nullptr };
};

class pluginB : public appbase::plugin<pluginB>
{
public:
   pluginB(){};
   ~pluginB(){};
   pluginB(const pluginB&) = delete;
   pluginB(pluginB&&) = delete;

   APPBASE_PLUGIN_REQUIRES( (pluginA) );

   virtual void set_program_options( options_description& cli, options_description& cfg ) override {
      cli.add_options()
         ("endpoint", bpo::value<string>()->default_value( "127.0.0.1:9876" ), "address and port.")
         ("log2", "log messages" )
         ("throw", "throw an exception in plugin_shutdown()" )
         ;
   }

   void plugin_initialize( const variables_map& options ) {
      endpoint_ = options.at("endpoint").as<string>();
      log_      = !!options.count("log");
      throw_    = !!options.count("throw");
      log("initialize pluginB");
   }
   
   void plugin_startup()  { log("starting pluginB"); }
   void plugin_shutdown() {
      log("shutdown pluginB");
      if (shutdown_counter)
         ++(*shutdown_counter);
      if (throw_)
         do_throw("throwing in shutdown");
   }

   const string& endpoint() const { return endpoint_; }
   
   void     do_throw(std::string msg) { throw std::runtime_error(msg); }
   void     set_shutdown_counter(uint32_t &c) { shutdown_counter = &c; }
   
   void     log(std::string_view s) const {
      if (log_)
         std::cout << s << "\n";
   }
   
private:
   bool   log_ {false};
   bool   throw_ {false};
   string endpoint_;
   uint32_t* shutdown_counter { nullptr };
};


// -----------------------------------------------------------------------------
// Check that program options are correctly passed to plugins
// -----------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(program_options)
{
   appbase::application::register_plugin<pluginB>();

   appbase::scoped_app app;
   
   const char* argv[] = { bu::framework::current_test_case().p_name->c_str(),
                          "--plugin", "pluginA", "--readonly", "--replay", "--dbsize", "10000",
                          "--plugin", "pluginB", "--endpoint", "127.0.0.1:55", "--throw" };
   
   BOOST_CHECK(app->initialize<pluginB>(sizeof(argv) / sizeof(char*), const_cast<char**>(argv)));

   auto& pA = app->get_plugin<pluginA>();
   BOOST_CHECK(pA.dbsize() == 10000);
   BOOST_CHECK(pA.readonly());

   auto& pB = app->get_plugin<pluginB>();
   BOOST_CHECK(pB.endpoint() == "127.0.0.1:55");
}

// -----------------------------------------------------------------------------
// Check that configured plugins are started correctly
// -----------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(app_execution)
{
   appbase::application::register_plugin<pluginB>();

   appbase::scoped_app app;
   
   const char* argv[] = { bu::framework::current_test_case().p_name->c_str(),
                          "--plugin", "pluginA", "--log",
                          "--plugin", "pluginB", "--log2" };
   
   BOOST_CHECK(app->initialize<pluginB>(sizeof(argv) / sizeof(char*), const_cast<char**>(argv)));

   std::promise<std::tuple<pluginA&, pluginB&>> plugin_promise;
   std::future<std::tuple<pluginA&, pluginB&>> plugin_fut = plugin_promise.get_future();
   std::thread app_thread( [&]() {
      app->startup();
      plugin_promise.set_value( {app->get_plugin<pluginA>(), app->get_plugin<pluginB>()} );
      app->exec();
   } );

   auto [pA, pB] = plugin_fut.get();
   BOOST_CHECK(pA.get_state() == appbase::abstract_plugin::started);
   BOOST_CHECK(pB.get_state() == appbase::abstract_plugin::started);

   app->quit();
   app_thread.join();
}

// -----------------------------------------------------------------------------
// Check application lifetime managed by appbase::scoped_app
// -----------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(scoped_app_lifetime)
{
   appbase::application::register_plugin<pluginB>();

   {
      // create and run an `application` instance
      appbase::scoped_app app;
   
      const char* argv[] = { bu::framework::current_test_case().p_name->c_str() };
   
      BOOST_CHECK(app->initialize<pluginB>(sizeof(argv) / sizeof(char*), const_cast<char**>(argv)));

      std::promise<std::tuple<pluginA&, pluginB&>> plugin_promise;
      std::future<std::tuple<pluginA&, pluginB&>> plugin_fut = plugin_promise.get_future();
      std::thread app_thread( [&]() {
         app->startup();
         plugin_promise.set_value( {app->get_plugin<pluginA>(), app->get_plugin<pluginB>()} );
         app->exec();
      } );

      auto [pA, pB] = plugin_fut.get();
      BOOST_CHECK(pA.get_state() == appbase::abstract_plugin::started);
      BOOST_CHECK(pB.get_state() == appbase::abstract_plugin::started);

      std::cout << "Started first application instance\n";
      app->quit();
      app_thread.join();
   }

   {
      // create and run another `application` instance
      appbase::scoped_app app;
   
      const char* argv[] = { bu::framework::current_test_case().p_name->c_str() };
   
      BOOST_CHECK(app->initialize<pluginB>(sizeof(argv) / sizeof(char*), const_cast<char**>(argv)));

      std::promise<std::tuple<pluginA&, pluginB&>> plugin_promise;
      std::future<std::tuple<pluginA&, pluginB&>> plugin_fut = plugin_promise.get_future();
      std::thread app_thread( [&]() {
         app->startup();
         plugin_promise.set_value( {app->get_plugin<pluginA>(), app->get_plugin<pluginB>()} );
         app->exec();
      } );

      auto [pA, pB] = plugin_fut.get();
      BOOST_CHECK(pA.get_state() == appbase::abstract_plugin::started);
      BOOST_CHECK(pB.get_state() == appbase::abstract_plugin::started);

      std::cout << "Started second application instance\n";
      app->quit();
      app_thread.join();
   }
   
}

// -----------------------------------------------------------------------------
// Here we make sure that if the app gets an exeption in the `app().exec()` loop,
// 1. the exception is caught by the appbase framework, and logged
// 2. all plugins are shutdown (verified with shutdown_counter)
// 3. the exception is rethrown so the `main()` program can catch it if desired.
// -----------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(exception_in_exec)
{
   appbase::application::register_plugin<pluginB>();

   appbase::scoped_app app;
   
   const char* argv[] = { bu::framework::current_test_case().p_name->c_str(),
                          "--plugin", "pluginA", "--log",
                          "--plugin", "pluginB", "--log2" };
   
   BOOST_CHECK(app->initialize<pluginB>(sizeof(argv) / sizeof(char*), const_cast<char**>(argv)));

   std::promise<std::tuple<pluginA&, pluginB&>> plugin_promise;
   std::future<std::tuple<pluginA&, pluginB&>> plugin_fut = plugin_promise.get_future();
   std::thread app_thread( [&]() {
      app->startup();
      plugin_promise.set_value( {app->get_plugin<pluginA>(), app->get_plugin<pluginB>()} );
      try {
         app->exec();
      } catch(const std::exception& e ) {
         std::cout << "exception in exec (as expected): " << e.what() << "\n";
      }
   } );

   auto [pA, pB] = plugin_fut.get();
   BOOST_CHECK(pA.get_state() == appbase::abstract_plugin::started);
   BOOST_CHECK(pB.get_state() == appbase::abstract_plugin::started);

   uint32_t shutdown_counter = 0;
   pA.set_shutdown_counter(shutdown_counter);
   pB.set_shutdown_counter(shutdown_counter);
   
   std::this_thread::sleep_for(std::chrono::milliseconds(20));

   // this will throw an exception causing `app->exec()` to exit
   app->executor().post(appbase::priority::high, [&pA=pA] () { pA.do_throw("throwing in pluginA"); });
   
   app_thread.join();

   BOOST_CHECK(shutdown_counter == 2); // make sure both plugins shutdonn correctly
}

// -----------------------------------------------------------------------------
// Here we make sure that if the app gets an exeption in the `app().exec()` loop,
// 1. the exception is caught by the appbase framework, and logged
// 2. all plugins are shutdown (verified with shutdown_counter)
// 3. if the first plugin to be shutdown (pluginB) throws an exception, the second
//    plugin is still shutdown before the exception is rethrown.
// -----------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(exception_in_shutdown)
{
   appbase::application::register_plugin<pluginB>();

   appbase::scoped_app app;
   
   const char* argv[] = { bu::framework::current_test_case().p_name->c_str(),
                          "--plugin", "pluginA", "--log",
                          "--plugin", "pluginB", "--log2", "--throw" };
   
   BOOST_CHECK(app->initialize<pluginB>(sizeof(argv) / sizeof(char*), const_cast<char**>(argv)));

   std::promise<std::tuple<pluginA&, pluginB&>> plugin_promise;
   std::future<std::tuple<pluginA&, pluginB&>> plugin_fut = plugin_promise.get_future();
   std::thread app_thread( [&]() {
      app->startup();
      plugin_promise.set_value( {app->get_plugin<pluginA>(), app->get_plugin<pluginB>()} );
      try {
         app->exec();
      } catch(const std::exception& e ) {
         std::cout << "exception in exec (as expected): " << e.what() << "\n";
      }
   } );

   auto [pA, pB] = plugin_fut.get();
   BOOST_CHECK(pA.get_state() == appbase::abstract_plugin::started);
   BOOST_CHECK(pB.get_state() == appbase::abstract_plugin::started);

   uint32_t shutdown_counter = 0;
   pA.set_shutdown_counter(shutdown_counter);
   pB.set_shutdown_counter(shutdown_counter);
   
   std::this_thread::sleep_for(std::chrono::milliseconds(20));

   // this will throw an exception causing `app->exec()` to exit
   app->executor().post(appbase::priority::high, [&pA=pA] () { pA.do_throw("throwing in pluginA"); });
   
   app_thread.join();

   BOOST_CHECK(shutdown_counter == 2); // make sure both plugins shutdonn correctly,
                                       // even though there was a throw
}

// -----------------------------------------------------------------------------
// Here we make sure that if a plugin throws during `plugin_startup()`
// 1. the exception is caught by the appbase framework, and logged
// 2. all plugins are shutdown (verified with shutdown_counter)
// -----------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(exception_in_startup)
{
   appbase::application::register_plugin<pluginB>();

   appbase::scoped_app app;

   const char* argv[] = { bu::framework::current_test_case().p_name->c_str(),
                          "--plugin", "pluginA", "--log", "--throw_during_startup",
                          "--plugin", "pluginB", "--log2" };

   BOOST_CHECK(app->initialize<pluginB>(sizeof(argv) / sizeof(char*), const_cast<char**>(argv)));

   std::thread app_thread( [&]() {
      auto& pA = app->get_plugin<pluginA>();
      uint32_t shutdown_counter(0);
      pA.set_shutdown_counter(shutdown_counter);

      try {
         app->startup();
      } catch(const std::exception& e ) {
         std::cout << "exception during startup (as expected): " << e.what() << "\n";
      }
      BOOST_CHECK(shutdown_counter == 1); // check that plugin_shutdown() was executed for pA
   } );

   app_thread.join();
}

// -----------------------------------------------------------------------------
// Here we make sure that if a plugin calls app().quit() during `plugin_startup()`,
// it doesn't interrupt the startup process for other plugins
// `producer_plugin` can do this and some tests like `terminate-scenarios-test-hard_replay`
// rely on other plugins getting initialized.
// -----------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(quit_in_startup)
{
   appbase::application::register_plugin<pluginB>();

   appbase::scoped_app app;

   const char* argv[] = { bu::framework::current_test_case().p_name->c_str(),
                          "--plugin", "pluginA", "--log", "--quit_during_startup",
                          "--plugin", "pluginB", "--log2" };

   BOOST_CHECK(app->initialize<pluginB>(sizeof(argv) / sizeof(char*), const_cast<char**>(argv)));

   std::thread app_thread( [&]() {
      auto& pA = app->get_plugin<pluginA>();
      uint32_t shutdown_counter(0);
      pA.set_shutdown_counter(shutdown_counter);

      try {
         app->startup();
      } catch(const std::exception& e ) {
         // appbase framework should *not* throw an exception if app.quit() is called during startup
         BOOST_CHECK(false);
         std::cout << "exception during startup: " << e.what() << "\n";
      }
      BOOST_CHECK(shutdown_counter == 0); // check that plugin_shutdown() was not executed for pA
   } );

   app_thread.join();
}

// -----------------------------------------------------------------------------
// Make sure that queue is emptied when `app->quit()` is called, and that the
// queued tasks are *not* executed
// -----------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(queue_emptied_at_quit)
{
   appbase::application::register_plugin<pluginB>();

   appbase::scoped_app app;
   
   const char* argv[] = { bu::framework::current_test_case().p_name->c_str() };
   
   BOOST_CHECK(app->initialize<pluginB>(sizeof(argv) / sizeof(char*), const_cast<char**>(argv)));

   std::promise<std::tuple<pluginA&, pluginB&>> plugin_promise;
   std::future<std::tuple<pluginA&, pluginB&>> plugin_fut = plugin_promise.get_future();
   std::thread app_thread( [&]() {
      app->startup();
      plugin_promise.set_value( {app->get_plugin<pluginA>(), app->get_plugin<pluginB>()} );
      app->exec();
   } );

   auto [pA, pB] = plugin_fut.get();
   BOOST_CHECK(pA.get_state() == appbase::abstract_plugin::started);
   BOOST_CHECK(pB.get_state() == appbase::abstract_plugin::started);

   auto fib = [](uint64_t x) -> uint64_t {
      auto fib_impl = [](uint64_t n, auto& impl) -> uint64_t {
         return (n <= 1) ? n : impl(n - 1, impl) + impl(n - 2, impl);
      };
      return fib_impl(x, fib_impl);
   };

   uint32_t shutdown_counter = 0;
   pA.set_shutdown_counter(shutdown_counter);
   pB.set_shutdown_counter(shutdown_counter);
   
   uint64_t num_computed = 0, res;

   // computing 100 fib(32) takes about a second on my machine... so the app->quit() should
   // be processed while there are still plenty in the queue
   for (uint64_t i=0; i<100; ++i)
      app->executor().post(appbase::priority::high, [&]() { res = fib(32); ++num_computed; });

   app->quit();
   
   app_thread.join();

   std::cout << "num_computed: " << num_computed << "\n";
   BOOST_CHECK(num_computed < 100);
   BOOST_CHECK(shutdown_counter == 2); // make sure both plugins shutdown correctly,
}
