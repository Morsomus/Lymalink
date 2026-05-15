/////////////////////////////////////////////////////////
// File: EncryptionTests.cpp
// Date: 2026-05-14
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Tests Encryption AES-GCM helpers
/////////////////////////////////////////////////////////

#include "../src/tools/Encryption.h"

#include <QByteArray>
#include <QDebug>
#include <QtTest/QtTest>

class EncryptionTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void encrypt_withEmptyKey_returnsEmpty();
    void decrypt_withEmptyKey_returnsEmpty();
    void encrypt_emptyValue_returnsEmpty();
    void encryptDecrypt_roundTripsUtf8Text();
    void encrypt_payloadIncludesVersion();
    void encrypt_sameValueTwice_producesDifferentCiphertext();
    void decrypt_withWrongKey_returnsEmpty();
    void decrypt_tamperedCiphertext_returnsEmpty();
    void decrypt_invalidPayload_returnsEmpty();
    void decrypt_tooShortPayload_returnsEmpty();
};

/////////////////////////////////////////////////////////////////////

void EncryptionTests::initTestCase()
{
    qputenv("QT_LOGGING_RULES", "*.debug=true");
}

/////////////////////////////////////////////////////////////////////

void EncryptionTests::encrypt_withEmptyKey_returnsEmpty()
{
    Encryption encryption;

    QCOMPARE(encryption.Encrypt("plain text", QString()), QString());
}

/////////////////////////////////////////////////////////////////////

void EncryptionTests::decrypt_withEmptyKey_returnsEmpty()
{
    Encryption encryption;

    QCOMPARE(encryption.Decrypt("not-encrypted", QString()), QString());
}

/////////////////////////////////////////////////////////////////////

void EncryptionTests::encrypt_emptyValue_returnsEmpty()
{
    Encryption encryption;

    QCOMPARE(encryption.Encrypt(QString(), "test-key"), QString());
}

/////////////////////////////////////////////////////////////////////

void EncryptionTests::encryptDecrypt_roundTripsUtf8Text()
{
    Encryption encryption;

    const QString plainText = QString::fromUtf8("secret text with UTF-8: äöå Привет こんにちは");
    const QString cipherText = encryption.Encrypt(plainText, "strong test key");

    QVERIFY(!cipherText.isEmpty());
    QVERIFY(cipherText != plainText);
    QCOMPARE(encryption.Decrypt(cipherText, "strong test key"), plainText);
}

/////////////////////////////////////////////////////////////////////

void EncryptionTests::encrypt_payloadIncludesVersion()
{
    Encryption encryption;

    const QString cipherText = encryption.Encrypt("secret text", "strong test key");
    QVERIFY(!cipherText.isEmpty());

    const QByteArray payload = QByteArray::fromBase64(cipherText.toLatin1());
    QVERIFY(payload.size() > 45);
    QCOMPARE(payload.at(0), char(1));
}

/////////////////////////////////////////////////////////////////////

void EncryptionTests::encrypt_sameValueTwice_producesDifferentCiphertext()
{
    Encryption encryption;

    const QString plainText = "same secret text";
    const QString firstCipherText = encryption.Encrypt(plainText, "strong test key");
    const QString secondCipherText = encryption.Encrypt(plainText, "strong test key");

    QVERIFY(!firstCipherText.isEmpty());
    QVERIFY(!secondCipherText.isEmpty());
    QVERIFY(firstCipherText != secondCipherText);
    QCOMPARE(encryption.Decrypt(firstCipherText, "strong test key"), plainText);
    QCOMPARE(encryption.Decrypt(secondCipherText, "strong test key"), plainText);
}

/////////////////////////////////////////////////////////////////////

void EncryptionTests::decrypt_withWrongKey_returnsEmpty()
{
    Encryption encryption;
    const QString cipherText = encryption.Encrypt("secret text", "correct key");
    QVERIFY(!cipherText.isEmpty());

    QCOMPARE(encryption.Decrypt(cipherText, "wrong key"), QString());
}

/////////////////////////////////////////////////////////////////////

void EncryptionTests::decrypt_tamperedCiphertext_returnsEmpty()
{
    Encryption encryption;

    const QString plainText = "secret text";
    const QString cipherText = encryption.Encrypt(plainText, "strong test key");
    QVERIFY(!cipherText.isEmpty());

    // Layout: [version 1B][salt 16B][IV 12B][ciphertext][tag 16B]
    QByteArray base = QByteArray::fromBase64(cipherText.toLatin1());
    
    // Tamper the GCM authentication tag (last 16 bytes)
    QByteArray tamperedTag = base;
    tamperedTag[tamperedTag.size() - 1] = tamperedTag.at(tamperedTag.size() - 1) ^ char(0x01);
    QCOMPARE(encryption.Decrypt(QString::fromLatin1(tamperedTag.toBase64()), "strong test key"), QString());

    // Tamper the ciphertext body (byte 29, first ciphertext byte)
    QByteArray tamperedCipher = base;
    tamperedCipher[29] = tamperedCipher.at(29) ^ char(0x01);
    QCOMPARE(encryption.Decrypt(QString::fromLatin1(tamperedCipher.toBase64()), "strong test key"), QString());

    // Tamper the IV (byte 17, first IV byte)
    QByteArray tamperedIv = base;
    tamperedIv[17] = tamperedIv.at(17) ^ char(0x01);
    QCOMPARE(encryption.Decrypt(QString::fromLatin1(tamperedIv.toBase64()), "strong test key"), QString());

    // Tamper the salt (byte 1, first salt byte)
    QByteArray tamperedSalt = base;
    tamperedSalt[1] = tamperedSalt.at(1) ^ char(0x01);
    QCOMPARE(encryption.Decrypt(QString::fromLatin1(tamperedSalt.toBase64()), "strong test key"), QString());

    // Tamper the version (byte 0)
    QByteArray tamperedVersion = base;
    tamperedVersion[0] = tamperedVersion.at(0) ^ char(0x01);
    QCOMPARE(encryption.Decrypt(QString::fromLatin1(tamperedVersion.toBase64()), "strong test key"), QString());
}

/////////////////////////////////////////////////////////////////////

void EncryptionTests::decrypt_invalidPayload_returnsEmpty()
{
    Encryption encryption;

    QCOMPARE(encryption.Decrypt("invalid-payload", "strong test key"), QString());
}

/////////////////////////////////////////////////////////////////////

void EncryptionTests::decrypt_tooShortPayload_returnsEmpty()
{
    Encryption encryption;

    QByteArray tooShort(44, 'x');
    QCOMPARE(encryption.Decrypt(QString::fromLatin1(tooShort.toBase64()), "strong test key"), QString());
}

/////////////////////////////////////////////////////////////////////

QTEST_MAIN(EncryptionTests)
#include "EncryptionTests.moc"
