# Copyright 2015 QReal Research Group
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

TEMPLATE = subdirs

include(../../../../global.pri)

win32 {
	LIBS += -llibusb-1.0
} else {
	PKGCONFIG *= libusb-1.0
}
CONFIG *= link_pkgconfig

# May be --variable=libdir?
win32 {
	contains(QT_ARCH, i386) {
		copyToDestdir($$PWD\MinGW32\dll\libusb-1.0.dll, NOW)
	} else {
		copyToDestdir($$PWD\MinGW64\dll\libusb-1.0.dll, NOW)
	}
}
macx: copyToDestdir($$system($$pkgConfigExecutable() libusb-1.0 --variable=prefix)/lib/libusb-1.0.dylib, NOW)
