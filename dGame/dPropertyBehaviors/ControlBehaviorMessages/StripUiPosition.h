#ifndef __STRIPUIPOSITION__H__
#define __STRIPUIPOSITION__H__

class AMFArrayValue;

/**
 * @brief The position of the first Action in a Strip
 * 
 */
class StripUiPosition {
public:
	StripUiPosition() noexcept = default;
	StripUiPosition(const AMFArrayValue* arguments, const std::string& uiKeyName = "ui");
	void SendBehaviorBlocksToClient(AMFArrayValue& args) const;
	[[nodiscard]] double GetX() const noexcept { return m_XPosition; }
	[[nodiscard]] double GetY() const noexcept { return m_YPosition; }

private:
	double m_XPosition{ 0.0 };
	double m_YPosition{ 0.0 };
};

#endif  //!__STRIPUIPOSITION__H__
