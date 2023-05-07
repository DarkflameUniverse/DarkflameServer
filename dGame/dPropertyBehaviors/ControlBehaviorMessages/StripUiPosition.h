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
	double GetX() { return xPosition; };
	double GetY() { return yPosition; };
private:
	double xPosition;
	double yPosition;
};

#endif  //!__STRIPUIPOSITION__H__
