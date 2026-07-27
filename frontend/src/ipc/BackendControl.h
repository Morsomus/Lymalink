/////////////////////////////////////////////////////////
// File: BackendControl.h
// Date: 2026-06-19
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Platform-neutral frontend control facade
//              for lymalinkd.
/////////////////////////////////////////////////////////

#pragma once

#include <QObject>
class BackendControl : public QObject
{
public:
    explicit BackendControl(QObject *parent = nullptr);
    ~BackendControl() override;

    virtual bool StopServiceIfNotEnabled() = 0;

    virtual void PingBackend() = 0;
    virtual bool StartService() = 0;
    virtual bool StopService() = 0;
    virtual bool RestartService() = 0;
    virtual bool SetServiceEnabled(bool enabled) = 0;
    virtual bool RefreshServiceStatus() = 0;
    virtual void ReloadAllTargets() = 0;
    virtual void StartManualAchievementDataScan(int appId) = 0;
    virtual void CancelManualAchievementDataScan(int appId) = 0;
    virtual void ReloadConfig() = 0;
    virtual void TestToast() = 0;
    virtual void TestSound() = 0;
};
