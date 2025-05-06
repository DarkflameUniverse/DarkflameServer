// Darkflame Universe
// Copyright 2025

#ifndef TINYXMLUTILS_H
#define TINYXMLUTILS_H

#include <string>

#include "DluAssert.h"

#include <tinyxml2.h>

namespace TinyXmlUtils {
	// See cstdlib for iterator technicalities
	struct ElementIterator {
		ElementIterator(tinyxml2::XMLElement* elem);

		ElementIterator& operator++();
		[[nodiscard]] tinyxml2::XMLElement* operator->() { DluAssert(m_CurElem); return m_CurElem; }
		[[nodiscard]] tinyxml2::XMLElement& operator*() { DluAssert(m_CurElem); return *m_CurElem; }

		bool operator==(const ElementIterator& other) const { return other.m_CurElem == m_CurElem; }

	private:
		tinyxml2::XMLElement* m_CurElem{ nullptr };
	};

	// Wrapper class to act as an iterator over xml elements.
	// All the normal rules that apply to Iterators in the std library apply here.
	class Element {
	public:
		Element(tinyxml2::XMLElement* xmlElem, const std::string_view elem);

		// The first child element of this element.
		[[nodiscard]] ElementIterator begin();

		// Always returns an ElementIterator which points to nullptr.
		// TinyXml2 return NULL when you've reached the last child element so
		// you can't do any funny one past end logic here.
		[[nodiscard]] ElementIterator end();

		// Get a child element
		[[nodiscard]] Element operator[](const std::string_view elem) const;
		[[nodiscard]] Element operator[](const char* elem) const { return operator[](std::string_view(elem)); };

		// Whether or not data exists for this element
		operator bool() const { return m_Elem != nullptr; }

		[[nodiscard]] const tinyxml2::XMLElement* operator->() const { return m_Elem; }
	private:
		const char* GetElementName() const { return m_IteratedName.empty() ? nullptr : m_IteratedName.c_str(); }
		const std::string m_IteratedName;
		tinyxml2::XMLElement* m_Elem;
	};

	class DocumentReader {
	public:
		DocumentReader(tinyxml2::XMLDocument& doc) : m_Doc{ doc } {}

		[[nodiscard]] Element operator[](const std::string_view elem) const;
	private:
		tinyxml2::XMLDocument& m_Doc;
	};
};

#endif  //!TINYXMLUTILS_H
