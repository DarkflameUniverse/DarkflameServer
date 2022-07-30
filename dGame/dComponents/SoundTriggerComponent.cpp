#include "SoundTriggerComponent.h"
#include "EntityManager.h"
#include "Game.h"

SoundTriggerComponent::SoundTriggerComponent(Entity* parent) : Component(parent) {
	const auto musicCueName = parent->GetVar<std::string>(u"NDAudioMusicCue_Name");
	const auto musicCueBoredomTime = parent->GetVar<float>(u"NDAudioMusicCue_BoredomTime");

	this->musicCues.push_back({
		musicCueName,
		1,
		musicCueBoredomTime
		});

	const auto mixerName = parent->GetVar<std::string>(u"NDAudioMixerProgram_Name");
	this->mixerPrograms.push_back(mixerName);

	const auto guid2String = parent->GetVar<std::string>(u"NDAudioEventGUID2");
	if (!guid2String.empty()) {
		this->guids.emplace_back(guid2String);
	}
}

SoundTriggerComponent::~SoundTriggerComponent() = default;

void SoundTriggerComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	if (bIsInitialUpdate)
		dirty = true;

	outBitStream->Write(dirty);

	if (dirty) {
		outBitStream->Write<uint8_t>(this->musicCues.size());
		for (const auto& musicCue : this->musicCues) {
			outBitStream->Write<uint8_t>(musicCue.name.size());
			outBitStream->Write(musicCue.name.c_str(), musicCue.name.size());
			outBitStream->Write<uint32_t>(musicCue.result);
			outBitStream->Write<float_t>(musicCue.boredomTime);
		}

		// Unknown part
		outBitStream->Write<uint16_t>(0);

		// GUID part
		outBitStream->Write<uint8_t>(this->guids.size());

		for (const auto guid : this->guids) {
			outBitStream->Write<uint32_t>(guid.GetData1());
			outBitStream->Write<uint16_t>(guid.GetData2());
			outBitStream->Write<uint16_t>(guid.GetData3());
			for (const auto& guidSubPart : guid.GetData4()) {
				outBitStream->Write<uint8_t>(guidSubPart);
			}
			outBitStream->Write<uint32_t>(1); // Unknown
		}

		// Mixer program part
		outBitStream->Write<uint8_t>(this->mixerPrograms.size());
		for (const auto& mixerProgram : mixerPrograms) {
			outBitStream->Write<uint8_t>(mixerProgram.size());
			outBitStream->Write(mixerProgram.c_str(), mixerProgram.size());
			outBitStream->Write<uint32_t>(1); // Unknown
		}

		dirty = false;
	}
}

void SoundTriggerComponent::ActivateMusicCue(const std::string& name) {
	if (std::find_if(this->musicCues.begin(), this->musicCues.end(), [name](const MusicCue& musicCue) {
		return  musicCue.name == name;
		}) == this->musicCues.end()) {
		this->musicCues.push_back({
			name,
			1,
			-1.0f
			});
		dirty = true;
		EntityManager::Instance()->SerializeEntity(m_Parent);
	}
}

void SoundTriggerComponent::DeactivateMusicCue(const std::string& name) {
	const auto musicCue = std::find_if(this->musicCues.begin(), this->musicCues.end(), [name](const MusicCue& musicCue) {
		return  musicCue.name == name;
		});

	if (musicCue != this->musicCues.end()) {
		this->musicCues.erase(musicCue);
		dirty = true;
		EntityManager::Instance()->SerializeEntity(m_Parent);
	}
}
