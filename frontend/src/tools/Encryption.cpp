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
    QString encryptedValue = "";

    if (value.isEmpty() || key.isEmpty())
    {
        return encryptedValue;
    }

    // Generate unique salt for key derivation per encrypted payload
    QByteArray saltData(PBKDF2_SALT_LEN, 0);
    if (RAND_bytes(reinterpret_cast<unsigned char *>(saltData.data()), PBKDF2_SALT_LEN) != 1)
    {
        qWarning() << "Encryption::Encrypt: salt generation failed:" << ERR_error_string(ERR_get_error(), nullptr);
        return encryptedValue;
    }

    // Derive AES key from caller key and payload salt
    QByteArray keyData = DeriveKey(key, saltData);
    if (keyData.isEmpty())
    {
        return encryptedValue;
    }

    // Generate random GCM IV for nonce uniqueness
    QByteArray ivData(GCM_IV_LEN, 0);
    if (RAND_bytes(reinterpret_cast<unsigned char *>(ivData.data()), GCM_IV_LEN) != 1)
    {
        qWarning() << "Encryption::Encrypt: IV generation failed:" << ERR_error_string(ERR_get_error(), nullptr);
        OPENSSL_cleanse(keyData.data(), keyData.size());
        return encryptedValue;
    }

    QByteArray inputData = value.toUtf8();

    // Allocate OpenSSL context for AES-256-GCM operation
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
    {
        qWarning() << "Encryption::Encrypt: OpenSSL context creation failed:" << ERR_error_string(ERR_get_error(), nullptr);
        OPENSSL_cleanse(keyData.data(), keyData.size());
        return encryptedValue;
    }

    // Bind derived key and IV to encryption context
    if (EVP_EncryptInit_ex(
            ctx,
            EVP_aes_256_gcm(),
            nullptr,
            reinterpret_cast<const unsigned char *>(keyData.constData()),
            reinterpret_cast<const unsigned char *>(ivData.constData()))
        != 1)
    {
        qWarning() << "Encryption::Encrypt: encryption init failed:" << ERR_error_string(ERR_get_error(), nullptr);
        EVP_CIPHER_CTX_free(ctx);
        OPENSSL_cleanse(keyData.data(), keyData.size());
        return encryptedValue;
    }

    QByteArray outputData(inputData.size(), 0);
    int outputLength = 0;

    // Encrypt plaintext bytes into ciphertext buffer
    if (EVP_EncryptUpdate(
            ctx,
            reinterpret_cast<unsigned char *>(outputData.data()),
            &outputLength,
            reinterpret_cast<const unsigned char *>(inputData.constData()),
            inputData.size())
        != 1)
    {
        qWarning() << "Encryption::Encrypt: encryption failed:" << ERR_error_string(ERR_get_error(), nullptr);
        EVP_CIPHER_CTX_free(ctx);
        OPENSSL_cleanse(keyData.data(), keyData.size());
        return encryptedValue;
    }

    int finalLength = 0;
    // Finalize encryption before extracting auth tag
    if (EVP_EncryptFinal_ex(
            ctx,
            reinterpret_cast<unsigned char *>(outputData.data() + outputLength),
            &finalLength)
        != 1)
    {
        qWarning() << "Encryption::Encrypt: encryption finalization failed:" << ERR_error_string(ERR_get_error(), nullptr);
        EVP_CIPHER_CTX_free(ctx);
        OPENSSL_cleanse(keyData.data(), keyData.size());
        return encryptedValue;
    }

    QByteArray tagData(GCM_TAG_LEN, 0);
    // Store GCM authentication tag with payload for tamper detection
    if (EVP_CIPHER_CTX_ctrl(
            ctx,
            EVP_CTRL_GCM_GET_TAG,
            GCM_TAG_LEN,
            reinterpret_cast<unsigned char *>(tagData.data()))
        != 1)
    {
        qWarning() << "Encryption::Encrypt: tag retrieval failed:" << ERR_error_string(ERR_get_error(), nullptr);
        EVP_CIPHER_CTX_free(ctx);
        OPENSSL_cleanse(keyData.data(), keyData.size());
        return encryptedValue;
    }

    EVP_CIPHER_CTX_free(ctx);
    OPENSSL_cleanse(keyData.data(), keyData.size());

    outputData.resize(outputLength + finalLength);

    QByteArray result;
    // Payload layout: version + salt + iv + ciphertext + tag
    result.append(static_cast<char>(ENCRYPTION_PAYLOAD_VERSION));
    result.append(saltData);
    result.append(ivData);
    result.append(outputData);
    result.append(tagData);

    encryptedValue = QString::fromLatin1(result.toBase64());
    return encryptedValue;
}

/////////////////////////////////////////////////////////////////////

QString Encryption::Decrypt(const QString &value, const QString &key) const
{
    QString decryptedValue = "";

    if (value.isEmpty() || key.isEmpty())
    {
        return decryptedValue;
    }

    // Decode payload built by Encrypt()
    QByteArray combined = QByteArray::fromBase64(value.toLatin1());

    const int headerLength = 1 + PBKDF2_SALT_LEN + GCM_IV_LEN;
    if (combined.size() <= headerLength + GCM_TAG_LEN)
    {
        qWarning() << "Encryption::Decrypt: invalid encrypted data, too short";
        return decryptedValue;
    }

    // Reject unknown payload versions before parsing fields
    const int payloadVersion = static_cast<unsigned char>(combined.at(0));
    if (payloadVersion != ENCRYPTION_PAYLOAD_VERSION)
    {
        qWarning() << "Encryption::Decrypt: unsupported encrypted data version:" << payloadVersion;
        return decryptedValue;
    }

    // Split payload fields for AES-GCM decrypt and tag verification
    QByteArray saltData       = combined.mid(1, PBKDF2_SALT_LEN);
    QByteArray ivData         = combined.mid(1 + PBKDF2_SALT_LEN, GCM_IV_LEN);
    QByteArray tagData        = combined.right(GCM_TAG_LEN);
    QByteArray encryptedData  = combined.mid(headerLength, combined.size() - headerLength - GCM_TAG_LEN);

    // Recreate AES key from caller key and stored salt
    QByteArray keyData = DeriveKey(key, saltData);
    if (keyData.isEmpty())
    {
        return decryptedValue;
    }

    // Allocate OpenSSL context for AES-256-GCM operation
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
    {
        qWarning() << "Encryption::Decrypt: OpenSSL context creation failed:" << ERR_error_string(ERR_get_error(), nullptr);
        OPENSSL_cleanse(keyData.data(), keyData.size());
        return decryptedValue;
    }

    // Bind derived key and stored IV to decryption context
    if (EVP_DecryptInit_ex(
            ctx,
            EVP_aes_256_gcm(),
            nullptr,
            reinterpret_cast<const unsigned char *>(keyData.constData()),
            reinterpret_cast<const unsigned char *>(ivData.constData()))
        != 1)
    {
        qWarning() << "Encryption::Decrypt: decryption init failed:" << ERR_error_string(ERR_get_error(), nullptr);
        EVP_CIPHER_CTX_free(ctx);
        OPENSSL_cleanse(keyData.data(), keyData.size());
        return decryptedValue;
    }

    // Provide stored GCM tag before finalization validates ciphertext
    if (EVP_CIPHER_CTX_ctrl(
            ctx,
            EVP_CTRL_GCM_SET_TAG,
            GCM_TAG_LEN,
            reinterpret_cast<unsigned char *>(tagData.data()))
        != 1)
    {
        qWarning() << "Encryption::Decrypt: tag set failed:" << ERR_error_string(ERR_get_error(), nullptr);
        EVP_CIPHER_CTX_free(ctx);
        OPENSSL_cleanse(keyData.data(), keyData.size());
        return decryptedValue;
    }

    QByteArray outputData(encryptedData.size(), 0);
    int outputLength = 0;

    // Decrypt ciphertext bytes into plaintext buffer
    if (EVP_DecryptUpdate(
            ctx,
            reinterpret_cast<unsigned char *>(outputData.data()),
            &outputLength,
            reinterpret_cast<const unsigned char *>(encryptedData.constData()),
            encryptedData.size())
        != 1)
    {
        qWarning() << "Encryption::Decrypt: decryption failed:" << ERR_error_string(ERR_get_error(), nullptr);
        EVP_CIPHER_CTX_free(ctx);
        OPENSSL_cleanse(keyData.data(), keyData.size());
        return decryptedValue;
    }

    int finalLength = 0;
    // Finalization fails when auth tag does not match
    if (EVP_DecryptFinal_ex(
            ctx,
            reinterpret_cast<unsigned char *>(outputData.data() + outputLength),
            &finalLength)
        != 1)
    {
        qWarning() << "Encryption::Decrypt: decryption finalization failed (auth tag mismatch?):" << ERR_error_string(ERR_get_error(), nullptr);
        EVP_CIPHER_CTX_free(ctx);
        OPENSSL_cleanse(keyData.data(), keyData.size());
        return decryptedValue;
    }

    EVP_CIPHER_CTX_free(ctx);
    OPENSSL_cleanse(keyData.data(), keyData.size());

    outputData.resize(outputLength + finalLength);

    decryptedValue = QString::fromUtf8(outputData);
    return decryptedValue;
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

QByteArray Encryption::DeriveKey(const QString &key, const QByteArray &saltData) const
{
    QByteArray derivedKey(32, 0);
    QByteArray passwordData = key.toUtf8();

    // PBKDF2 hardens caller key before AES-256-GCM use
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
        qWarning() << "Encryption::DeriveKey: PBKDF2 key derivation failed:" << ERR_error_string(ERR_get_error(), nullptr);
        derivedKey = {};
        return derivedKey;
    }

    return derivedKey;
}
