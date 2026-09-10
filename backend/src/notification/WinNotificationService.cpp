/////////////////////////////////////////////////////////
// File: WinNotificationService.cpp
// Date: 2026-06-20
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements QT based Windows notification adapter
/////////////////////////////////////////////////////////

#include "WinNotificationService.h"
#include "Defines.h"
#include "tools/Logger.h"

#include <QApplication>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QIcon>
#include <QMetaObject>
#include <QLabel>
#include <QPushButton>
#include <QRect>
#include <QScreen>
#include <QStyle>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

#define COMPONENT "WinNotificationService"

/////////////////////////////////////////////////////////////////////

WinNotificationService::WinNotificationService()
{
    // Constructor
}

WinNotificationService::~WinNotificationService()
{
    if (m_popup)
    {
        m_popup->close();
        m_popup = nullptr;
    }
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool WinNotificationService::ShowErrorToast(const std::string& summary, const std::string& body, const std::string& iconPath)
{
    if (!QApplication::instance())
    {
        LOG_BE(Urgency::Warning, "Cannot show error notification: application instance unavailable.");
        return false;
    }

    const QString title = QString::fromStdString(summary);
    const QString message = QString::fromStdString(body);
    const QString path = QString::fromStdString(iconPath);

    const bool queued = QMetaObject::invokeMethod(QApplication::instance(), [this, title, message, path]() {
        if (m_popup)
        {
            m_popup->close();
            m_popup = nullptr;
        }

        // OUTER WINDOW (for transparency)
        QWidget* popup = new QWidget();
        popup->setAttribute(Qt::WA_DeleteOnClose);
        popup->setAttribute(Qt::WA_QuitOnClose, false);
        popup->setAttribute(Qt::WA_TranslucentBackground);
        popup->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        popup->setStyleSheet("background: transparent;");

        QString fontFamily = QStringLiteral("Inter");
        const int fontId = QFontDatabase::addApplicationFont(QStringLiteral(":/qt/qml/Lymalink/res/fonts/Inter/Inter-VariableFont_opsz,wght.ttf"));
        if (fontId != -1)
        {
            const QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
            if (!fontFamilies.isEmpty())
            {
                fontFamily = fontFamilies.first();
            }
        }

        // INNER CONTAINER
        QWidget* container = new QWidget(popup);
        container->setObjectName(QStringLiteral("ErrorNotificationContainer"));
        container->setFont(QFont(fontFamily));
        container->setStyleSheet(
            QStringLiteral(
            "QWidget#ErrorNotificationContainer {"
            "  background-color: rgba(43, 45, 49, 230);"
            "  border: 1px solid rgba(63, 65, 71, 230);"
            "  border-radius: 8px;"
            "}"
            "QLabel#NotificationTitle {"
            "  color: #f2f3f5;"
            "  font-family: '%1';"
            "  font-size: 16px;"
            "  font-weight: 600;"
            "}"
            "QLabel#NotificationBody {"
            "  color: #b5bac1;"
            "  font-family: '%1';"
            "  font-size: 14px;"
            "  margin-top: -4px;"
            "}"
            "QPushButton#NotificationClose {"
            "  background-color: transparent;"
            "  border: none;"
            "  color: #b5bac1;"
            "  font-family: '%1';"
            "  font-size: 20px;"
            "  font-weight: 600;"
            "  border-radius: 6px;"
            "  padding: 0px;"
            "}"
            "QPushButton#NotificationClose:hover {"
            "  color: #ffffff;"
            "  background-color: #3f4147;"
            "}"
            "QPushButton#NotificationClose:pressed {"
            "  background-color: #4a4d55;"
            "}"
            ).arg(fontFamily)
        );

        // ICON
        QLabel* iconLabel = new QLabel(container);
        QIcon icon;
        if (!path.isEmpty() && QFileInfo::exists(path))
        {
            icon = QIcon(path);
        }
        if (icon.isNull())
        {
            icon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxCritical);
        }
        iconLabel->setPixmap(icon.pixmap(48, 48));
        iconLabel->setFixedSize(52, 60);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setContentsMargins(0, 8, 0, 0);

        // TITLE
        QLabel* titleLabel = new QLabel(title, container);
        titleLabel->setObjectName(QStringLiteral("NotificationTitle"));
        titleLabel->setWordWrap(true);

        // BODY
        QLabel* bodyLabel = new QLabel(message, container);
        bodyLabel->setObjectName(QStringLiteral("NotificationBody"));
        bodyLabel->setWordWrap(true);
        bodyLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

        // CLOSE BUTTON
        QPushButton* closeButton = new QPushButton(QStringLiteral("✕"), container);
        closeButton->setObjectName(QStringLiteral("NotificationClose"));
        closeButton->setFixedSize(26, 26);
        closeButton->setCursor(Qt::PointingHandCursor);
        QObject::connect(closeButton, &QPushButton::clicked, popup, &QWidget::close);

        // HEADER
        QHBoxLayout* headerLayout = new QHBoxLayout();
        headerLayout->setContentsMargins(0, 0, 0, 0);
        headerLayout->setSpacing(1);
        headerLayout->addWidget(titleLabel, 1);
        headerLayout->addWidget(closeButton, 0, Qt::AlignTop);

        // TEXT COLUMN
        QVBoxLayout* textLayout = new QVBoxLayout();
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(1);
        textLayout->addLayout(headerLayout);
        textLayout->addWidget(bodyLabel);

        // ROOT LAYOUT (on container)
        QHBoxLayout* rootLayout = new QHBoxLayout(container);
        rootLayout->setContentsMargins(20, 14, 14, 14);
        rootLayout->setSpacing(14);
        rootLayout->addWidget(iconLabel, 0, Qt::AlignTop);
        rootLayout->addLayout(textLayout, 1);

        // OUTER LAYOUT
        QVBoxLayout* outerLayout = new QVBoxLayout(popup);
        outerLayout->setContentsMargins(0, 0, 0, 0);
        outerLayout->addWidget(container);

        // SIZE + POSITION
        popup->setFixedWidth(400);
        popup->adjustSize();

        if (QScreen* screen = QApplication::primaryScreen())
        {
            const QRect geometry = screen->availableGeometry();
            popup->move(geometry.right() - popup->width() - 24, geometry.bottom() - popup->height() - 24);
        }

        m_popup = popup;
        popup->show();
        popup->raise();
    }, Qt::QueuedConnection);

    if (!queued)
    {
        LOG_BE(Urgency::Warning, "Failed to queue error notification popup.");
        return false;
    }

    LOG_BE(Urgency::Debug, "Error notification popup queued.");
    return true;
}
