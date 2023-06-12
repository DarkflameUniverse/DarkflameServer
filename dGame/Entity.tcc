// Template definitions for Entity.

#ifndef __ENTITY__H__
#error "Include Entity.h instead of Entity.tpp"
#endif

// Access definitions
template <typename Cmpt>
Cmpt* Entity::GetComponent() const {
	const auto& componentItr = this->m_Components.find(Cmpt::ComponentType);
	return componentItr == this->m_Components.end() ? nullptr : dynamic_cast<Cmpt*>(componentItr->second.get());
}

template<typename Cmpt, typename...ConstructorValues>
Cmpt* Entity::AddComponent(ConstructorValues...arguments) {
	auto* component = GetComponent<Cmpt>();
	if (component) return dynamic_cast<Cmpt*>(component);

	auto& insertedComponent = m_Components.insert_or_assign(Cmpt::ComponentType,
		std::make_unique<Cmpt>(this, std::forward<ConstructorValues>(arguments)...)).first->second;
	return dynamic_cast<Cmpt*>(insertedComponent.get());
}

template<typename Cmpt>
void Entity::RemoveComponent() {
	m_Components.erase(Cmpt::ComponentType);
}

template<typename T>
const T& Entity::GetVar(const std::u16string& name) const {
	auto* data = GetVarData(name);

	if (data == nullptr) {
		return LDFData<T>::Default;
	}

	auto* typed = dynamic_cast<LDFData<T>*>(data);

	if (typed == nullptr) {
		return LDFData<T>::Default;
	}

	return typed->GetValue();
}

template<typename T>
T Entity::GetVarAs(const std::u16string& name) const {
	const auto data = GetVarAsString(name);

	T value;

	if (!GeneralUtils::TryParse(data, value)) {
		return LDFData<T>::Default;
	}

	return value;
}

template<typename T>
void Entity::SetVar(const std::u16string& name, const T& value) {
	auto* data = GetVarData(name);

	if (data == nullptr) {
		auto* data = new LDFData<T>(name, value);

		m_Settings.push_back(data);

		return;
	}

	auto* typed = dynamic_cast<LDFData<T>*>(data);

	if (typed == nullptr) {
		return;
	}

	typed->SetValue(value);
}

template<typename T>
void Entity::SetNetworkVar(const std::u16string& name, const T& value, const SystemAddress& sysAddr) {
	LDFData<T>* newData = nullptr;

	for (auto* data : m_NetworkSettings) {
		if (data->GetKey() != name)
			continue;

		newData = dynamic_cast<LDFData<T>*>(data);
		if (newData != nullptr) {
			newData->SetValue(value);
		} else {  // If we're changing types
			m_NetworkSettings.erase(
				std::remove(m_NetworkSettings.begin(), m_NetworkSettings.end(), data), m_NetworkSettings.end()
			);
			delete data;
		}

		break;
	}

	if (newData == nullptr) {
		newData = new LDFData<T>(name, value);
	}

	m_NetworkSettings.push_back(newData);
	SendNetworkVar(newData->GetString(true), sysAddr);
}

template<typename T>
void Entity::SetNetworkVar(const std::u16string& name, const std::vector<T>& values, const SystemAddress& sysAddr) {
	std::stringstream updates;
	auto index = 1;

	for (const auto& value : values) {
		LDFData<T>* newData = nullptr;
		const auto& indexedName = name + u"." + GeneralUtils::to_u16string(index);

		for (auto* data : m_NetworkSettings) {
			if (data->GetKey() != indexedName)
				continue;

			newData = dynamic_cast<LDFData<T>*>(data);
			newData->SetValue(value);
			break;
		}

		if (newData == nullptr) {
			newData = new LDFData<T>(indexedName, value);
		}

		m_NetworkSettings.push_back(newData);

		if (index == values.size()) {
			updates << newData->GetString(true);
		} else {
			updates << newData->GetString(true) << "\n";
		}

		index++;
	}

	SendNetworkVar(updates.str(), sysAddr);
}

template<typename T>
T Entity::GetNetworkVar(const std::u16string& name) {
	for (auto* data : m_NetworkSettings) {
		if (data == nullptr || data->GetKey() != name)
			continue;

		auto* typed = dynamic_cast<LDFData<T>*>(data);
		if (typed == nullptr)
			continue;

		return typed->GetValue();
	}

	return LDFData<T>::Default;
}
