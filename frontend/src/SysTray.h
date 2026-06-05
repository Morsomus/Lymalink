/////////////////////////////////////////////////////////
// File: SysTray.h
// Date: 2026-05-15
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares SysTray
/////////////////////////////////////////////////////////

#pragma once

#include <QObject>
#include <QSystemTrayIcon>

class SysTray : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ IsAvailable CONSTANT)

public:
    explicit SysTray(QObject *parent = nullptr);
    ~SysTray();

    bool IsAvailable() const;

    Q_INVOKABLE void ShowToastNotification(const QString &title, const QString &message);
    Q_INVOKABLE void SetTrayIconVisibility(bool state);

signals:
    void signalOpenWindow();

private:
    QSystemTrayIcon m_trayIcon;

    void Init();
};
