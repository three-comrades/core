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

#ifndef INCLUDED_FRAMEWORK_SOURCE_INC_LOADENV_DELIVERYHELPER_HXX
#define INCLUDED_FRAMEWORK_SOURCE_INC_LOADENV_DELIVERYHELPER_HXX

#include <sal/types.h>
#include <rtl/ustring.hxx>
#include <recipients.h>

namespace framework {

/** @short  use to check if a name (used e.g. for XFrame.findFrame()) has a special meaning
            or not thus can be used as frame’s name (e.g. for XFrame.setName())

    @author as96863
 */
class DeliveryHelper
{

    public:

        enum ESpecialRecipient
        {
            E_NOT_SPECIAL   ,
            E_SELF          ,
            E_PARENT        ,
            E_TOP           ,
            E_BLANK         ,
            E_DEFAULT       ,
            E_BEAMER        ,
            E_HELPAGENT     ,
            E_HELPTASK
        };

    // interface

    public:

        /** @short  it checks the given unknown recipient name
                    if it's the expected special one.

            @note   An empty name is similar to "_self"

            @param  sName
                    the name to check.

            @param  eSpecial
                    represent the expected type.

            @return It returns <TRUE/> if <var>sName</var> represent
                    the expected <var>eSpecial</var> value; <FALSE/> otherwise.
         */
        static bool isSpecialRecipient( const OUString& sName,
                                              ESpecialRecipient eSpecial );

        /** @short  it checks, if the given name can be used
                    to set it at a frame using XFrame.setName() method.

            @descr  Because we handle special cases in a hard coded way
                    (means we do not check the real name of a frame then)
                    such named frames will never be found

                    And in case such special names can exists one times only
                    by definition inside the same frame tree (e.g. _beamer and
                    OFFICE_HELP_TASK) its not a good idea to allow anything here :-)

                    Of course we can't check unknown names, which are not special ones.
                    But we decide, that it's not allowed to use "_" as first sign
                    (because we reserve this letter for our own purposes!)
                    and the value must not a well known special recipient

            @param  sName
                    the new frame name, which should be checked.
         */
        static bool isValidNameForFrame(const OUString& sName);
};

} // namespace framework

#endif // INCLUDED_FRAMEWORK_SOURCE_INC_LOADENV_DELIVERYHELPER_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
