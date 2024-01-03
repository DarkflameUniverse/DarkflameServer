#ifndef __STRIPUIPOSITION__H__
#define __STRIPUIPOSITION__H__

class AMFArrayValue;

/**
 * @brief The position of the first Action in a Strip
 * 
 */
class StripUiPosition {
public:
	StripUiPosition();
	StripUiPosition(AMFArrayValue* arguments, std::string uiKeyName = "ui");
	void SendBehaviorBlocksToClient(AMFArrayValue& args) const;
	double GetX() const { return xPosition; };
	double GetY() const { return yPosition; };
private:
	double xPosition;
	double yPosition;
};

#endif  //!__STRIPUIPOSITION__H__
