#pragma once

/// @author: ccat

#if defined(__cplusplus) && __cplusplus >= 201703L || defined(_MSVC_LANG) && _MSVC_LANG >= 201703L
#include <type_traits>
#include <variant>
#include <optional>
#include <stdexcept>

namespace ccat {

template<typename T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

template<typename T, typename E>
class expected;

template<typename E>
class unexpected;

template<typename X>
struct is_template_expected_instance_class {
    constexpr static bool value = false;
};
template<typename X, typename Y>
struct is_template_expected_instance_class<expected<X, Y>> {
    constexpr static bool value = true;
};
template<typename X>
constexpr bool is_template_expected_instance_class_v = is_template_expected_instance_class<X>::value;

template<typename X>
struct is_template_unexpected_instance_class {
    constexpr static bool value = false;
};
template<typename X>
struct is_template_unexpected_instance_class<unexpected<X>> {
    constexpr static bool value = true;
};
template<typename X>
constexpr bool is_template_unexpected_instance_class_v = is_template_unexpected_instance_class<X>::value;

class bad_expected_access : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct unexpect_t {
    explicit unexpect_t() = default;
};
inline constexpr unexpect_t unexpect{};
using std::in_place_t;
using std::in_place;

template<typename E>
class unexpected {
    static_assert(std::is_object_v<E>, "type `E` must be an object-type");
    static_assert(!std::is_array_v<E>, "type `E` can't be an array-type");
    static_assert(!std::is_const_v<E> && !std::is_volatile_v<E>, "cv qualifiers can't be applied to type `E`");
    static_assert(!is_template_unexpected_instance_class_v<E>, "type `E` can't be an `unexpected`");
public:
    unexpected(const unexpected&) = default;
    unexpected(unexpected&&) = default;
    ~unexpected() = default;

    template<typename Err = E>
    explicit unexpected(Err&& e_) : e(std::forward<Err>(e_)) {}
    template<typename... Args>
    explicit unexpected(std::in_place_t, Args&&... args ) : e(std::forward<Args>(args)...) {}
    template<typename U, typename... Args>
    explicit unexpected(std::in_place_t, std::initializer_list<U> il, Args&&... args ) : e(il, std::forward<Args>(args)...) {}

    auto error() & noexcept ->E& {
        return e;
    }
    auto error() const& noexcept ->const E& {
        return e;
    }
    auto error() && noexcept ->E&& {
        return std::move(e);
    }
    auto error() const&& noexcept ->const E&& {
        return std::move(e);
    }
    auto swap(unexpected& other) ->void {
        std::swap(e, other.e);
    }
    template<typename E2>
    friend auto operator==(const unexpected<E>& x, const unexpected<E2>& y) ->bool {
        return x.e == y.e;
    }
    template<typename E2>
    friend auto operator!=(const unexpected<E>& x, const unexpected<E2>& y) ->bool {
        return x.e != y.e;
    }
private:
    E e;
};

template<typename E>
unexpected(E) -> unexpected<E>;

template<typename T, typename E>
class expected {
    static_assert(std::is_destructible_v<T>, "type `T` must be destructible");
    static_assert(std::is_object_v<E>, "type `E` must be an object-type");
    static_assert(!std::is_array_v<E>, "type `E` can't be an array-type");
    static_assert(!std::is_const_v<E> && !std::is_volatile_v<E>, "cv qualifiers can't be applied to type `E`");
    static_assert(std::is_move_constructible_v<E>, "type `E` must be move-constructible");
public:
    using value_type = T;
    using error_type = E;
    using unexpected_type = unexpected<E>;
    template<typename U>
    using rebind = expected<U, error_type>;

    expected() = default;
    expected(const expected&) = default;
    expected(expected&&) = default;

    expected(const T& t) : context_(t) {}
    expected(T&& t) : context_(std::move(t)) {}

    template<typename U, typename = std::enable_if_t<std::is_convertible_v<U, T>>>
    expected(U&& u) : context_(std::in_place_index<0>, std::forward<U>(u)) {}
    template<typename G>
    expected(const unexpected<G>& e) : expected(unexpect, e.error()) {}
    template<typename G>
    expected(unexpected<G>&& e) : expected(unexpect, std::move(e.error())) {}


    template<typename... Args>
    explicit expected(std::in_place_t, Args&&... args) : context_(std::in_place_index<0>, std::forward<Args>(args)...) {}
	template<typename U, typename... Args>
    explicit expected(std::in_place_t, std::initializer_list<U> il, Args&&... args) : context_(std::in_place_index<0>, il, std::forward<Args>(args)...) {}

    template<typename... Args>
    explicit expected(unexpect_t, Args&&... args ) : context_(std::in_place_index<1>, std::forward<Args>(args)...) {}
    template<typename U, typename... Args>
    explicit expected(unexpect_t, std::initializer_list<U> il, Args&&... args ) : context_(std::in_place_index<1>, il, std::forward<Args>(args)...) {}

    ~expected() = default;

    auto operator= (const expected&) ->expected& = default;
    auto operator= (expected&&) ->expected& = default;
    template<typename G = T>
    auto operator= (G&& t) ->expected& {
        context_.template emplace<0>(std::forward<G>(t));
        return *this;
    }
    template<typename G>
    auto operator= (const unexpected<G>& other) ->expected& {
        context_.template emplace<1>(other.error());
        return *this;
    }
    template<typename G>
    auto operator= (unexpected<G>&& other ) ->expected& {
        context_.template emplace<1>(std::move(other.error()));
        return *this;
    }

    template<typename... Args>
    auto emplace(Args&&... args) noexcept ->T& {
        context_.template emplace<0>(std::forward<Args>(args)...);
        return value();
    }
	template<typename U, typename... Args>
	auto emplace(std::initializer_list<U> il, Args&&... args) noexcept ->T& {
        context_.template emplace<0>(il, std::forward<Args>(args)...);
        return value();
    }
    auto has_value() const noexcept ->bool {
        return context_.index() == 0;
    }
	explicit operator bool() const noexcept {
		return has_value();
	}

    auto error() & noexcept ->E& {
        /// @warning: if result of `has_value` is true, the behavior is undefined
        return std::get<1>(context_);
    }
	auto error() && noexcept ->E&& {
		/// @warning: if result of `has_value` is true, the behavior is undefined
		return std::move(std::get<1>(context_));
	}
	auto error() const& noexcept ->const E& {
        /// @warning: if result of `has_value` is true, the behavior is undefined
        return std::get<1>(context_);
    }
	auto error() const&& noexcept ->const E&& {
        /// @warning: if result of `has_value` is true, the behavior is undefined
        return std::move(std::get<1>(context_));
    }

    auto value() & ->T& {
        if (!has_value()) throw bad_expected_access("ccat::bad_expected_access: this expect has no value");
        return std::get<0>(context_);
    }
	auto value() && ->T&& {
        if (!has_value()) throw bad_expected_access("ccat::bad_expected_access: this expect has no value");
        return std::move(std::get<0>(context_));
    }
    auto value() const& ->const T& {
        if (!has_value()) throw bad_expected_access("ccat::bad_expected_access: this expect has no value");
        return std::get<0>(context_);
    }
	auto value() const&& ->const T&& {
        if (!has_value()) throw bad_expected_access("ccat::bad_expected_access: this expect has no value");
        return std::move(std::get<0>(context_));
    }
	template<typename U>
	auto value_or(U&& default_value) const& ->T {
		static_assert(std::is_convertible_v<U, T>, "there is no conversion from `U` to `T`");
		if (has_value()) return std::get<0>(context_);
		return std::forward<U>(default_value);
	}
	template<typename U>
	auto value_or(U&& default_value) && ->T {
		static_assert(std::is_convertible_v<U, T>, "there is no conversion from `U` to `T`");
		if (has_value()) return std::move(std::get<0>(context_));
		return std::forward<U>(default_value);
	}
    auto operator*() & noexcept ->T& {
        /// @warning: if result of `has_value` is false, the behavior is undefined
        return std::get<0>(context_);
    }
	auto operator*() && noexcept ->T&& {
        /// @warning: if result of `has_value` is false, the behavior is undefined
        return std::move(std::get<0>(context_));
    }
    auto operator*() const& noexcept ->const T& {
        /// @warning: if result of `has_value` is false, the behavior is undefined
        return std::get<0>(context_);
    }
	auto operator*() const&& noexcept ->const T&& {
        /// @warning: if result of `has_value` is false, the behavior is undefined
        return std::move(std::get<0>(context_));
    }
    auto operator->() noexcept ->T* {
        /// @warning: if result of `has_value` is false, the behavior is undefined
        return &std::get<0>(context_);
    }
    auto operator->() const noexcept ->const T* {
        /// @warning: if result of `has_value` is false, the behavior is undefined
        return &std::get<0>(context_);
    }

    template<typename X, typename Y>
    friend auto swap(expected<X, Y>&, expected<X, Y>&) noexcept ->void;

	auto swap(expected& other)  noexcept ->void {
		std::swap(context_, other.context_);
	}

    template<typename F, typename RetTy = remove_cvref_t<std::invoke_result_t<F, T&>>>
    auto and_then(F&& f) & ->RetTy {
        static_assert(std::is_invocable_v<decltype(std::forward<F>(f)), T&>, "type `F` must be able to accept `T&`");
        if (has_value())
            return std::forward<F>(f)(value());
        else
            return RetTy(unexpect, error());
    }
    template<typename F, typename RetTy = remove_cvref_t<std::invoke_result_t<F, const T&>>>
    auto and_then(F&& f) const& ->RetTy {
        static_assert(std::is_invocable_v<decltype(std::forward<F>(f)), const T&>, "type `F` must be able to accept `const T&`");
        if (has_value())
            return std::forward<F>(f)(value());
        else
            return RetTy(unexpect, error());
    }
    template<typename F, typename RetTy = remove_cvref_t<std::invoke_result_t<F, T&&>>>
    auto and_then(F&& f) && ->RetTy {
        static_assert(std::is_invocable_v<decltype(std::forward<F>(f)), T&&>, "type `F` must be able to accept `T&&`");
        if (has_value())
            return std::forward<F>(f)(std::move(value()));
        else
            return RetTy(unexpect, std::move(error()));
    }
    template<typename F, typename RetTy = remove_cvref_t<std::invoke_result_t<F, const T&&>>>
    auto and_then(F&& f) const&& ->RetTy {
        static_assert(std::is_invocable_v<decltype(std::forward<F>(f)), const T&&>, "type `F` must be able to accept `const T&&`");
        if (has_value())
            return std::forward<F>(f)(std::move(value()));
        else
            return RetTy(unexpect, std::move(error()));
    }

    template<typename F, typename RetTy = remove_cvref_t<std::invoke_result_t<F, E&>>>
    auto or_else(F&& f) & ->RetTy {
        static_assert(std::is_invocable_v<decltype(std::forward<F>(f)), E&>, "type `F` must be able to accept `E&`");
        if (has_value())
            return RetTy(std::in_place, value());
        else
            return std::forward<F>(f)(error());
    }
    template<typename F, typename RetTy = remove_cvref_t<std::invoke_result_t<F, const E&>>>
    auto or_else(F&& f) const& ->RetTy {
        static_assert(std::is_invocable_v<decltype(std::forward<F>(f)), const E&>, "type `F` must be able to accept `const E&`");
        if (has_value())
            return RetTy(std::in_place, value());
        else
            return std::forward<F>(f)(error());
    }
    template<typename F, typename RetTy = remove_cvref_t<std::invoke_result_t<F, E&&>>>
    auto or_else(F&& f) && ->RetTy {
        static_assert(std::is_invocable_v<decltype(std::forward<F>(f)), E&&>, "type `F` must be able to accept `E&&`");
        if (has_value())
            return RetTy(std::in_place, std::move(value()));
        else
            return std::forward<F>(f)(std::move(error()));
    }
    template<typename F, typename RetTy = remove_cvref_t<std::invoke_result_t<F, const E&&>>>
    auto or_else(F&& f) const&& ->RetTy {
        static_assert(std::is_invocable_v<decltype(std::forward<F>(f)), const E&&>, "type `F` must be able to accept `const E&&`");
        if (has_value())
            return RetTy(std::in_place, std::move(value()));
        else
            return std::forward<F>(f)(std::move(error()));
    }

    template<typename F, typename RetTy = expected<remove_cvref_t<std::invoke_result_t<F, T&>>, E>>
    auto transform(F&& f) & ->RetTy {
        if (has_value())
            return std::forward<F>(f)(value());
        else
            return RetTy(unexpect, error());
    }
    template<typename F, typename RetTy = expected<remove_cvref_t<std::invoke_result_t<F, const T&>>, E>>
    auto transform(F&& f) const& ->RetTy {
        if (has_value())
            return std::forward<F>(f)(value());
        else
            return RetTy(unexpect, error());
    }
    template<typename F, typename RetTy = expected<remove_cvref_t<std::invoke_result_t<F, T&&>>, E>>
    auto transform(F&& f) && ->RetTy {
        if (has_value())
            return std::forward<F>(f)(std::move(value()));
        else
            return RetTy(unexpect, std::move(error()));
    }
    template<typename F, typename RetTy = expected<remove_cvref_t<std::invoke_result_t<F, const T&&>>, E>>
    auto transform(F&& f) const&& ->RetTy {
        if (has_value())
            return std::forward<F>(f)(std::move(value()));
        else
            return RetTy(unexpect, std::move(error()));
    }

    template<typename F, typename RetTy = expected<T, remove_cvref_t<std::invoke_result_t<F, E&>>>>
    auto transform_error(F&& f) & ->RetTy {
        if (has_value())
            return value();
        else
            return RetTy(unexpect, std::forward<F>(f)(error()));
    }
    template<typename F, typename RetTy = expected<T, remove_cvref_t<std::invoke_result_t<F, const E&>>>>
    auto transform_error(F&& f) const& ->RetTy {
        if (has_value())
            return value();
        else
            return RetTy(unexpect, std::forward<F>(f)(error()));
    }
    template<typename F, typename RetTy = expected<T, remove_cvref_t<std::invoke_result_t<F, E&&>>>>
    auto transform_error(F&& f) && ->RetTy {
        if (has_value())
            return std::move(value());
        else
            return RetTy(unexpect, std::forward<F>(f)(std::move(error())));
    }
    template<typename F, typename RetTy = expected<T, remove_cvref_t<std::invoke_result_t<F, const E&&>>>>
    auto transform_error(F&& f) const&& ->RetTy {
        if (has_value())
            return std::move(value());
        else
            return RetTy(unexpect, std::forward<F>(f)(std::move(error())));
    }

private:
    std::variant<T, E> context_;
};


template<typename E>
class expected<void, E> {
    static_assert(std::is_object_v<E>, "type `E` must be an object-type");
    static_assert(!std::is_array_v<E>, "type `E` can't be an array-type");
    static_assert(!std::is_const_v<E> && !std::is_volatile_v<E>, "cv qualifiers can't be applied to type `E`");
    static_assert(std::is_move_constructible_v<E>, "type `E` must be move-constructible");
public:
    using value_type = void;
    using error_type = E;
    using unexpected_type = unexpected<E>;
    template<typename U>
    using rebind = expected<U, error_type>;

    expected() = default;
    expected(const expected&) = default;
    expected(expected&&) = default;
    explicit expected(std::in_place_t) : context_(std::nullopt) {}
    template<typename... Args>
    expected(unexpect_t, Args... args) : context_(std::forward<Args>(args)...) {};
    template<typename U, typename... Args>
    expected(unexpect_t, std::initializer_list<U> il, Args... args) : context_(il, std::forward<Args>(args)...) {};

    ~expected() = default;

    auto operator= (const expected&) ->expected& = default;
    auto operator= (expected&&) ->expected& = default;
    template<typename G>
    auto operator= (const unexpected<G>& other) ->expected& {
        context_.emplace(other.error());
        return *this;
    }
    template<typename G>
    auto operator= (unexpected<G>&& other ) ->expected& {
        context_.emplace(std::move(other.error()));
        return *this;
    }

    auto emplace() noexcept ->void {
        context_ = std::nullopt;
    }

    auto value() ->void {
        if (!has_value()) throw bad_expected_access("ccat::bad_expected_access: this expect has no value");
    }
    auto value_or() ->void {}
    auto operator*() ->void {}

    auto error() & noexcept ->E& {
        /// @warning: if result of `has_value` is true, the behavior is undefined
        return *context_;
    }
    auto error() && noexcept ->E&& {
        /// @warning: if result of `has_value` is true, the behavior is undefined
        return std::move(*context_);
    }
    auto error() const& noexcept ->const E& {
        /// @warning: if result of `has_value` is true, the behavior is undefined
        return *context_;
    }
    auto error() const&& noexcept ->const E&& {
        /// @warning: if result of `has_value` is true, the behavior is undefined
        return std::move(*context_);
    }

    auto has_value() const noexcept ->bool {
        return !context_.has_value();
    }
    explicit operator bool() const noexcept {
        return has_value();
    }

    template<typename X, typename Y>
    friend auto swap(expected<X, Y>&, expected<X, Y>&) noexcept ->void;
    
    auto swap(expected& other) noexcept ->void {
        return std::swap(context_, other.context_);
    }

    template<typename F, typename RetTy = remove_cvref_t<std::invoke_result_t<F>>>
    auto and_then(F&& f) const& ->RetTy {
        if (has_value())
            return std::forward<F>(f)();
        else
            return RetTy(unexpect, error());
    }
    template<typename F, typename RetTy = remove_cvref_t<std::invoke_result_t<F>>>
    auto and_then(F&& f) const&& ->RetTy {
        if (has_value())
            return std::forward<F>(f)();
        else
            return RetTy(unexpect, std::move(error()));
    }

    template<typename F, typename RetTy = remove_cvref_t<std::invoke_result_t<F, E&>>>
    auto or_else(F&& f) & ->RetTy {
        static_assert(std::is_invocable_v<decltype(std::forward<F>(f)), E&>, "type `F` must be able to accept `E&`");
        if (has_value())
            return RetTy(std::in_place);
        else
            return std::forward<F>(f)(error());
    }
    template<typename F, typename RetTy = remove_cvref_t<std::invoke_result_t<F, const E&>>>
    auto or_else(F&& f) const& ->RetTy {
        static_assert(std::is_invocable_v<decltype(std::forward<F>(f)), const E&>, "type `F` must be able to accept `const E&`");
        if (has_value())
            return RetTy(std::in_place);
        else
            return std::forward<F>(f)(error());
    }
    template<typename F, typename RetTy = remove_cvref_t<std::invoke_result_t<F, E&&>>>
    auto or_else(F&& f) && ->RetTy {
        static_assert(std::is_invocable_v<decltype(std::forward<F>(f)), E&&>, "type `F` must be able to accept `E&&`");
        if (has_value())
            return RetTy(std::in_place);
        else
            return std::forward<F>(f)(std::move(error()));
    }
    template<typename F, typename RetTy = remove_cvref_t<std::invoke_result_t<F, const E&&>>>
    auto or_else(F&& f) const&& ->RetTy {
        static_assert(std::is_invocable_v<decltype(std::forward<F>(f)), const E&&>, "type `F` must be able to accept `const E&&`");
        if (has_value())
            return RetTy(std::in_place);
        else
            return std::forward<F>(f)(std::move(error()));
    }

    template<typename F, typename RetTy = expected<remove_cvref_t<std::invoke_result_t<F>>, E>>
    auto transform(F&& f) const& ->RetTy {
        if (has_value())
            return std::forward<F>(f)();
        else
            return RetTy(unexpect, error());
    }
    template<typename F, typename RetTy = expected<remove_cvref_t<std::invoke_result_t<F>>, E>>
    auto transform(F&& f) const&& ->RetTy {
        if (has_value())
            return std::forward<F>(f)();
        else
            return RetTy(unexpect, std::move(error()));
    }

    template<typename F, typename RetTy = expected<void, remove_cvref_t<std::invoke_result_t<F, E&>>>>
    auto transform_error(F&& f) & ->RetTy {
        if (has_value())
            return RetTy(in_place);
        else
            return RetTy(unexpect, std::forward<F>(f)(error()));
    }
    template<typename F, typename RetTy = expected<void, remove_cvref_t<std::invoke_result_t<F, const E&>>>>
    auto transform_error(F&& f) const& ->RetTy {
        if (has_value())
            return RetTy(in_place);
        else
            return RetTy(unexpect, std::forward<F>(f)(error()));
    }
    template<typename F, typename RetTy = expected<void, remove_cvref_t<std::invoke_result_t<F, E&&>>>>
    auto transform_error(F&& f) && ->RetTy {
        if (has_value())
            return RetTy(in_place);
        else
            return RetTy(unexpect, std::forward<F>(f)(std::move(error())));
    }
    template<typename F, typename RetTy = expected<void, remove_cvref_t<std::invoke_result_t<F, const E&&>>>>
    auto transform_error(F&& f) const&& ->RetTy {
        if (has_value())
            return RetTy(in_place);
        else
            return RetTy(unexpect, std::forward<F>(f)(std::move(error())));
    }
private:
	std::optional<E> context_;
};

template<typename T, typename E>
auto swap(expected<T, E>& lhs, expected<T, E>& rhs ) noexcept ->void{
	std::swap(lhs.context_, rhs.context_);
}

}

#else
#error "minimum required c++ standard: C++17"
#endif
