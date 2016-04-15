/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <loadenv/deliveryhelper.hxx>

namespace framework {

bool DeliveryHelper::isSpecialRecipient( const OUString& sName ,
                                               ESpecialRecipient eSpecial )
{
    switch( eSpecial )
    {
        case E_SELF :
            return ( sName.isEmpty() || sName == SPECIAL_SELF );

        case E_PARENT :
            return sName == SPECIAL_PARENT;

        case E_TOP :
            return sName == SPECIAL_TOP;

        case E_BLANK :
            return sName == SPECIAL_BLANK;

        case E_DEFAULT :
            return sName == SPECIAL_DEFAULT;

        case E_BEAMER :
            return sName == SPECIAL_BEAMER;

        case E_HELPTASK :
            return sName == SPECIAL_HELPTASK;
        default:
            return false;
    }
}

bool DeliveryHelper::isValidNameForFrame( const OUString& sName )
{
    // some special recipients are really special ones :-)
    // e.g. the are really used to locate one frame inside the frame tree
    if (
         ( sName.isEmpty() ) ||
         ( DeliveryHelper::isSpecialRecipient(sName, E_HELPTASK) ) ||
         ( DeliveryHelper::isSpecialRecipient(sName, E_BEAMER) )
       )
        return true;

    // special cases begin with a "_"
    return ( sName.indexOf('_') != 0 );
}

} // namespace framework

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
