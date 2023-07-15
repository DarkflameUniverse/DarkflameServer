#include "StatProperty.h"

#include <sstream>

StatProperty::StatProperty(eStatTypes type, eStatModifier modifier, float value) {
	this->type = type;
	this->modifier = modifier;
	this->value = value;
}

std::string StatProperty::HtmlString() {
	// "<font color=\"#38B6FF\">Physical: +20%</font>\n..."

	std::stringstream ss;
	ss << "<font color=\"";

	switch (type)
	{
	case eStatTypes::Health:
		ss << "#FF0000";
		break;
	case eStatTypes::Armor:
		ss << "#525252";
		break;
	case eStatTypes::Imagination:
		ss << "#0077FF";
		break;
	case eStatTypes::Physical:
		ss << "#FF9500";
		break;
	case eStatTypes::Electric:
		ss << "#0059FF";
		break;
	case eStatTypes::Corruption:
		ss << "#5500FF";
		break;
	case eStatTypes::Heat:
		ss << "#FF6A00";
		break;
	case eStatTypes::Shadow:
		ss << "#0D0061";
		break;
	case eStatTypes::Pierce:
		ss << "#611200";
		break;
	case eStatTypes::Vitality:
		ss << "#2D0800";
		break;
	case eStatTypes::Domination:
		ss << "#CF00A5";
		break;
	default:
		ss << "#FFFFFF";
		break;
	}

	ss << "\">";

	switch (type)
	{
	case eStatTypes::Health:
		ss << "Health";
		break;
	case eStatTypes::Armor:
		ss << "Armor";
		break;
	case eStatTypes::Imagination:
		ss << "Imagination";
		break;
	case eStatTypes::Physical:
		ss << "Physical";
		break;
	case eStatTypes::Electric:
		ss << "Electric";
		break;
	case eStatTypes::Corruption:
		ss << "Corruption";
		break;
	case eStatTypes::Heat:
		ss << "Heat";
		break;
	case eStatTypes::Shadow:
		ss << "Shadow";
		break;
	case eStatTypes::Pierce:
		ss << "Pierce";
		break;
	case eStatTypes::Vitality:
		ss << "Vitality";
		break;
	case eStatTypes::Domination:
		ss << "Domination";
		break;
	default:
		ss << "Unknown";
		break;
	}

	switch (modifier)
	{
	case eStatModifier::DamageResistance:
		ss << " Resistance";
		break;
	case eStatModifier::DamagePercent:
	case eStatModifier::DamageAbsolute:
		ss << " Damage";
		break;
	}

	ss << "</font>";

	switch (modifier)
	{
	case eStatModifier::Percent:
	case eStatModifier::DamagePercent:
	case eStatModifier::DamageResistance:
		{
			float percent = value * 100.0f;
			// Round to 2 decimal places
			percent = static_cast<float>(static_cast<int32_t>(percent * 100.0f)) / 100.0f;
			ss << ": +" << percent << "%";
		}
		break;
	case eStatModifier::Absolute:
	case eStatModifier::DamageAbsolute:
	default:
		ss << ": +" << value;
		break;
	}

	return ss.str();
}
