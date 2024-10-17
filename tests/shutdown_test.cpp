#include <appbase/application.hpp>
#include <thread>

#include <boost/test/unit_test.hpp>

namespace bpo = boost::program_options;
using bpo::options_description;
using bpo::variables_map;

static bool thing = true;
struct thing_better_be_alive {
   ~thing_better_be_alive() noexcept(false) {
      if(!thing)
         throw "BOOM";
   }
};

class thready_plugin : public appbase::plugin<thready_plugin> {
   public:

     template <typename Lambda>
     void plugin_requires(Lambda&& l) {}

     void set_program_options( options_description& cli, options_description& cfg ) override {}

     void thread_work() {
        boost::asio::post(ctx, [&]() {
           thing_better_be_alive better_be;
           boost::asio::post(appbase::app().get_io_context(), [&,is_it=std::move(better_be)]() {
	      thread_work();
	   });
	});
     }

     void plugin_initialize( const variables_map& options ) {}
     void plugin_startup()  {
        for(unsigned i = 0; i < 48*4; i++)
           thread_work();

        for(unsigned i = 0; i < 48; ++i)
           threads.emplace_back([this]() {
              ctx.run();
           });
     }
     void plugin_shutdown() {
        usleep(100000);  //oh gee it takes a while to stop
        ctx.stop();
        for(unsigned i = 0; i < 48; ++i)
          threads[i].join();
     }

     ~thready_plugin() {
        thing = false;
     }

     boost::asio::io_context ctx;
     boost::asio::executor_work_guard<boost::asio::io_context::executor_type> wg = boost::asio::make_work_guard(ctx);

   private:
     std::vector<std::thread> threads;
};


BOOST_AUTO_TEST_CASE(test_shutdown)
{
   appbase::application::register_plugin<thready_plugin>();
   appbase::scoped_app app;

   const char* argv[] = { "nodoes" };
   if( !app->initialize<thready_plugin>( 1, const_cast<char**>(argv) ) )
      return;
   app->startup();
   boost::asio::post(appbase::app().get_io_context(), [&](){
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      app->quit();
   });
   app->exec();
   
}
