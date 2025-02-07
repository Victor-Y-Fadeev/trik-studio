# Copyright 2012-2016 QReal Research Group
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

include (qreal.pri)

plugins.file = $$PWD/plugins/robots.pro

RESOURCES += \
	qrgui/mainwindow/mainWindow.qrc

include (installer/platform/install-trik-studio.pri)

tests {

	SUBDIRS += \
		googletest \
		testUtils \
		robotsTests \
		trikStudioSimulatorTests \

	googletest.file = $$PWD/qrtest/thirdparty/googletest/googletest.pro
	testUtils.subdir = $$PWD/qrtest/unitTests/testUtils
	robotsTests.subdir = $$PWD/qrtest/unitTests/pluginsTests/robotsTests
	trikStudioSimulatorTests.subdir = $$PWD/qrtest/trikStudioSimulatorTests

	googletest.depends = initvars
	robotsTests.depends = \
		qrxc \
		qrgui \
		qrrepo \
		plugins \
		qrkernel \
		qrutils \
		thirdparty \
		testUtils \

	testUtils.depends = \
		googletest \
		qrgui \

	trikStudioSimulatorTests.depends =  qrgui
}
