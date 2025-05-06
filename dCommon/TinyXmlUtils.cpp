#include "TinyXmlUtils.h"

#include <tinyxml2.h>

using namespace TinyXmlUtils;

Element DocumentReader::operator[](const std::string_view elem) const {
	return Element(m_Doc.FirstChildElement(elem.empty() ? nullptr : elem.data()), elem);
}

Element::Element(tinyxml2::XMLElement* xmlElem, const std::string_view elem) :
	m_IteratedName{ elem },
	m_Elem{ xmlElem } {
}

Element Element::operator[](const std::string_view elem) const {
	const auto* usedElem = elem.empty() ? nullptr : elem.data();
	auto* toReturn = m_Elem ? m_Elem->FirstChildElement(usedElem) : nullptr;
	return Element(toReturn, m_IteratedName);
}

ElementIterator Element::begin() {
	return ElementIterator(m_Elem ? m_Elem->FirstChildElement() : nullptr);
}

ElementIterator Element::end() {
	return ElementIterator(nullptr);
}

ElementIterator::ElementIterator(tinyxml2::XMLElement* elem) :
	m_CurElem{ elem } {
}

ElementIterator& ElementIterator::operator++() {
	if (m_CurElem) m_CurElem = m_CurElem->NextSiblingElement();
	return *this;
}
