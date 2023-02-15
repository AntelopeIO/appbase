#pragma once
#include <boost/asio.hpp>

#include <queue>

namespace appbase {
// adapted from: https://www.boost.org/doc/libs/1_69_0/doc/html/boost_asio/example/cpp11/invocation/prioritised_handlers.cpp

struct priority {
   static constexpr int lowest      = std::numeric_limits<int>::min();
   static constexpr int low         = 10;
   static constexpr int medium_low  = 25;
   static constexpr int medium      = 50;
   static constexpr int medium_high = 75;
   static constexpr int high        = 100;
   static constexpr int highest     = std::numeric_limits<int>::max();
};

// the index of the default queue
constexpr static int default_queue = 0;

class execution_priority_queue : public boost::asio::execution_context
{
public:
   execution_priority_queue()
   {
      // add the default queue
      multi_queue_add_queue();
   }

   template <typename Function>
   void add(int priority, int index, Function function)
   {
      std::unique_ptr<queued_handler_base> handler(new queued_handler<Function>(priority, --order_, std::move(function)));

      handlers_queue_[index]->push(std::move(handler));
   }

   void clear()
   {
      for (auto& q: handlers_queue_)
         q = std::make_unique<prio_queue>();
   }
   
   void execute_all()
   {
      while (!handlers_queue_[default_queue]->empty()) {
         handlers_queue_[default_queue]->top()->execute();
         handlers_queue_[default_queue]->pop();
      }
   }

   bool execute_highest()
   {
      if( !handlers_queue_[default_queue]->empty() ) {
         handlers_queue_[default_queue]->top()->execute();
         handlers_queue_[default_queue]->pop();
      }

      return !handlers_queue_[default_queue]->empty();
   }

   size_t size() { return handlers_queue_[default_queue]->size(); }

   // multi-queue support

   void multi_queue_register_next_handler_func(std::function<std::tuple<bool, int, bool>()> f) {
      next_handler_func_ = f;
   }

   int multi_queue_add_queue() {
      handlers_queue_.push_back( std::make_unique<prio_queue>() );
      return handlers_queue_.size() - 1; // the index of the queue just added
   }

   int multi_queue_size(int i) {
      return handlers_queue_[i]->size();
   }

   int multi_queue_empty(int i) {
      return handlers_queue_[i]->empty();
   }

   bool multi_queue_less_than(int i, int j) {
      return ( *handlers_queue_[i]->top() < *handlers_queue_[j]->top() );
   }

   bool multi_queue_execute_highest()
   {
      auto [has_handler, i, more] = next_handler_func_();
      if( has_handler ) {
         handlers_queue_[i]->top()->execute();
         handlers_queue_[i]->pop();
      }

      return more;
   }

   class executor
   {
   public:
      executor(execution_priority_queue& q, int p, int index)
            : context_(q), priority_(p), index_(index)
      {
      }

      execution_priority_queue& context() const noexcept
      {
         return context_;
      }

      template <typename Function, typename Allocator>
      void dispatch(Function f, const Allocator&) const
      {
         context_.add(priority_, index_, std::move(f));
      }

      template <typename Function, typename Allocator>
      void post(Function f, const Allocator&) const
      {
         context_.add(priority_, index_,std::move(f));
      }

      template <typename Function, typename Allocator>
      void defer(Function f, const Allocator&) const
      {
         context_.add(priority_, index_, std::move(f));
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
      int priority_;
      int index_;
   };

   template <typename Function>
   boost::asio::executor_binder<Function, executor>
   wrap(int priority, Function&& func, int index = 0)
   {
      return boost::asio::bind_executor( executor(*this, priority, index), std::forward<Function>(func) );
   }

private:
   class queued_handler_base
   {
   public:
      queued_handler_base( int p, size_t order )
            : priority_( p )
            , order_( order )
      {
      }

      virtual ~queued_handler_base() = default;

      virtual void execute() = 0;

      int priority() const { return priority_; }
      // C++20
      // friend std::weak_ordering operator<=>(const queued_handler_base&,
      //                                       const queued_handler_base&) noexcept = default;
      friend bool operator<(const queued_handler_base& a,
                            const queued_handler_base& b) noexcept
      {
         return std::tie( a.priority_, a.order_ ) < std::tie( b.priority_, b.order_ );
      }

   private:
      int priority_;
      size_t order_;
   };

   template <typename Function>
   class queued_handler : public queued_handler_base
   {
   public:
      queued_handler(int p, size_t order, Function f)
            : queued_handler_base( p, order )
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
   std::size_t order_ = std::numeric_limits<size_t>::max(); // to maintain FIFO ordering in queue within priority

   std::vector<std::unique_ptr<prio_queue>> handlers_queue_;
   std::function<std::tuple<bool, int, bool>()>  next_handler_func_;
};

} // appbase
