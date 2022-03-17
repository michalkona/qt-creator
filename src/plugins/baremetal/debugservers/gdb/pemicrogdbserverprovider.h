#pragma once

#include "gdbserverprovider.h"

QT_BEGIN_NAMESPACE
    class QPlainTextEdit;
QT_END_NAMESPACE
    
    namespace Utils { class PathChooser; }

namespace BareMetal {
namespace Internal {

class PEMicroGdbServerProvider final : public GdbServerProvider
{
public:
    QVariantMap toMap() const final;
    bool fromMap(const QVariantMap &data) final;
    
    bool operator==(const IDebugServerProvider &other) const final;
    
    QString channelString() const final;
    Utils::CommandLine command() const final;
    
    QSet<StartupMode> supportedStartupModes() const final;
    bool isValid() const final;
    
private:
    PEMicroGdbServerProvider();
    
    static QString defaultInitCommands();
    static QString defaultResetCommands();
    
    Utils::FilePath m_executableFile;
    QString m_pemicroDevice;
    QString m_pemicroTargetIface = {"OPENSDA"};
    QString m_pemicroTargetIfaceSpeed = {"12000"};
    QString m_additionalArguments;
    
    friend class PEMicroGdbServerProviderFactory;
};

class PEMicroGdbServerProviderFactory final
    : public IDebugServerProviderFactory
{
public:
    PEMicroGdbServerProviderFactory();
};

} // namespace Internal
} // namespace BareMetal
