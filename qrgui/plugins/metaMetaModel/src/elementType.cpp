/* Copyright 2016 Dmitry Mordvinov
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

#include "metaMetaModel/elementType.h"

#include "metaMetaModel/metamodel.h"
#include "metaMetaModel/nodeElementType.h"
#include "metaMetaModel/edgeElementType.h"
#include "metaMetaModel/patternType.h"

using namespace qReal;

ElementType::ElementType(Metamodel &metamodel)
	: qrgraph::Node(metamodel)
{
}

Metamodel &ElementType::metamodel() const
{
	return *static_cast<Metamodel *>(&graph());
}

const NodeElementType &ElementType::toNode() const
{
	Q_ASSERT(type() == Type::node);
	return *dynamic_cast<const NodeElementType *>(this);
}

const EdgeElementType &ElementType::toEdge() const
{
	Q_ASSERT(type() == Type::edge);
	return *dynamic_cast<const EdgeElementType *>(this);
}

const PatternType &ElementType::toPattern() const
{
	Q_ASSERT(type() == Type::pattern);
	return *dynamic_cast<const PatternType *>(this);
}

void ElementType::updateRendererContent(const QString &shape)
{
	Q_UNUSED(shape)
}
