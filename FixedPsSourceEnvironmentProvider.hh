#ifndef FIXED_PS_SOURCE_ENVIRONMENT_PROVIDER_HH
#define FIXED_PS_SOURCE_ENVIRONMENT_PROVIDER_HH

#include "PsSourceEnvironmentProvider.hh"

class FixedPsSourceEnvironmentProvider
    : public IPsSourceEnvironmentProvider {
public:
    explicit FixedPsSourceEnvironmentProvider(
        const PsEnvironment& environment
    )
        : m_environment(environment)
    {
    }

    PsEnvironment ResolveEnvironment(
        const PsTerminalState&
    ) const override
    {
        return m_environment;
    }

private:
    PsEnvironment m_environment;
};

#endif
