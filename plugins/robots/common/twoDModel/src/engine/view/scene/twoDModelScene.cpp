/* Copyright 2012-2016 CyberTech Labs Ltd.
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

#include "twoDModelScene.h"
#include <QtCore/QUuid>
#include <QtWidgets/QGraphicsSceneMouseEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>

#include <qrkernel/settingsManager.h>
#include <qrkernel/platformInfo.h>
#include <qrutils/graphicsUtils/gridDrawer.h>
#include <qrutils/deleteLaterHelper.h>
#include <qrutils/widgets/qRealFileDialog.h>
#include <qrutils/widgets/qRealMessageBox.h>
#include <qrgui/controller/controllerInterface.h>

#include <kitBase/robotModel/robotParts/touchSensor.h>
#include <kitBase/robotModel/robotParts/colorSensor.h>
#include <kitBase/robotModel/robotParts/lightSensor.h>
#include <kitBase/robotModel/robotParts/rangeSensor.h>
#include <kitBase/robotModel/robotParts/vectorSensor.h>

#include "robotItem.h"

#include "twoDModel/engine/model/model.h"
#include "twoDModel/engine/model/image.h"
#include "src/engine/view/scene/sensorItem.h"
#include "src/engine/view/scene/rangeSensorItem.h"
#include "src/engine/items/wallItem.h"
#include "src/engine/items/skittleItem.h"
#include "src/engine/items/ballItem.h"
#include "src/engine/items/curveItem.h"
#include "src/engine/items/stylusItem.h"
#include "src/engine/items/rectangleItem.h"
#include "src/engine/items/ellipseItem.h"
#include "src/engine/items/imageItem.h"
#include "src/engine/items/regions/regionItem.h"
#include "src/engine/items/startPosition.h"

#include "src/engine/commands/createWorldItemCommand.h"
#include "src/engine/commands/createWorldItemsCommand.h"
#include "src/engine/commands/removeWorldItemsCommand.h"
#include "src/engine/commands/removeSensorCommand.h"
#include "src/engine/commands/reshapeCommand.h"

using namespace twoDModel;
using namespace view;
using namespace qReal;
using namespace graphicsUtils;

TwoDModelScene::TwoDModelScene(model::Model &model
		, AbstractView *view
		, QObject *parent)
	: AbstractScene(view, parent)
	, mModel(model)
	, mDrawingAction(none)
{
	mFirstPenWidth = 6;
	mSizeEmptyRectX = 1000;
	mSizeEmptyRectY = 1000;
	setEmptyRect(-500, -500, mSizeEmptyRectX, mSizeEmptyRectY);
	setItemIndexMethod(NoIndex);
	setEmptyPenBrushItems();

	connect(&mModel.worldModel(), &model::WorldModel::wallAdded, this, &TwoDModelScene::onWallAdded);
	connect(&mModel.worldModel(), &model::WorldModel::skittleAdded, this, &TwoDModelScene::onSkittleAdded);
	connect(&mModel.worldModel(), &model::WorldModel::ballAdded, this, &TwoDModelScene::onBallAdded);
	connect(&mModel.worldModel(), &model::WorldModel::colorItemAdded, this, &TwoDModelScene::onColorItemAdded);
	connect(&mModel.worldModel(), &model::WorldModel::imageItemAdded, this, &TwoDModelScene::onImageItemAdded);
	connect(&mModel.worldModel(), &model::WorldModel::regionItemAdded
			, this, [=](items::RegionItem *item) { addItem(item); });
	connect(&mModel.worldModel(), &model::WorldModel::traceItemAdded
			, this, [=](QGraphicsLineItem *item) { addItem(item); });
	connect(&mModel.worldModel(), &model::WorldModel::itemRemoved, this, &TwoDModelScene::onItemRemoved);

	connect(&mModel.worldModel(), &model::WorldModel::backgroundImageItemAdded
			, this, &TwoDModelScene::handleBackgroundImageItem);

	connect(&mModel, &model::Model::robotAdded, this, &TwoDModelScene::onRobotAdd);
	connect(&mModel, &model::Model::robotRemoved, this, &TwoDModelScene::onRobotRemove);
}

TwoDModelScene::~TwoDModelScene()
{
}

bool TwoDModelScene::oneRobot() const
{
	return mRobots.size() == 1;
}

void TwoDModelScene::setController(ControllerInterface &controller)
{
	mController = &controller;
}

void TwoDModelScene::setInteractivityFlags(kitBase::ReadOnlyFlags flags)
{
	mWorldReadOnly = (flags & kitBase::ReadOnly::World) != 0;
	mRobotReadOnly = (flags & kitBase::ReadOnly::RobotPosition) != 0;
	mSensorsReadOnly = (flags & kitBase::ReadOnly::Sensors) != 0;

	for (const auto item : items()) {
		const auto robotItem = dynamic_cast<RobotItem *>(item);
		const auto sensorItem = dynamic_cast<SensorItem *>(item);
		const auto worldItem = dynamic_cast<items::ColorFieldItem *>(item);
		const auto startPosition = dynamic_cast<items::StartPosition *>(item);
		if (worldItem) {
			worldItem->setEditable(!mWorldReadOnly);
		} else if (robotItem) {
			robotItem->setEditable(!mRobotReadOnly);
		} else if (sensorItem) {
			sensorItem->setEditable(!mSensorsReadOnly);
		} else if (startPosition) {
			startPosition->setEditable(!mRobotReadOnly);
		}
	}
}

bool TwoDModelScene::hasIntersect(const AbstractItem *item1, const AbstractItem *item2) const
{
	return item1->realShape().intersects(item2->realShape());
}

void TwoDModelScene::handleBackgroundImageItem(items::ImageItem *backgroundImageItem)
{
	connect(backgroundImageItem, &items::ImageItem::selectedChanged, this, [=](bool selected) {
		if (!selected) {
			QRect backgroundRect;
			backgroundRect.setLeft(backgroundImageItem->x1() + backgroundImageItem->x());
			backgroundRect.setTop(backgroundImageItem->y1() + backgroundImageItem->y());
			backgroundRect.setRight(backgroundImageItem->x2() + backgroundImageItem->x());
			backgroundRect.setBottom(backgroundImageItem->y2() + backgroundImageItem->y());
			mModel.worldModel().setBackground(backgroundImageItem->image(), backgroundRect);
			mModel.worldModel().removeImageItem(backgroundImageItem);
			update();
		}

		setDragMode(QGraphicsView::NoDrag);
	});
}

bool TwoDModelScene::isCorrectScene(const QList<QGraphicsItem *> checkItems) const
{
	for (auto item : checkItems) {
		if (!item) {
			continue;
		} else if (auto robot = dynamic_cast<const RobotItem *>(item)) {
			for (const auto wall : mModel.worldModel().walls()) {
				if (hasIntersect(robot, wall)) return false;
			}
			for (const auto ball : mModel.worldModel().balls()) {
				if (hasIntersect(robot, ball)) return false;
			}
			for (const auto skittle : mModel.worldModel().skittles()) {
				if (hasIntersect(robot, skittle)) return false;
			}
		} else if (auto ball = dynamic_cast<const items::BallItem *>(item)) {
			for (const auto wall : mModel.worldModel().walls()) {
				if (hasIntersect(ball, wall)) return false;
			}
			for (const auto robot : mRobots.values()) {
				if (hasIntersect(ball, robot)) return false;
			}
			for (const auto skittle : mModel.worldModel().skittles()) {
				if (hasIntersect(ball, skittle)) return false;
			}
		} else if (auto skittle = dynamic_cast<const items::SkittleItem *>(item)) {
			for (const auto wall : mModel.worldModel().walls()) {
				if (hasIntersect(skittle, wall)) return false;
			}
			for (const auto robot : mRobots.values()) {
				if (hasIntersect(skittle, robot)) return false;
			}
			for (const auto ball : mModel.worldModel().balls()) {
				if (hasIntersect(skittle, ball)) return false;
			}
		} else if (auto wall = dynamic_cast<const items::WallItem *>(item)) {
			for (const auto ball : mModel.worldModel().balls()) {
				if (hasIntersect(wall, ball)) return false;
			}
			for (const auto skittle : mModel.worldModel().skittles()) {
				if (hasIntersect(wall, skittle)) return false;
			}
			for (const auto robot : mRobots.values()) {
				if (hasIntersect(wall, robot)) return false;
			}
		}
	}
	return true;
}

void TwoDModelScene::onRobotAdd(model::RobotModel *robotModel)
{
	RobotItem * const robotItem = new RobotItem(robotModel->info().robotImage(), *robotModel);

	connect(robotItem, &RobotItem::mousePressed, this, &TwoDModelScene::robotPressed);
	connect(robotItem, &RobotItem::drawTrace, &mModel.worldModel(), &model::WorldModel::appendRobotTrace);

	robotItem->setEditable(!mRobotReadOnly);

	addItem(robotItem);
	robotItem->robotModel().startPositionMarker()->setZValue(robotItem->zValue() - lowPrecision);
	addItem(robotItem->robotModel().startPositionMarker());
	subscribeItem(static_cast<AbstractItem *>(robotModel->startPositionMarker()));

	mRobots.insert(robotModel, robotItem);

	emit robotListChanged(robotItem);
}

void TwoDModelScene::onRobotRemove(model::RobotModel *robotModel)
{
	RobotItem *robotItem = mRobots[robotModel];

	removeItem(robotItem);
	mRobots.remove(robotModel);

	delete robotItem;

	emit robotListChanged(nullptr);
}

void TwoDModelScene::onWallAdded(items::WallItem *wall)
{
	addItem(wall);
	subscribeItem(wall);
	connect(wall, &items::WallItem::deletedWithContextMenu, this, &TwoDModelScene::deleteSelectedItems);
	wall->setEditable(!mWorldReadOnly);
}

void TwoDModelScene::onSkittleAdded(items::SkittleItem *skittle)
{
	onAbstractItemAdded(skittle);
	connect(skittle, &items::SkittleItem::mouseInteractionStopped
			, this, &TwoDModelScene::handleMouseInteractionWithSelectedItems);
}

void TwoDModelScene::onBallAdded(items::BallItem *ball)
{
	onAbstractItemAdded(ball);
	connect(ball, &items::BallItem::mouseInteractionStopped
			, this, &TwoDModelScene::handleMouseInteractionWithSelectedItems);
}

void TwoDModelScene::handleMouseInteractionWithSelectedItems()
{
	for (QGraphicsItem *item : selectedItems()) {
		if (auto ball = dynamic_cast<items::BallItem *>(item)) {
			ball->saveStartPosition();
		} else if (auto skittle = dynamic_cast<items::SkittleItem *>(item)) {
			skittle->saveStartPosition();
		}
	}
}

void TwoDModelScene::onColorItemAdded(graphicsUtils::AbstractItem *item)
{
	onAbstractItemAdded(item);
}

void TwoDModelScene::onImageItemAdded(graphicsUtils::AbstractItem *item)
{
	onAbstractItemAdded(item);
}

void TwoDModelScene::onAbstractItemAdded(AbstractItem *item)
{
	addItem(item);
	subscribeItem(item);
	connect(item, &graphicsUtils::AbstractItem::deletedWithContextMenu, this, &TwoDModelScene::deleteSelectedItems);
	item->setEditable(!mWorldReadOnly);
}

void TwoDModelScene::onItemRemoved(QGraphicsItem *item)
{
	mGraphicsItem = nullptr;
	removeItem(item);
	// We delete the item not immediately cause in other handlers of WorldModel`s itemRemoved() signal
	// it may still be used.
	utils::DeleteLaterHelper<QGraphicsItem>::deleteLater(item);
}

void TwoDModelScene::drawAxes(QPainter *painter)
{
	painter->save();

	const int arrowSize = 5;
	const QRectF visibleRect = views().first()->mapToScene(views().first()->viewport()->geometry()).boundingRect();

	QPen pen = painter->pen();
	pen.setColor(Qt::gray);
	pen.setWidth(2);
	pen.setStyle(Qt::DashLine);
	painter->setPen(pen);

	painter->drawLine(QLineF(0, visibleRect.top(), 0, visibleRect.bottom()));
	painter->drawLine(QLineF(0, visibleRect.bottom(), -arrowSize, visibleRect.bottom() - arrowSize));
	painter->drawLine(QLineF(0, visibleRect.bottom(), arrowSize, visibleRect.bottom() - arrowSize));
	painter->drawLine(QLineF(visibleRect.left(), 0, visibleRect.right(), 0));
	painter->drawLine(QLineF(visibleRect.right(), 0, visibleRect.right() - arrowSize, -arrowSize));
	painter->drawLine(QLineF(visibleRect.right(), 0, visibleRect.right() - arrowSize, arrowSize));

	painter->restore();
}

void TwoDModelScene::mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
	if (mouseEvent->button() != Qt::LeftButton) {
		AbstractScene::mousePressEvent(mouseEvent);
		return;
	}

	const QPointF position = mouseEvent->scenePos();

	emit mousePressed();

	auto initItem = [this, mouseEvent](QGraphicsItem *item) {
		removeMoveFlag(mouseEvent, item);
		// This will deselect all items
		setSelectionArea(QPainterPath());
	};

	auto initColorField = [this, &initItem](items::ColorFieldItem *item) {
		initItem(item);
		item->setPenBrush(penStyleItems(), penWidthItems(), penColorItems()
				, brushStyleItems(), brushColorItems());
		mModel.worldModel().addColorField(item);
	};

	for (auto selectedItem : selectedItems()) {
		if (auto item = dynamic_cast<AbstractItem *>(selectedItem)) {
			item->savePos();
		}
	}

	for (RobotItem * const robotItem : mRobots.values()) {
		if (!robotItem->realBoundingRect().contains(position)) {
			switch (mDrawingAction) {
			case wall:
				mCurrentWall = new items::WallItem(position, position);
				initItem(mCurrentWall);
				mModel.worldModel().addWall(mCurrentWall);
				break;
			case skittle:
				mCurrentSkittle = new items::SkittleItem(position);
				initItem(mCurrentSkittle);
				mModel.worldModel().addSkittle(mCurrentSkittle);
				break;
			case ball:
				mCurrentBall = new items::BallItem(position);
				initItem(mCurrentBall);
				mModel.worldModel().addBall(mCurrentBall);
				break;
			case line:
				mCurrentLine = new items::LineItem(position, position);
				initColorField(mCurrentLine);
				break;
			case bezier:
				mCurrentCurve = new items::CurveItem(position, position);
				initColorField(mCurrentCurve);
				break;
			case stylus:
				mCurrentStylus = new items::StylusItem(position.x(), position.y());
				initColorField(mCurrentStylus);
				break;
			case rectangle:
				mCurrentRectangle = new items::RectangleItem(position, position);
				initColorField(mCurrentRectangle);
				break;
			case ellipse:
				mCurrentEllipse = new items::EllipseItem(position, position);
				initColorField(mCurrentEllipse);
				break;
			default:
				break;
			}
		}
	}

	if (mDrawingAction == none) {
		forPressResize(mouseEvent);
	}

	AbstractScene::mousePressEvent(mouseEvent);
}

void TwoDModelScene::mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
	bool needUpdate = true;
	switch (mDrawingAction) {
	case wall:
		reshapeWall(mouseEvent);
		break;
	case line:
		reshapeLine(mouseEvent);
		break;
	case bezier:
		reshapeCurve(mouseEvent);
		break;
	case stylus:
		reshapeStylus(mouseEvent);
		break;
	case rectangle:
		reshapeRectangle(mouseEvent);
		break;
	case ellipse:
		reshapeEllipse(mouseEvent);
		break;
	default:
		needUpdate = false;
		if (mouseEvent->buttons() & Qt::LeftButton) {
			forMoveResize(mouseEvent);
		}

		AbstractScene::mouseMoveEvent(mouseEvent);
		break;
	}

	if (needUpdate) {
		update();
	}
}

void TwoDModelScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
	if (mouseEvent->button() != Qt::LeftButton) {
		AbstractScene::mouseReleaseEvent(mouseEvent);
		return;
	}

	emit mouseReleased();

	// After dragging item may be null. We mustn`t select it in that case.
	AbstractItem *createdItem = nullptr;
	switch (mDrawingAction){
	case wall: {
		reshapeWall(mouseEvent);
		createdItem = mCurrentWall;
		mCurrentWall = nullptr;
		break;
	}
	case skittle: {
		createdItem = mCurrentSkittle;
		mCurrentSkittle = nullptr;
		break;
	}
	case ball: {
		createdItem = mCurrentBall;
		mCurrentBall = nullptr;
		break;
	}
	case line: {
		reshapeLine(mouseEvent);
		createdItem = mCurrentLine;
		mCurrentLine = nullptr;
		break;
	}
	case bezier: {
		reshapeCurve(mouseEvent);
		createdItem = mCurrentCurve;
		mCurrentCurve = nullptr;
		break;
	}
	case stylus: {
		reshapeStylus(mouseEvent);
		createdItem = mCurrentStylus;
		mCurrentStylus = nullptr;
		break;
	}
	case rectangle: {
		reshapeRectangle(mouseEvent);
		createdItem = mCurrentRectangle;
		mCurrentRectangle = nullptr;
		break;
	}
	case ellipse: {
		reshapeEllipse(mouseEvent);
		createdItem = mCurrentEllipse;
		mCurrentEllipse = nullptr;
		break;
	}
	default:
		break;
	}

	forReleaseResize(mouseEvent);

	if (!isCorrectScene({createdItem})) {
		// Qt bug. You need to manually release item before removing
		createdItem->mouseReleaseEvent(mouseEvent);
		mModel.worldModel().removeItem(createdItem->id());
	} else {
		registerInUndoStack(createdItem);
	}

	if (!isCorrectScene(selectedItems())) {
		for (auto selectedItem : selectedItems()) {
			if (auto item = dynamic_cast<AbstractItem *>(selectedItem)) {
				item->restorePos();
			}
		}
	}

	for (RobotItem * const robotItem : mRobots) {
		setSceneRect(sceneRect().united(robotItem->sceneBoundingRect()));
	}
	for (auto const wall : mModel.worldModel().walls()) {
		setSceneRect(sceneRect().united(wall->sceneBoundingRect()));
	}
	for (auto const ball : mModel.worldModel().balls()) {
		setSceneRect(sceneRect().united(ball->sceneBoundingRect()));
	}
	for (auto const skittle : mModel.worldModel().skittles()) {
		setSceneRect(sceneRect().united(skittle->sceneBoundingRect()));
	}

	setMoveFlag(mouseEvent);
	handleMouseInteractionWithSelectedItems();
	update();
	AbstractScene::mouseReleaseEvent(mouseEvent);
}

void TwoDModelScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
	const QList<QGraphicsItem *> itemsUnderCursor = items(mouseEvent->scenePos());
	const bool isSceneClick = itemsUnderCursor.count() == 0;
	if (isSceneClick && mModel.worldModel().backgroundRect().contains(mouseEvent->scenePos().toPoint())) {
		setDragMode(QGraphicsView::NoDrag);
		items::ImageItem *item = new items::ImageItem(mModel.worldModel().background()
				, mModel.worldModel().backgroundRect());
		item->setBackgroundRole(true);
		mModel.worldModel().setBackground(nullptr, QRect());
		mModel.worldModel().addImageItem(item);
		item->setSelected(true);
		handleBackgroundImageItem(item);
	}
}

void TwoDModelScene::copySelectedItems()
{
	mClipboard.clear();
	for (auto &&id : parseItemsToID(selectedItems()).first) {
		mClipboard << mModel.worldModel().serializeItem(id);
	}
}

void TwoDModelScene::pasteItemsFromClipboard()
{
	for (auto &&item : selectedItems()) {
		item->setSelected(false);
	}

	QList<QDomElement> newItems;
	QStringList newIds;
	for (auto &&item : mClipboard) {
		QDomElement newItem(item);
		QString newId = QUuid::createUuid().toString();
		newItem.setAttribute("id", newId);
		newIds << newId;
		newItems << newItem;
	}
	auto command = new commands::CreateWorldItemsCommand(mModel, newItems);
	mController->execute(command);

	for (auto id : newIds) {
		findItem(id)->setSelected(true);
		findItem(id)->setPos(findItem(id)->pos() + QPointF(20, 20));
		findItem(id)->savePos();
	}
}

QPair<QStringList, QList<QPair<model::RobotModel *
		, kitBase::robotModel::PortInfo>>> TwoDModelScene::parseItemsToID (QList<QGraphicsItem*> items)
{
	QStringList worldItems;
	QList<QPair<model::RobotModel *, kitBase::robotModel::PortInfo>> sensors;
	for (QGraphicsItem * const item : items) {
		SensorItem * const sensor = dynamic_cast<SensorItem *>(item);
		items::WallItem * const wall = dynamic_cast<items::WallItem *>(item);
		items::ColorFieldItem * const colorField = dynamic_cast<items::ColorFieldItem *>(item);
		items::ImageItem * const image = dynamic_cast<items::ImageItem *>(item);
		items::SkittleItem * const skittle = dynamic_cast<items::SkittleItem *>(item);
		items::BallItem * const ball = dynamic_cast<items::BallItem *>(item);

		if (sensor && !mSensorsReadOnly) {
			for (RobotItem * const robotItem : mRobots.values()) {
				const kitBase::robotModel::PortInfo port = robotItem->sensors().key(sensor);
				if (port.isValid()) {
					sensors << qMakePair(&robotItem->robotModel(), port);
				}
			}
		} else if (wall && !mWorldReadOnly) {
			worldItems << wall->id();
		} else if (skittle && !mWorldReadOnly) {
			worldItems << skittle->id();
		} else if (ball && !mWorldReadOnly) {
			worldItems << ball->id();
		} else if (colorField && !mWorldReadOnly) {
			worldItems << colorField->id();
		} else if (image && !mWorldReadOnly) {
			worldItems << image->id();
		}
	}
	return QPair<QStringList, QList<QPair<model::RobotModel *, kitBase::robotModel::PortInfo>>>(worldItems, sensors);
}


void TwoDModelScene::deleteSelectedItems()
{
	auto ids = parseItemsToID(selectedItems());

	mCurrentWall = nullptr;
	mCurrentSkittle = nullptr;
	mCurrentBall = nullptr;
	mCurrentLine = nullptr;
	mCurrentStylus = nullptr;
	mCurrentEllipse = nullptr;
	mCurrentRectangle = nullptr;
	mCurrentCurve = nullptr;

	deleteWithCommand(ids.first, ids.second, {});
}

void TwoDModelScene::deleteWithCommand(const QStringList &worldItems
		, const QList<QPair<model::RobotModel *, kitBase::robotModel::PortInfo>> &sensors
		, const QList<qReal::commands::AbstractCommand *> &additionalCommands)
{
	const bool shouldCreateCommand = !worldItems.isEmpty() || !sensors.isEmpty();
	if (mController && shouldCreateCommand) {
		auto command = new commands::RemoveWorldItemsCommand(mModel, worldItems);
		// Appending sensors deletion commands
		for (const QPair<model::RobotModel *, kitBase::robotModel::PortInfo> &sensor : sensors) {
			command->addPostAction(new commands::RemoveSensorCommand(sensor.first->configuration()
					, sensor.first->info().robotId(), sensor.second));
		}

		for (qReal::commands::AbstractCommand * const additionalCommand : additionalCommands) {
			command->addPostAction(additionalCommand);
		}

		mController->execute(command);
	}
}

void TwoDModelScene::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Delete) {
		deleteSelectedItems();
	} else if (event->matches(QKeySequence::Copy)) {
		copySelectedItems();
	} else if (event->matches(QKeySequence::Cut)) {
		copySelectedItems();
		deleteSelectedItems();
	} else if (event->matches(QKeySequence::Paste)) {
		pasteItemsFromClipboard();
	} else {
		QGraphicsScene::keyPressEvent(event);
	}
}

void TwoDModelScene::drawBackground(QPainter *painter, const QRectF &rect)
{
	if (mModel.worldModel().background() && mModel.worldModel().background()->isValid()) {
		mModel.worldModel().background()->draw(*painter, mModel.worldModel().backgroundRect(), currentZoom());
	}

	if (SettingsManager::value("2dShowGrid").toBool()) {
		mWidthOfGrid = SettingsManager::value("GridWidth").toReal() / 100;
		painter->setPen(QPen(Qt::black, mWidthOfGrid));
		QGraphicsScene::drawBackground(painter, rect);
		const int cellSize = SettingsManager::value("2dGridCellSize").toInt();
		mGridDrawer.drawGrid(painter, rect, cellSize);
		drawAxes(painter);
	}
}

void TwoDModelScene::addWall()
{
	mDrawingAction = wall;
}

void TwoDModelScene::addSkittle()
{
	mDrawingAction = skittle;
}

void TwoDModelScene::addBall()
{
	mDrawingAction = ball;
}

void TwoDModelScene::addLine()
{
	mDrawingAction = line;
}

void TwoDModelScene::addBezier()
{
	mDrawingAction = bezier;
}

void TwoDModelScene::addStylus()
{
	mDrawingAction = stylus;
}

void TwoDModelScene::addRectangle()
{
	mDrawingAction = rectangle;
}

void TwoDModelScene::addEllipse()
{
	mDrawingAction = ellipse;
}

void TwoDModelScene::addImage()
{
	// Loads world and robot models simultaneously.
	const QString loadFileName = utils::QRealFileDialog::getOpenFileName("2DSelectImage", views().first()
			, tr("Select image")
			, qReal::PlatformInfo::invariantSettingsPath("pathToImages") + "/../fields"
			, tr("Graphics (*.*)"));
	if (loadFileName.isEmpty()) {
		return;
	}

	QFile imageFile(loadFileName);
	if (imageFile.size() >  5 * 1024 * 1024) {
		if (utils::QRealMessageBox::question(QApplication::focusWidget(), tr("Warning")
				, tr("You are trying to load to big image, it may freeze execution for some time. Continue?"))
						!= QMessageBox::Yes) {
			return;
		}
	}

	auto newImage = new model::Image(loadFileName, false);
	auto size = newImage->preferedSize();
	if (size.width() == 0 || size.height() == 0) {
		if (utils::QRealMessageBox::question(QApplication::focusWidget(), tr("Error")
				, tr("Cannot load file. Try another one.")
				, QMessageBox::StandardButtons(QMessageBox::Retry | QMessageBox::Close))
					== QMessageBox::Retry) {
			addImage();
		}
		return;
	}
	mDrawingAction = image;
	const QRect rect(QPoint(-size.width() / 2, -size.height() / 2), size);
	auto result = new twoDModel::items::ImageItem(newImage, rect);
	result->setMemorize(true);
	mModel.worldModel().addImageItem(result);
	registerInUndoStack(result);
	setNoneStatus();
}

void TwoDModelScene::setNoneStatus()
{
	mDrawingAction = none;
}

void TwoDModelScene::clearScene(bool removeRobot, Reason reason)
{
	if (reason == Reason::userAction) {
		// User pressed clear button, this action must be undone when required, so executing it with command.

		QStringList worldItemsToDelete;
		for (const items::WallItem *wall : mModel.worldModel().walls()) {
			worldItemsToDelete << wall->id();
		}

		for (const items::SkittleItem *skittle : mModel.worldModel().skittles()) {
			worldItemsToDelete << skittle->id();
		}

		for (const items::BallItem *ball : mModel.worldModel().balls()) {
			worldItemsToDelete << ball->id();
		}

		for (const items::ColorFieldItem *colorField : mModel.worldModel().colorFields()) {
			worldItemsToDelete << colorField->id();
		}

		for (const items::RegionItem *region : mModel.worldModel().regions()) {
			worldItemsToDelete << region->id();
		}

		QList<QPair<model::RobotModel *, kitBase::robotModel::PortInfo>> sensorsToDelete;
		QList<qReal::commands::AbstractCommand *> additionalCommands;
		for (model::RobotModel *robotModel : mRobots.keys()) {
			commands::ReshapeCommand * const reshapeCommand = new commands::ReshapeCommand(*this, mModel
					, {mRobots[robotModel]->id()});
			reshapeCommand->startTracking();
			robotModel->clear();
			reshapeCommand->stopTracking();
			additionalCommands << reshapeCommand;
			if (removeRobot) {
				for (const kitBase::robotModel::PortInfo &port : robot(*robotModel)->sensors().keys()) {
					sensorsToDelete << qMakePair(robotModel, port);
				}
			}
		}

		deleteWithCommand(worldItemsToDelete, sensorsToDelete, additionalCommands);

		// Clear trace action mustn`t be undone though
		mModel.worldModel().clearRobotTrace();

	} else {
		// The model is being reloaded, we can make it without undo-redo, so a bit faster

		mModel.worldModel().clear();

		for (model::RobotModel *robotModel : mRobots.keys()) {
			robotModel->clear();
			if (removeRobot) {
				for (const kitBase::robotModel::PortInfo &port : configuredPorts(robotModel->info().robotId())) {
					deviceConfigurationChanged(robotModel->info().robotId()
							, port, kitBase::robotModel::DeviceInfo(), reason);
				}
			}
		}

	}
}

void TwoDModelScene::reshapeWall(QGraphicsSceneMouseEvent *event)
{
	const QPointF pos = event->scenePos();
	if (mCurrentWall) {
		mCurrentWall->setX2(pos.x());
		mCurrentWall->setY2(pos.y());
		if (SettingsManager::value("2dShowGrid").toBool()) {
			mCurrentWall->reshapeBeginWithGrid(SettingsManager::value("2dGridCellSize").toInt());
			mCurrentWall->reshapeEndWithGrid(SettingsManager::value("2dGridCellSize").toInt());
		} else {
			if (event->modifiers() & Qt::ShiftModifier) {
				mCurrentWall->reshapeRectWithShift();
			}
		}
	}
}

void TwoDModelScene::reshapeLine(QGraphicsSceneMouseEvent *event)
{
	const QPointF pos = event->scenePos();
	if (mCurrentLine) {
		mCurrentLine->setX2(pos.x());
		mCurrentLine->setY2(pos.y());
		if (event->modifiers() & Qt::ShiftModifier) {
			mCurrentLine->reshapeRectWithShift();
		}
	}
}

void TwoDModelScene::reshapeCurve(QGraphicsSceneMouseEvent *event)
{
	const QPointF pos = event->scenePos();
	if (mCurrentCurve) {
		mCurrentCurve->setX2(pos.x());
		mCurrentCurve->setY2(pos.y());
		if (event->modifiers() & Qt::ShiftModifier) {
			mCurrentCurve->reshapeRectWithShift();
		}
	}
}

void TwoDModelScene::reshapeStylus(QGraphicsSceneMouseEvent *event)
{
	const QPointF pos = event->scenePos();
	if (mCurrentStylus) {
		mCurrentStylus->addLine(pos.x(), pos.y());
	}
}

void TwoDModelScene::reshapeRectangle(QGraphicsSceneMouseEvent *event)
{
	const QPointF pos = event->scenePos();
	if (mCurrentRectangle) {
		mCurrentRectangle->setX2(pos.x());
		mCurrentRectangle->setY2(pos.y());
		if (event->modifiers() & Qt::ShiftModifier) {
			mCurrentRectangle->reshapeRectWithShift();
		}
	}
}

void TwoDModelScene::reshapeEllipse(QGraphicsSceneMouseEvent *event)
{
	const QPointF pos = event->scenePos();
	if (mCurrentEllipse) {
		mCurrentEllipse->setX2(pos.x());
		mCurrentEllipse->setY2(pos.y());
		if (event->modifiers() & Qt::ShiftModifier) {
			mCurrentEllipse->reshapeRectWithShift();
		}
	}
}

void TwoDModelScene::registerInUndoStack(AbstractItem *item)
{
	if (item) {
		item->setSelected(true);
		if (mDrawingAction != none && mController) {
			commands::CreateWorldItemCommand *command = new commands::CreateWorldItemCommand(mModel, item->id());
			// Command was already executed when element was drawn by user. So we should create it in redone state.
			command->setRedoEnabled(false);
			mController->execute(command);
			command->setRedoEnabled(true);
		}
	}
}

void TwoDModelScene::subscribeItem(AbstractItem *item)
{
	connect(item, &AbstractItem::mouseInteractionStarted, this, [=]() {
		if (mDrawingAction == none) {
			QStringList selectedIds;
			for (const QGraphicsItem *item : selectedItems()) {
				if (const AbstractItem *itemWithId = dynamic_cast<const AbstractItem *>(item)) {
					selectedIds << itemWithId->id();
				}
			}

			mCurrentReshapeCommand = new commands::ReshapeCommand(*this, mModel, selectedIds);
			mCurrentReshapeCommand->startTracking();
		}
	});

	connect(item, &AbstractItem::mouseInteractionStopped, this, [=]() {
		if (mDrawingAction == none) {
			mCurrentReshapeCommand->stopTracking();
			if (mController) {
				mCurrentReshapeCommand->setRedoEnabled(false);
				mController->execute(mCurrentReshapeCommand);
				mCurrentReshapeCommand->setRedoEnabled(true);
			}

			mCurrentReshapeCommand = nullptr;
		}
	});
}

qreal TwoDModelScene::currentZoom() const
{
	return views().isEmpty() ? 1.0 : views().first()->transform().m11();
}

void TwoDModelScene::alignWalls()
{
	if (SettingsManager::value("2dShowGrid").toBool()) {
		for (items::WallItem * const wall : mModel.worldModel().walls()) {
			if (items().contains(wall)) {
				wall->alignTheWall(SettingsManager::value("2dGridCellSize").toInt());
			}
		}
	}
}

RobotItem *TwoDModelScene::robot(model::RobotModel &robotModel)
{
	return mRobots.value(&robotModel);
}

void TwoDModelScene::centerOnRobot(RobotItem *selectedItem)
{
	RobotItem *robotItem = mRobots.values().first();

	if (selectedItem) {
		robotItem = selectedItem;
	}

	for (QGraphicsView * const view : views()) {
		const QRectF viewPortRect = view->mapToScene(view->viewport()->rect()).boundingRect();
		if (!viewPortRect.contains(robotItem->sceneBoundingRect().toRect())) {
			const QRectF requiredViewPort = viewPortRect.translated(robotItem->scenePos() - viewPortRect.center());
			setSceneRect(itemsBoundingRect().united(requiredViewPort));
			view->centerOn(robotItem);
		}
	}
}

void TwoDModelScene::setBackground(model::Image * const background, const QRect &backgroundRect)
{
	mModel.worldModel().setBackground(background, backgroundRect);
	update();
}

void TwoDModelScene::reinitSensor(RobotItem *robotItem, const kitBase::robotModel::PortInfo &port)
{
	robotItem->removeSensor(port);
	model::RobotModel &robotModel = robotItem->robotModel();

	const kitBase::robotModel::DeviceInfo &device = robotModel.configuration().type(port);
	if (device.isNull() || !device.simulated())
	{
		return;
	}

	SensorItem *sensor = device.isA<kitBase::robotModel::robotParts::RangeSensor>()
			? new RangeSensorItem(mModel.worldModel(), robotModel.configuration()
					, port
					, robotModel.info().rangeSensorAngleAndDistance(device)
					, robotModel.info().sensorImagePath(device)
					, robotModel.info().sensorImageRect(device)
					)
			: new SensorItem(robotModel.configuration()
					, port
					, robotModel.info().sensorImagePath(device)
					, robotModel.info().sensorImageRect(device)
					);

	sensor->setEditable(!mSensorsReadOnly);
	subscribeItem(sensor);

	robotItem->addSensor(port, sensor);
}
