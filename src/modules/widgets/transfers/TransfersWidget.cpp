/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2018 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*
**************************************************************************/

#include "TransfersWidget.h"
#include "../../../core/Application.h"
#include "../../../core/ThemesManager.h"
#include "../../../core/TransfersManager.h"
#include "../../../core/Utils.h"
#include "../../../ui/Action.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QtMath>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QFileIconProvider>
#include <QtWidgets/QFrame>
#include <QtWidgets/QMenu>
#include <QtWidgets/QWidgetAction>

namespace Otter
{

TransfersWidget::TransfersWidget(const ToolBarsManager::ToolBarDefinition::Entry &definition, QWidget *parent) : ToolButtonWidget(definition, parent),
	m_icon(ThemesManager::createIcon(QLatin1String("transfers")))
{
	setMenu(new QMenu(this));
	setPopupMode(QToolButton::InstantPopup);
	setToolTip(tr("Downloads"));
	updateState();

	connect(TransfersManager::getInstance(), &TransfersManager::transferChanged, this, &TransfersWidget::updateState);
	connect(TransfersManager::getInstance(), &TransfersManager::transferStarted, this, [&](Transfer *transfer)
	{
		if ((!transfer->isArchived() || transfer->getState() == Transfer::RunningState) && menu()->isVisible())
		{
			QAction *firstAction(menu()->actions().value(0));
			QWidgetAction *widgetAction(new QWidgetAction(menu()));
			widgetAction->setDefaultWidget(new TransferActionWidget(transfer, menu()));

			menu()->insertAction(firstAction, widgetAction);
			menu()->insertSeparator(firstAction);
		}

		updateState();
	});
	connect(TransfersManager::getInstance(), &TransfersManager::transferFinished, [&](Transfer *transfer)
	{
		const QList<QAction*> actions(menu()->actions());

		for (int i = 0; i < actions.count(); ++i)
		{
			const QWidgetAction *widgetAction(qobject_cast<QWidgetAction*>(actions.at(i)));

			if (widgetAction && widgetAction->defaultWidget())
			{
				const TransferActionWidget *transferActionWidget(qobject_cast<TransferActionWidget*>(widgetAction->defaultWidget()));

				if (transferActionWidget && transferActionWidget->getTransfer() == transfer)
				{
					menu()->removeAction(actions.at(i));
					menu()->removeAction(actions.value(i + 1));

					break;
				}
			}
		}

		updateState();
	});
	connect(TransfersManager::getInstance(), &TransfersManager::transferRemoved, this, &TransfersWidget::updateState);
	connect(TransfersManager::getInstance(), &TransfersManager::transferStopped, this, &TransfersWidget::updateState);
	connect(menu(), &QMenu::aboutToShow, this, &TransfersWidget::populateMenu);
	connect(menu(), &QMenu::aboutToHide, menu(), &QMenu::clear);
}

void TransfersWidget::changeEvent(QEvent *event)
{
	ToolButtonWidget::changeEvent(event);

	if (event->type() == QEvent::LanguageChange)
	{
		setToolTip(tr("Downloads"));
	}
}

void TransfersWidget::populateMenu()
{
	const QVector<Transfer*> transfers(TransfersManager::getInstance()->getTransfers());

	for (int i = 0; i < transfers.count(); ++i)
	{
		Transfer *transfer(transfers.at(i));

		if (!transfer->isArchived() || transfer->getState() == Transfer::RunningState)
		{
			QWidgetAction *widgetAction(new QWidgetAction(menu()));
			widgetAction->setDefaultWidget(new TransferActionWidget(transfer, menu()));

			menu()->addAction(widgetAction);
			menu()->addSeparator();
		}
	}

	menu()->addAction(new Action(ActionsManager::TransfersAction, {}, {{QLatin1String("text"), tr("Show all Downloads")}}, ActionExecutor::Object(Application::getInstance(), Application::getInstance()), this));
}

void TransfersWidget::updateState()
{
	const QVector<Transfer*> transfers(TransfersManager::getInstance()->getTransfers());
	qint64 bytesTotal(0);
	qint64 bytesReceived(0);
	qint64 transferAmount(0);

	for (int i = 0; i < transfers.count(); ++i)
	{
		Transfer *transfer(transfers.at(i));

		if (transfer->getState() == Transfer::RunningState && transfer->getBytesTotal() > 0)
		{
			++transferAmount;

			bytesTotal += transfer->getBytesTotal();
			bytesReceived += transfer->getBytesReceived();
		}
	}

	setIcon(getIcon());
}

QIcon TransfersWidget::getIcon() const
{
	return m_icon;
}

TransferActionWidget::TransferActionWidget(Transfer *transfer, QWidget *parent) : QWidget(parent),
	m_transfer(transfer),
	m_fileNameLabel(new QLabel(this)),
	m_iconLabel(new QLabel(this)),
	m_progressBar(new QProgressBar(this)),
	m_toolButton(new QToolButton(this)),
	m_centralWidget(new QWidget(this))
{
	QVBoxLayout *centralLayout(new QVBoxLayout(m_centralWidget));
	centralLayout->setContentsMargins(0, 0, 0, 0);
	centralLayout->addWidget(m_fileNameLabel);
	centralLayout->addWidget(m_progressBar);

	QFrame *leftSeparatorFrame(new QFrame(this));
	leftSeparatorFrame->setFrameShape(QFrame::VLine);

	QFrame *rightSeparatorFrame(new QFrame(this));
	rightSeparatorFrame->setFrameShape(QFrame::VLine);

	QHBoxLayout *mainLayout(new QHBoxLayout(this));
	mainLayout->addWidget(m_iconLabel);
	mainLayout->addWidget(leftSeparatorFrame);
	mainLayout->addWidget(m_centralWidget);
	mainLayout->addWidget(rightSeparatorFrame);
	mainLayout->addWidget(m_toolButton);

	setLayout(mainLayout);
	updateState();

	m_iconLabel->setFixedSize(32, 32);
	m_toolButton->setIconSize({16, 16});
	m_toolButton->setAutoRaise(true);

	connect(transfer, &Transfer::changed, this, &TransferActionWidget::updateState);
	connect(transfer, &Transfer::finished, this, &TransferActionWidget::updateState);
	connect(transfer, &Transfer::stopped, this, &TransferActionWidget::updateState);
	connect(transfer, &Transfer::progressChanged, this, &TransferActionWidget::updateState);
	connect(m_toolButton, &QToolButton::clicked, [&]()
	{
		switch (m_transfer->getState())
		{
			case Transfer::CancelledState:
			case Transfer::ErrorState:
				m_transfer->restart();

				break;
			case Transfer::FinishedState:
				Utils::runApplication({}, QUrl::fromLocalFile(QFileInfo(m_transfer->getTarget()).dir().canonicalPath()));

				break;
			default:
				m_transfer->cancel();

				break;
		}
	});
}

void TransferActionWidget::mousePressEvent(QMouseEvent *event)
{
	event->accept();
}

void TransferActionWidget::mouseReleaseEvent(QMouseEvent *event)
{
	event->accept();

	if (event->button() == Qt::LeftButton)
	{
		m_transfer->openTarget();
	}
}

void TransferActionWidget::updateState()
{
	const QString iconName(m_transfer->getMimeType().iconName());
	const bool isIndeterminate(m_transfer->getBytesTotal() <= 0);
	const bool hasError(m_transfer->getState() == Transfer::UnknownState || m_transfer->getState() == Transfer::ErrorState);

	m_fileNameLabel->setText(Utils::elideText(QFileInfo(m_transfer->getTarget()).fileName(), nullptr, 300));
	m_iconLabel->setPixmap(QIcon::fromTheme(iconName, QFileIconProvider().icon(iconName)).pixmap(32, 32));
	m_progressBar->setRange(0, ((isIndeterminate && !hasError) ? 0 : 100));
	m_progressBar->setValue(isIndeterminate ? (hasError ? 0 : -1) : ((m_transfer->getBytesTotal() > 0) ? qFloor(Utils::calculatePercent(m_transfer->getBytesReceived(), m_transfer->getBytesTotal())) : -1));
	m_progressBar->setFormat(isIndeterminate ? tr("Unknown") : QLatin1String("%p%"));

	switch (m_transfer->getState())
	{
		case Transfer::CancelledState:
		case Transfer::ErrorState:
			m_toolButton->setIcon(ThemesManager::createIcon(QLatin1String("view-refresh")));
			m_toolButton->setToolTip(tr("Redownload"));

			break;
		case Transfer::FinishedState:
			m_toolButton->setIcon(ThemesManager::createIcon(QLatin1String("document-open-folder")));
			m_toolButton->setToolTip(tr("Open Folder"));

			break;
		default:
			m_toolButton->setIcon(ThemesManager::createIcon(QLatin1String("task-reject")));
			m_toolButton->setToolTip(tr("Cancel"));

			break;
	}
}

Transfer* TransferActionWidget::getTransfer() const
{
	return m_transfer;
}

}
