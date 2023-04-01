#include "NiPoint3.h"
#include "eTacArcMethod.h"

class Entity;

struct TacArcTargetInfo {
	float angle;
	float distance;
	float weight;
};

struct TacArcInfo {
	NiPoint3 position;
    NiPoint3 forwardVector;
    float weight = 0.0;
    float angleInDegrees;
    float minRange;
    float maxRange;
    float farWidth;
    float farHeight;
    float nearWidth;
    float nearHeight;
    bool addRadiusToPosition;
    bool bGetHeight;
    float heightLowerBound;
    float heightUpperBound;
    eTacArcMethod method;
	std::vector<Entity*> targets;
	std::map<Entity*, TacArcTargetInfo*> targetsInfo;
};
