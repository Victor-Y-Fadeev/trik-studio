/* Copyright 2007-2015 QReal Research Group
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. */

#include "ev3RbfGeneratorPlugin.h"

#include <QtWidgets/QApplication>
#include <QtCore/QDirIterator>
#include <QtCore/QProcess>

#include <qrutils/widgets/qRealMessageBox.h>
#include <qrkernel/logging.h>
#include <ev3Kit/communication/ev3RobotCommunicationThread.h>
#include <ev3GeneratorBase/robotModel/ev3GeneratorRobotModel.h>
#include <qrkernel/settingsManager.h>
#include "ev3RbfMasterGenerator.h"

using namespace ev3::rbf;
using namespace qReal;

Ev3RbfGeneratorPlugin::Ev3RbfGeneratorPlugin()
	: Ev3GeneratorPluginBase("Ev3RbfUsbGeneratorRobotModel", tr("Autonomous mode (USB)"), 9
			, "Ev3RbfBluetoothGeneratorRobotModel", tr("Autonomous mode (Bluetooth)"), 8)
	, mGenerateCodeAction(new QAction(nullptr))
	, mUploadProgramAction(new QAction(nullptr))
	, mRunProgramAction(new QAction(nullptr))
	, mStopRobotAction(new QAction(nullptr))
{
	mGenerateCodeAction->setText(tr("Generate to Ev3 Robot Byte Code File"));
	mGenerateCodeAction->setIcon(QIcon(":/ev3/rbf/images/generateRbfCode.svg"));
	connect(mGenerateCodeAction, &QAction::triggered, this, &Ev3RbfGeneratorPlugin::generateCode);

	mUploadProgramAction->setText(tr("Upload program"));
	mUploadProgramAction->setIcon(QIcon(":/ev3/rbf/images/uploadProgram.svg"));
	connect(mUploadProgramAction, &QAction::triggered, this, &Ev3RbfGeneratorPlugin::uploadAndRunProgram);

	mRunProgramAction->setObjectName("runEv3RbfProgram");
	mRunProgramAction->setText(tr("Run program"));
	mRunProgramAction->setIcon(QIcon(":/ev3/rbf/images/run.png"));
	connect(mRunProgramAction, &QAction::triggered, this, &Ev3RbfGeneratorPlugin::runProgram, Qt::UniqueConnection);

	mStopRobotAction->setObjectName("stopEv3RbfRobot");
	mStopRobotAction->setText(tr("Stop robot"));
	mStopRobotAction->setIcon(QIcon(":/ev3/rbf/images/stop.png"));
	connect(mStopRobotAction, &QAction::triggered, this, &Ev3RbfGeneratorPlugin::stopRobot, Qt::UniqueConnection);

	text::Languages::registerLanguage(text::LanguageInfo{ "lms"
			, tr("EV3 Source Code language")
			, true
			, 4
			, 2
			, "//"
			, QString()
			, "/*"
			, "*/"
			, nullptr
			, {}
	});
}

QList<ActionInfo> Ev3RbfGeneratorPlugin::customActions()
{
	const ActionInfo generateCodeActionInfo(mGenerateCodeAction, "generators", "tools");
	const ActionInfo uploadProgramActionInfo(mUploadProgramAction, "generators", "tools");
	const ActionInfo runProgramActionInfo(mRunProgramAction, "interpreters", "tools");
	const ActionInfo stopRobotActionInfo(mStopRobotAction, "interpreters", "tools");
	return {generateCodeActionInfo, uploadProgramActionInfo, runProgramActionInfo, stopRobotActionInfo};
}

QList<HotKeyActionInfo> Ev3RbfGeneratorPlugin::hotKeyActions()
{
	mGenerateCodeAction->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_G));
	mUploadProgramAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_U));
	mRunProgramAction->setShortcut(QKeySequence(Qt::Key_F5));
	mStopRobotAction->setShortcut(QKeySequence(Qt::SHIFT + Qt::Key_F5));

	HotKeyActionInfo generateActionInfo("Generator.GenerateEv3Rbf"
			, tr("Generate Ev3 Robot Byte Code File"), mGenerateCodeAction);
	HotKeyActionInfo uploadProgramInfo("Generator.UploadEv3", tr("Upload EV3 Program"), mUploadProgramAction);
	HotKeyActionInfo runProgramInfo("Generator.RunEv3", tr("Run EV3 Program"), mRunProgramAction);
	HotKeyActionInfo stopRobotInfo("Generator.StopEv3", tr("Stop EV3 Program"), mStopRobotAction);

	return { generateActionInfo, uploadProgramInfo, runProgramInfo, stopRobotInfo };
}

QIcon Ev3RbfGeneratorPlugin::iconForFastSelector(const kitBase::robotModel::RobotModelInterface &robotModel) const
{
	Q_UNUSED(robotModel)
	return QIcon(":/ev3/rbf/images/switch-to-ev3-rbf.svg");
}

int Ev3RbfGeneratorPlugin::priority() const
{
	return 9;
}

QString Ev3RbfGeneratorPlugin::defaultFilePath(QString const &projectName) const
{
	return QString("ev3-rbf/%1/%1.lms").arg(projectName);
}

text::LanguageInfo Ev3RbfGeneratorPlugin::language() const
{
	return text::Languages::pickByExtension("lms");
}

QString Ev3RbfGeneratorPlugin::generatorName() const
{
	return "ev3/rbf";
}

QString Ev3RbfGeneratorPlugin::uploadProgram()
{
	if (!javaInstalled()) {
		mMainWindowInterface->errorReporter()->addError(tr("<a href=\"https://java.com/ru/download/\">Java</a> is "\
				"not installed. Please download and install it."));
		return QString();
	}

	QFileInfo const fileInfo = generateCodeForProcessing();
	if (!fileInfo.exists()) {
		return QString();
	}

	if (!copySystemFiles(fileInfo.absolutePath())) {
		mMainWindowInterface->errorReporter()->addError(tr("Can't write source code files to disk!"));
		return QString();
	}

	if (!compile(fileInfo)) {
		QLOG_ERROR() << "EV3 bytecode compillation process failed!";
		mMainWindowInterface->errorReporter()->addError(tr("Compilation error occured."));
		return QString();
	}

	const QString fileOnRobot = upload(fileInfo);

	return fileOnRobot;
}

void Ev3RbfGeneratorPlugin::uploadAndRunProgram()
{
	const QString fileOnRobot = uploadProgram();
	communication::Ev3RobotCommunicationThread * const communicator = currentCommunicator();

	if (fileOnRobot.isEmpty() || !communicator) {
		return;
	}

	const RunPolicy runPolicy = static_cast<RunPolicy>(SettingsManager::value("ev3RunPolicy").toInt());
	switch (runPolicy) {
	case RunPolicy::Ask:
		if (utils::QRealMessageBox::question(mMainWindowInterface->windowWidget(), tr("The program has been uploaded")
				, tr("Do you want to run it?")) == QMessageBox::Yes) {
			QMetaObject::invokeMethod(communicator, "runProgram", Q_ARG(QString, fileOnRobot));
		}
		break;
	case RunPolicy::AlwaysRun:
		QMetaObject::invokeMethod(communicator, "runProgram", Q_ARG(QString, fileOnRobot));
		break;
	case RunPolicy::NeverRun:
		break;
	}
}

void Ev3RbfGeneratorPlugin::runProgram()
{
	const QString fileOnRobot = uploadProgram();
	communication::Ev3RobotCommunicationThread * const communicator = currentCommunicator();
	if (!fileOnRobot.isEmpty() && communicator) {
		QMetaObject::invokeMethod(communicator, "runProgram", Q_ARG(QString, fileOnRobot));
	}
}

void Ev3RbfGeneratorPlugin::stopRobot()
{
	if (communication::Ev3RobotCommunicationThread * const communicator = currentCommunicator()) {
		QMetaObject::invokeMethod(communicator, "stopProgram");
	}
}

bool Ev3RbfGeneratorPlugin::javaInstalled()
{
	QProcess java;
	java.setEnvironment(QProcess::systemEnvironment());

	java.start("java");
	java.waitForFinished();
	return !java.readAllStandardError().isEmpty();
}

bool Ev3RbfGeneratorPlugin::copySystemFiles(const QString &destination)
{
	QDirIterator iterator(":/ev3/rbf/thirdparty");
	while (iterator.hasNext()) {
		const QFileInfo fileInfo(iterator.next());
		const QString destFile = destination + "/" + fileInfo.fileName();
		if (!QFile::exists(destFile) && !QFile::copy(fileInfo.absoluteFilePath(), destFile)) {
			return false;
		}
	}

	return true;
}

bool Ev3RbfGeneratorPlugin::compile(const QFileInfo &lmsFile)
{
	QFile rbfFile(lmsFile.absolutePath() + "/" + lmsFile.baseName() + ".rbf");
	if (rbfFile.exists()) {
		rbfFile.remove();
	}

	QProcess java;
	QEventLoop loop;
	java.setEnvironment(QProcess::systemEnvironment());
	java.setWorkingDirectory(lmsFile.absolutePath());
	java.setProgram("java");
	java.setArguments({"-jar", "assembler.jar", lmsFile.baseName()});

	connect(&java, &QProcess::readyRead, &loop, [&java]() { QLOG_INFO() << java.readAll(); });
	connect(&java, &QProcess::errorOccurred, &loop, [&java, &loop](QProcess::ProcessError e) {
		QLOG_ERROR() << "Failed to start process (status" << e << "):"
					 << java.program() << java.arguments();
		loop.quit();
	});
	connect(&java, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished)
		, &loop, [&loop, &java](int e, QProcess::ExitStatus s) {
		if (e || s != QProcess::ExitStatus::NormalExit) {
			QLOG_ERROR() << "Failed to execute process (errCode:" << e << ", exitStatus:" << s << "):"
						 << java.program() << java.arguments();
		}
		loop.quit();
	});

	java.start();
	loop.exec();
	return true;
}

QString Ev3RbfGeneratorPlugin::upload(const QFileInfo &lmsFile)
{
	const QString folderName = SettingsManager::value("Ev3CommonFolderChecboxChecked", false).toBool()
			? SettingsManager::value("Ev3CommonFolderName", "ts").toString() : lmsFile.baseName();
	const QString targetPath = "../prjs/" + folderName;
	const QString rbfPath = lmsFile.absolutePath() + "/" + lmsFile.baseName() + ".rbf";
	bool connected = false;
	communication::Ev3RobotCommunicationThread *communicator = currentCommunicator();
	if (!communicator) {
		return QString();
	}
	QMetaObject::invokeMethod(communicator, "connect"
							, (communicator->thread()==QThread::currentThread()
								? Qt::DirectConnection : Qt::BlockingQueuedConnection)
							, Q_RETURN_ARG(bool, connected));

	if (!connected) {
		const bool isUsb = mRobotModelManager->model().name().contains("usb", Qt::CaseInsensitive);
		mMainWindowInterface->errorReporter()->addError(tr("Could not upload file to robot. "\
				"Connect to a robot via %1.").arg(isUsb ? tr("USB") : tr("Bluetooth")));
		return QString();
	}

	QString res;
	QMetaObject::invokeMethod(communicator, "uploadFile"
								, (communicator->thread()==QThread::currentThread()
									? Qt::DirectConnection : Qt::BlockingQueuedConnection)
								, Q_RETURN_ARG(QString, res), Q_ARG(QString, rbfPath), Q_ARG(QString, targetPath));
	return res;
}

generatorBase::MasterGeneratorBase *Ev3RbfGeneratorPlugin::masterGenerator()
{
	return new Ev3RbfMasterGenerator(*mRepo
			, *mMainWindowInterface->errorReporter()
			, *mParserErrorReporter
			, *mRobotModelManager
			, *mTextLanguage
			, mMainWindowInterface->activeDiagram()
			, generatorName());
}
