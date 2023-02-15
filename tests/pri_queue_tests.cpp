#include <thread>
#include <boost/test/unit_test.hpp>
#include <appbase/application.hpp>

using namespace appbase;

BOOST_AUTO_TEST_SUITE(pri_queues)

constexpr static int q0 = appbase::default_queue;

// start app thread and call execution loop based on use_default_queue flag 
std::thread start_app_thread(appbase::scoped_app& app, bool use_default_queue) {
   const char* argv[] = { boost::unit_test::framework::current_test_case().p_name->c_str() };
   BOOST_CHECK(app->initialize(sizeof(argv) / sizeof(char*), const_cast<char**>(argv)));
   app->startup();
   std::thread app_thread( [&, use_default_queue]() {
      if ( use_default_queue ) {
         app->exec();
      } else {
         app->multi_queue_exec();
      }
   } );
   return app_thread;
}

// wait for time out or the number of results has reached
void wait_for_results(const std::vector<int>& results, size_t expected) {
   auto i = 0;
   while ( i < 10 && results.size() < expected) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      ++i;
   }
}

// test single default queue
BOOST_AUTO_TEST_CASE( default_single_queue ) {
   // start app thread with default queue
   appbase::scoped_app app;
   auto app_thread = start_app_thread(app, true);

   // post functions. each function pushs a value into the results queue
   std::vector<int> results;
   app->post( priority::medium, [&results]() { results.emplace_back(1); } );
   app->post( priority::medium, [&results]() { results.emplace_back(2); } );
   app->post( priority::high,   [&results]() { results.emplace_back(3); } );
   app->post( priority::lowest, [&results]() { results.emplace_back(4); } );
   app->post( priority::low,    [&results]() { results.emplace_back(5); } );
   app->post( priority::low,    [&results]() { results.emplace_back(6); } );

   // wait until app thread has executed all the functions
   auto num_rslts_expected = 6;
   wait_for_results(results, num_rslts_expected);
   app->quit();
   app_thread.join();
   
   // make sure functions are executed in right order
   BOOST_REQUIRE_EQUAL( results.size(), num_rslts_expected );
   BOOST_REQUIRE_EQUAL( results[0], 3 );
   BOOST_REQUIRE_EQUAL( results[1], 1 );
   BOOST_REQUIRE_EQUAL( results[2], 2 );
   BOOST_REQUIRE_EQUAL( results[3], 5 );
   BOOST_REQUIRE_EQUAL( results[4], 6 );
   BOOST_REQUIRE_EQUAL( results[5], 4 );
}

// two subqueues: execute from only one queue
BOOST_AUTO_TEST_CASE( execute_from_one_subqueue ) {
   appbase::scoped_app app;
   auto app_thread = start_app_thread(app, false);

   auto q1 = app->multi_queue_add_queue();
   auto next = [&app, q1]() {
      // only execute functions from q1
      return std::make_tuple(!app->multi_queue_empty(q1), q1, app->multi_queue_size(q1) > 1);
   };
   app->multi_queue_register_next_handler_func(next);

   std::vector<int> results;
   app->post( priority::medium,  [&results]() { results.emplace_back(1); }, q0 );
   app->post( priority::medium,  [&results]() { results.emplace_back(2); }, q1 );
   app->post( priority::high,    [&results]() { results.emplace_back(3); }, q1 );
   app->post( priority::lowest,  [&results]() { results.emplace_back(4); }, q0 );
   app->post( priority::low,     [&results]() { results.emplace_back(5); }, q0 );
   app->post( priority::low,     [&results]() { results.emplace_back(6); }, q1 );
   app->post( priority::highest, [&results]() { results.emplace_back(7); }, q0 );
   app->post( priority::low,     [&results]() { results.emplace_back(8); }, q1 );
   app->post( priority::low,     [&results]() { results.emplace_back(9); }, q1 );

   auto num_rslts_expected = 5;
   wait_for_results(results, num_rslts_expected);
   app->quit();
   app_thread.join();
   
   // queues are emptied after quit
   BOOST_REQUIRE_EQUAL( app->multi_queue_size(q0), 0);
   BOOST_REQUIRE_EQUAL( app->multi_queue_size(q1), 0);

   // only queue 1's handlers are executed
   BOOST_REQUIRE_EQUAL( results.size(), num_rslts_expected ); // only queue 1's handlers executed
   BOOST_REQUIRE_EQUAL( results[0], 3 );
   BOOST_REQUIRE_EQUAL( results[1], 2 );
   BOOST_REQUIRE_EQUAL( results[2], 6 );
   BOOST_REQUIRE_EQUAL( results[3], 8 );
   BOOST_REQUIRE_EQUAL( results[4], 9 );
}

// two multi_queues: execute from both queues in correct priority order
BOOST_AUTO_TEST_CASE( execute_from_both_multi_queues ) {
   appbase::scoped_app app;
   auto app_thread = start_app_thread(app, false);

   auto q1 = app->multi_queue_add_queue();
   auto next = [&app, q1]() {
      int index = q0;
      auto has_handler = false;

      if( !app->multi_queue_empty(q1) && ( app->multi_queue_empty(q0) || app->multi_queue_less_than(q0, q1) ) ) {
         // q1 non-empty but q0 empty, or q1's top handler's priority greater than q0
         index = q1;
         has_handler = true;
      } else if( !app->multi_queue_empty(q0) ) {
         index = q0;
         has_handler = true;
      }
      auto more = ( app->multi_queue_size(q0) + app->multi_queue_size(q1) > 1 );
      return std::make_tuple(has_handler, index, more);
   };
   app->multi_queue_register_next_handler_func(next);

   std::vector<int> results;
   app->post( priority::medium,  [&results]() { results.emplace_back(1); }, q0 );
   app->post( priority::medium,  [&results]() { results.emplace_back(2); }, q1 );
   app->post( priority::high,    [&results]() { results.emplace_back(3); }, q1 );
   app->post( priority::lowest,  [&results]() { results.emplace_back(4); }, q0 );
   app->post( priority::low,     [&results]() { results.emplace_back(5); }, q0 );
   app->post( priority::low,     [&results]() { results.emplace_back(6); }, q1 );
   app->post( priority::highest, [&results]() { results.emplace_back(7); }, q0 );
   app->post( priority::low,     [&results]() { results.emplace_back(8); }, q1 );
   app->post( priority::lowest,  [&results]() { results.emplace_back(9); }, q0 );
   app->post( priority::low,     [&results]() { results.emplace_back(0); }, q1 );

   auto num_rslts_expected = 10;
   wait_for_results(results, num_rslts_expected);
   app->quit();
   app_thread.join();

   // queues are emptied after quit
   BOOST_REQUIRE_EQUAL( app->multi_queue_size(q0), 0);
   BOOST_REQUIRE_EQUAL( app->multi_queue_size(q1), 0);

   // all handlers are executed in right order among both queues
   BOOST_REQUIRE_EQUAL( results.size(), num_rslts_expected );
   BOOST_REQUIRE_EQUAL( results[0], 7 );
   BOOST_REQUIRE_EQUAL( results[1], 3 );
   BOOST_REQUIRE_EQUAL( results[2], 1 );
   BOOST_REQUIRE_EQUAL( results[3], 2 );
   BOOST_REQUIRE_EQUAL( results[4], 5 );
   BOOST_REQUIRE_EQUAL( results[5], 6 );
   BOOST_REQUIRE_EQUAL( results[6], 8 );
   BOOST_REQUIRE_EQUAL( results[7], 0 );
   BOOST_REQUIRE_EQUAL( results[8], 4 );
   BOOST_REQUIRE_EQUAL( results[9], 9 );
}

BOOST_AUTO_TEST_SUITE_END()
