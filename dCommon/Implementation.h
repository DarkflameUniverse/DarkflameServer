#ifndef __IMPLEMENTATION_H__
#define __IMPLEMENTATION_H__

#include <functional>
#include <optional>

/**
 * @brief A way to defer the implementation of an action.
 * 
 * @tparam R The result of the action.
 * @tparam T The types of the arguments that the implementation requires.
 */
template <typename R, typename... T>
class Implementation {
public:
	typedef std::function<std::optional<R>(T...)> ImplementationFunction;

	/**
	 * @brief Sets the implementation of the action.
	 * 
	 * @param implementation The implementation of the action.
	 */
	void SetImplementation(const ImplementationFunction& implementation) {
		this->implementation = implementation;
	}

	/**
	 * @brief Clears the implementation of the action.
	 */
	void ClearImplementation() {
		implementation.reset();
	}

	/**
	 * @brief Checks if the implementation is set.
	 * 
	 * @return true If the implementation is set.
	 * @return false If the implementation is not set.
	 */
	bool IsSet() const {
		return implementation.has_value();
	}

	/**
	 * @brief Executes the implementation if it is set.
	 * 
	 * @param args The arguments to pass to the implementation.
	 * @return std::optional<R> The optional result of the implementation. If the result is not set, it indicates that the default action should be taken.
	 */
	std::optional<R> Execute(T... args) const {
		return IsSet() ? implementation.value()(args...) : std::nullopt;
	}

	/**
	 * @brief Exectues the implementation if it is set, otherwise returns a default value.
	 * 
	 * @param args The arguments to pass to the implementation.
	 * @param defaultValue The default value to return if the implementation is not set or should not be deferred.
	 */
	R ExecuteWithDefault(T... args, const R& defaultValue) const {
		return Execute(args...).value_or(defaultValue);
	}

	/**
	 * = operator overload.
	 */
	Implementation& operator=(const Implementation& other) {
		implementation = other.implementation;
		return *this;
	}

	/**
	 * = operator overload.
	 */
	Implementation& operator=(const ImplementationFunction& implementation) {
		this->implementation = implementation;
		return *this;
	}

	/**
	 * () operator overload.
	 */
	std::optional<R> operator()(T... args) {
		return !IsSet() ? std::nullopt : implementation(args...);
	}

private:
	std::optional<ImplementationFunction> implementation;

};

#endif  //!__IMPLEMENTATION_H__
