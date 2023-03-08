// ---------------------------------------------------------------------------------------------
// to use a different executor:
//
//   1. create a copy of this file in your own application
//
//   2. create your own version of appbase/default_executor.hpp
//           (it needs to define a type `appbase_executor`)
//      and include this file instead of <appbase/default_executor.hpp>
//
//   3. include your own `application.hpp` in your project
//
// ---------------------------------------------------------------------------------------------



#pragma once

#include <appbase/application_base.hpp>
#include <appbase/execution_priority_queue.hpp>

class my_executor {
public:
   template <typename Func>
   auto post( int priority, Func&& func ) {
      return boost::asio::post(io_serv, pri_queue.wrap(priority, --order, std::forward<Func>(func)));
   }

   auto& get_priority_queue() { return pri_queue; }
     
   bool execute_highest() { return pri_queue.execute_highest(); }
     
   void clear() { pri_queue.clear(); }

   boost::asio::io_service& get_io_service() { return io_serv; }

private:
   // members are ordered taking into account that the last one is destructed first
   boost::asio::io_service  io_serv;
   appbase::execution_priority_queue pri_queue;
   std::size_t order = std::numeric_limits<size_t>::max(); // to maintain FIFO ordering in queue within priority
};

namespace appbase {
   using application = application_t<my_executor>;
}

#include <appbase/application_instance.hpp>


