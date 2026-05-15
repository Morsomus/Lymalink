/////////////////////////////////////////////////////////
// File: Encryption.cpp
// Date: 2026-05-14
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements basic Encryption class
/////////////////////////////////////////////////////////

#include "Encryption.h"

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <QDebug>

#define GCM_TAG_LEN 16
#define GCM_IV_LEN 12
#define PBKDF2_ITERATIONS 100000
#define PBKDF2_SALT_LEN 16
#define ENCRYPTION_PAYLOAD_VERSION 1

/////////////////////////////////////////////////////////////////////

Encryption::Encryption(QObject *parent) : QObject(parent)
{
    // Constructor
}

Encryption::~Encryption()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

QString Encryption::Encrypt(const QString &value, const QString &key) const
{
    if (value.isEmpty() || key.isEmpty())
    {
        return QString();
    }

    QByteArray saltData(PBKDF2_SALT_LEN, 0);
    if (RAND_bytes(reinterpret_cast<unsigned char *>(saltData.data()), PBKDF2_SALT_LEN) != 1)
    {
        qDebug() << "Salt generation failed:" << ERR_error_string(ERR_get_error(), nullptr);
        return QString();
    }

    QByteArray keyData = DeriveKey(key, saltData);
    if (keyData.isEmpty())
    {
        return QString();
    }

    QByteArray ivData(GCM_IV_LEN, 0);
    if (RAND_bytes(reinterpret_cast<unsigned char *>(ivData.data()), GCM_IV_LEN) != 1)
    {
        qDebug() << "IV generation failed:" << ERR_error_string(ERR_get_error(), nullptr);
        OPENSSL_cleanse(keyData.data(), keyData.size());
        return QString();
    }

    QByteArray inputData = value.toUtf8();

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
    {
        qDebug() << "OpenSSL context creation failed:" << ERR_error_string(ERR_get_error(), nullptr);
        OPENSSL_cleanse(keyData.data(), keyData.size());
        return QString();
    }

    if (EVP_EncryptInit_ex(
            ctx,
            EVP_aes_256_gcm(),
            nullptr,
            reinterpret_cast<const unsigned char *>(keyData.constData()),
            reinterpret_cast<const unsigned char *>(ivData.constData()))
        != 1)
    {
        qDebug() << "Encryption init failed:" << ERR_error_string(ERR_get_error(), nullptr);
        EVP_CIPHER_CTX_free(ctx);
        OPENSSL_cleanse(keyData.data(), keyData.size());
        return QString();
    }

    QByteArray outputData(inputData.size(), 0);
    int outputLength = 0;

    if (EVP_EncryptUpdate(
            ctx,
            reinterpret_cast<unsigned char *>(outputData.data()),
            &outputLength,
            reinterpret_cast<const unsigned char *>(inputData.constData()),
            inputData.size())
        != 1)
    {
        qDebug() << "Encryption failed:" << ERR_error_string(ERR_get_error(), nullptr);
        EVP_CIPHER_CTX_free(ctx);
        OPENSSL_cleanse(keyData.data(), keyData.size());
        return QString();
    }

    int finalLength = 0;
    if (EVP_EncryptFinal_ex(
            ctx,
            reinterpret_cast<unsigned char *>(outputData.data() + outputLength),
            &finalLength)
        != 1)
    {
        qDebug() << "Encryption finalization failed:" << ERR_error_string(ERR_get_error(), nullptr);
        EVP_CIPHER_CTX_free(ctx);
        OPENSSL_cleanse(keyData.data(), keyData.size());
        return QString();
    }

    QByteArray tagData(GCM_TAG_LEN, 0);
    if (EVP_CIPHER_CTX_ctrl(
            ctx,
            EVP_CTRL_GCM_GET_TAG,
            GCM_TAG_LEN,
            reinterpret_cast<unsigned char *>(tagData.data()))
        != 1)
    {
        qDebug() << "Tag retrieval failed:" << ERR_error_string(ERR_get_error(), nullptr);
        EVP_CIPHER_CTX_free(ctx);
        OPENSSL_cleanse(keyData.data(), keyData.size());
        return QString();
    }

    EVP_CIPHER_CTX_free(ctx);
    OPENSSL_cleanse(keyData.data(), keyData.size());

    outputData.resize(outputLength + finalLength);

    QByteArray result;
    result.append(static_cast<char>(ENCRYPTION_PAYLOAD_VERSION));
    result.append(saltData);
    result.append(ivData);
    result.append(outputData);
    result.append(tagData);

    return QString::fromLatin1(result.toBase64());
}

/////////////////////////////////////////////////////////////////////

QString Encryption::Decrypt(const QString &value, const QString &key) const
{
    if (value.isEmpty() || key.isEmpty())
    {
        return QString();
    }

    QByteArray combined = QByteArray::fromBase64(value.toLatin1());

    const int headerLength = 1 + PBKDF2_SALT_LEN + GCM_IV_LEN;
    if (combined.size() <= headerLength + GCM_TAG_LEN)
    {
        qDebug() << "Invalid encrypted data: too short";
        return QString();
    }

    const int payloadVersion = static_cast<unsigned char>(combined.at(0));
    if (payloadVersion != ENCRYPTION_PAYLOAD_VERSION)
    {
        qDebug() << "Unsupported encrypted data version:" << payloadVersion;
        return QString();
    }

    QByteArray saltData       = combined.mid(1, PBKDF2_SALT_LEN);
    QByteArray ivData         = combined.mid(1 + PBKDF2_SALT_LEN, GCM_IV_LEN);
    QByteArray tagData        = combined.right(GCM_TAG_LEN);
    QByteArray encryptedData  = combined.mid(headerLength, combined.size() - headerLength - GCM_TAG_LEN);

    QByteArray keyData = DeriveKey(key, saltData);
    if (keyData.isEmpty())
    {
        return QString();
    }

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
    {
        qDebug() << "OpenSSL context creation failed:" << ERR_error_string(ERR_get_error(), nullptr);
        OPENSSL_cleanse(keyData.data(), keyData.size());
        return QString();
    }

    if (EVP_DecryptInit_ex(
            ctx,
            EVP_aes_256_gcm(),
            nullptr,
            reinterpret_cast<const unsigned char *>(keyData.constData()),
            reinterpret_cast<const unsigned char *>(ivData.constData()))
        != 1)
    {
        qDebug() << "Decryption init failed:" << ERR_error_string(ERR_get_error(), nullptr);
        EVP_CIPHER_CTX_free(ctx);
        OPENSSL_cleanse(keyData.data(), keyData.size());
        return QString();
    }

    if (EVP_CIPHER_CTX_ctrl(
            ctx,
            EVP_CTRL_GCM_SET_TAG,
            GCM_TAG_LEN,
            reinterpret_cast<unsigned char *>(tagData.data()))
        != 1)
    {
        qDebug() << "Tag set failed:" << ERR_error_string(ERR_get_error(), nullptr);
        EVP_CIPHER_CTX_free(ctx);
        OPENSSL_cleanse(keyData.data(), keyData.size());
        return QString();
    }

    QByteArray outputData(encryptedData.size(), 0);
    int outputLength = 0;

    if (EVP_DecryptUpdate(
            ctx,
            reinterpret_cast<unsigned char *>(outputData.data()),
            &outputLength,
            reinterpret_cast<const unsigned char *>(encryptedData.constData()),
            encryptedData.size())
        != 1)
    {
        qDebug() << "Decryption failed:" << ERR_error_string(ERR_get_error(), nullptr);
        EVP_CIPHER_CTX_free(ctx);
        OPENSSL_cleanse(keyData.data(), keyData.size());
        return QString();
    }

    int finalLength = 0;
    if (EVP_DecryptFinal_ex(
            ctx,
            reinterpret_cast<unsigned char *>(outputData.data() + outputLength),
            &finalLength)
        != 1)
    {
        qDebug() << "Decryption finalization failed (auth tag mismatch?):" << ERR_error_string(ERR_get_error(), nullptr);
        EVP_CIPHER_CTX_free(ctx);
        OPENSSL_cleanse(keyData.data(), keyData.size());
        return QString();
    }

    EVP_CIPHER_CTX_free(ctx);
    OPENSSL_cleanse(keyData.data(), keyData.size());

    outputData.resize(outputLength + finalLength);

    return QString::fromUtf8(outputData);
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

QByteArray Encryption::DeriveKey(const QString &key, const QByteArray &saltData) const
{
    QByteArray derivedKey(32, 0);
    QByteArray passwordData = key.toUtf8();

    const int result = PKCS5_PBKDF2_HMAC(
            passwordData.constData(),
            passwordData.size(),
            reinterpret_cast<const unsigned char *>(saltData.constData()),
            saltData.size(),
            PBKDF2_ITERATIONS,
            EVP_sha256(),
            derivedKey.size(),
            reinterpret_cast<unsigned char *>(derivedKey.data()));

    OPENSSL_cleanse(passwordData.data(), passwordData.size());

    if (result != 1)
    {
        qDebug() << "PBKDF2 key derivation failed:" << ERR_error_string(ERR_get_error(), nullptr);
        return QByteArray();
    }

    return derivedKey;
}
