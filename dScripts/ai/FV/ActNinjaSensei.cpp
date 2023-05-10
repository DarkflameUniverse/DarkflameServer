#include "ActNinjaSensei.h"
#include "Entity.h"
#include "EntityManager.h"
#include "GameMessages.h"

void ActNinjaSensei::OnStartup(Entity* self) {
	auto students = EntityManager::Instance()->GetEntitiesInGroup(this->m_StudentGroup);
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
        	if (student) GameMessages::SendPlayAnimation(student, u"crane");
        }
		GameMessages::SendPlayAnimation(self, u"crane");
        self->AddTimer("bow", 15.33);
    }

    if (timerName == "bow") {
        GameMessages::SendPlayAnimation(self, u"bow");
        for (auto student : students) {
        	if (student) GameMessages::SendPlayAnimation(student, u"bow");
        }
        GameMessages::SendPlayAnimation(self, u"bow");
    	self->AddTimer("tiger", 5);
    }

    if (timerName == "tiger") {
        GameMessages::SendPlayAnimation(self, u"tiger");
        for (auto student : students) {
        	if (student) GameMessages::SendPlayAnimation(student, u"tiger");
        }
        GameMessages::SendPlayAnimation(self, u"tiger");
    	self->AddTimer("bow2", 15.33);
    }

    if (timerName == "bow2") {
        GameMessages::SendPlayAnimation(self, u"bow");
        for (auto student : students) {
        	if (student) GameMessages::SendPlayAnimation(student, u"bow");
        }
        GameMessages::SendPlayAnimation(self, u"bow");
    	self->AddTimer("mantis", 5);
    }

    if (timerName == "mantis") {
        GameMessages::SendPlayAnimation(self, u"mantis");
        for (auto student : students) {
        	if (student) GameMessages::SendPlayAnimation(student, u"mantis");
        }
        GameMessages::SendPlayAnimation(self, u"mantis");
    	self->AddTimer("bow3", 15.3);
    }

    if (timerName == "bow3") {
        GameMessages::SendPlayAnimation(self, u"bow");
        for (auto student : students) {
        	if (student) GameMessages::SendPlayAnimation(student, u"bow");
        }
        GameMessages::SendPlayAnimation(self, u"bow");
   	 self->AddTimer("repeat", 5);
    }

    if (timerName == "repeat") {
        self->CancelAllTimers();
		self->AddTimer("crane", 5);
    }
}

