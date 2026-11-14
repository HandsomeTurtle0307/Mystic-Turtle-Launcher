/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#pragma once

#include <QString>
#include <QUrl>

#include <functional>

struct PlayerAccount;

class ExternalAuth
{
public:
    // Normalizes a user-entered API root (for example
    // https://littleskin.cn -> https://littleskin.cn/api/yggdrasil).
    static QUrl apiRoot(const QString &serverUrl);

    // Refreshes an existing Yggdrasil token, or falls back to
    // /authserver/authenticate. Updates account->accessToken, clientToken,
    // uuid, username and normalized serverUrl on success.
    static bool authenticate(PlayerAccount *account, QString *errorMessage,
                             const std::function<bool()> &cancelled = {});
};
