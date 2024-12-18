#include "Core.h"

namespace dECS {
    class Iter {
    public:
        [[nodiscard]]
        bool Next();

    private:
        WeakWorldPtr m_World;
    };
}
