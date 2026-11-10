#ifndef NICOLOR_H
#define NICOLOR_H

struct NiColor {
	float m_Red;
	float m_Green;
	float m_Blue;

	constexpr NiColor(float red, float green, float blue) : m_Red(red), m_Green(green), m_Blue(blue) {}
	constexpr NiColor() : NiColor(0.0f, 0.0f, 0.0f) {}

	// Relative luminance (Rec. 709 weights) of a linear color
	constexpr float Luminance() const { return (m_Red * 0.212671f) + (m_Green * 0.71516f) + (m_Blue * 0.072169f); }
};

#endif // NICOLOR_H
