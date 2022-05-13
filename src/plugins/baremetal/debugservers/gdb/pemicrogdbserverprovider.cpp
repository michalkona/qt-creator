/****************************************************************************
**
** Copyright (C) 2022 Michal Kona <kona@oakrey.cz>
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

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
    setConfigurationWidgetCreator([this] { return new PEMicroGdbServerProviderConfigWidget(this); });
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

PEMicroGdbServerProviderConfigWidget::PEMicroGdbServerProviderConfigWidget(
    PEMicroGdbServerProvider *provider)
    : GdbServerProviderConfigWidget(provider)
{
    Q_ASSERT(provider);

    m_hostWidget = new HostWidget(this);
    m_mainLayout->addRow(tr("Host:"), m_hostWidget);

    m_executableFileChooser = new Utils::PathChooser;
    m_executableFileChooser->setExpectedKind(Utils::PathChooser::ExistingCommand);
    m_executableFileChooser->setCommandVersionArguments({"-showhardware"});
    if (HostOsInfo::hostOs() == OsTypeWindows) {
        m_executableFileChooser->setPromptDialogFilter(tr("PE Micro GDB Server (pegdbserver_power_console.exe)"));
        m_executableFileChooser->lineEdit()->setPlaceholderText("pegdbserver_power_console.exe");
    } else {
        m_executableFileChooser->setPromptDialogFilter(tr("PE Micro GDB Server (pegdbserver_power_console)"));
        m_executableFileChooser->lineEdit()->setPlaceholderText("pegdbserver_power_console");
    }
    m_mainLayout->addRow(tr("Executable file:"), m_executableFileChooser);

    // Target interface settings.
    m_targetInterfaceWidget = new QWidget(this);
    m_targetInterfaceComboBox = new QComboBox(m_targetInterfaceWidget);
    m_targetInterfaceSpeedLabel = new QLabel(m_targetInterfaceWidget);
    m_targetInterfaceSpeedLabel->setText(tr("Speed"));
    m_targetInterfaceSpeedComboBox = new QComboBox(m_targetInterfaceWidget);
    const auto targetInterfaceLayout = new QHBoxLayout(m_targetInterfaceWidget);
    targetInterfaceLayout->setContentsMargins(0, 0, 0, 0);
    targetInterfaceLayout->addWidget(m_targetInterfaceComboBox);
    targetInterfaceLayout->addWidget(m_targetInterfaceSpeedLabel);
    targetInterfaceLayout->addWidget(m_targetInterfaceSpeedComboBox);
    m_mainLayout->addRow(tr("Target interface:"), m_targetInterfaceWidget);

    m_pemicroDeviceLineEdit = new QLineEdit(this);
    m_mainLayout->addRow(tr("Device:"), m_pemicroDeviceLineEdit);
    
    m_additionalArgumentsTextEdit = new QPlainTextEdit(this);
    m_mainLayout->addRow(tr("Additional arguments:"), m_additionalArgumentsTextEdit);

    m_initCommandsTextEdit = new QPlainTextEdit(this);
    m_initCommandsTextEdit->setToolTip(defaultInitCommandsTooltip());
    m_mainLayout->addRow(tr("Init commands:"), m_initCommandsTextEdit);
    m_resetCommandsTextEdit = new QPlainTextEdit(this);
    m_resetCommandsTextEdit->setToolTip(defaultResetCommandsTooltip());
    m_mainLayout->addRow(tr("Reset commands:"), m_resetCommandsTextEdit);

    populateTargetInterfaces();
    populateTargetSpeeds();
    addErrorLabel();
    setFromProvider();

    const auto chooser = new VariableChooser(this);
    chooser->addSupportedWidget(m_initCommandsTextEdit);
    chooser->addSupportedWidget(m_resetCommandsTextEdit);

    connect(m_hostWidget, &HostWidget::dataChanged,
            this, &GdbServerProviderConfigWidget::dirty);
    connect(m_executableFileChooser, &Utils::PathChooser::rawPathChanged,
            this, &GdbServerProviderConfigWidget::dirty);
    connect(m_pemicroDeviceLineEdit, &QLineEdit::textChanged,
            this, &GdbServerProviderConfigWidget::dirty);
    connect(m_additionalArgumentsTextEdit, &QPlainTextEdit::textChanged,
            this, &GdbServerProviderConfigWidget::dirty);
    connect(m_initCommandsTextEdit, &QPlainTextEdit::textChanged,
            this, &GdbServerProviderConfigWidget::dirty);
    connect(m_resetCommandsTextEdit, &QPlainTextEdit::textChanged,
            this, &GdbServerProviderConfigWidget::dirty);
    connect(m_targetInterfaceComboBox, &QComboBox::currentTextChanged,
            this, &GdbServerProviderConfigWidget::dirty);
    connect(m_targetInterfaceSpeedComboBox, &QComboBox::currentTextChanged,
            this, &GdbServerProviderConfigWidget::dirty);

    connect(m_targetInterfaceComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PEMicroGdbServerProviderConfigWidget::updateAllowedControls);
    connect(m_targetInterfaceSpeedComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PEMicroGdbServerProviderConfigWidget::updateAllowedControls);
}

void PEMicroGdbServerProviderConfigWidget::apply()
{
    const auto p = static_cast<PEMicroGdbServerProvider *>(m_provider);
    Q_ASSERT(p);

    p->setChannel(m_hostWidget->channel());
    p->m_executableFile = m_executableFileChooser->filePath();
    p->m_pemicroDevice = m_pemicroDeviceLineEdit->text();
    p->m_pemicroTargetIface = m_targetInterfaceComboBox->currentData().toString();
    p->m_pemicroTargetIfaceSpeed = m_targetInterfaceSpeedComboBox->currentData().toString();
    p->m_additionalArguments = m_additionalArgumentsTextEdit->toPlainText();
    p->setInitCommands(m_initCommandsTextEdit->toPlainText());
    p->setResetCommands(m_resetCommandsTextEdit->toPlainText());
    GdbServerProviderConfigWidget::apply();
}

void PEMicroGdbServerProviderConfigWidget::discard()
{
    setFromProvider();
    GdbServerProviderConfigWidget::discard();
}

void PEMicroGdbServerProviderConfigWidget::populateTargetInterfaces()
{
    m_targetInterfaceComboBox->addItem(tr("Default"));
    m_targetInterfaceComboBox->addItem(tr("OpenSDA"), "OPENSDA");
    m_targetInterfaceComboBox->addItem(tr("USB Multi Link"), "USBMULTILINK");
}

void PEMicroGdbServerProviderConfigWidget::populateTargetSpeeds()
{
    m_targetInterfaceSpeedComboBox->addItem(tr("Default"));

    const QStringList fixedSpeeds = {"1", "5", "6", "10", "20", "30", "50", "60", "100", "200",
                                     "300", "400", "500", "600", "750", "800", "900", "1000", "1334",
                                     "1600", "2000",  "2667" ,"3200", "4000", "4800", "5000",
                                     "6000", "8000", "9600", "12000", "15000", "20000", "25000",
                                     "30000", "40000", "50000"};
    for (const auto &fixedSpeed : fixedSpeeds)
        m_targetInterfaceSpeedComboBox->addItem(tr("%1 kHz").arg(fixedSpeed), fixedSpeed);
}

void PEMicroGdbServerProviderConfigWidget::setTargetInterface(const QString &newIface)
{
    for (int index = 0; index < m_targetInterfaceComboBox->count(); ++index) {
        const auto iface = m_targetInterfaceComboBox->itemData(index).toString();
        if (iface == newIface) {
            m_targetInterfaceComboBox->setCurrentIndex(index);
            return;
        }
    }
    // Falling back to the first default entry.
    m_targetInterfaceComboBox->setCurrentIndex(0);
}

void PEMicroGdbServerProviderConfigWidget::setTargetSpeed(const QString &newSpeed)
{
    for (int index = 0; index < m_targetInterfaceSpeedComboBox->count(); ++index) {
        const auto speed = m_targetInterfaceSpeedComboBox->itemData(index).toString();
        if (speed == newSpeed) {
            m_targetInterfaceSpeedComboBox->setCurrentIndex(index);
            return;
        }
    }
    // Falling back to the first default entry.
    m_targetInterfaceSpeedComboBox->setCurrentIndex(0);
}

void PEMicroGdbServerProviderConfigWidget::updateAllowedControls()
{
    const bool isTargetIfaceDefaultSelected = !m_targetInterfaceComboBox->currentData().isValid();
    m_targetInterfaceSpeedLabel->setVisible(!isTargetIfaceDefaultSelected);
    m_targetInterfaceSpeedComboBox->setVisible(!isTargetIfaceDefaultSelected);
}

void PEMicroGdbServerProviderConfigWidget::setFromProvider()
{
    const auto p = static_cast<PEMicroGdbServerProvider *>(m_provider);
    Q_ASSERT(p);

    const QSignalBlocker blocker(this);
    m_additionalArgumentsTextEdit->setPlainText(p->m_additionalArguments);
    m_executableFileChooser->setFilePath(p->m_executableFile);
    m_hostWidget->setChannel(p->channel());
    m_initCommandsTextEdit->setPlainText(p->initCommands());
    m_pemicroDeviceLineEdit->setText(p->m_pemicroDevice);
    m_resetCommandsTextEdit->setPlainText(p->resetCommands());

    setTargetInterface(p->m_pemicroTargetIface);
    setTargetSpeed(p->m_pemicroTargetIfaceSpeed);

    updateAllowedControls();
}

} // namespace Internal
} // namespace BareMetal
