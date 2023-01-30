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

   virtual void set_program_options( options_description& cli, options_description& cfg ) override
   {
      cli.add_options()
         ("readonly", "open db in read only mode")
         ("dbsize", bpo::value<uint64_t>()->default_value( 8*1024 ), "Minimum size MB of database shared memory file")
         ("replay", "clear db and replay all blocks" )
         ("log", "log messages" );
   }

   void plugin_initialize( const variables_map& options ) {
      readonly_ = !!options.count("readonly");
      replay_   = !!options.count("replay");
      log_      = !!options.count("log");
      dbsize_   = options.at("dbsize").as<uint64_t>();
      log("initialize pluginA");
   }
   
   void plugin_startup()  { log("starting pluginA"); }
   void plugin_shutdown() { log("shutdown pluginA");  if (shutdown_counter) ++(*shutdown_counter); }
   
   uint64_t dbsize() const { return dbsize_; }
   bool     readonly() const { return readonly_; }
   
   void     do_throw(std::string msg) { throw std::runtime_error(msg); }
   void     set_shutdown_counter(uint32_t &c) { shutdown_counter = &c; }
   
   void     log(std::string_view s) const {
      if (log_)
         std::cout << s << "\n";
   }

private:
   bool     readonly_ {false};
   bool     replay_ {false};
   bool     log_ {false};
   uint64_t dbsize_ {0};
   uint32_t *shutdown_counter { nullptr };
};

class pluginB : public appbase::plugin<pluginB>
{
public:
   pluginB(){};
   ~pluginB(){};

   APPBASE_PLUGIN_REQUIRES( (pluginA) );

   virtual void set_program_options( options_description& cli, options_description& cfg ) override
   {
      cli.add_options()
         ("endpoint", bpo::value<string>()->default_value( "127.0.0.1:9876" ), "address and port.")
         ("log2", "log messages" );
   }

   void plugin_initialize( const variables_map& options ) {
      endpoint_ = options.at("endpoint").as<string>();
      log_      = !!options.count("log");
      log("initialize pluginB");
   }
   
   void plugin_startup()  { log("starting pluginB"); }
   void plugin_shutdown() { log("shutdown pluginB"); if (shutdown_counter) ++(*shutdown_counter); }

   const string& endpoint() const { return endpoint_; }
   
   void     do_throw(std::string msg) { throw std::runtime_error(msg); }
   void     set_shutdown_counter(uint32_t &c) { shutdown_counter = &c; }
   
   void     log(std::string_view s) const {
      if (log_)
         std::cout << s << "\n";
   }
   
private:
   bool   log_ {false};
   string endpoint_;
   uint32_t *shutdown_counter { nullptr };
};


BOOST_AUTO_TEST_CASE(program_options)
{
   auto& app = appbase::app();
   
   app.register_plugin<pluginB>();

   const char *argv[] = { bu::framework::current_test_case().p_name->c_str(),
                          "--plugin", "pluginA", "--readonly", "--replay", "--dbsize", "10000",
                          "--plugin", "pluginB", "--endpoint", "127.0.0.1:55" };
   
   BOOST_CHECK(app.initialize(sizeof(argv) / sizeof(char *), const_cast<char **>(argv)));

   auto& pA = app.get_plugin<pluginA>();
   BOOST_CHECK(pA.dbsize() == 10000);
   BOOST_CHECK(pA.readonly());

   auto& pB = app.get_plugin<pluginB>();
   BOOST_CHECK(pB.endpoint() == "127.0.0.1:55");

   app.reset_app_singleton(); // needed if we don't call app.exec();
}

BOOST_AUTO_TEST_CASE(app_execution)
{
   auto& app = appbase::app();
   
   app.register_plugin<pluginB>();

   const char *argv[] = { bu::framework::current_test_case().p_name->c_str(),
                          "--plugin", "pluginA", "--log",
                          "--plugin", "pluginB", "--log2" };
   
   BOOST_CHECK(app.initialize(sizeof(argv) / sizeof(char *), const_cast<char **>(argv)));

   std::promise<std::tuple<pluginA&, pluginB&>> plugin_promise;
   std::future<std::tuple<pluginA&, pluginB&>> plugin_fut = plugin_promise.get_future();
   std::thread app_thread( [&]() {
      app.startup();
      plugin_promise.set_value( {app.get_plugin<pluginA>(), app.get_plugin<pluginB>()} );
      app.exec();
   } );

   auto [pA, pB] = plugin_fut.get();
   BOOST_CHECK(pA.get_state() == appbase::abstract_plugin::started);
   BOOST_CHECK(pB.get_state() == appbase::abstract_plugin::started);

   app.quit(); // can't use app after app.quit()
   app_thread.join();
}

BOOST_AUTO_TEST_CASE(exception_in_exec)
{
   auto& app = appbase::app();
   
   app.register_plugin<pluginB>();

   const char *argv[] = { bu::framework::current_test_case().p_name->c_str(),
                          "--plugin", "pluginA", "--log",
                          "--plugin", "pluginB", "--log2" };
   
   BOOST_CHECK(app.initialize(sizeof(argv) / sizeof(char *), const_cast<char **>(argv)));

   std::promise<std::tuple<pluginA&, pluginB&>> plugin_promise;
   std::future<std::tuple<pluginA&, pluginB&>> plugin_fut = plugin_promise.get_future();
   std::thread app_thread( [&]() {
      app.startup();
      plugin_promise.set_value( {app.get_plugin<pluginA>(), app.get_plugin<pluginB>()} );
      try {
         app.exec();
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

   // this will throw an exception causing `app.exec()` to exit
   app.post(appbase::priority::high, [&] () { pA.do_throw("throwing in pluginA"); });
   
   app_thread.join();

   BOOST_CHECK(shutdown_counter == 2); // make sure both plugins shutdonn correctly
}

