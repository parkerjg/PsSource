#ifndef COPY_NUMBER_PS_SOURCE_ENVIRONMENT_PROVIDER_HH
#define COPY_NUMBER_PS_SOURCE_ENVIRONMENT_PROVIDER_HH

#include "PsSourceEnvironmentProvider.hh"

class CopyNumberPsSourceEnvironmentProvider
    : public IPsSourceEnvironmentProvider {
public:
    CopyNumberPsSourceEnvironmentProvider(
        int selected_copy_number,
        const PsEnvironment& selected_environment,
        const PsEnvironment& default_environment
    )
        : m_selected_copy_number(selected_copy_number),
          m_selected_environment(selected_environment),
          m_default_environment(default_environment)
    {
    }

    PsEnvironment ResolveEnvironment(
        const PsTerminalState& terminal_state
    ) const override
    {
        if (
            terminal_state.copy_number ==
            m_selected_copy_number
        ) {
            return m_selected_environment;
        }

        return m_default_environment;
    }

private:
    int m_selected_copy_number = -1;

    PsEnvironment m_selected_environment;
    PsEnvironment m_default_environment;
};

#endif
