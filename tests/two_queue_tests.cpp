#define BOOST_TEST_MODULE appbase_two_queue_executor_test
#include <boost/test/included/unit_test.hpp>
#include <thread>
#include <iostream>

#include <appbase/two_queue_executor.hpp>

using namespace appbase;

BOOST_AUTO_TEST_SUITE(executor_test)

std::thread start_app_thread(appbase::scoped_app& app) {
   const char* argv[] = { boost::unit_test::framework::current_test_case().p_name->c_str() };
   BOOST_CHECK(app->initialize(sizeof(argv) / sizeof(char*), const_cast<char**>(argv)));
   app->startup();
   std::thread app_thread( [&]() {
      app->exec();
   } );
   return app_thread;
}

// verify functions only from default queue1 are executed when execution mode is not explictly set
BOOST_AUTO_TEST_CASE( execute_from_default_queue ) {
   appbase::scoped_app app;
   auto app_thread = start_app_thread(app);
   
   // post functions
   std::map<int, int> rslts {};
   int seq_num = 0;
   app->executor().post( priority::medium, app->executor().queue1(), [&]() { rslts[0]=seq_num; ++seq_num; } );
   app->executor().post( priority::medium, app->executor().queue1(), [&]() { rslts[1]=seq_num; ++seq_num; } );
   app->executor().post( priority::high,   app->executor().queue2(), [&]() { rslts[2]=seq_num; ++seq_num; } );
   app->executor().post( priority::high,   app->executor().queue1(), [&]() { rslts[3]=seq_num; ++seq_num; } );
   app->executor().post( priority::low,    app->executor().queue1(), [&]() { rslts[4]=seq_num; ++seq_num; } );
   app->executor().post( priority::low,    app->executor().queue2(), [&]() { rslts[5]=seq_num; ++seq_num; } );
   app->executor().post( priority::highest,app->executor().queue1(), [&]() { rslts[6]=seq_num; ++seq_num; } );
   app->executor().post( priority::high,   app->executor().queue2(), [&]() { rslts[7]=seq_num; ++seq_num; } );

   // stop application. Use lowest at the end to make sure this executes the last
   app->executor().post( priority::lowest, app->executor().queue1(), [&]() { app->quit(); } );
   app_thread.join();

   // both queues are cleared after execution 
   BOOST_REQUIRE_EQUAL( app->executor().queue1().empty(), true);
   BOOST_REQUIRE_EQUAL( app->executor().queue2().empty(), true);

   // exactly number of queue1 functions processed
   BOOST_REQUIRE_EQUAL( rslts.size(), 5 );

   // same priority (medidum) of functions executed by the post order
   BOOST_CHECK_LT( rslts[0], rslts[1] );

   // higher priority posted earlier executed earlier
   BOOST_CHECK_LT( rslts[3], rslts[4] );
}

// verify functions only from queue1 are processed when mode is explicitly set to queue1_only
BOOST_AUTO_TEST_CASE( execute_from_queue1 ) {
   appbase::scoped_app app;
   auto app_thread = start_app_thread(app);
   
   // set to run functions from both queues
   app->executor().set_exec_mode(appbase::exec_mode::queue1_only);

   // post functions
   std::map<int, int> rslts {};
   int seq_num = 0;
   app->executor().post( priority::medium, app->executor().queue2(), [&]() { rslts[0]=seq_num; ++seq_num; } );
   app->executor().post( priority::high,   app->executor().queue1(), [&]() { rslts[1]=seq_num; ++seq_num; } );
   app->executor().post( priority::high,   app->executor().queue2(), [&]() { rslts[2]=seq_num; ++seq_num; } );
   app->executor().post( priority::high,   app->executor().queue1(), [&]() { rslts[3]=seq_num; ++seq_num; } );
   app->executor().post( priority::low,    app->executor().queue1(), [&]() { rslts[4]=seq_num; ++seq_num; } );
   app->executor().post( priority::low,    app->executor().queue2(), [&]() { rslts[5]=seq_num; ++seq_num; } );
   app->executor().post( priority::highest,app->executor().queue1(), [&]() { rslts[6]=seq_num; ++seq_num; } );
   app->executor().post( priority::highest,app->executor().queue1(), [&]() { rslts[7]=seq_num; ++seq_num; } );
   app->executor().post( priority::high,   app->executor().queue1(), [&]() { rslts[8]=seq_num; ++seq_num; } );
   app->executor().post( priority::high,   app->executor().queue2(), [&]() { rslts[9]=seq_num; ++seq_num; } );

   // stop application. Use lowest at the end to make sure this executes the last
   app->executor().post( priority::lowest, app->executor().queue1(), [&]() { app->quit(); } );
   app_thread.join();

   // both queues are cleared after execution 
   BOOST_REQUIRE_EQUAL( app->executor().queue1().empty(), true);
   BOOST_REQUIRE_EQUAL( app->executor().queue2().empty(), true);

   // exactly number of posts processed
   BOOST_REQUIRE_EQUAL( rslts.size(), 6 );

   // same priority (medidum) of functions executed by the post order
   BOOST_CHECK_LT( rslts[0], rslts[1] );

   // higher priority posted earlier executed earlier
   BOOST_CHECK_LT( rslts[3], rslts[4] );
}

// verify no functions is executed if queue1 is empty when mode is set to queue1_only
BOOST_AUTO_TEST_CASE( execute_from_empty_queue1 ) {
   appbase::scoped_app app;
   auto app_thread = start_app_thread(app);
   
   // set to run functions from both queues
   app->executor().set_exec_mode(appbase::exec_mode::queue1_only);

   // post functions
   std::map<int, int> rslts {};
   int seq_num = 0;
   app->executor().post( priority::medium, app->executor().queue2(), [&]() { rslts[0]=seq_num; ++seq_num; } );
   app->executor().post( priority::high,   app->executor().queue2(), [&]() { rslts[1]=seq_num; ++seq_num; } );
   app->executor().post( priority::high,   app->executor().queue2(), [&]() { rslts[2]=seq_num; ++seq_num; } );
   app->executor().post( priority::high,   app->executor().queue2(), [&]() { rslts[3]=seq_num; ++seq_num; } );
   app->executor().post( priority::low,    app->executor().queue2(), [&]() { rslts[4]=seq_num; ++seq_num; } );
   app->executor().post( priority::low,    app->executor().queue2(), [&]() { rslts[5]=seq_num; ++seq_num; } );
   app->executor().post( priority::highest,app->executor().queue2(), [&]() { rslts[6]=seq_num; ++seq_num; } );
   app->executor().post( priority::highest,app->executor().queue2(), [&]() { rslts[7]=seq_num; ++seq_num; } );
   app->executor().post( priority::high,   app->executor().queue2(), [&]() { rslts[8]=seq_num; ++seq_num; } );
   app->executor().post( priority::high,   app->executor().queue2(), [&]() { rslts[9]=seq_num; ++seq_num; } );

   // Stop application. Use lowest at the end to make sure this executes the last
   app->executor().post( priority::lowest, app->executor().queue1(), [&]() { app->quit(); } );
   app_thread.join();

   // both queues are cleared after execution 
   BOOST_REQUIRE_EQUAL( app->executor().queue1().empty(), true);
   BOOST_REQUIRE_EQUAL( app->executor().queue2().empty(), true);

   // no results
   BOOST_REQUIRE_EQUAL( rslts.size(), 0 );
}

// verify functions from both queues are processed when mode is set to both_queues
BOOST_AUTO_TEST_CASE( execute_from_both_queues ) {
   appbase::scoped_app app;
   auto app_thread = start_app_thread(app);


   // set to run functions from both queues
   app->executor().set_exec_mode(appbase::exec_mode::both_queues);

   // post functions
   std::map<int, int> rslts {};
   int seq_num = 0;
   app->executor().post( priority::medium, app->executor().queue1(), [&]() { rslts[0]=seq_num; ++seq_num; } );
   app->executor().post( priority::medium, app->executor().queue2(), [&]() { rslts[1]=seq_num; ++seq_num; } );
   app->executor().post( priority::high,   app->executor().queue2(), [&]() { rslts[2]=seq_num; ++seq_num; } );
   app->executor().post( priority::lowest, app->executor().queue1(), [&]() { rslts[3]=seq_num; ++seq_num; } );
   app->executor().post( priority::low,    app->executor().queue1(), [&]() { rslts[4]=seq_num; ++seq_num; } );
   app->executor().post( priority::low,    app->executor().queue2(), [&]() { rslts[5]=seq_num; ++seq_num; } );
   app->executor().post( priority::highest,app->executor().queue1(), [&]() { rslts[6]=seq_num; ++seq_num; } );
   app->executor().post( priority::low,    app->executor().queue2(), [&]() { rslts[7]=seq_num; ++seq_num; } );
   app->executor().post( priority::lowest, app->executor().queue1(), [&]() { rslts[8]=seq_num; ++seq_num; } );
   app->executor().post( priority::lowest, app->executor().queue1(), [&]() { rslts[9]=seq_num; ++seq_num; } );
   app->executor().post( priority::low,    app->executor().queue2(), [&]() { rslts[10]=seq_num; ++seq_num; } );
   app->executor().post( priority::medium, app->executor().queue2(), [&]() { rslts[11]=seq_num; ++seq_num; } );

   // stop application. Use lowest at the end to make sure this executes the last
   app->executor().post( priority::lowest, app->executor().queue1(), [&]() { app->quit(); } );

   app_thread.join();

   // queues are emptied after quit
   BOOST_REQUIRE_EQUAL( app->executor().queue1().empty(), true);
   BOOST_REQUIRE_EQUAL( app->executor().queue2().empty(), true);

   // exactly number of posts processed
   BOOST_REQUIRE_EQUAL( rslts.size(), 12 );

   // all low must be processed the in order of posting
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
