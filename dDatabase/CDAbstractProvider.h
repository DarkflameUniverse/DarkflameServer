#pragma once

#include "GeneralUtils.h"

#include "Game.h"
#include "dLogger.h"
#include "dServer.h"

#include "CDTable.h"

template <
    typename KeyType,
    typename MappedType
>
class CDAbstractProvider
{
public:
    virtual void LoadClient() = 0;
    virtual void LoadHost() = 0;

    virtual const MappedType& GetEntry(const KeyType& key, const MappedType& defaultValue) = 0;
};
