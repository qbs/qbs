/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "applecodesignutils.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qcryptographichash.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qdebug.h>

#include <QtNetwork/qsslcertificate.h>
#include <QtNetwork/qsslcertificateextension.h>

#include <QtCore/private/qcore_mac_p.h>
#include <Security/Security.h>

#include <mutex>

namespace qbs {
namespace Internal {

QByteArray smimeMessageContent(const QByteArray &data)
{
    QCFType<CMSDecoderRef> decoder = NULL;
    if (CMSDecoderCreate(&decoder) != noErr)
        return {};

    if (CMSDecoderUpdateMessage(decoder, data.constData(), data.size()) != noErr)
        return {};

    if (CMSDecoderFinalizeMessage(decoder) != noErr)
        return {};

    QCFType<CFDataRef> content = NULL;
    if (CMSDecoderCopyContent(decoder, &content) != noErr)
        return {};

    return QByteArray::fromCFData(content);
}

QVariantMap certificateInfo(const QByteArray &data)
{
    const QSslCertificate cert(data, QSsl::Der);

    // Also potentially useful, but these are for signing pkgs which aren't used here
    // 1.2.840.113635.100.4.9 - 3rd Party Mac Developer Installer: <name>
    // 1.2.840.113635.100.4.13 - Developer ID Installer: <name>
    const auto extensions = cert.extensions();
    for (const auto &extension : extensions) {
        if (extension.name() == QStringLiteral("extendedKeyUsage")) {
             if (!extension.value().toStringList().contains(QStringLiteral("Code Signing")))
                 return {};
        }
    }

    const auto subjectInfo = [](const QSslCertificate &cert) {
        QVariantMap map;
        const auto attributes = cert.subjectInfoAttributes();
        for (const auto &attr : attributes)
            map.insert(QString::fromUtf8(attr), cert.subjectInfo(attr).front());
        return map;
    };

    return {
        {QStringLiteral("SHA1"), cert.digest(QCryptographicHash::Sha1).toHex().toUpper()},
        {QStringLiteral("subjectInfo"), subjectInfo(cert)},
        {QStringLiteral("validBefore"), cert.effectiveDate()},
        {QStringLiteral("validAfter"), cert.expiryDate()}
    };
}

QVariantMap identitiesProperties()
{
    // Apple documentation states that the Sec* family of functions are not thread-safe on macOS
    // https://developer.apple.com/library/mac/documentation/Security/Reference/certifkeytrustservices/
    static std::mutex securityMutex;
    std::lock_guard<std::mutex> locker(securityMutex);
    Q_UNUSED(locker);

    const void *keys[] = {kSecClass, kSecMatchLimit, kSecAttrCanSign};
    const void *values[] = {kSecClassIdentity, kSecMatchLimitAll, kCFBooleanTrue};
    QCFType<CFDictionaryRef> query = CFDictionaryCreate(kCFAllocatorDefault,
                                                        keys,
                                                        values,
                                                        sizeof(keys) / sizeof(keys[0]),
                                                        &kCFTypeDictionaryKeyCallBacks,
                                                        &kCFTypeDictionaryValueCallBacks);
    QCFType<CFTypeRef> result = NULL;
    if (SecItemCopyMatching(query, &result) != errSecSuccess)
        return {};

    QVariantMap items;
    const auto tryAppend = [&](SecIdentityRef identity) {
        if (!identity)
            return;

        QCFType<SecCertificateRef> certificate = NULL;
        if (SecIdentityCopyCertificate(identity, &certificate) != errSecSuccess)
            return;

        QCFType<CFDataRef> certificateData = SecCertificateCopyData(certificate);
        if (!certificateData)
            return;

        auto props = certificateInfo(QByteArray::fromRawCFData(certificateData));
        if (!props.empty())
            items.insert(props[QStringLiteral("SHA1")].toString(), props);
    };

    if (CFGetTypeID(result) == SecIdentityGetTypeID()) {
        tryAppend((SecIdentityRef)result.operator const void *());
    } else if (CFGetTypeID(result) == CFArrayGetTypeID()) {
        for (CFIndex i = 0; i < CFArrayGetCount((CFArrayRef)result.operator const void *()); ++i)
            tryAppend((SecIdentityRef)CFArrayGetValueAtIndex(result.as<CFArrayRef>(), i));
    }

    return items;
}

} // namespace Internal
} // namespace qbs
