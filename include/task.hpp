///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_TASK_HPP_INCLUDED
#define CPPCORO_TASK_HPP_INCLUDED

#include "config.hpp"
#include "awaitable_traits.hpp"
#include "broken_promise.hpp"

#include "remove_rvalue_reference.hpp"

#include <atomic>
#include <exception>
#include <utility>
#include <type_traits>
#include <cstdint>
#include <cassert>

#include <coroutine>

namespace cppcoro
{
	template <typename T>
	class disposable_task;
	
	template <typename T>
	class task;

	namespace detail
	{
		static int counter = 0;
		class task_promise_base
		{
			friend struct final_awaitable;
			struct final_awaitable
			{
				bool await_ready() const noexcept
				{
					std::cout << __PRETTY_FUNCTION__ << std::endl;
					return false;
				}

#if CPPCORO_COMPILER_SUPPORTS_SYMMETRIC_TRANSFER
				template <typename PROMISE>
				std::coroutine_handle<> await_suspend(
					std::coroutine_handle<PROMISE> coro) noexcept
				{
					return coro.promise().m_continuation;
				}
#else
				// HACK: Need to add CPPCORO_NOINLINE to await_suspend() method
				// to avoid MSVC 2017.8 from spilling some local variables in
				// await_suspend() onto the coroutine frame in some cases.
				// Without this, some tests in async_auto_reset_event_tests.cpp
				// were crashing under x86 optimised builds.
				template <typename PROMISE>
				CPPCORO_NOINLINE void await_suspend(std::coroutine_handle<PROMISE> coroutine) noexcept
				{
					std::cout << __PRETTY_FUNCTION__ << std::endl;

					task_promise_base &promise = coroutine.promise();

					// Use 'release' memory semantics in case we finish before the
					// awaiter can suspend so that the awaiting thread sees our
					// writes to the resulting value.
					// Use 'acquire' memory semantics in case the caller registered
					// the continuation before we finished. Ensure we see their write
					// to m_continuation.
					if (promise.m_state.exchange(true, std::memory_order_acq_rel))
					{
						promise.m_continuation.resume();
					}
					//TODO: move this to the other type
					else
					{
						coroutine.destroy();
					}
				}
#endif

				void await_resume() noexcept
				{
				}
			};

			friend struct final_awaitable_for_disposable_task;
			struct final_awaitable_for_disposable_task : final_awaitable
			{
				// HACK: Need to add CPPCORO_NOINLINE to await_suspend() method
				// to avoid MSVC 2017.8 from spilling some local variables in
				// await_suspend() onto the coroutine frame in some cases.
				// Without this, some tests in async_auto_reset_event_tests.cpp
				// were crashing under x86 optimised builds.
				template <typename PROMISE>
				CPPCORO_NOINLINE void await_suspend(std::coroutine_handle<PROMISE> coroutine) noexcept
				{
					std::cout << __PRETTY_FUNCTION__ << std::endl;

					task_promise_base &promise = coroutine.promise();

					// Use 'release' memory semantics in case we finish before the
					// awaiter can suspend so that the awaiting thread sees our
					// writes to the resulting value.
					// Use 'acquire' memory semantics in case the caller registered
					// the continuation before we finished. Ensure we see their write
					// to m_continuation.
					if (promise.m_state.exchange(true, std::memory_order_acq_rel))
					{
						promise.m_continuation.resume();
					}
					else
					{
						coroutine.destroy();
					}
				}
			};

		public:
			int number;
			task_promise_base() noexcept
#if !CPPCORO_COMPILER_SUPPORTS_SYMMETRIC_TRANSFER
				: m_state(false)
#endif
			{
				number = ++counter;
				std::cout << __PRETTY_FUNCTION__ << "#" << number << std::endl;
			}

			~task_promise_base()
			{
				std::cout << __PRETTY_FUNCTION__ << "#" << number << std::endl;
			}

			auto initial_suspend() noexcept
			{
				std::cout << __PRETTY_FUNCTION__ << "#" << number << std::endl;

				return std::suspend_always{};
			}

			auto final_suspend() noexcept
			{
				std::cout << __PRETTY_FUNCTION__ << "#" << number << std::endl;

				return final_awaitable{};
			}

#if CPPCORO_COMPILER_SUPPORTS_SYMMETRIC_TRANSFER
			void set_continuation(std::coroutine_handle<> continuation) noexcept
			{
				m_continuation = continuation;
			}
#else
			bool try_set_continuation(std::coroutine_handle<> continuation)
			{
				m_continuation = continuation;
				return !m_state.exchange(true, std::memory_order_acq_rel);
			}
#endif

		private:
			std::coroutine_handle<> m_continuation;

#if !CPPCORO_COMPILER_SUPPORTS_SYMMETRIC_TRANSFER
			// Initially false. Set to true when either a continuation is registered
			// or when the coroutine has run to completion. Whichever operation
			// successfully transitions from false->true got there first.
			std::atomic<bool> m_state;
#endif
		};

		template <typename T>
		class task_promise final : public task_promise_base
		{

		public:
			task_promise() noexcept
			{

				std::cout << __PRETTY_FUNCTION__ << std::endl;
			}

			~task_promise()
			{
				std::cout << __PRETTY_FUNCTION__ << std::endl;

				switch (m_resultType)
				{
				case result_type::value:
					m_value.~T();
					break;
				case result_type::exception:
					m_exception.~exception_ptr();
					break;
				default:
					break;
				}
			}

			disposable_task<T> get_return_object() noexcept;

			void unhandled_exception() noexcept
			{
				::new (static_cast<void *>(std::addressof(m_exception))) std::exception_ptr(
					std::current_exception());
				m_resultType = result_type::exception;
			}

			template <
				typename VALUE,
				typename = std::enable_if_t<std::is_convertible_v<VALUE &&, T>>>
			void return_value(VALUE &&value) noexcept(std::is_nothrow_constructible_v<T, VALUE &&>)
			{
				::new (static_cast<void *>(std::addressof(m_value))) T(std::forward<VALUE>(value));
				m_resultType = result_type::value;
			}

			T &result() &
			{
				if (m_resultType == result_type::exception)
				{
					std::rethrow_exception(m_exception);
				}

				assert(m_resultType == result_type::value);

				return m_value;
			}

			// HACK: Need to have co_await of task<int> return prvalue rather than
			// rvalue-reference to work around an issue with MSVC where returning
			// rvalue reference of a fundamental type from await_resume() will
			// cause the value to be copied to a temporary. This breaks the
			// sync_wait() implementation.
			// See https://github.com/lewissbaker/cppcoro/issues/40#issuecomment-326864107
			using rvalue_type = std::conditional_t<
				std::is_arithmetic_v<T> || std::is_pointer_v<T>,
				T,
				T &&>;

			rvalue_type result() &&
			{
				if (m_resultType == result_type::exception)
				{
					std::rethrow_exception(m_exception);
				}

				assert(m_resultType == result_type::value);

				return std::move(m_value);
			}

		private:
			enum class result_type
			{
				empty,
				value,
				exception
			};

			result_type m_resultType = result_type::empty;

			union
			{
				T m_value;
				std::exception_ptr m_exception;
			};
		};

		template <>
		class task_promise<void> : public task_promise_base
		{
		public:
			task_promise() noexcept = default;

			disposable_task<void> get_return_object() noexcept;

			void return_void() noexcept
			{
			}

			void unhandled_exception() noexcept
			{
				m_exception = std::current_exception();
			}

			void result()
			{
				if (m_exception)
				{
					std::rethrow_exception(m_exception);
				}
			}

		private:
			std::exception_ptr m_exception;
		};

		template <typename T>
		class task_promise<T &> : public task_promise_base
		{
		public:
			task_promise() noexcept = default;

			disposable_task<T &> get_return_object() noexcept;

			void unhandled_exception() noexcept
			{
				m_exception = std::current_exception();
			}

			void return_value(T &value) noexcept
			{
				m_value = std::addressof(value);
			}

			T &result()
			{
				if (m_exception)
				{
					std::rethrow_exception(m_exception);
				}

				return *m_value;
			}

		private:
			T *m_value = nullptr;
			std::exception_ptr m_exception;
		};
	}

	/// \brief
	/// A task represents an operation that produces a result both lazily
	/// and asynchronously.
	///
	/// When you call a coroutine that returns a task, the coroutine
	/// simply captures any passed parameters and returns exeuction to the
	/// caller. Execution of the coroutine body does not start until the
	/// coroutine is first co_await'ed.
	template <typename T = void>
	class disposable_task
	{
	public:
		using promise_type = detail::task_promise<T>;

		using value_type = T;

	private:
		struct awaitable_base
		{
			std::coroutine_handle<promise_type> m_coroutine;

			awaitable_base(std::coroutine_handle<promise_type> coroutine) noexcept
				: m_coroutine(coroutine)
			{
			}

			bool await_ready() const noexcept
			{
				return !m_coroutine || m_coroutine.done();
			}

#if CPPCORO_COMPILER_SUPPORTS_SYMMETRIC_TRANSFER
			std::coroutine_handle<> await_suspend(
				std::coroutine_handle<> awaitingCoroutine) noexcept
			{
				m_coroutine.promise().set_continuation(awaitingCoroutine);
				return m_coroutine;
			}
#else
			bool await_suspend(std::coroutine_handle<> awaitingCoroutine) noexcept
			{
				// NOTE: We are using the bool-returning version of await_suspend() here
				// to work around a potential stack-overflow issue if a coroutine
				// awaits many synchronously-completing tasks in a loop.
				//
				// We first start the task by calling resume() and then conditionally
				// attach the continuation if it has not already completed. This allows us
				// to immediately resume the awaiting coroutine without increasing
				// the stack depth, avoiding the stack-overflow problem. However, it has
				// the down-side of requiring a std::atomic to arbitrate the race between
				// the coroutine potentially completing on another thread concurrently
				// with registering the continuation on this thread.
				//
				// We can eliminate the use of the std::atomic once we have access to
				// coroutine_handle-returning await_suspend() on both MSVC and Clang
				// as this will provide ability to suspend the awaiting coroutine and
				// resume another coroutine with a guaranteed tail-call to resume().
				m_coroutine.resume();
				return m_coroutine.promise().try_set_continuation(awaitingCoroutine);
			}
#endif
		};

	public:
		disposable_task() noexcept
			: m_coroutine(nullptr)
		{
			std::cout << __PRETTY_FUNCTION__ << std::endl;
		}

		explicit disposable_task(std::coroutine_handle<promise_type> coroutine)
			: m_coroutine(coroutine)
		{
		}

		disposable_task(disposable_task &&t) noexcept
			: m_coroutine(t.m_coroutine)
		{
			t.m_coroutine = nullptr;
		}

		/// Disable copy construction/assignment.
		disposable_task(const disposable_task &) = delete;
		disposable_task &operator=(const disposable_task &) = delete;

		/// Frees resources used by this task.
		virtual ~disposable_task()
		{
			std::cout << __PRETTY_FUNCTION__ << std::endl;
			if (m_coroutine)
			{
				std::cout << "have you finished ? " << m_coroutine.done() << std::endl;
				if (m_coroutine.done())
				{
					m_coroutine.destroy();
					std::cout << "acabo de destruir la m_coro" << std::endl;
				}
				else
				{
					std::cout << "todavia no termino...no destruir la m_coro" << std::endl;
				}
			}
		}

		void resume()
		{
			std::cout << __PRETTY_FUNCTION__ << "#" << m_coroutine.promise().number << std::endl;
			m_coroutine.resume();
		}

		disposable_task &operator=(disposable_task &&other) noexcept
		{
			if (std::addressof(other) != this)
			{
				if (m_coroutine)
				{
					m_coroutine.destroy();
				}

				m_coroutine = other.m_coroutine;
				other.m_coroutine = nullptr;
			}

			return *this;
		}

		/// \brief
		/// Query if the task result is complete.
		///
		/// Awaiting a task that is ready is guaranteed not to block/suspend.
		bool is_ready() const noexcept
		{
			std::cout << __PRETTY_FUNCTION__ << "#" << m_coroutine.promise().number << std::endl;
			return !m_coroutine || m_coroutine.done();
		}

		auto operator co_await() const &noexcept
		{
			struct awaitable : awaitable_base
			{
				using awaitable_base::awaitable_base;

				decltype(auto) await_resume()
				{
					if (!this->m_coroutine)
					{
						throw broken_promise{};
					}

					return this->m_coroutine.promise().result();
				}
			};

			return awaitable{m_coroutine};
		}

		auto operator co_await() const &&noexcept
		{
			struct awaitable : awaitable_base
			{
				using awaitable_base::awaitable_base;

				decltype(auto) await_resume()
				{
					if (!this->m_coroutine)
					{
						throw broken_promise{};
					}
					return std::move(this->m_coroutine.promise()).result();
				}
			};

			return awaitable{m_coroutine};
		}

		/// \brief
		/// Returns an awaitable that will await completion of the task without
		/// attempting to retrieve the result.
		/// todo: https://github.com/lewissbaker/cppcoro/blob/a87e97fe5b6091ca9f6de4637736b8e0d8b109cf/test/io_service_tests.cpp#L178
		auto when_ready() const noexcept
		{
			struct awaitable : awaitable_base
			{
				using awaitable_base::awaitable_base;

				void await_resume() const noexcept
				{
					std::cout << __PRETTY_FUNCTION__ << "#" << m_coroutine.promise.number << std::endl;
				}
			};

			return awaitable{m_coroutine};
		}

	protected:
		std::coroutine_handle<promise_type> m_coroutine;
	};




	

	template <typename T = void>
	class [[nodiscard]] task : public disposable_task<T>
	{
		using disposable_task<T>::disposable_task;
	public:
	/*	using promise_type = detail::task_promise<T>;

		using value_type = T;

		task() noexcept: disposable_task<T>() 
		{
			std::cout << __PRETTY_FUNCTION__ << std::endl;
		}

		explicit task(std::coroutine_handle<promise_type> coroutine) noexcept : disposable_task<T>(coroutine)
		{
		}

		task(task &&t) noexcept : disposable_task<T>(t)
		{
		}*/

		~task()
		{
			std::cout << __PRETTY_FUNCTION__ << std::endl;
			if (this->m_coroutine)
			{
				std::cout << "have you finished ? " << this->m_coroutine.done() << std::endl;
				this->m_coroutine.destroy();
				std::cout << "anyway, i've just destroyed m_coroutine, i'm not disposable" << std::endl;
			}
		}
	};

	namespace detail
	{
		template <typename T>
		disposable_task<T> task_promise<T>::get_return_object() noexcept
		{
			return disposable_task<T>{std::coroutine_handle<task_promise>::from_promise(*this)};
		}

		inline disposable_task<void> task_promise<void>::get_return_object() noexcept
		{
			return disposable_task<void>{std::coroutine_handle<task_promise>::from_promise(*this)};
		}

		template <typename T>
		disposable_task<T &> task_promise<T &>::get_return_object() noexcept
		{
			return disposable_task<T &>{std::coroutine_handle<task_promise>::from_promise(*this)};
		}
	}

	template <typename AWAITABLE>
	auto make_task(AWAITABLE awaitable)
		-> disposable_task<detail::remove_rvalue_reference_t<typename awaitable_traits<AWAITABLE>::await_result_t>>
	{
		co_return co_await static_cast<AWAITABLE &&>(awaitable);
	}
}

#endif
