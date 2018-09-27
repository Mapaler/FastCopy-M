#ifndef SCOPEEXIT_H
#define SCOPEEXIT_H

// From N4189, C++ standard proposal, 2014
//  ref: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n4189.pdf
//
//   void func() {  // Example for scope_raii (renamed make_unique_resource.)
//     auto fd = scope_raii(open(...), [&](auto fd) { if (fd != -1) close(fd); });
//     if (fd == -1) return;
//
//     main_proc();
//   }
//
//   void func() {  // Example for scope_defer (extend make_scope_exit, no need temporary variable)
//     ULONG_PTR           token = NULL;
//     GdiplusStartupInput gdisi = {1};
//
//     GdiplusStartup(&token, &gdisi, NULL);
//     scope_defer([&]() { GdiplusShutdown(token); });
//
//     main_proc();
//   }

template <typename EF>
struct scope_exit {
	// construction
	explicit scope_exit(EF &&f) noexcept
		:exit_function(std::move(f))
		, execute_on_destruction{ true } {}
	// move
	scope_exit(scope_exit &&rhs) noexcept
		: exit_function(std::move(rhs.exit_function))
		, execute_on_destruction{ rhs.execute_on_destruction } {
		rhs.release();
	}
	// release
	~scope_exit() noexcept(noexcept(this->exit_function())) {
		if (execute_on_destruction)
			this->exit_function();
	}
	void release() noexcept { this->execute_on_destruction = false; }
private:
	scope_exit(scope_exit const &) = delete;
	void operator=(scope_exit const &) = delete;
	scope_exit& operator=(scope_exit &&) = delete;
	EF exit_function;
	bool execute_on_destruction; // exposition only
};
// factory function
template <typename EF>
auto make_scope_exit(EF &&exit_function) noexcept {
	return scope_exit<std::remove_reference_t<EF>>(std::forward<EF>(exit_function));
}

template<typename R, typename D>
class unique_resource {
	R resource;
	D deleter;
	bool execute_on_destruction; // exposition only
	unique_resource& operator=(unique_resource const &) = delete;
	unique_resource(unique_resource const &) = delete; // no copies!
public:
	// construction
	explicit
		unique_resource(R && resource, D && deleter, bool shouldrun = true) noexcept
		: resource(std::move(resource))
		, deleter(std::move(deleter))
		, execute_on_destruction{ shouldrun } {}
	// move
	unique_resource(unique_resource &&other) noexcept
		: resource(std::move(other.resource))
		, deleter(std::move(other.deleter))
		, execute_on_destruction{ other.execute_on_destruction } {
		other.release();
	}
	unique_resource&
		operator=(unique_resource &&other) noexcept(noexcept(this->reset())) {
		this->reset();
		this->deleter = std::move(other.deleter);
		this->resource = std::move(other.resource);
		this->execute_on_destruction = other.execute_on_destruction;
		other.release();
		return *this;
	}
	// resource release
	~unique_resource() noexcept(noexcept(this->reset())) {
		this->reset();
	}
	void reset() noexcept(noexcept(this->get_deleter()(resource))) {
		if (execute_on_destruction) {
			this->execute_on_destruction = false;
			this->get_deleter()(resource);
		}
	}
	void reset(R && newresource) noexcept(noexcept(this->reset())) {
		this->reset();
		this->resource = std::move(newresource);
		this->execute_on_destruction = true;
	}
	R const & release() noexcept {
		this->execute_on_destruction = false;
		return this->get();
	}
	// resource access
	R const & get() const noexcept {
		return this->resource;
	}
	operator R const &() const noexcept {
		return this->resource;
	}
	R operator->() const noexcept {
		return this->resource;
	}
	std::add_lvalue_reference_t<std::remove_pointer_t<R>> operator*() const {
		return *this->resource;
	}
	// deleter access
	const D &
		get_deleter() const noexcept {
		return this->deleter;
	}
};

//factories
template<typename R, typename D>
auto make_unique_resource(R && r, D &&d) noexcept {
	return unique_resource<R, std::remove_reference_t<D>>(
		std::move(r)
		, std::forward<std::remove_reference_t<D>>(d)
		, true);
}
template<typename R, typename D>
auto make_unique_resource_checked(R r, R invalid, D d) noexcept {
	bool shouldrun = not bool(r == invalid);
	return unique_resource<R, D>(std::move(r), std::move(d), shouldrun);
}

// rename and extend by H.Shirouzu
#define scope_raii       make_unique_resource
#define scope_defer      auto SCOPE_DEFER_(__LINE__) = make_scope_exit
#define SCOPE_DEFER_(l)  SCOPE_DEFER__(l)
#define SCOPE_DEFER__(l) SCOPE_DEFER___##l

#define SCOPE_NEW(v,t)   auto v = make_unique_resource(new t, [&](auto v) { delete v; })
#define SCOPE_ARRAY(v,t,n) auto v = make_unique_resource(new t[n], [&](auto v) { delete [] v; })

#endif

