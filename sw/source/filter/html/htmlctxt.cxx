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

#include "hintids.hxx"
#include <svl/itemiter.hxx>
#include <editeng/lrspitem.hxx>
#include <editeng/ulspitem.hxx>
#include <editeng/brushitem.hxx>
#include <editeng/fhgtitem.hxx>
#include <svtools/htmltokn.h>
#include <editeng/boxitem.hxx>

#include "doc.hxx"
#include "pam.hxx"
#include "ndtxt.hxx"
#include "shellio.hxx"
#include "paratr.hxx"
#include "htmlnum.hxx"
#include "css1kywd.hxx"
#include "swcss1.hxx"
#include "swhtml.hxx"

using namespace ::com::sun::star;

class HTMLAttrContext_SaveDoc
{
    SwHTMLNumRuleInfo aNumRuleInfo; // numbering that is valid in the context
    SwPosition  *pPos;              // jump back to this point when leaving
                                    // context
    HTMLAttrTable *pAttrTab;        // attributes that are valid in the context,
                                    // if the attribute is not supposed to be
                                    // kept.

    size_t nContextStMin;           // the lower stack limit in the context,
                                    // if the context is supposed to be
                                    // protected.
    size_t nContextStAttrMin;       // the lower stack limit in the contect,
                                    // if the attributes are not suppoed to
                                    // be kept.

    bool bStripTrailingPara : 1;    // delete the last paragraph?
    bool bKeepNumRules : 1;         // keep the numbering?
    bool bFixHeaderDist : 1;
    bool bFixFooterDist : 1;

public:

    HTMLAttrContext_SaveDoc() :
        pPos( nullptr ), pAttrTab( nullptr ),
        nContextStMin( SIZE_MAX ), nContextStAttrMin( SIZE_MAX ),
        bStripTrailingPara( false ), bKeepNumRules( false ),
        bFixHeaderDist( false ), bFixFooterDist( false )
    {}

    ~HTMLAttrContext_SaveDoc() { delete pPos; delete pAttrTab; }

    // We own the position, i.e., it must be created and destroyed.
    void SetPos( const SwPosition& rPos ) { pPos = new SwPosition(rPos); }
    const SwPosition *GetPos() const { return pPos; }

    // We don't own the index. Don't create and destroy.
    void SetNumInfo( const SwHTMLNumRuleInfo& rInf ) { aNumRuleInfo.Set(rInf); }
    const SwHTMLNumRuleInfo& GetNumInfo() const { return aNumRuleInfo; }

    HTMLAttrTable *GetAttrTab( bool bCreate= false );

    void SetContextStMin( size_t nMin ) { nContextStMin = nMin; }
    size_t GetContextStMin() const { return nContextStMin; }

    void SetContextStAttrMin( size_t nMin ) { nContextStAttrMin = nMin; }
    size_t GetContextStAttrMin() const { return nContextStAttrMin; }

    void SetStripTrailingPara( bool bSet ) { bStripTrailingPara = bSet; }
    bool GetStripTrailingPara() const { return bStripTrailingPara; }

    void SetKeepNumRules( bool bSet ) { bKeepNumRules = bSet; }
    bool GetKeepNumRules() const { return bKeepNumRules; }

    void SetFixHeaderDist( bool bSet ) { bFixHeaderDist = bSet; }
    bool GetFixHeaderDist() const { return bFixHeaderDist; }

    void SetFixFooterDist( bool bSet ) { bFixFooterDist = bSet; }
    bool GetFixFooterDist() const { return bFixFooterDist; }
};

HTMLAttrTable *HTMLAttrContext_SaveDoc::GetAttrTab( bool bCreate )
{
    if( !pAttrTab && bCreate )
    {
        pAttrTab = new HTMLAttrTable;
        memset( pAttrTab, 0, sizeof( HTMLAttrTable ));
    }
    return pAttrTab;
}

HTMLAttrContext_SaveDoc *HTMLAttrContext::GetSaveDocContext( bool bCreate )
{
    if( !pSaveDocContext && bCreate )
        pSaveDocContext = new HTMLAttrContext_SaveDoc;

    return pSaveDocContext;
}

void HTMLAttrContext::ClearSaveDocContext()
{
    delete pSaveDocContext;
    pSaveDocContext = nullptr;
}

void SwHTMLParser::SplitAttrTab( const SwPosition& rNewPos )
{
    // preliminary paragraph attributes are not allowed here, they could
    // be set here and then the pointers become invalid!
    OSL_ENSURE(m_aParaAttrs.empty(),
        "Danger: there are non-final paragraph attributes");
    if( !m_aParaAttrs.empty() )
        m_aParaAttrs.clear();

    const SwNodeIndex* pOldEndPara = &m_pPam->GetPoint()->nNode;
    sal_Int32 nOldEndCnt = m_pPam->GetPoint()->nContent.GetIndex();

    const SwNodeIndex& rNewSttPara = rNewPos.nNode;
    sal_Int32 nNewSttCnt = rNewPos.nContent.GetIndex();

    bool bMoveBack = false;

    // close all attributes that are still open and open them again after
    // the table
    HTMLAttr** pHTMLAttributes = reinterpret_cast<HTMLAttr**>(&m_aAttrTab);
    for (auto nCnt = sizeof(HTMLAttrTable) / sizeof(HTMLAttr*); nCnt--; ++pHTMLAttributes)
    {
        HTMLAttr *pAttr = *pHTMLAttributes;
        while( pAttr )
        {
            HTMLAttr *pNext = pAttr->GetNext();
            HTMLAttr *pPrev = pAttr->GetPrev();

            sal_uInt16 nWhich = pAttr->pItem->Which();
            if( !nOldEndCnt && RES_PARATR_BEGIN <= nWhich &&
                pAttr->GetSttParaIdx() < pOldEndPara->GetIndex() )
            {
                // The attribute must be closed one Content position earlier
                if( !bMoveBack )
                {
                    bMoveBack = m_pPam->Move( fnMoveBackward );
                    nOldEndCnt = m_pPam->GetPoint()->nContent.GetIndex();
                }
            }
            else if( bMoveBack )
            {
                m_pPam->Move( fnMoveForward );
                nOldEndCnt = m_pPam->GetPoint()->nContent.GetIndex();
            }

            if( (RES_PARATR_BEGIN <= nWhich && bMoveBack) ||
                pAttr->GetSttParaIdx() < pOldEndPara->GetIndex() ||
                (pAttr->GetSttPara() == *pOldEndPara &&
                 pAttr->GetSttCnt() != nOldEndCnt) )
            {
                // The attribute must be set. We must clone it, because we
                // still need the original, because there still exist pointers
                // in the contexts that point to the attribute. Doing this, we
                // lose the Next list, but the Previous list is preserved.
                HTMLAttr *pSetAttr = pAttr->Clone( *pOldEndPara, nOldEndCnt );

                if( pNext )
                    pNext->InsertPrev( pSetAttr );
                else
                {
                    if (pSetAttr->bInsAtStart)
                        m_aSetAttrTab.push_front( pSetAttr );
                    else
                        m_aSetAttrTab.push_back( pSetAttr );
                }
            }
            else if( pPrev )
            {
                // If the attribute doesn't have to be set before the table,
                // the Previous attributes must be set anyway.
                if( pNext )
                    pNext->InsertPrev( pPrev );
                else
                {
                    if (pPrev->bInsAtStart)
                        m_aSetAttrTab.push_front( pPrev );
                    else
                        m_aSetAttrTab.push_back( pPrev );
                }
            }

            // set the start of the attribute again
            pAttr->nSttPara = rNewSttPara;
            pAttr->nEndPara = rNewSttPara;
            pAttr->nSttContent = nNewSttCnt;
            pAttr->nEndContent = nNewSttCnt;
            pAttr->pPrev = nullptr;

            pAttr = pNext;
        }
    }

    if( bMoveBack )
        m_pPam->Move( fnMoveForward );

}

void SwHTMLParser::SaveDocContext( HTMLAttrContext *pCntxt,
                                   sal_uInt16 nFlags,
                                   const SwPosition *pNewPos )
{
    HTMLAttrContext_SaveDoc *pSave = pCntxt->GetSaveDocContext( true );
    pSave->SetStripTrailingPara( (HTML_CNTXT_STRIP_PARA & nFlags) != 0 );
    pSave->SetKeepNumRules( (HTML_CNTXT_KEEP_NUMRULE & nFlags) != 0 );
    pSave->SetFixHeaderDist( (HTML_CNTXT_HEADER_DIST & nFlags) != 0 );
    pSave->SetFixFooterDist( (HTML_CNTXT_FOOTER_DIST & nFlags) != 0 );

    if( pNewPos )
    {
        // If the PaM is set to a different position, the numbering must be
        // rescued.
        if( !pSave->GetKeepNumRules() )
        {
            // The numbering is not supposed to be kept. Thus, the current
            // state must be rescued and, afterwards, the numbering must be
            // switched off.
            pSave->SetNumInfo( GetNumInfo() );
            GetNumInfo().Clear();
        }

        if( (HTML_CNTXT_KEEP_ATTRS & nFlags) != 0 )
        {
            // Close the attribute at the current position and start over at
            // the new position
            SplitAttrTab( *pNewPos );
        }
        else
        {
            HTMLAttrTable *pSaveAttrTab = pSave->GetAttrTab( true );
            SaveAttrTab( *pSaveAttrTab );
        }

        pSave->SetPos( *m_pPam->GetPoint() );
        *m_pPam->GetPoint() = *pNewPos;
    }

    // Setting nContextStMin automatically entails that currently opened
    // lists (DL/OL/UL) are not closed anymore.
    if( (HTML_CNTXT_PROTECT_STACK & nFlags) != 0  )
    {
        pSave->SetContextStMin( m_nContextStMin );
        m_nContextStMin = m_aContexts.size();

        if( (HTML_CNTXT_KEEP_ATTRS & nFlags) == 0 )
        {
            pSave->SetContextStAttrMin( m_nContextStAttrMin );
            m_nContextStAttrMin = m_aContexts.size();
        }
    }
}

void SwHTMLParser::RestoreDocContext( HTMLAttrContext *pCntxt )
{
    HTMLAttrContext_SaveDoc *pSave = pCntxt->GetSaveDocContext();
    if( !pSave )
        return;

    if( pSave->GetStripTrailingPara() )
        StripTrailingPara();

    if( pSave->GetPos() )
    {
        if( pSave->GetFixHeaderDist() || pSave->GetFixFooterDist() )
            FixHeaderFooterDistance( pSave->GetFixHeaderDist(),
                                     pSave->GetPos() );

        HTMLAttrTable *pSaveAttrTab = pSave->GetAttrTab();
        if( !pSaveAttrTab )
        {
            // Close the attributes at the current position and start over at
            // the new position
            SplitAttrTab( *pSave->GetPos() );
        }
        else
        {
            RestoreAttrTab( *pSaveAttrTab );
        }

        *m_pPam->GetPoint() = *pSave->GetPos();

        // Let's set all the attributes we have so far.
        SetAttr();
    }

    if( SIZE_MAX != pSave->GetContextStMin() )
    {
        m_nContextStMin = pSave->GetContextStMin();
        if( SIZE_MAX != pSave->GetContextStAttrMin() )
            m_nContextStAttrMin = pSave->GetContextStAttrMin();
    }

    if( !pSave->GetKeepNumRules() )
    {
        // Set the numbering again that we have remembered so far.
        GetNumInfo().Set( pSave->GetNumInfo() );
    }

    pCntxt->ClearSaveDocContext();
}

void SwHTMLParser::EndContext( HTMLAttrContext *pContext )
{
    if( pContext->GetPopStack() )
    {
        // Close all contexts that are still open. The own context must
        // already have been deleted!
        while( m_aContexts.size() > m_nContextStMin )
        {
            HTMLAttrContext *pCntxt = PopContext();
            OSL_ENSURE( pCntxt != pContext,
                    "Context still on the stack" );
            if( pCntxt == pContext )
                break;

            EndContext( pCntxt );
            delete pCntxt;
        }
    }

    // Close all attributes that are still open
    if( pContext->HasAttrs() )
        EndContextAttrs( pContext );

    // If a section was opened, leave it. This has to happen before an old
    // document context is restored, because sections are also created
    // inside of absolutely-positioned objects.
    if( pContext->GetSpansSection() )
        EndSection();

    // Leave frames and all other special sections.
    if( pContext->HasSaveDocContext() )
        RestoreDocContext( pContext );

    // If necessary, insert another paragraph break
    if( AM_NONE != pContext->GetAppendMode() &&
        m_pPam->GetPoint()->nContent.GetIndex() )
        AppendTextNode( pContext->GetAppendMode() );

    // Restart PRE/LISTING and XMP contexts
    if( pContext->IsFinishPREListingXMP() )
        FinishPREListingXMP();

    if( pContext->IsRestartPRE() )
        StartPRE();

    if( pContext->IsRestartXMP() )
        StartXMP();

    if( pContext->IsRestartListing() )
        StartListing();
}

void SwHTMLParser::ClearContext( HTMLAttrContext *pContext )
{
    HTMLAttrs &rAttrs = pContext->GetAttrs();
    for( auto pAttr : rAttrs )
    {
        // Simple deletion is not enough here, because the attribute must
        // also be removed from its list. Theoretically, you could of course
        // also delete the list and the attributes separately, but if you
        // then do anything wrong, the result looks horrible.
        DeleteAttr( pAttr );
    }

    OSL_ENSURE( !pContext->GetSpansSection(),
            "Cannot leave this section anymore" );

    OSL_ENSURE( !pContext->HasSaveDocContext(),
            "Cannot leave this frame anymore" );

    // Restart PRE/LISTING and XMP contexts
    if( pContext->IsFinishPREListingXMP() )
        FinishPREListingXMP();

    if( pContext->IsRestartPRE() )
        StartPRE();

    if( pContext->IsRestartXMP() )
        StartXMP();

    if( pContext->IsRestartListing() )
        StartListing();
}

bool SwHTMLParser::DoPositioning( SfxItemSet &rItemSet,
                                  SvxCSS1PropertyInfo &rPropInfo,
                                  HTMLAttrContext *pContext )
{
    bool bRet = false;

    // Under the following circumstances a frame is now created:
    // - the tag is positioned absolutely and left/top were both supplied and
    //   don't contain a % value, or
    // - the tag is supposed to float and
    // - a width was specified (which is needed in both cases).
    if( SwCSS1Parser::MayBePositioned( rPropInfo ) )
    {
        SfxItemSet aFrameItemSet( m_pDoc->GetAttrPool(),
                                RES_FRMATR_BEGIN, RES_FRMATR_END-1 );
        if( !IsNewDoc() )
            Reader::ResetFrameFormatAttrs(aFrameItemSet );

        // set the alignment
        SetAnchorAndAdjustment( text::VertOrientation::NONE, text::HoriOrientation::NONE, rItemSet, rPropInfo,
                                aFrameItemSet );

        // set the size
        SetVarSize( rItemSet, rPropInfo, aFrameItemSet );

        // set the spacings
        SetSpace( Size(0,0), rItemSet, rPropInfo, aFrameItemSet );

        // set all the other CSS1 attributes
        SetFrameFormatAttrs( rItemSet, rPropInfo,
                        HTML_FF_BOX|HTML_FF_PADDING|HTML_FF_BACKGROUND|HTML_FF_DIRECTION,
                        aFrameItemSet );

        InsertFlyFrame( aFrameItemSet, pContext, rPropInfo.aId,
                        CONTEXT_FLAGS_ABSPOS );
        pContext->SetPopStack( true );
        rPropInfo.aId.clear();
        bRet = true;
    }

    return bRet;
}

bool SwHTMLParser::CreateContainer( const OUString& rClass,
                                    SfxItemSet &rItemSet,
                                    SvxCSS1PropertyInfo &rPropInfo,
                                    HTMLAttrContext *pContext )
{
    bool bRet = false;
    if( rClass.equalsIgnoreAsciiCase( "sd-abs-pos" ) &&
        SwCSS1Parser::MayBePositioned( rPropInfo ) )
    {
        // Container class
        SfxItemSet *pFrameItemSet = pContext->GetFrameItemSet( m_pDoc );
        if( !IsNewDoc() )
            Reader::ResetFrameFormatAttrs( *pFrameItemSet );

        SetAnchorAndAdjustment( text::VertOrientation::NONE, text::HoriOrientation::NONE,
                                rItemSet, rPropInfo, *pFrameItemSet );
        Size aDummy(0,0);
        SetFixSize( aDummy, aDummy, false, false, rItemSet, rPropInfo,
                    *pFrameItemSet );
        SetSpace( aDummy, rItemSet, rPropInfo, *pFrameItemSet );
        SetFrameFormatAttrs( rItemSet, rPropInfo, HTML_FF_BOX|HTML_FF_BACKGROUND|HTML_FF_DIRECTION,
                        *pFrameItemSet );

        bRet = true;
    }

    return bRet;
}

void SwHTMLParser::InsertAttrs( SfxItemSet &rItemSet,
                                SvxCSS1PropertyInfo &rPropInfo,
                                HTMLAttrContext *pContext,
                                bool bCharLvl )
{
    // Build a DropCap attribute, if there is a float: left before the first
    // character on the character layer.
    if( bCharLvl && !m_pPam->GetPoint()->nContent.GetIndex() &&
        SVX_ADJUST_LEFT == rPropInfo.eFloat )
    {
        SwFormatDrop aDrop;
        aDrop.GetChars() = 1;

        m_pCSS1Parser->FillDropCap( aDrop, rItemSet );

        // Only set the DropCap attribute, if the dropcase spans several lines.
        // Otherwise, we set hard attributes.
        if( aDrop.GetLines() > 1 )
        {
            NewAttr( &m_aAttrTab.pDropCap, aDrop );

            HTMLAttrs &rAttrs = pContext->GetAttrs();
            rAttrs.push_back( m_aAttrTab.pDropCap );

            return;
        }
    }

    if( !bCharLvl )
        m_pCSS1Parser->SetFormatBreak( rItemSet, rPropInfo );

    OSL_ENSURE(m_aContexts.size() <= m_nContextStAttrMin ||
            m_aContexts.back() != pContext,
            "SwHTMLParser::InsertAttrs: Context already on the Stack");

    SfxItemIter aIter( rItemSet );

    const SfxPoolItem *pItem = aIter.FirstItem();
    while( pItem )
    {
        HTMLAttr **ppAttr = nullptr;

        switch( pItem->Which() )
        {
        case RES_LR_SPACE:
            {
                // Paragraph indentations must be added and are always set
                // for each paragraph (here, for the first paragraph and for
                // all paragraph that follow in SetTextCollAttrs)

                const SvxLRSpaceItem *pLRItem =
                    static_cast<const SvxLRSpaceItem *>(pItem);

                // Get all paragraph spacings up to now (excluding the ones
                // of the top context, because these are the ones we are
                // currently editing)
                sal_uInt16 nOldLeft = 0, nOldRight = 0;
                short nOldIndent = 0;
                bool bIgnoreTop = m_aContexts.size() > m_nContextStMin &&
                                  m_aContexts.back() == pContext;
                GetMarginsFromContext( nOldLeft, nOldRight, nOldIndent,
                                       bIgnoreTop  );

                // and the ones that are currently valid
                sal_uInt16 nLeft = nOldLeft, nRight = nOldRight;
                short nIndent = nOldIndent;
                pContext->GetMargins( nLeft, nRight, nIndent );

                // ... and add the new spacings to the old ones.
                // Here, we don't take the ones of the item, but the ones
                // we specially remembered, because they can also be negative.
                // The query about the item works anyway, because for negative
                // values the item is also inserted (using a value of 0).
                if( rPropInfo.bLeftMargin )
                {
                    OSL_ENSURE( rPropInfo.nLeftMargin < 0 ||
                            rPropInfo.nLeftMargin == pLRItem->GetTextLeft(),
                            "left margin is not equivalent to the item" );
                    if( rPropInfo.nLeftMargin < 0 &&
                        -rPropInfo.nLeftMargin > nOldLeft )
                        nLeft = 0;
                    else
                        nLeft = nOldLeft + static_cast< sal_uInt16 >(rPropInfo.nLeftMargin);
                }
                if( rPropInfo.bRightMargin )
                {
                    OSL_ENSURE( rPropInfo.nRightMargin < 0 ||
                            rPropInfo.nRightMargin == pLRItem->GetRight(),
                            "right margin is not equivalent to the item" );
                    if( rPropInfo.nRightMargin < 0 &&
                        -rPropInfo.nRightMargin > nOldRight )
                        nRight = 0;
                    else
                        nRight = nOldRight + static_cast< sal_uInt16 >(rPropInfo.nRightMargin);
                }
                if( rPropInfo.bTextIndent )
                    nIndent = pLRItem->GetTextFirstLineOfst();

                // and remember the values for the following paragraphs
                pContext->SetMargins( nLeft, nRight, nIndent );

                // set the attribute for the current paragraph
                SvxLRSpaceItem aLRItem( *pLRItem );
                aLRItem.SetTextFirstLineOfst( nIndent );
                aLRItem.SetTextLeft( nLeft );
                aLRItem.SetRight( nRight );
                NewAttr( &m_aAttrTab.pLRSpace, aLRItem );
                EndAttr( m_aAttrTab.pLRSpace, nullptr, false );
            }
            break;

        case RES_UL_SPACE:
            if( !rPropInfo.bTopMargin || !rPropInfo.bBottomMargin )
            {
                sal_uInt16 nUpper = 0, nLower = 0;
                GetULSpaceFromContext( nUpper, nLower );
                SvxULSpaceItem aULSpace( *static_cast<const SvxULSpaceItem *>(pItem) );
                if( !rPropInfo.bTopMargin )
                    aULSpace.SetUpper( nUpper );
                if( !rPropInfo.bBottomMargin )
                    aULSpace.SetLower( nLower );

                NewAttr( &m_aAttrTab.pULSpace, aULSpace );

                // ... and save the context information
                HTMLAttrs &rAttrs = pContext->GetAttrs();
                rAttrs.push_back( m_aAttrTab.pULSpace );

                pContext->SetULSpace( aULSpace.GetUpper(), aULSpace.GetLower() );
            }
            else
            {
                ppAttr = &m_aAttrTab.pULSpace;
            }
            break;
        case RES_CHRATR_FONTSIZE:
            // don't set attributes that have % values
            if( static_cast<const SvxFontHeightItem *>(pItem)->GetProp() == 100 )
                ppAttr = &m_aAttrTab.pFontHeight;
            break;
        case RES_CHRATR_CJK_FONTSIZE:
            // don't set attributes that have % values
            if( static_cast<const SvxFontHeightItem *>(pItem)->GetProp() == 100 )
                ppAttr = &m_aAttrTab.pFontHeightCJK;
            break;
        case RES_CHRATR_CTL_FONTSIZE:
            // don't set attributes that have % values
            if( static_cast<const SvxFontHeightItem *>(pItem)->GetProp() == 100 )
                ppAttr = &m_aAttrTab.pFontHeightCTL;
            break;

        case RES_BACKGROUND:
            if( bCharLvl )
            {
                // if necessary, convert the frame attribute to a char attribute
                SvxBrushItem aBrushItem( *static_cast<const SvxBrushItem *>(pItem) );
                aBrushItem.SetWhich( RES_CHRATR_BACKGROUND );

                // Set the attribute ...
                NewAttr( &m_aAttrTab.pCharBrush, aBrushItem );

                // ... and save the context information
                HTMLAttrs &rAttrs = pContext->GetAttrs();
                rAttrs.push_back( m_aAttrTab.pCharBrush );
            }
            else if( pContext->GetToken() != HTML_TABLEHEADER_ON &&
                     pContext->GetToken() != HTML_TABLEDATA_ON )
            {
                ppAttr = &m_aAttrTab.pBrush;
            }
            break;

        case RES_BOX:
            if( bCharLvl )
            {
                SvxBoxItem aBoxItem( *static_cast<const SvxBoxItem *>(pItem) );
                aBoxItem.SetWhich( RES_CHRATR_BOX );

                NewAttr( &m_aAttrTab.pCharBox, aBoxItem );

                HTMLAttrs &rAttrs = pContext->GetAttrs();
                rAttrs.push_back( m_aAttrTab.pCharBox );
            }
            else
            {
                ppAttr = &m_aAttrTab.pBox;
            }
            break;

        default:
            // find the item's corresponding table entry
            ppAttr = GetAttrTabEntry( pItem->Which() );
            break;
        }

        if( ppAttr )
        {
            // set the attribute
            NewAttr( ppAttr, *pItem );

            // ... and save the context information
            HTMLAttrs &rAttrs = pContext->GetAttrs();
            rAttrs.push_back( *ppAttr );
        }

        // on to the next item
        pItem = aIter.NextItem();
    }

    if( !rPropInfo.aId.isEmpty() )
        InsertBookmark( rPropInfo.aId );
}

void SwHTMLParser::InsertAttr( HTMLAttr **ppAttr, const SfxPoolItem & rItem,
                               HTMLAttrContext *pCntxt )
{
    if( !ppAttr )
    {
        ppAttr = GetAttrTabEntry( rItem.Which() );
        if( !ppAttr )
            return;
    }

    // set the attribute
    NewAttr( ppAttr, rItem );

    // and remember it in the context
    HTMLAttrs &rAttrs = pCntxt->GetAttrs();
    rAttrs.push_back( *ppAttr );
}

void SwHTMLParser::SplitPREListingXMP( HTMLAttrContext *pCntxt )
{
    // PRE/Listing/XMP is supposed to be closed when the context is closed
    pCntxt->SetFinishPREListingXMP( true );

    // Also, the flags that are now valid are supposed to be set again.
    if( IsReadPRE() )
        pCntxt->SetRestartPRE( true );
    if( IsReadXMP() )
        pCntxt->SetRestartXMP( true );
    if( IsReadListing() )
        pCntxt->SetRestartListing( true );

    // Moreover, PRE/Listing/XMP is closed immediately.
    FinishPREListingXMP();
}

SfxItemSet *HTMLAttrContext::GetFrameItemSet( SwDoc *pCreateDoc )
{
    if( !pFrameItemSet && pCreateDoc )
        pFrameItemSet = new SfxItemSet( pCreateDoc->GetAttrPool(),
                        RES_FRMATR_BEGIN, RES_FRMATR_END-1 );
    return pFrameItemSet;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
