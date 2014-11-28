#include <QtWidgets/QPushButton>
#include <QtWidgets/QComboBox>

#include <qrutils/widgetFinder.h>
#include "commonTwoDModel/engine/twoDModelGuiFacade.h"
#include "view/d2ModelWidget.h"

using namespace twoDModel::engine;
using namespace twoDModel::view;

TwoDModelGuiFacade::TwoDModelGuiFacade(D2ModelWidget &view)
	: mD2ModelWidget(view)
{
}

QWidget *TwoDModelGuiFacade::widget(QString const &type, QString const &name)
{
	return utils::WidgetFinder::widget(&mD2ModelWidget, type, name);
}

QWidget *TwoDModelGuiFacade::d2ModelSceneViewport()
{
	return mD2ModelWidget.scene()->views()[0]->viewport();
}

QWidget *TwoDModelGuiFacade::d2ModelWidget()
{
	return &mD2ModelWidget;
}
