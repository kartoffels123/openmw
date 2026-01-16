#ifndef MWLUA_VIDEOBINDINGS_H
#define MWLUA_VIDEOBINDINGS_H

#include <sol/forward.hpp>

namespace MWLua
{
    struct Context;

    sol::table initVideoPackage(const Context& context);
}

#endif // MWLUA_VIDEOBINDINGS_H
