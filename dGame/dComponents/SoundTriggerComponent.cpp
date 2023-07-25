#include "SoundTriggerComponent.h"
#include "EntityManager.h"
#include "Game.h"

SoundTriggerComponent::SoundTriggerComponent(Entity* parent) : Component(parent) {
	const auto musicCueName = parent->GetVar<std::string>(u"NDAudioMusicCue_Name");
	const auto musicCueBoredomTime = parent->GetVar<float>(u"NDAudioMusicCue_BoredomTime");
	this->m_MusicCues.push_back(MusicCue(musicCueName, musicCueBoredomTime));

	const auto musicParemeterName = parent->GetVar<std::string>(u"NDAudioMusicParameter_Name");
	const auto musicParemeterValue = parent->GetVar<float>(u"NDAudioMusicParameter_Value");
	this->m_MusicParameters.push_back(MusicParemeter( musicParemeterName, musicParemeterValue));

	const auto guidString = parent->GetVar<std::string>(u"NDAudioEventGUID");
	if (!guidString.empty()) this->m_2DAmbientSounds.push_back(GUIDResults(guidString));

	const auto guid2String = parent->GetVar<std::string>(u"NDAudioEventGUID2");
	if (!guid2String.empty()) this->m_3DAmbientSounds.push_back(GUIDResults(guid2String));

	const auto mixerName = parent->GetVar<std::string>(u"NDAudioMixerProgram_Name");
	this->m_MixerPrograms.push_back(MixerProgram(mixerName));
}

void SoundTriggerComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	if (bIsInitialUpdate) this->m_Dirty = true;

	outBitStream->Write(this->m_Dirty);

	if (this->m_Dirty) {
		outBitStream->Write<uint8_t>(this->m_MusicCues.size());
		for (auto& musicCue : this->m_MusicCues) {
			musicCue.Serialize(outBitStream);
		}

		outBitStream->Write<uint8_t>(this->m_MusicParameters.size());
		for (auto& musicParam : this->m_MusicParameters) {
			musicParam.Serialize(outBitStream);
		}

		outBitStream->Write<uint8_t>(this->m_2DAmbientSounds.size());
		for (auto twoDAmbientSound : this->m_2DAmbientSounds) {
			twoDAmbientSound.Serialize(outBitStream);
		}

		outBitStream->Write<uint8_t>(this->m_3DAmbientSounds.size());
		for (auto threeDAmbientSound : this->m_3DAmbientSounds) {
			threeDAmbientSound.Serialize(outBitStream);
		}

		outBitStream->Write<uint8_t>(this->m_MixerPrograms.size());
		for (auto& mixerProgram : this->m_MixerPrograms) {
			mixerProgram.Serialize(outBitStream);
		}

		this->m_Dirty = false;
	}
}

void SoundTriggerComponent::ActivateMusicCue(const std::string& name) {
	if (std::find_if(this->m_MusicCues.begin(), this->m_MusicCues.end(), [name](const MusicCue& musicCue) {
		return  musicCue.name == name;
		}) == this->m_MusicCues.end()) {
		this->m_MusicCues.push_back(MusicCue(name, -1.0f));
		this->m_Dirty = true;
		Game::entityManager->SerializeEntity(m_Parent);
	}
}

void SoundTriggerComponent::DeactivateMusicCue(const std::string& name) {
	const auto musicCue = std::find_if(this->m_MusicCues.begin(), this->m_MusicCues.end(), [name](const MusicCue& musicCue) {
		return  musicCue.name == name;
		});

	if (musicCue != this->m_MusicCues.end()) {
		this->m_MusicCues.erase(musicCue);
		this->m_Dirty = true;
		Game::entityManager->SerializeEntity(m_Parent);
	}
}
