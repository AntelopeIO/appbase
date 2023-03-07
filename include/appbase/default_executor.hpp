#pragma once

#include <appbase/application_base.hpp>
#include <appbase/execution_priority_queue.hpp>

namespace appbase {

class default_executor {
public:
   template <typename Func>
   auto post(int priority, Func&& func) {
      return boost::asio::post(io_serv, pri_queue.wrap(priority, --order, std::forward<Func>(func)));
   }

   /**
    * Provide access to execution priority queue so it can be used to wrap functions for
    * prioritized execution.
    *
    * Example:
    *   boost::asio::steady_timer timer( app().get_io_service() );
    *   timer.async_wait( app().get_priority_queue().wrap(priority::low, [](){ do_something(); }) );
    */
   auto& get_priority_queue() {
      return pri_queue;
   }

   bool execute_highest() {
      return pri_queue.execute_highest();
   }

   void clear() {
      pri_queue.clear();
   }

   /**
    * Do not run io_service in any other threads, as application assumes single-threaded execution in exec().
    * @return io_serivice of application
    */
   boost::asio::io_service& get_io_service() {
      return io_serv;
   }

private:
   // members are ordered taking into account that the last one is destructed first
   boost::asio::io_service io_serv;
   execution_priority_queue pri_queue;
   std::size_t order = std::numeric_limits<size_t>::max(); // to maintain FIFO ordering in queue within priority
};

} // namespace appbase
