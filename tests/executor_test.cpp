#define BOOST_TEST_MODULE appbase_executor_test
#include <boost/test/included/unit_test.hpp>
#include <thread>
#include <iostream>

#include <appbase/application_base.hpp>

using namespace appbase;

class my_executor {
public:
   template <typename Func>
   auto post( int priority, Func&& func ) {
      return boost::asio::post(io_serv, q1.wrap(priority, std::forward<Func>(func)));
   }


   auto& get_priority_queue() { return q1; }
     
   bool execute_highest() {
      if( !q2.empty() && ( q1.empty() || q1.top_func_priority() < q2.top_func_priority()) )  {
         // q2's top function's priority greater than q1's top function's, or q2 empty
         q2.execute_highest();
      } else if( !q1.empty() ) {
         q1.execute_highest();
      }
      return !q1.empty() || !q2.empty();
   }
     
   void clear() {
      q1.clear();
      q2.clear();
   }

   boost::asio::io_service& get_io_service() { return io_serv; }

   // additional functions
   auto& get_q1() { return q1; }

   auto& get_q2() { return q2; }

   template <typename Func>
   auto post( int priority, appbase::execution_priority_queue& q, Func&& func ) {
      return boost::asio::post(io_serv, q.wrap(priority, std::forward<Func>(func)));
   }

   // members are ordered taking into account that the last one is destructed first
   boost::asio::io_service  io_serv;
   appbase::execution_priority_queue q1;
   appbase::execution_priority_queue q2;

};

using appbase_executor = my_executor;

#include <appbase/application_instance.hpp>

BOOST_AUTO_TEST_SUITE(executor_test)

static std::atomic<size_t> num_pushed = 0;

// wait for time out or the number of results has reached
void wait_for_results(size_t expected) {
   auto i = 0;
   while ( i < 10 && num_pushed.load() < expected) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      ++i;
   }
}

BOOST_AUTO_TEST_CASE( execute_from_both_queues ) {
   appbase::scoped_app app;
   const char* argv[] = { boost::unit_test::framework::current_test_case().p_name->c_str() };
   BOOST_CHECK(app->initialize(sizeof(argv) / sizeof(char*), const_cast<char**>(argv)));
   app->startup();
   std::thread app_thread( [&]() {
      app->exec();
   } );

   constexpr size_t num_rslts = 12;
   std::map<int, int> rslts {};
   int seq_num = 0;

   app->executor().post( priority::medium, app->executor().get_q1(), [&]() { rslts[0]=seq_num; ++seq_num; ++num_pushed; } );
   app->executor().post( priority::medium, app->executor().get_q2(), [&]() { rslts[1]=seq_num; ++seq_num; ++num_pushed; } );
   app->executor().post( priority::high,   app->executor().get_q2(), [&]() { rslts[2]=seq_num; ++seq_num; ++num_pushed; } );
   app->executor().post( priority::lowest, app->executor().get_q1(), [&]() { rslts[3]=seq_num; ++seq_num; ++num_pushed; } );
   app->executor().post( priority::low,    app->executor().get_q1(), [&]() { rslts[4]=seq_num; ++seq_num; ++num_pushed; } );
   app->executor().post( priority::low,    app->executor().get_q2(), [&]() { rslts[5]=seq_num; ++seq_num; ++num_pushed; } );
   app->executor().post( priority::highest,app->executor().get_q1(), [&]() { rslts[6]=seq_num; ++seq_num; ++num_pushed; } );
   app->executor().post( priority::low,    app->executor().get_q2(), [&]() { rslts[7]=seq_num; ++seq_num; ++num_pushed; } );
   app->executor().post( priority::lowest, app->executor().get_q1(), [&]() { rslts[8]=seq_num; ++seq_num; ++num_pushed; } );
   app->executor().post( priority::lowest, app->executor().get_q1(), [&]() { rslts[9]=seq_num; ++seq_num; ++num_pushed; } );
   app->executor().post( priority::low,    app->executor().get_q2(), [&]() { rslts[10]=seq_num; ++seq_num; ++num_pushed; } );
   app->executor().post( priority::medium, app->executor().get_q2(), [&]() { rslts[11]=seq_num; ++seq_num; ++num_pushed; } );

   wait_for_results(num_rslts);
   app->quit();
   app_thread.join();

   // queues are emptied after quit
   BOOST_REQUIRE_EQUAL( app->executor().get_q1().empty(), true);
   BOOST_REQUIRE_EQUAL( app->executor().get_q2().empty(), true);

   // exactly number of posts processed
   BOOST_REQUIRE_EQUAL( rslts.size(), num_rslts );

   // all low  must be processed the in order of posting
   BOOST_CHECK_LT( rslts[4], rslts[5] );
   BOOST_CHECK_LT( rslts[5], rslts[7] );
   BOOST_CHECK_LT( rslts[7], rslts[10] );

   // all medium must be processed the in order of posting
   BOOST_CHECK_LT( rslts[0], rslts[1] );
   BOOST_CHECK_LT( rslts[1], rslts[11] );

   // all functions posted after high before highest must be processed after high
   BOOST_CHECK_LT( rslts[2], rslts[3] );
   BOOST_CHECK_LT( rslts[2], rslts[4] );
   BOOST_CHECK_LT( rslts[2], rslts[5] );

   // all functions posted after highest must be processed after it
   BOOST_CHECK_LT( rslts[6], rslts[7] );
   BOOST_CHECK_LT( rslts[6], rslts[8] );
   BOOST_CHECK_LT( rslts[6], rslts[9] );
   BOOST_CHECK_LT( rslts[6], rslts[10] );
   BOOST_CHECK_LT( rslts[6], rslts[11] );
   BOOST_CHECK_LT( rslts[6], rslts[11] );
}

BOOST_AUTO_TEST_SUITE_END()
