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

#include "trikQtsGeneratorLibrary/trikQtsGeneratorPluginBase.h"

#include <QtWidgets/QApplication>
#include <QtCore/QFileInfo>
#include <QtCore/QStateMachine>
#include <QtCore/QState>
#include <QtCore/QFinalState>

#include <qrkernel/logging.h>
#include <trikGeneratorBase/trikGeneratorPluginBase.h>
#include <trikGeneratorBase/robotModel/generatorModelExtensionInterface.h>
#include <trikKit/robotModel/trikRobotModelBase.h>
#include <utils/robotCommunication/tcpRobotCommunicator.h>
#include <utils/robotCommunication/runProgramProtocol.h>
#include <utils/robotCommunication/stopRobotProtocol.h>
#include <utils/robotCommunication/uploadProgramProtocol.h>
#include <utils/robotCommunication/networkCommunicationErrorReporter.h>
#include <qrgui/textEditor/qscintillaTextEdit.h>

#include "trikQtsMasterGenerator.h"
#include "emptyShell.h"

using namespace trik::qts;
using namespace kitBase::robotModel;
using namespace qReal;
using namespace utils::robotCommunication;

TrikQtsGeneratorPluginBase::TrikQtsGeneratorPluginBase(
		trik::robotModel::TrikRobotModelBase * const robotModel
		, kitBase::blocksBase::BlocksFactoryInterface * const blocksFactory
		, const QStringList &pathsToTemplates)
	: TrikGeneratorPluginBase(robotModel, blocksFactory)
	, mGenerateCodeAction(new QAction(nullptr))
	, mUploadProgramAction(new QAction(nullptr))
	, mRunProgramAction(new QAction(nullptr))
	, mStopRobotAction(new QAction(nullptr))
	, mRobotModel(*robotModel)
	, mPathsToTemplates(pathsToTemplates)
{
}

TrikQtsGeneratorPluginBase::~TrikQtsGeneratorPluginBase()
{
}

void TrikQtsGeneratorPluginBase::init(const kitBase::KitPluginConfigurator &configurer)
{
	const auto errorReporter = configurer.qRealConfigurator().mainWindowInterpretersInterface().errorReporter();
	RobotsGeneratorPluginBase::init(configurer);
	mCommunicator.reset(new TcpRobotCommunicator("TrikTcpServer"));
	NetworkCommunicationErrorReporter::connectErrorReporter(*mCommunicator, *errorReporter);

	mUploadProgramProtocol.reset(new UploadProgramProtocol(*mCommunicator));
	mRunProgramProtocol.reset(new RunProgramProtocol(*mCommunicator, mRobotModel.robotConfigFileVersion()));
	mStopRobotProtocol.reset(new StopRobotProtocol(*mCommunicator));

	const auto timeout = [this, errorReporter]() {
		errorReporter->addError(tr("Network operation timed out"));
		onProtocolFinished();
	};

	connect(mUploadProgramProtocol.data(), &UploadProgramProtocol::timeout, this, timeout);
	connect(mRunProgramProtocol.data(), &RunProgramProtocol::timeout, this, timeout);
	connect(mStopRobotProtocol.data(), &StopRobotProtocol::timeout, this, timeout);

	connect(mUploadProgramProtocol.data(), &UploadProgramProtocol::error
			, this, &TrikQtsGeneratorPluginBase::onProtocolFinished);
	connect(mRunProgramProtocol.data(), &RunProgramProtocol::error
			, this, &TrikQtsGeneratorPluginBase::onProtocolFinished);
	connect(mStopRobotProtocol.data(), &StopRobotProtocol::error
			, this, &TrikQtsGeneratorPluginBase::onProtocolFinished);

	connect(mUploadProgramProtocol.data(), &UploadProgramProtocol::success
			, this, &TrikQtsGeneratorPluginBase::onProtocolFinished);
	connect(mRunProgramProtocol.data(), &RunProgramProtocol::success
			, this, &TrikQtsGeneratorPluginBase::onProtocolFinished);
	connect(mStopRobotProtocol.data(), &StopRobotProtocol::success
			, this, &TrikQtsGeneratorPluginBase::onProtocolFinished);

	connect(mRunProgramProtocol.data(), &RunProgramProtocol::configVersionMismatch
			, this, [errorReporter](const QString &expected, const QString &actual) {
				Q_UNUSED(expected)
				Q_UNUSED(actual)
				errorReporter->addError(
						QString(tr("Casing model mismatch, check TRIK Studio settings, \"Robots\" page. It seems that "
								"TRIK casing version selected in TRIK Studio differs from version on robot.")));
			}
	);
}

QList<ActionInfo> TrikQtsGeneratorPluginBase::customActions()
{
	mGenerateCodeAction->setObjectName("generateTRIKCode");
	mGenerateCodeAction->setText(tr("Generate TRIK code"));
	mGenerateCodeAction->setIcon(QIcon(":/trik/qts/images/generateQtsCode.svg"));
	ActionInfo generateCodeActionInfo(mGenerateCodeAction, "generators", "tools");
	connect(mGenerateCodeAction, &QAction::triggered, this
			, &TrikQtsGeneratorPluginBase::generateCode, Qt::UniqueConnection);

	mUploadProgramAction->setObjectName("uploadProgram");
	mUploadProgramAction->setText(tr("Upload program"));
	mUploadProgramAction->setIcon(QIcon(":/trik/qts/images/uploadProgram.svg"));
	ActionInfo uploadProgramActionInfo(mUploadProgramAction, "generators", "tools");
	connect(mUploadProgramAction, &QAction::triggered, this
			, &TrikQtsGeneratorPluginBase::uploadProgram, Qt::UniqueConnection);

	mRunProgramAction->setObjectName("runProgram");
	mRunProgramAction->setText(tr("Run program"));
	mRunProgramAction->setIcon(QIcon(":/trik/qts/images/run.png"));
	ActionInfo runProgramActionInfo(mRunProgramAction, "interpreters", "tools");
	connect(mRunProgramAction, &QAction::triggered, this
			, &TrikQtsGeneratorPluginBase::runProgram, Qt::UniqueConnection);

	mStopRobotAction->setObjectName("stopRobot");
	mStopRobotAction->setText(tr("Stop robot"));
	mStopRobotAction->setIcon(QIcon(":/trik/qts/images/stop.png"));
	ActionInfo stopRobotActionInfo(mStopRobotAction, "interpreters", "tools");
	connect(mStopRobotAction, &QAction::triggered, this
			, &TrikQtsGeneratorPluginBase::stopRobot, Qt::UniqueConnection);

	return {generateCodeActionInfo, uploadProgramActionInfo, runProgramActionInfo, stopRobotActionInfo};
}

QList<HotKeyActionInfo> TrikQtsGeneratorPluginBase::hotKeyActions()
{
	mGenerateCodeAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_G));
	mUploadProgramAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_U));
	mRunProgramAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_F5));
	mStopRobotAction->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_F5));

	HotKeyActionInfo generateCodeInfo("Generator.GenerateTrik", tr("Generate TRIK Code"), mGenerateCodeAction);
	HotKeyActionInfo uploadProgramInfo("Generator.UploadTrik", tr("Upload TRIK Program"), mUploadProgramAction);
	HotKeyActionInfo runProgramInfo("Generator.RunTrik", tr("Run TRIK Program"), mRunProgramAction);
	HotKeyActionInfo stopRobotInfo("Generator.StopTrik", tr("Stop TRIK Robot"), mStopRobotAction);

	return {generateCodeInfo, uploadProgramInfo, runProgramInfo, stopRobotInfo};
}

QIcon TrikQtsGeneratorPluginBase::iconForFastSelector(const RobotModelInterface &robotModel) const
{
	Q_UNUSED(robotModel)

	return QIcon(":/trik/qts/images/switch-to-trik-qts.svg");
}

generatorBase::MasterGeneratorBase *TrikQtsGeneratorPluginBase::masterGenerator()
{
	return new TrikQtsMasterGenerator(*mRepo
			, *mMainWindowInterface->errorReporter()
			, *mParserErrorReporter
			, *mRobotModelManager
			, *mTextLanguage
			, mMainWindowInterface->activeDiagram()
			, mPathsToTemplates);
}

QString TrikQtsGeneratorPluginBase::defaultFilePath(const QString &projectName) const
{
	return QString("trik/%1/%1.js").arg(projectName);
}

text::LanguageInfo TrikQtsGeneratorPluginBase::language() const
{
	return qReal::text::Languages::pickByExtension("js");
}

QString TrikQtsGeneratorPluginBase::generatorName() const
{
	return "trikQts";
}

void TrikQtsGeneratorPluginBase::addShellDevice(robotModel::GeneratorModelExtensionInterface &robotModel) const
{
	const PortInfo shellPort("ShellPort", output);
	EmptyShell * const shell = new EmptyShell(DeviceInfo::create<trik::robotModel::parts::TrikShell>(), shellPort);
	connect(mCommunicator.data(), &TcpRobotCommunicator::printText, shell, &EmptyShell::print);
	robotModel.addDevice(shellPort, shell);
}

void TrikQtsGeneratorPluginBase::uploadProgram()
{
	if (mUploadProgramProtocol) {
		QList<QFileInfo> files;
		auto const &tabs = mMainWindowInterface->allTabs();
		for (auto &&tab : tabs) {
			if (auto * code = dynamic_cast<qReal::text::QScintillaTextEdit *>(tab)) {
				auto const &ext = code->currentLanguage().extension;
				if (ext == "js" || ext == "py") {
					files << QFileInfo(mTextManager->path(code));
				}
			}
		}
		if (!files.isEmpty()) {
			disableButtons();
			mUploadProgramProtocol->run(files);
		} else {
			mMainWindowInterface->errorReporter()->addError(
					tr("There are no files to upload. You must open or generate at least one *.js or *.py file."));
		}
	} else {
		QLOG_ERROR() << "Upload program protocol is not initialized";
	}
}

void TrikQtsGeneratorPluginBase::runProgram()
{
	const QFileInfo fileInfo = generateCodeForProcessing();
	if (fileInfo != QFileInfo() && !fileInfo.absoluteFilePath().isEmpty()) {
		if (mRunProgramProtocol) {
			disableButtons();
			mRunProgramProtocol->run(fileInfo);
		} else {
			QLOG_ERROR() << "Run program protocol is not initialized";
		}
	} else {
		QLOG_ERROR() << "Code generation failed, aborting";
	}
}

void TrikQtsGeneratorPluginBase::stopRobot()
{
	if (mStopRobotProtocol) {
		disableButtons();
		mStopRobotProtocol->run(
				"script.system(\"killall aplay\"); \n"
				"script.system(\"killall vlc\");"
				);
	} else {
		QLOG_ERROR() << "Stop robot protocol is not initialized";
	}
}

void TrikQtsGeneratorPluginBase::onProtocolFinished()
{
	mUploadProgramAction->setEnabled(true);
	mRunProgramAction->setEnabled(true);
	mStopRobotAction->setEnabled(true);
}

void TrikQtsGeneratorPluginBase::disableButtons()
{
	mUploadProgramAction->setEnabled(false);
	mRunProgramAction->setEnabled(false);
	mStopRobotAction->setEnabled(false);
}
