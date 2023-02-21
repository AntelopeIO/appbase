#pragma once

#include <appbase/application_base.hpp>
#include <appbase/execution_priority_queue.hpp>

namespace appbase { 

enum class exec_mode {
   queue1_only,
   both_queues
};

class two_queue_executor {
public:
   template <typename Func>
   auto post( int priority, appbase::execution_priority_queue& q, Func&& func ) {
      return boost::asio::post(io_serv_, q.wrap(priority, --order_, std::forward<Func>(func)));
   }

   /**
    * @deprecated
   */
   template <typename Func>
   auto post( int priority, Func&& func ) {
      return boost::asio::post(io_serv_, queue1_.wrap(priority, --order_, std::forward<Func>(func)));
   }

   auto& queue1() { return queue1_; }
   auto& queue2() { return queue2_; }
   /**
    * @deprecated
   */
   auto& get_priority_queue() { return queue1_; }
     
   boost::asio::io_service& get_io_service() { return io_serv_; }

   bool execute_highest() {
      if ( exec_mode_ == appbase::exec_mode::queue1_only ) {
         return queue1_.execute_highest();
      } else {
         if( !queue2_.empty() && ( queue1_.empty() || *queue1_.top() < *queue2_.top()) )  {
            // queue2_'s top function's priority greater than queue1_'s top function's, or queue2_ empty
            queue2_.execute_highest();
         } else if( !queue1_.empty() ) {
            queue1_.execute_highest();
         }
         return !queue1_.empty() || !queue2_.empty();
      }
   }
     
   void clear() {
      queue1_.clear();
      queue2_.clear();
   }

   void set_exec_mode(appbase::exec_mode mode) {
      exec_mode_ = mode;
   }

   // members are ordered taking into account that the last one is destructed first
private:
   boost::asio::io_service           io_serv_;
   appbase::execution_priority_queue queue1_;
   appbase::execution_priority_queue queue2_;
   std::atomic<std::size_t>          order_ { std::numeric_limits<size_t>::max() }; // to maintain FIFO ordering in both queues within priority
   appbase::exec_mode                exec_mode_ { appbase::exec_mode::queue1_only };
};

using application = application_t<two_queue_executor>;
}

#include <appbase/application_instance.hpp>
