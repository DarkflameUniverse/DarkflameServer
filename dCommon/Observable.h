#ifndef __OBSERVABLE_H__
#define __OBSERVABLE_H__

#include <vector>
#include <functional>

/**
 * @brief An event which can be observed by multiple observers.
 * 
 * @tparam T The types of the arguments to be passed to the observers.
 */
template <typename... T>
class Observable {
public:
	typedef std::function<void(T...)> Observer;

	/**
	 * @brief Adds an observer to the event.
	 * 
	 * @param observer The observer to add.
	 */
	void AddObserver(const Observer& observer) {
		observers.push_back(observer);
	}

	/**
	 * @brief Removes an observer from the event.
	 * 
	 * @param observer The observer to remove.
	 */
	void RemoveObserver(const Observer& observer) {
		observers.erase(std::remove(observers.begin(), observers.end(), observer), observers.end());
	}

	/**
	 * @brief Notifies all observers of the event.
	 * 
	 * @param args The arguments to pass to the observers.
	 */
	void Notify(T... args) {
		for (const auto& observer : observers) {
			observer(args...);
		}
	}

	/**
	 * += operator overload.
	 */
	Observable& operator+=(const Observer& observer) {
		AddObserver(observer);
		return *this;
	}

	/**
	 * -= operator overload.
	 */
	Observable& operator-=(const Observer& observer) {
		RemoveObserver(observer);
		return *this;
	}
	
	/**
	 * () operator overload.
	 */
	void operator()(T... args) {
		Notify(args...);
	}

private:
	std::vector<Observer> observers;
};

#endif  //!__OBSERVABLE_H__