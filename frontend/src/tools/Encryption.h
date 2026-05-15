/////////////////////////////////////////////////////////
// File: Encryption.h
// Date: 2026-05-14
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares basic Encryption class
/////////////////////////////////////////////////////////

#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class Encryption : public QObject
{
    Q_OBJECT
public:
    explicit Encryption(QObject *parent = nullptr);
    ~Encryption();

    Q_INVOKABLE QString Encrypt(const QString &value, const QString &key) const;
    Q_INVOKABLE QString Decrypt(const QString &value, const QString &key) const;

private:
    QByteArray DeriveKey(const QString &key, const QByteArray &saltData) const;
};
