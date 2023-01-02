#ifndef __BEHAVIORACTION__H__
#define __BEHAVIORACTION__H__

#include <string>

template<typename Type>
class BehaviorAction {
public:
	/**
	 * @brief Construct a BehaviorAction object with initial values
	 *
	 * @param actionName The name of the action this behavior represents
	 * @param parameterName The parameters' name.  This value is not needed if the action name is all that is needed.
	 * @param parameterValue The parameters' value, if the value is to be a string.  This value is not needed if the action name is all that is needed.
	 * @param callbackID Unused
	 */
	BehaviorAction(std::string actionName = "", std::string parameterName = "", Type parameterValue = Type(), std::string callbackID = "") {
		this->actionName = actionName;
		this->parameterName = parameterName;
		this->parameterValue = parameterValue;
		this->callbackID = callbackID;
	}

	 /**
	  * @brief Sets the actions name
	  *
	  * @param value The new action name
	  */
	void SetActionName(std::string& value) { this->actionName = value; };

	/**
	 * @brief Gets the actions name
	 *
	 * @return This actions name
	 */
	std::string GetActionName() const { return actionName; };

	/**
	 * @brief Sets the parameter name for this action.
	 *
	 * @param value The actions' new name.
	 */
	void SetParameterName(std::string& value) { this->parameterName = value; };

	/**
	 * @brief Gets the parameter name for this action.
	 *
	 * @return The actions' name.
	 */
	std::string GetParameterName() const { return parameterName; };

	/**
	 * @brief Sets the actions' string value.
	 *
	 * @param value The new parameter value.
	 */
	void SetParameterValue(Type& value) { this->parameterValueString = value; };

	/**
	 * @brief Gets this actions' string value.
	 *
	 * @return The actions' string value.
	 */
	Type GetParameterValue() const { return parameterValue; };

	/**
	 * Unused parameter.  Always serialized as an empty string. __callbackID__
	 */
	void SetCallbackID(std::string& value) { this->callbackID = value; };

	/**
	 * Unused parameter.  Always serialized as an empty string. __callbackID__
	 */
	std::string GetCallbackID() const { return callbackID; };

private:
	std::string actionName;                 //!< The name of the action this behavior performs
	std::string parameterName;              //!< The (optional) name of the parameter this action has
	Type parameterValue;					//!< The (optional) parameters value
	std::string callbackID;            		//!< Unknown what this is used for.  Kept in code as it is in packet captures
};

#endif  //!__BEHAVIORACTION__H__
