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

#include <helper/oframes.hxx>

#include <com/sun/star/frame/XDesktop.hpp>
#include <com/sun/star/frame/FrameSearchFlag.hpp>

#include <vcl/svapp.hxx>

namespace framework{

using namespace ::com::sun::star::container;
using namespace ::com::sun::star::frame;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::uno;
using namespace ::cppu;
using namespace ::osl;
using namespace ::std;

//  constructor

OFrames::OFrames( const css::uno::Reference< XFrame >&  xOwner ,
                        FrameContainer*            pFrameContainer )
        :   m_xOwner( xOwner )
        ,   m_pFrameContainer( pFrameContainer )
        ,   m_bRecursiveSearchLock( false )
{
    if ( ! xOwner.is() )
        SAL_WARN( "fwk", "OFrames(): owner is nil" );
    if ( ! pFrameContainer )
        SAL_WARN( "fwk", "OFrames(): container is nil" );
}

OFrames::~OFrames()
{
    // Reset instance, free memory
    impl_resetObject();
}

//  XFrames
void SAL_CALL OFrames::append( const css::uno::Reference< XFrame >& xFrame ) throw( RuntimeException, std::exception )
{
    SolarMutexGuard g;

    SAL_WARN_IF( !xFrame.is(), "fwk", "append(): every frame implements XFrames interface but xFrame parameter is nil" );

    // Lock owner for follow operations - make a "hard reference"
    css::uno::Reference< XFramesSupplier > xOwner( m_xOwner.get(), UNO_QUERY );
    if ( xOwner.is() )
    {
        // Append frame to the end of the container
        m_pFrameContainer->append( xFrame );
        // Set owner of this instance as parent of the new frame in container!
        xFrame->setCreator( xOwner );
    }
    else
    {
        // owner is absent
        SAL_WARN( "fwk", "append(): owner is absent and wouldn't append any frames" );
    }
}

//  XFrames
void SAL_CALL OFrames::remove( const css::uno::Reference< XFrame >& xFrame ) throw( RuntimeException, std::exception )
{
    SolarMutexGuard g;

    SAL_WARN_IF( !xFrame.is(), "fwk", "remove(): how are you going to remove nil frame?" );

    // Do the follow only, if owner instance valid!
    // Lock owner for follow operations - make a "hard reference"!
    css::uno::Reference< XFramesSupplier > xOwner( m_xOwner.get(), UNO_QUERY );
    if ( xOwner.is() )
    {
        // Search frame and remove it from container ...
        m_pFrameContainer->remove( xFrame );
        // Don't reset owner-property of removed frame!
        // This must do the caller of this method himself.
        // See documentation of interface XFrames for further information.
    }
    else
    {
        // owner is quit
        SAL_WARN( "fwk", "remove(): owner is quit and wouldn't remove any frames" );
    }
}

//  XFrames
Sequence< css::uno::Reference< XFrame > > SAL_CALL OFrames::queryFrames( sal_Int32 nSearchOptions )
    throw( RuntimeException, std::exception )
{
    SolarMutexGuard g;

    if  ( nSearchOptions != ( nSearchOptions & FrameSearchOption::FullMask ) )
    {
        SAL_WARN( "fwk", "queryFrames(): I don't know these search options" );
    }

    // default return value is an empty sequence
    Sequence< css::uno::Reference< XFrame > > seqFrames;

    // creating a "hard reference" locks owner for future operations
    css::uno::Reference< XFrame > xOwner( m_xOwner.get(), UNO_QUERY );
    if ( xOwner.is() ) // do it only if owner instance exists
    {
        // continue only if search was not started here
        if( !m_bRecursiveSearchLock )
        {
            // This class is a helper for services which implement XFrames
            // His parent and children are MY parent and children too

            // Add parent to list ... if any exist
            if( nSearchOptions & FrameSearchOption::Parent )
            {
                css::uno::Reference< XFrame > xParent( xOwner->getCreator(), UNO_QUERY );
                if( xParent.is() )
                {
                    Sequence< css::uno::Reference< XFrame > > seqParent( 1 );
                    seqParent[0] = xParent;
                    impl_appendSequence( seqFrames, seqParent );
                }
            }

            // Add owner to list if Self is searched
            if( nSearchOptions & FrameSearchOption::Self )
            {
                Sequence< css::uno::Reference< XFrame > > seqSelf( 1 );
                seqSelf[0] = xOwner;
                impl_appendSequence( seqFrames, seqSelf );
            }

            // Add Siblings to list
            if( nSearchOptions & FrameSearchOption::Siblings )
            {
                m_bRecursiveSearchLock = true;

                // Ask parent of my owner for frames and append results to return list.
                css::uno::Reference< XFramesSupplier > xParent( xOwner->getCreator(), UNO_QUERY );
                // If a parent exist ...
                if ( xParent.is() )
                {
                    // ... ask him for frames
                    impl_appendSequence( seqFrames, xParent->getFrames()->queryFrames( nSearchOptions ) );
                }

                m_bRecursiveSearchLock = false;
            }

            // If search is for children, step over all elements in container and collect the information
            if ( nSearchOptions & FrameSearchOption::Children )
            {
                // Don't search for parents, siblings and self at children
                // These things are supported by this instance himself
                sal_Int32 nChildSearchOptions = FrameSearchOption::Self | FrameSearchOption::Children;
                // Step over all items of container and ask children for frames.
                sal_uInt32 nCount = m_pFrameContainer->getCount();
                for ( sal_uInt32 nIndex=0; nIndex<nCount; ++nIndex )
                {
                    css::uno::Reference< XFramesSupplier > xItem( (*m_pFrameContainer)[nIndex], UNO_QUERY );
                    impl_appendSequence( seqFrames, xItem->getFrames()->queryFrames( nChildSearchOptions ) );
                }
            }
        }
    }
    else
    {
        SAL_WARN( "fwk", "queryFrames(): owner is nil and wouldn't tell about frames" );
    }

    return seqFrames;
}

//  XIndexAccess
sal_Int32 SAL_CALL OFrames::getCount() throw( RuntimeException, std::exception )
{
    SolarMutexGuard g;

    sal_Int32 nCount = 0;

    // Do it only if owner instance exists
    css::uno::Reference< XFrame > xOwner( m_xOwner.get(), UNO_QUERY );
    if ( xOwner.is() )
    {
        // get size of container for return
        nCount = m_pFrameContainer->getCount();
    }

    return nCount;
}

//  XIndexAccess

Any SAL_CALL OFrames::getByIndex( sal_Int32 nIndex ) throw( IndexOutOfBoundsException   ,
                                                            WrappedTargetException      ,
                                                            RuntimeException, std::exception            )
{
    SolarMutexGuard g;

      sal_uInt32 nCount = m_pFrameContainer->getCount();
      if ( nIndex < 0 || ( sal::static_int_cast< sal_uInt32 >( nIndex ) >= nCount ))
          throw IndexOutOfBoundsException("OFrames::getByIndex - Index out of bounds",
                                           static_cast<OWeakObject *>(this) );

    // Set default return value.
    Any aReturnValue;

    // Do the follow only, if owner instance valid.
    // Lock owner for follow operations - make a "hard reference"!
    css::uno::Reference< XFrame > xOwner( m_xOwner.get(), UNO_QUERY );
    if ( xOwner.is() )
    {
        // Get element form container.
        // (If index not valid, FrameContainer return NULL!)
            aReturnValue <<= (*m_pFrameContainer)[nIndex];
    }

    // Return result of this operation.
    return aReturnValue;
}

//  XElementAccess
Type SAL_CALL OFrames::getElementType() throw( RuntimeException, std::exception )
{
    // This "container" support XFrame-interfaces only!
    return cppu::UnoType<XFrame>::get();
}

//  XElementAccess
sal_Bool SAL_CALL OFrames::hasElements() throw( RuntimeException, std::exception )
{
    SolarMutexGuard g;

    // Set default return value.
    bool bHasElements = false;
    // Do the follow only, if owner instance valid.
    // Lock owner for follow operations - make a "hard reference"!
    css::uno::Reference< XFrame > xOwner( m_xOwner.get(), UNO_QUERY );
    if ( xOwner.is() )
    {
        // If some elements exist ...
        if ( m_pFrameContainer->getCount() > 0 )
        {
            // ... change this state value!
            bHasElements = true;
        }
    }
    // Return result of this operation.
    return bHasElements;
}

//  proteced method

void OFrames::impl_resetObject()
{
    // Attention:
    // Write this for multiple calls - NOT AT THE SAME TIME - but for more than one call again)!
    // It exist two ways to call this method. From destructor and from disposing().
    // I can't say, which one is the first. Normally the disposing-call - but other way ....

    // This instance can't work if the weakreference to owner is invalid!
    // Destroy this to reset this object.
    m_xOwner.clear();
    // Reset pointer to shared container to!
    m_pFrameContainer = nullptr;
}

void OFrames::impl_appendSequence(          Sequence< css::uno::Reference< XFrame > >&  seqDestination  ,
                                     const  Sequence< css::uno::Reference< XFrame > >&  seqSource       )
{
    // Get some information about the sequences.
    sal_Int32                       nSourceCount        = seqSource.getLength();
    sal_Int32                       nDestinationCount   = seqDestination.getLength();
    const css::uno::Reference< XFrame >*        pSourceAccess       = seqSource.getConstArray();
    css::uno::Reference< XFrame >*          pDestinationAccess  = seqDestination.getArray();

    // Get memory for result list.
    Sequence< css::uno::Reference< XFrame > >   seqResult           ( nSourceCount + nDestinationCount );
    css::uno::Reference< XFrame >*          pResultAccess       = seqResult.getArray();
    sal_Int32                       nResultPosition     = 0;

    // Copy all items from first sequence.
    for ( sal_Int32 nSourcePosition=0; nSourcePosition<nSourceCount; ++nSourcePosition )
    {
        pResultAccess[nResultPosition] = pSourceAccess[nSourcePosition];
        ++nResultPosition;
    }

    // Don't manipulate nResultPosition between these two loops!
    // Its the current position in the result list.

    // Copy all items from second sequence.
    for ( sal_Int32 nDestinationPosition=0; nDestinationPosition<nDestinationCount; ++nDestinationPosition )
    {
        pResultAccess[nResultPosition] = pDestinationAccess[nDestinationPosition];
        ++nResultPosition;
    }

    // Return result of this operation.
    seqDestination.realloc( 0 );
    seqDestination = seqResult;
}

}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
