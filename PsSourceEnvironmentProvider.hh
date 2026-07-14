#ifndef PS_SOURCE_ENVIRONMENT_PROVIDER_HH
#define PS_SOURCE_ENVIRONMENT_PROVIDER_HH

#include "PsSourceTypes.hh"
#include "PsTerminalState.hh"

class IPsSourceEnvironmentProvider {
public:
    virtual ~IPsSourceEnvironmentProvider() = default;

    virtual PsEnvironment ResolveEnvironment(
        const PsTerminalState& terminal_state
    ) const = 0;
};

#endif
