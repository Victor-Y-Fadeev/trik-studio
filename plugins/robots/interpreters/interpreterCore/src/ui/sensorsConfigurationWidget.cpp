#include "sensorsConfigurationWidget.h"

#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>

#include <interpreterBase/robotModel/robotModelInterface.h>

using namespace interpreterCore::ui;
using namespace interpreterBase::robotModel;

SensorsConfigurationWidget::SensorsConfigurationWidget(QWidget *parent, bool autosaveMode)
	: QWidget(parent)
	, mAutosaveMode(autosaveMode)
	, mLayout(new QVBoxLayout)
	, mSaving(false)
	, mRefreshing(false)
{
	mLayout->setContentsMargins(0, 0, 0, 0);
	mLayout->setMargin(0);
	mLayout->setSpacing(0);
	setLayout(mLayout);
}

void SensorsConfigurationWidget::loadRobotModels(QList<RobotModelInterface *> const &models)
{
	for (RobotModelInterface * const model : models) {
		QString const name = model->name();
		mRobotModels[name] = model;
		QWidget * const configurer = configurerForRobotModel(*model);
		mRobotModelConfigurers[name] = configurer;
		mLayout->addWidget(configurer);
	}

	hideAllConfigurers();
}

void SensorsConfigurationWidget::selectRobotModel(RobotModelInterface &robotModel)
{
	QString const robotModelId = robotModel.name();
	mCurrentModel = robotModelId;
	hideAllConfigurers();
	if (mRobotModels.contains(robotModelId)) {
		mRobotModelConfigurers[robotModelId]->show();
		refresh();
	}
}

QWidget *SensorsConfigurationWidget::configurerForRobotModel(RobotModelInterface &robotModel)
{
	/// @todo: What if robot model has no configurable sensors?
	QWidget *result = new QWidget;
	QVBoxLayout * const layout = new QVBoxLayout;
	result->setLayout(layout);
	QList<PortInfo> const configurablePorts = robotModel.configurablePorts();
	for (PortInfo const &port : configurablePorts) {
		layout->addLayout(initPort(robotModel.name(), port, robotModel.allowedDevices(port)));
	}

	return result;
}

QLayout *SensorsConfigurationWidget::initPort(QString const &robotModel
		, PortInfo const &port, QList<DeviceInfo> const &sensors)
{
	QLabel * const portLabel = new QLabel(tr("Port %1:").arg(port.name()), this);
	QComboBox * const comboBox = new QComboBox(this);
	comboBox->setProperty("robotModel", robotModel);
	comboBox->setProperty("port", QVariant::fromValue(port));
	mConfigurers << comboBox;
	comboBox->addItem(tr("Unused"), QVariant::fromValue(DeviceInfo()));
	for (DeviceInfo const &sensor : sensors) {
		comboBox->addItem(sensor.friendlyName(), QVariant::fromValue(sensor));
	}

	if (mAutosaveMode) {
		connect(comboBox, &QComboBox::currentTextChanged, this, &SensorsConfigurationWidget::save);
	}

	QHBoxLayout * const layout = new QHBoxLayout;
	layout->addWidget(portLabel);
	layout->addWidget(comboBox);
	layout->setStretch(0, 0);
	layout->setStretch(1, 1);

	return layout;
}

void SensorsConfigurationWidget::hideAllConfigurers()
{
	for (QWidget * const configurer : mRobotModelConfigurers.values()) {
		configurer->hide();
	}
}

void SensorsConfigurationWidget::onSensorConfigurationChanged(QString const &robotModel
		, PortInfo const &port, DeviceInfo const &sensor)
{
	Q_UNUSED(robotModel)
	Q_UNUSED(port)
	Q_UNUSED(sensor)

	// This method can be called when we did not accomplish processing all combo boxes during saving.
	// So ignoring such case.
	if (!mSaving) {
		refresh();
	}
}


void SensorsConfigurationWidget::refresh()
{
	mRefreshing = true;
	for (QComboBox * const box : mConfigurers) {
		PortInfo const port = box->property("port").value<PortInfo>();
		DeviceInfo const device = mCurrentConfiguration[mCurrentModel][port];
		if (device.isNull()) {
			box->setCurrentIndex(0);
		} else {
			box->setCurrentText(device.friendlyName());
		}
	}

	mRefreshing = false;
}

void SensorsConfigurationWidget::save()
{
	// Refreshing may affect combobox current index. No saving is needed then.
	if (mRefreshing) {
		return;
	}

	mSaving = true;
	for (QComboBox * const box : mConfigurers) {
		if (!box->isVisible()) {
			continue;
		}

		QString const robotModel = box->property("robotModel").toString();
		PortInfo const port = box->property("port").value<PortInfo>();
		DeviceInfo const device = box->itemData(box->currentIndex()).value<DeviceInfo>();
		if (mCurrentConfiguration[robotModel][port] != device) {
			propagateChanges(port, device);
		}
	}

	mSaving = false;
}

void SensorsConfigurationWidget::propagateChanges(PortInfo const &port, DeviceInfo const &sensor)
{
	for (QString const &robotModelId : mRobotModels.keys()) {
		RobotModelInterface const *robotModel = mRobotModels[robotModelId];
		for (PortInfo const &otherPort : robotModel->configurablePorts()) {
			if (areConvertible(port, otherPort)) {
				if (sensor.isNull()) {
					sensorConfigurationChanged(robotModelId, otherPort, DeviceInfo());
				} else {
					DeviceInfo const otherDevice = convertibleDevice(robotModel, otherPort, sensor);
					if (!otherDevice.isNull()) {
						sensorConfigurationChanged(robotModelId, otherPort, otherDevice);
					}
				}
			}
		}
	}
}

bool SensorsConfigurationWidget::areConvertible(PortInfo const &port1, PortInfo const &port2) const
{
	return port1.name() == port2.name()
			|| port1.nameAliases().contains(port2.name())
			|| port2.nameAliases().contains(port1.name());
}

DeviceInfo SensorsConfigurationWidget::convertibleDevice(RobotModelInterface const *robotModel
		, PortInfo const &port, DeviceInfo const &device) const
{
	QList<DeviceInfo> const convertibleBases = robotModel->convertibleBases();
	for (DeviceInfo const &allowedDevice : robotModel->allowedDevices(port)) {
		for (DeviceInfo const &convertibleBase : convertibleBases) {
			if (device.isA(convertibleBase) && allowedDevice.isA(convertibleBase)) {
				return allowedDevice;
			}
		}
	}

	return DeviceInfo();
}
