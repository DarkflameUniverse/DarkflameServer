#ifndef NICOLOR_H
#define NICOLOR_H

struct NiColor {
    float m_Red;
    float m_Green;
    float m_Blue;

    NiColor(float red, float green, float blue) : m_Red(red), m_Green(green), m_Blue(blue) {}
    NiColor() : NiColor(0.0f, 0.0f, 0.0f) {}

    /* reduce RGB files to grayscale, with or without alpha
     * using the equation given in Poynton's ColorFAQ at
     * <http://www.inforamp.net/~poynton/> // dead link
     * Copyright (c) 1998-01-04 Charles Poynton poynton at inforamp.net
     *
     *     Y = 0.212671 * R + 0.715160 * G + 0.072169 * B
     *
     *  We approximate this with
     *
     *     Y = 0.21268 * R    + 0.7151 * G    + 0.07217 * B
     *
     *  which can be expressed with integers as
     *
     *     Y = (6969 * R + 23434 * G + 2365 * B)/32768
     *
     *  The calculation is to be done in a linear colorspace.
     *
     *  Other integer coefficents can be used via png_set_rgb_to_gray().
     */
    float ToXYZ() const { return (m_Red * 0.212671f) + (m_Green * 0.71516f) + (m_Blue * 0.072169f); };
};

#endif // NICOLOR_H
