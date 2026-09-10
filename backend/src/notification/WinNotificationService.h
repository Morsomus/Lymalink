/////////////////////////////////////////////////////////
// File: WinNotificationService.h
// Date: 2026-06-20
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares QT based Windows notification adapter
/////////////////////////////////////////////////////////

#pragma once

#include <QPointer>
#include <string>

class QWidget;

class WinNotificationService
{
public:
    WinNotificationService();
    ~WinNotificationService();

    bool ShowErrorToast(const std::string& summary, const std::string& body, const std::string& iconPath);

private:
    QPointer<QWidget> m_popup;
};
