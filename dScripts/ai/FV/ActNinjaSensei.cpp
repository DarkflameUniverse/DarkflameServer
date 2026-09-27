#include "ActNinjaSensei.h"
#include "Entity.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "EffectsMessages.h"

void ActNinjaSensei::OnStartup(Entity* self) {
	auto students = Game::entityManager->GetEntitiesInGroup(this->m_StudentGroup);
	std::vector<Entity*> validStudents = {};
	for (auto* student : students) {
		if (student && student->GetLOT() == this->m_StudentLOT) validStudents.push_back(student);
	}
	self->SetVar(u"students", validStudents);
	self->AddTimer("crane", 5);
}

void ActNinjaSensei::OnTimerDone(Entity* self, std::string timerName) {
	auto students = self->GetVar<std::vector<Entity*>>(u"students");
	if (students.empty()) return;

	if (timerName == "crane") {
        for (auto student : students) {
        	if (student) GameMessages::PlayAnimation(student->GetObjectID(), u"crane").Send(UNASSIGNED_SYSTEM_ADDRESS);
        }
		GameMessages::PlayAnimation(self->GetObjectID(), u"crane").Send(UNASSIGNED_SYSTEM_ADDRESS);
        self->AddTimer("bow", 15.33);
    }

    if (timerName == "bow") {
        GameMessages::PlayAnimation(self->GetObjectID(), u"bow").Send(UNASSIGNED_SYSTEM_ADDRESS);
        for (auto student : students) {
        	if (student) GameMessages::PlayAnimation(student->GetObjectID(), u"bow").Send(UNASSIGNED_SYSTEM_ADDRESS);
        }
        GameMessages::PlayAnimation(self->GetObjectID(), u"bow").Send(UNASSIGNED_SYSTEM_ADDRESS);
    	self->AddTimer("tiger", 5);
    }

    if (timerName == "tiger") {
        GameMessages::PlayAnimation(self->GetObjectID(), u"tiger").Send(UNASSIGNED_SYSTEM_ADDRESS);
        for (auto student : students) {
        	if (student) GameMessages::PlayAnimation(student->GetObjectID(), u"tiger").Send(UNASSIGNED_SYSTEM_ADDRESS);
        }
        GameMessages::PlayAnimation(self->GetObjectID(), u"tiger").Send(UNASSIGNED_SYSTEM_ADDRESS);
    	self->AddTimer("bow2", 15.33);
    }

    if (timerName == "bow2") {
        GameMessages::PlayAnimation(self->GetObjectID(), u"bow").Send(UNASSIGNED_SYSTEM_ADDRESS);
        for (auto student : students) {
        	if (student) GameMessages::PlayAnimation(student->GetObjectID(), u"bow").Send(UNASSIGNED_SYSTEM_ADDRESS);
        }
        GameMessages::PlayAnimation(self->GetObjectID(), u"bow").Send(UNASSIGNED_SYSTEM_ADDRESS);
    	self->AddTimer("mantis", 5);
    }

    if (timerName == "mantis") {
        GameMessages::PlayAnimation(self->GetObjectID(), u"mantis").Send(UNASSIGNED_SYSTEM_ADDRESS);
        for (auto student : students) {
        	if (student) GameMessages::PlayAnimation(student->GetObjectID(), u"mantis").Send(UNASSIGNED_SYSTEM_ADDRESS);
        }
        GameMessages::PlayAnimation(self->GetObjectID(), u"mantis").Send(UNASSIGNED_SYSTEM_ADDRESS);
    	self->AddTimer("bow3", 15.3);
    }

    if (timerName == "bow3") {
        GameMessages::PlayAnimation(self->GetObjectID(), u"bow").Send(UNASSIGNED_SYSTEM_ADDRESS);
        for (auto student : students) {
        	if (student) GameMessages::PlayAnimation(student->GetObjectID(), u"bow").Send(UNASSIGNED_SYSTEM_ADDRESS);
        }
        GameMessages::PlayAnimation(self->GetObjectID(), u"bow").Send(UNASSIGNED_SYSTEM_ADDRESS);
   	 self->AddTimer("repeat", 5);
    }

    if (timerName == "repeat") {
        self->CancelAllTimers();
		self->AddTimer("crane", 5);
    }
}

