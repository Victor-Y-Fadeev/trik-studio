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

#pragma once

#include "generatorBase/converters/templateParametrizedConverter.h"
#include "generatorBase/robotsGeneratorDeclSpec.h"

namespace generatorBase {
namespace converters {

/// A base class for converters that have finite input cases number and
/// each of them must be mapped to some template
class ROBOTS_GENERATOR_EXPORT EnumConverterBase : public TemplateParametrizedConverter
{
public:
	EnumConverterBase(const QStringList &pathsToTemplates
			, QMap<QString, QString> const &valuesToTemplatePaths = QMap<QString, QString>());

	QString convert(const QString &data) const override;

protected:
	void addMapping(const QString &enumValue, const QString &templatePath);

private:
	QMap<QString, QString> mValuesToTemplates;
};

}
}
