#include "SoundTriggerComponent.h"
#include "Game.h"
#include "Logger.h"

void MusicCue::Serialize(RakNet::BitStream& outBitStream){
	outBitStream.Write<uint8_t>(name.size());
	outBitStream.Write(name.c_str(), name.size());
	outBitStream.Write(result);
	outBitStream.Write(boredomTime);
}

void MusicParameter::Serialize(RakNet::BitStream& outBitStream){
	outBitStream.Write<uint8_t>(name.size());
	outBitStream.Write(name.c_str(), name.size());
	outBitStream.Write(value);
}

void GUIDResults::Serialize(RakNet::BitStream& outBitStream){
	guid.Serialize(outBitStream);
	outBitStream.Write(result);
}

void MixerProgram::Serialize(RakNet::BitStream& outBitStream){
	outBitStream.Write<uint8_t>(name.size());
	outBitStream.Write(name.c_str(), name.size());
	outBitStream.Write(result);
}
SoundTriggerComponent::SoundTriggerComponent(Entity* parent) : Component(parent) {

	const auto musicCueName = parent->GetVar<std::string>(u"NDAudioMusicCue_Name");
	if (!musicCueName.empty()) {
		auto newCue = MusicCue(musicCueName);
		const auto musicCueBoredomTime = parent->GetVar<float>(u"NDAudioMusicCue_BoredomTime");
		if (musicCueBoredomTime) newCue.boredomTime = musicCueBoredomTime;
		this->m_MusicCues.push_back(newCue);
	}

	const auto musicParameterName = parent->GetVar<std::string>(u"NDAudioMusicParameter_Name");
	if (!musicParameterName.empty()) {
		auto newParams = MusicParameter(musicParameterName);
		const auto musicParameterValue = parent->GetVar<float>(u"NDAudioMusicParameter_Value");
		if (musicParameterValue) newParams.value = musicParameterValue;
		this->m_MusicParameters.push_back(newParams);
	}

	const auto guidString = parent->GetVar<std::string>(u"NDAudioEventGUID");
	if (!guidString.empty())
		this->m_2DAmbientSounds.push_back(GUIDResults(guidString));

	const auto guid2String = parent->GetVar<std::string>(u"NDAudioEventGUID2");
	if (!guid2String.empty())
		this->m_3DAmbientSounds.push_back(GUIDResults(guid2String));

	const auto mixerName = parent->GetVar<std::string>(u"NDAudioMixerProgram_Name");
	if (!mixerName.empty()) this->m_MixerPrograms.push_back(MixerProgram(mixerName));
}

void SoundTriggerComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	outBitStream.Write(this->m_Dirty || bIsInitialUpdate);
	if (this->m_Dirty || bIsInitialUpdate) {
		outBitStream.Write<uint8_t>(this->m_MusicCues.size());
		for (auto& musicCue : this->m_MusicCues) {
			musicCue.Serialize(outBitStream);
		}

		outBitStream.Write<uint8_t>(this->m_MusicParameters.size());
		for (auto& musicParam : this->m_MusicParameters) {
			musicParam.Serialize(outBitStream);
		}

		outBitStream.Write<uint8_t>(this->m_2DAmbientSounds.size());
		for (auto twoDAmbientSound : this->m_2DAmbientSounds) {
			twoDAmbientSound.Serialize(outBitStream);
		}

		outBitStream.Write<uint8_t>(this->m_3DAmbientSounds.size());
		for (auto threeDAmbientSound : this->m_3DAmbientSounds) {
			threeDAmbientSound.Serialize(outBitStream);
		}

		outBitStream.Write<uint8_t>(this->m_MixerPrograms.size());
		for (auto& mixerProgram : this->m_MixerPrograms) {
			mixerProgram.Serialize(outBitStream);
		}

		if (!bIsInitialUpdate) this->m_Dirty = false;
	}
}

void SoundTriggerComponent::ActivateMusicCue(const std::string& name, float bordemTime) {
	const auto musicCue = std::find_if(this->m_MusicCues.begin(), this->m_MusicCues.end(), [name](const MusicCue& musicCue) {
		return  musicCue.name == name;
		}
	);

	if (musicCue == this->m_MusicCues.end()) {
		this->m_MusicCues.push_back(MusicCue(name, bordemTime));
		this->m_Dirty = true;
		Game::entityManager->SerializeEntity(m_Parent);
	}
}

void SoundTriggerComponent::DeactivateMusicCue(const std::string& name) {
	const auto musicCue = std::find_if(this->m_MusicCues.begin(), this->m_MusicCues.end(), [name](const MusicCue& musicCue) {
		return  musicCue.name == name;
		}
	);

	if (musicCue != this->m_MusicCues.end()) {
		this->m_MusicCues.erase(musicCue);
		this->m_Dirty = true;
		Game::entityManager->SerializeEntity(m_Parent);
	}
}
