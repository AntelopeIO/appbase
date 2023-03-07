#pragma once
#include <boost/asio.hpp>

#include <queue>

namespace appbase {

struct default_priority {  
   int priority_;
   int order_;

   default_priority(int prio) : priority_(prio), order_(0) {}
   
   friend bool operator<(const default_priority& a, const default_priority& b) noexcept {
      return std::tie( a.priority_,  a.order_ ) < std::tie( b.priority_,  b.order_ );
   }
};

// adapted from: https://www.boost.org/doc/libs/1_69_0/doc/html/boost_asio/example/cpp11/invocation/prioritised_handlers.cpp
template<class Priority>
class execution_priority_queue : public boost::asio::execution_context
{
public:

   template <typename Function>
   void add(Priority priority, Function function)

   {
      std::unique_ptr<queued_handler_base> handler(new queued_handler<Function>(priority, std::move(function)));

      handlers_.push(std::move(handler));
   }

   void clear()
   {
      handlers_ = prio_queue();
   }
   
   void execute_all()
   {
      while (!handlers_.empty()) {
         handlers_.top()->execute();
         handlers_.pop();
      }
   }

   bool execute_highest()
   {
      if( !handlers_.empty() ) {
         handlers_.top()->execute();
         handlers_.pop();
      }

      return !handlers_.empty();
   }

   class executor
   {
   public:
      executor(execution_priority_queue& q, Priority p)
            : context_(q), priority_(p)
      {
      }

      execution_priority_queue& context() const noexcept
      {
         return context_;
      }

      template <typename Function, typename Allocator>
      void dispatch(Function f, const Allocator&) const
      {
         context_.add(priority_, std::move(f));
      }

      template <typename Function, typename Allocator>
      void post(Function f, const Allocator&) const
      {
         context_.add(priority_, std::move(f));
      }

      template <typename Function, typename Allocator>
      void defer(Function f, const Allocator&) const
      {
         context_.add(priority_, std::move(f));
      }

      void on_work_started() const noexcept {}
      void on_work_finished() const noexcept {}

      bool operator==(const executor& other) const noexcept
      {
         return &context_ == &other.context_ && priority_ == other.priority_;
      }

      bool operator!=(const executor& other) const noexcept
      {
         return !operator==(other);
      }

   private:
      execution_priority_queue& context_;
      Priority priority_;
   };

   template <typename Function>
   boost::asio::executor_binder<Function, executor>
   wrap(Priority priority, Function&& func)
   {
      return boost::asio::bind_executor( executor(*this, priority), std::forward<Function>(func) );
   }

private:
   class queued_handler_base
   {
   public:
      queued_handler_base( Priority p )
            : priority_( p )
      {
      }

      virtual ~queued_handler_base() = default;

      virtual void execute() = 0;

      Priority priority() const { return priority_; }
      // C++20
      // friend std::weak_ordering operator<=>(const queued_handler_base&,
      //                                       const queued_handler_base&) noexcept = default;
      friend bool operator<(const queued_handler_base& a,
                            const queued_handler_base& b) noexcept
      {
         return a.priority_ < b.priority_;
      }

   private:
      Priority priority_;
   };

   template <typename Function>
   class queued_handler : public queued_handler_base
   {
   public:
      queued_handler(Priority p, Function f)
            : queued_handler_base( p )
            , function_( std::move(f) )
      {
      }

      void execute() override
      {
         function_();
      }

   private:
      Function function_;
   };

   struct deref_less
   {
      template<typename Pointer>
      bool operator()(const Pointer& a, const Pointer& b) noexcept(noexcept(*a < *b))
      {
         return *a < *b;
      }
   };

   using prio_queue = std::priority_queue<std::unique_ptr<queued_handler_base>, std::deque<std::unique_ptr<queued_handler_base>>, deref_less>;
   prio_queue handlers_;
};

} // appbase
