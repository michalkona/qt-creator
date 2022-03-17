#include "pemicrogdbserverprovider.h"

#include <baremetal/baremetalconstants.h>
#include <baremetal/debugserverprovidermanager.h>

#include <utils/fileutils.h>
#include <utils/pathchooser.h>
#include <utils/qtcassert.h>
#include <utils/qtcprocess.h>
#include <utils/variablechooser.h>

#include <QComboBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QLabel>

using namespace Utils;

namespace BareMetal {
namespace Internal {

const char executableFileKeyC[] = "ExecutableFile";
const char pemicroDeviceKeyC[] = "PEMicroDevice";
const char pemicroTargetInterfaceKeyC[] = "PEMicroTargetInterface";
const char pemicroTargetInterfaceSpeedKeyC[] = "PEMicroTargetInterfaceSpeed";
const char additionalArgumentsKeyC[] = "AdditionalArguments";

PEMicroGdbServerProvider::PEMicroGdbServerProvider()
    : GdbServerProvider(Constants::GDBSERVER_PEMICRO_PROVIDER_ID)
{
    setInitCommands(defaultInitCommands());
    setResetCommands(defaultResetCommands());
    setChannel("localhost", 7224);
    setTypeDisplayName(GdbServerProvider::tr("PEMicro"));
}

QString PEMicroGdbServerProvider::defaultInitCommands()
{
    return {"set remote hardware-breakpoint-limit 6\n"
            "set remote hardware-watchpoint-limit 4\n"
            "monitor reset halt\n"
            "load\n"
            "monitor reset halt\n"};
}

QString PEMicroGdbServerProvider::defaultResetCommands()
{
    return {"monitor reset halt\n"};
}

QString PEMicroGdbServerProvider::channelString() const
{
    switch (startupMode()) {
    case StartupOnNetwork:
        // Just return as "host:port" form.
        return GdbServerProvider::channelString();
    default: // wrong
        return {};
    }
}

CommandLine PEMicroGdbServerProvider::command() const
{
    CommandLine cmd{m_executableFile};

    if (startupMode() == StartupOnNetwork) {
        cmd.addArgs("-serverport=" + QString::number(channel().port()), CommandLine::Raw);
    }
    
    cmd.addArgs("-startserver -singlesession", CommandLine::Raw);

    if (!m_pemicroTargetIface.isEmpty()) {
        cmd.addArgs("-interface=" + m_pemicroTargetIface, CommandLine::Raw);
        if (!m_pemicroTargetIfaceSpeed.isEmpty())
            cmd.addArgs("-speed=" + m_pemicroTargetIfaceSpeed, CommandLine::Raw);
    }

    if (!m_pemicroDevice.isEmpty()) {
        cmd.addArgs("-device=" + m_pemicroDevice, CommandLine::Raw);
    }

    if (!m_additionalArguments.isEmpty()) {
        cmd.addArgs(m_additionalArguments, CommandLine::Raw);
    }
    
    return cmd;
}

QSet<GdbServerProvider::StartupMode>
PEMicroGdbServerProvider::supportedStartupModes() const
{
    return {StartupOnNetwork};
}

bool PEMicroGdbServerProvider::isValid() const
{
    if (!GdbServerProvider::isValid())
        return false;
    
    const StartupMode m = startupMode();
    
    if (m == StartupOnNetwork) {
        if (channel().host().isEmpty())
            return false;
    }
    
    return true;
}

QVariantMap PEMicroGdbServerProvider::toMap() const
{
    QVariantMap data = GdbServerProvider::toMap();
    data.insert(executableFileKeyC, m_executableFile.toVariant());
    data.insert(pemicroDeviceKeyC, m_pemicroDevice);
    data.insert(pemicroTargetInterfaceKeyC, m_pemicroTargetIface);
    data.insert(pemicroTargetInterfaceSpeedKeyC, m_pemicroTargetIfaceSpeed);
    data.insert(additionalArgumentsKeyC, m_additionalArguments);
    return data;
}

bool PEMicroGdbServerProvider::fromMap(const QVariantMap &data)
{
    if (!GdbServerProvider::fromMap(data))
        return false;
    
    m_executableFile = FilePath::fromVariant(data.value(executableFileKeyC));
    m_pemicroDevice = data.value(pemicroDeviceKeyC).toString();
    m_additionalArguments = data.value(additionalArgumentsKeyC).toString();
    m_pemicroTargetIface = data.value(pemicroTargetInterfaceKeyC).toString();
    m_pemicroTargetIfaceSpeed = data.value(pemicroTargetInterfaceSpeedKeyC).toString();
    return true;
}

bool PEMicroGdbServerProvider::operator==(const IDebugServerProvider &other) const
{
    if (!GdbServerProvider::operator==(other))
        return false;
    
    const auto p = static_cast<const PEMicroGdbServerProvider *>(&other);
    return m_executableFile == p->m_executableFile
           && m_additionalArguments == p->m_additionalArguments;
}

PEMicroGdbServerProviderFactory::PEMicroGdbServerProviderFactory()
{
    setId(Constants::GDBSERVER_PEMICRO_PROVIDER_ID);
    setDisplayName(GdbServerProvider::tr("PEMicro"));
    setCreator([] { return new PEMicroGdbServerProvider; });
}

} // namespace Internal
} // namespace BareMetal
