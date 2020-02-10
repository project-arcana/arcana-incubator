#pragma once

#include <clean-core/macros.hh>

#include "lib/Remotery.h"

/// some very simple convenience wrappers for the bare remotery API
/// adheres to RMT_ENABLED macro, CPU only and no flags support for now
///
/// instance:
///
///     // RAII remotery instance, no copy, no move
///     inc::RmtInstance rmt_inst;
///
/// scoped trace:
///
///     {
///         INC_RMT_TRACE_NAMED("scope_name");
///
///         // ...
///     }
///
/// scoped trace with pretty_func naming:
///
///     struct Foo
///     {
///         void bar() {
///            INC_RMT_TRACE(); // will be named "Foo::bar"
///
///             // ...
///         };
///     };
///
///

namespace inc
{
class RmtInstance
{
    RmtInstance(RmtInstance&&) = delete;
    RmtInstance(RmtInstance const&) = delete;

#if RMT_ENABLED

public:
    RmtInstance() { _rmt_CreateGlobalInstance(&mInstance); }
    ~RmtInstance() { _rmt_DestroyGlobalInstance(mInstance); }

private:
    ::Remotery* mInstance = nullptr;

#else

public:
    RmtInstance() = default;
    ~RmtInstance() {} // non-trivial dtor to prevent unused variable warnings

#endif
};
}

#if RMT_ENABLED

#define INC_RMT_IMPL_BEGIN_TRACE(_name_, _flags_, _varname_) \
    {                                                        \
        static ::rmtU32 _varname_ = 0;                       \
        ::_rmt_BeginCPUSample(_name_, _flags_, &_varname_);  \
    }

#define INC_RMT_TRACE_NAMED(_name_)                                                       \
    INC_RMT_IMPL_BEGIN_TRACE(_name_, 0, CC_MACRO_JOIN(_inc_rmt_samplehash_, __COUNTER__)) \
    ::rmt_EndCPUSampleOnScopeExit const CC_MACRO_JOIN(_inc_rmt_scope_exit_, __COUNTER__)

#define INC_RMT_TRACE() INC_RMT_TRACE_NAMED(CC_PRETTY_FUNC)

#else

#define INC_RMT_TRACE_NAMED(_name_) (void)0
#define INC_RMT_TRACE() (void)0

#endif
