/*
 *  This file is part of KIMToy, an input method frontend for KDE
 *  Copyright (C) 2011-2012 Ni Hui <shuizhuyuanluo@126.com>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of
 *  the License or (at your option) version 3 or any later version
 *  accepted by the membership of KDE e.V. (or its successor approved
 *  by the membership of KDE e.V.), which shall act as a proxy
 *  defined in Section 14 of version 3 of the license.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SCIM_PANEL_H
#define SCIM_PANEL_H

#include <dbus-c++/dbus.h>
#include "inputmethod_adaptor.h"
#include "impanel_proxy.h"

class Panel : public org::kde::kimpanel::inputmethod_adaptor,
              public org::kde::impanel_proxy,
              public DBus::IntrospectableAdaptor,
              public DBus::ObjectAdaptor,
              public DBus::IntrospectableProxy,
              public DBus::ObjectProxy
{
public:
    explicit Panel(DBus::Connection &connection);
    virtual ~Panel();
    virtual void MovePreeditCaret(const int32_t& pos);
    virtual void SelectCandidate(const int32_t& index);
    virtual void LookupTablePageUp();
    virtual void LookupTablePageDown();
    virtual void TriggerProperty(const std::string& key);
    virtual void PanelCreated();
    virtual void Exit();
    virtual void ReloadConfig();
    virtual void Configure();
private:
};

#endif // SCIM_PANEL_H