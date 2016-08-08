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
#include <vcl/svapp.hxx>
#include <vcl/wrkwin.hxx>
#include <svx/svdmodel.hxx>
#include <svx/svdpage.hxx>
#include <svx/svdobj.hxx>
#include <svx/svdotext.hxx>
#include <editeng/eeitem.hxx>
#include <editeng/outliner.hxx>
#include <svx/xfillit.hxx>
#include <editeng/colritem.hxx>
#include <editeng/brushitem.hxx>
#include <editeng/lrspitem.hxx>
#include <editeng/ulspitem.hxx>
#include <svl/itemiter.hxx>
#include <svl/whiter.hxx>
#include <svtools/htmlout.hxx>
#include <svtools/htmltokn.h>
#include <svtools/htmlkywd.hxx>
#include <svx/svdpool.hxx>

#include "charatr.hxx"
#include "drawdoc.hxx"
#include <frmfmt.hxx>
#include <fmtanchr.hxx>
#include <fmtsrnd.hxx>
#include "ndtxt.hxx"
#include "doc.hxx"
#include <IDocumentDrawModelAccess.hxx>
#include "dcontact.hxx"
#include "poolfmt.hxx"
#include "swcss1.hxx"
#include "swhtml.hxx"
#include <shellio.hxx>
#include <rtl/strbuf.hxx>

using namespace css;

static HTMLOptionEnum aHTMLMarqBehaviorTable[] =
{
    { OOO_STRING_SVTOOLS_HTML_BEHAV_scroll,     SDRTEXTANI_SCROLL       },
    { OOO_STRING_SVTOOLS_HTML_BEHAV_alternate,  SDRTEXTANI_ALTERNATE    },
    { OOO_STRING_SVTOOLS_HTML_BEHAV_slide,      SDRTEXTANI_SLIDE        },
    { nullptr,                                        0                       }
};

static HTMLOptionEnum aHTMLMarqDirectionTable[] =
{
    { OOO_STRING_SVTOOLS_HTML_AL_left,          SDRTEXTANI_LEFT         },
    { OOO_STRING_SVTOOLS_HTML_AL_right,         SDRTEXTANI_RIGHT        },
    { nullptr,                                        0                       }
};

void SwHTMLParser::InsertDrawObject( SdrObject* pNewDrawObj,
                                     const Size& rPixSpace,
                                     sal_Int16 eVertOri,
                                     sal_Int16 eHoriOri,
                                     SfxItemSet& rCSS1ItemSet,
                                     SvxCSS1PropertyInfo& rCSS1PropInfo )
{
    // always on top of text.
    // but in invisible layer. <ConnectToLayout> will move the object
    // to the visible layer.
    pNewDrawObj->SetLayer( m_pDoc->getIDocumentDrawModelAccess().GetInvisibleHeavenId() );

    SfxItemSet aFrameSet( m_pDoc->GetAttrPool(),
                        RES_FRMATR_BEGIN, RES_FRMATR_END-1 );
    if( !IsNewDoc() )
        Reader::ResetFrameFormatAttrs( aFrameSet );

    sal_uInt16 nLeftSpace = 0, nRightSpace = 0, nUpperSpace = 0, nLowerSpace = 0;
    if( (rPixSpace.Width() || rPixSpace.Height()) && Application::GetDefaultDevice() )
    {
        Size aTwipSpc( rPixSpace.Width(), rPixSpace.Height() );
        aTwipSpc =
            Application::GetDefaultDevice()->PixelToLogic( aTwipSpc,
                                                MapMode(MAP_TWIP) );
        nLeftSpace = nRightSpace = (sal_uInt16)aTwipSpc.Width();
        nUpperSpace = nLowerSpace = (sal_uInt16)aTwipSpc.Height();
    }

    // set the right/left margin
    const SfxPoolItem *pItem;
    if( SfxItemState::SET==rCSS1ItemSet.GetItemState( RES_LR_SPACE, true, &pItem ) )
    {
        // If necessary, get rid of the first line indent
        const SvxLRSpaceItem *pLRItem = static_cast<const SvxLRSpaceItem *>(pItem);
        SvxLRSpaceItem aLRItem( *pLRItem );
        aLRItem.SetTextFirstLineOfst( 0 );
        if( rCSS1PropInfo.bLeftMargin )
        {
            nLeftSpace = static_cast< sal_uInt16 >(aLRItem.GetLeft());
            rCSS1PropInfo.bLeftMargin = false;
        }
        if( rCSS1PropInfo.bRightMargin )
        {
            nRightSpace = static_cast< sal_uInt16 >(aLRItem.GetRight());
            rCSS1PropInfo.bRightMargin = false;
        }
        rCSS1ItemSet.ClearItem( RES_LR_SPACE );
    }
    if( nLeftSpace || nRightSpace )
    {
        SvxLRSpaceItem aLRItem( RES_LR_SPACE );
        aLRItem.SetLeft( nLeftSpace );
        aLRItem.SetRight( nRightSpace );
        aFrameSet.Put( aLRItem );
    }

    // set the top/bottom margin
    if( SfxItemState::SET==rCSS1ItemSet.GetItemState( RES_UL_SPACE, true, &pItem ) )
    {
        // if necessary, get rid of the first line indent
        const SvxULSpaceItem *pULItem = static_cast<const SvxULSpaceItem *>(pItem);
        if( rCSS1PropInfo.bTopMargin )
        {
            nUpperSpace = pULItem->GetUpper();
            rCSS1PropInfo.bTopMargin = false;
        }
        if( rCSS1PropInfo.bBottomMargin )
        {
            nLowerSpace = pULItem->GetLower();
            rCSS1PropInfo.bBottomMargin = false;
        }

        rCSS1ItemSet.ClearItem( RES_UL_SPACE );
    }
    if( nUpperSpace || nLowerSpace )
    {
        SvxULSpaceItem aULItem( RES_UL_SPACE );
        aULItem.SetUpper( nUpperSpace );
        aULItem.SetLower( nLowerSpace );
        aFrameSet.Put( aULItem );
    }

    SwFormatAnchor aAnchor( FLY_AS_CHAR );
    if( SVX_CSS1_POS_ABSOLUTE == rCSS1PropInfo.ePosition &&
        SVX_CSS1_LTYPE_TWIP == rCSS1PropInfo.eLeftType &&
        SVX_CSS1_LTYPE_TWIP == rCSS1PropInfo.eTopType )
    {
        const SwStartNode *pFlySttNd =
            m_pPam->GetPoint()->nNode.GetNode().FindFlyStartNode();

        if( pFlySttNd )
        {
            aAnchor.SetType( FLY_AT_FLY );
            SwPosition aPos( *pFlySttNd );
            aAnchor.SetAnchor( &aPos );
        }
        else
        {
            aAnchor.SetType( FLY_AT_PAGE );
        }
        // #i26791# - direct positioning for <SwDoc::Insert(..)>
        pNewDrawObj->SetRelativePos( Point(rCSS1PropInfo.nLeft + nLeftSpace,
                                           rCSS1PropInfo.nTop + nUpperSpace) );
        aFrameSet.Put( SwFormatSurround(SURROUND_THROUGHT) );
    }
    else if( SVX_ADJUST_LEFT == rCSS1PropInfo.eFloat ||
             text::HoriOrientation::LEFT == eHoriOri )
    {
        aAnchor.SetType( FLY_AT_PARA );
        aFrameSet.Put( SwFormatSurround(SURROUND_RIGHT) );
        // #i26791# - direct positioning for <SwDoc::Insert(..)>
        pNewDrawObj->SetRelativePos( Point(nLeftSpace, nUpperSpace) );
    }
    else if( text::VertOrientation::NONE != eVertOri )
    {
        aFrameSet.Put( SwFormatVertOrient( 0, eVertOri ) );
    }

    if (FLY_AT_PAGE == aAnchor.GetAnchorId())
    {
        aAnchor.SetPageNum( 1 );
    }
    else if( FLY_AT_FLY != aAnchor.GetAnchorId() )
    {
        aAnchor.SetAnchor( m_pPam->GetPoint() );
    }
    aFrameSet.Put( aAnchor );

    m_pDoc->getIDocumentContentOperations().InsertDrawObj( *m_pPam, *pNewDrawObj, aFrameSet );
}

static void PutEEPoolItem( SfxItemSet &rEEItemSet,
                           const SfxPoolItem& rSwItem )
{

    sal_uInt16 nEEWhich = 0;

    switch( rSwItem.Which() )
    {
    case RES_CHRATR_COLOR:          nEEWhich = EE_CHAR_COLOR; break;
    case RES_CHRATR_CROSSEDOUT:     nEEWhich = EE_CHAR_STRIKEOUT; break;
    case RES_CHRATR_ESCAPEMENT:     nEEWhich = EE_CHAR_ESCAPEMENT; break;
    case RES_CHRATR_FONT:           nEEWhich = EE_CHAR_FONTINFO; break;
    case RES_CHRATR_CJK_FONT:       nEEWhich = EE_CHAR_FONTINFO_CJK; break;
    case RES_CHRATR_CTL_FONT:       nEEWhich = EE_CHAR_FONTINFO_CTL; break;
    case RES_CHRATR_FONTSIZE:       nEEWhich = EE_CHAR_FONTHEIGHT; break;
    case RES_CHRATR_CJK_FONTSIZE:   nEEWhich = EE_CHAR_FONTHEIGHT_CJK; break;
    case RES_CHRATR_CTL_FONTSIZE:   nEEWhich = EE_CHAR_FONTHEIGHT_CTL; break;
    case RES_CHRATR_KERNING:        nEEWhich = EE_CHAR_KERNING; break;
    case RES_CHRATR_POSTURE:        nEEWhich = EE_CHAR_ITALIC; break;
    case RES_CHRATR_CJK_POSTURE:    nEEWhich = EE_CHAR_ITALIC_CJK; break;
    case RES_CHRATR_CTL_POSTURE:    nEEWhich = EE_CHAR_ITALIC_CTL; break;
    case RES_CHRATR_UNDERLINE:      nEEWhich = EE_CHAR_UNDERLINE; break;
    case RES_CHRATR_WEIGHT:         nEEWhich = EE_CHAR_WEIGHT; break;
    case RES_CHRATR_CJK_WEIGHT:     nEEWhich = EE_CHAR_WEIGHT_CJK; break;
    case RES_CHRATR_CTL_WEIGHT:     nEEWhich = EE_CHAR_WEIGHT_CTL; break;
    case RES_BACKGROUND:
    case RES_CHRATR_BACKGROUND:
        {
            const SvxBrushItem& rBrushItem = static_cast<const SvxBrushItem&>(rSwItem);
            rEEItemSet.Put( XFillStyleItem(drawing::FillStyle_SOLID) );
            rEEItemSet.Put( XFillColorItem(aEmptyOUStr,
                            rBrushItem.GetColor()) );
        }
        break;
    }

    if( nEEWhich )
    {
        SfxPoolItem *pEEItem = rSwItem.Clone();
        pEEItem->SetWhich( nEEWhich );
        rEEItemSet.Put( *pEEItem );
        delete pEEItem;
    }
}

void SwHTMLParser::NewMarquee( HTMLTable *pCurTable )
{

    OSL_ENSURE( !m_pMarquee, "Marquee in marquee?" );
    m_aContents.clear();

    OUString aId, aStyle, aClass;

    long nWidth=0, nHeight=0;
    bool bPrcWidth = false, bDirection = false, bBGColor = false;
    Size aSpace( 0, 0 );
    sal_Int16 eVertOri = text::VertOrientation::TOP;
    sal_Int16 eHoriOri = text::HoriOrientation::NONE;
    SdrTextAniKind eAniKind = SDRTEXTANI_SCROLL;
    SdrTextAniDirection eAniDir = SDRTEXTANI_LEFT;
    sal_uInt16 nCount = 0, nDelay = 60;
    sal_Int16 nAmount = -6;
    Color aBGColor;

    const HTMLOptions& rHTMLOptions = GetOptions();
    for (const auto & rOption : rHTMLOptions)
    {
        switch( rOption.GetToken() )
        {
            case HTML_O_ID:
                aId = rOption.GetString();
                break;
            case HTML_O_STYLE:
                aStyle = rOption.GetString();
                break;
            case HTML_O_CLASS:
                aClass = rOption.GetString();
                break;

            case HTML_O_BEHAVIOR:
                eAniKind =
                    (SdrTextAniKind)rOption.GetEnum( aHTMLMarqBehaviorTable,
                                                      static_cast< sal_uInt16 >(eAniKind) );
                break;

            case HTML_O_BGCOLOR:
                rOption.GetColor( aBGColor );
                bBGColor = true;
                break;

            case HTML_O_DIRECTION:
                eAniDir =
                    (SdrTextAniDirection)rOption.GetEnum( aHTMLMarqDirectionTable,
                                                      static_cast< sal_uInt16 >(eAniDir) );
                bDirection = true;
                break;

            case HTML_O_LOOP:
                if (rOption.GetString().
                    equalsIgnoreAsciiCase(OOO_STRING_SVTOOLS_HTML_LOOP_infinite))
                {
                    nCount = 0;
                }
                else
                {
                    const sal_Int32 nLoop = rOption.GetSNumber();
                    nCount = static_cast<sal_uInt16>(nLoop>0 ? nLoop : 0);
                }
                break;

            case HTML_O_SCROLLAMOUNT:
                nAmount = -((sal_Int16)rOption.GetNumber());
                break;

            case HTML_O_SCROLLDELAY:
                nDelay = (sal_uInt16)rOption.GetNumber();
                break;

            case HTML_O_WIDTH:
                // for now, only remember it as pixel values!
                nWidth = rOption.GetNumber();
                bPrcWidth = rOption.GetString().indexOf('%') != -1;
                if( bPrcWidth && nWidth>100 )
                    nWidth = 100;
                break;

            case HTML_O_HEIGHT:
                // for now, only remember it as pixel values!
                nHeight = rOption.GetNumber();
                if( rOption.GetString().indexOf('%') != -1 )
                    nHeight = 0;
                break;

            case HTML_O_HSPACE:
                // for now, only remember it as pixel values!
                aSpace.Height() = rOption.GetNumber();
                break;

                // for now, only remember it as pixel values!
                aSpace.Width() = rOption.GetNumber();
                break;

            case HTML_O_ALIGN:
                eVertOri =
                    rOption.GetEnum( aHTMLImgVAlignTable,
                                                    text::VertOrientation::TOP );
                eHoriOri =
                    rOption.GetEnum( aHTMLImgHAlignTable );
                break;
        }
    }

    // Create a DrawTextobj
    // #i52858# - method name changed
    SwDrawModel* pModel = m_pDoc->getIDocumentDrawModelAccess().GetOrCreateDrawModel();
    SdrPage* pPg = pModel->GetPage( 0 );
    m_pMarquee = SdrObjFactory::MakeNewObject( SdrInventor,
                                             OBJ_TEXT, pPg, pModel );
    if( !m_pMarquee )
        return;

    pPg->InsertObject( m_pMarquee );

    if( !aId.isEmpty() )
        InsertBookmark( aId );

    // (Only) Alternate flows from left to right per default
    if( SDRTEXTANI_ALTERNATE==eAniKind && !bDirection )
        eAniDir = SDRTEXTANI_RIGHT;

    // attributes that are needed for scrolling
    sal_uInt16 aWhichMap[7] =   { XATTR_FILL_FIRST,   XATTR_FILL_LAST,
                              SDRATTR_MISC_FIRST, SDRATTR_MISC_LAST,
                              EE_CHAR_START,      EE_CHAR_END,
                              0 };
    SfxItemSet aItemSet( pModel->GetItemPool(), aWhichMap );
    aItemSet.Put( makeSdrTextAutoGrowWidthItem( false ) );
    aItemSet.Put( makeSdrTextAutoGrowHeightItem( true ) );
    aItemSet.Put( SdrTextAniKindItem( eAniKind ) );
    aItemSet.Put( SdrTextAniDirectionItem( eAniDir ) );
    aItemSet.Put( SdrTextAniCountItem( nCount ) );
    aItemSet.Put( SdrTextAniDelayItem( nDelay ) );
    aItemSet.Put( SdrTextAniAmountItem( nAmount ) );
    if( SDRTEXTANI_ALTERNATE==eAniKind )
    {
        // (only) Alternate starts and stops per default Inside
        aItemSet.Put( SdrTextAniStartInsideItem(true) );
        aItemSet.Put( SdrTextAniStopInsideItem(true) );
        if( SDRTEXTANI_LEFT==eAniDir )
            aItemSet.Put( SdrTextHorzAdjustItem(SDRTEXTHORZADJUST_RIGHT) );
    }

    // set the default color (taken from the standard style), so that
    // at at least any practical color is set.
    const Color& rDfltColor =
        m_pCSS1Parser->GetTextCollFromPool( RES_POOLCOLL_STANDARD )
            ->GetColor().GetValue();
    aItemSet.Put( SvxColorItem( rDfltColor, EE_CHAR_COLOR ) );

    // set the attributes of the current paragraph style
    sal_uInt16 nWhichIds[] =
    {
        RES_CHRATR_COLOR,   RES_CHRATR_CROSSEDOUT, RES_CHRATR_ESCAPEMENT,
        RES_CHRATR_FONT,    RES_CHRATR_FONTSIZE,   RES_CHRATR_KERNING,
        RES_CHRATR_POSTURE, RES_CHRATR_UNDERLINE,  RES_CHRATR_WEIGHT,
        RES_CHRATR_BACKGROUND,
        RES_CHRATR_CJK_FONT, RES_CHRATR_CJK_FONTSIZE,
        RES_CHRATR_CJK_POSTURE, RES_CHRATR_CJK_WEIGHT,
        RES_CHRATR_CTL_FONT, RES_CHRATR_CTL_FONTSIZE,
        RES_CHRATR_CTL_POSTURE, RES_CHRATR_CTL_WEIGHT,
        0
    };
    SwTextNode const*const pTextNd =
        m_pPam->GetPoint()->nNode.GetNode().GetTextNode();
    if( pTextNd )
    {
        const SfxItemSet& rItemSet = pTextNd->GetAnyFormatColl().GetAttrSet();
        const SfxPoolItem *pItem;
        for( int i=0; nWhichIds[i]; ++i )
        {
            if( SfxItemState::SET == rItemSet.GetItemState( nWhichIds[i], true, &pItem ) )
                PutEEPoolItem( aItemSet, *pItem );
        }
    }

    // set the attributes of the context for the Draw object
    HTMLAttr** pHTMLAttributes = reinterpret_cast<HTMLAttr**>(&m_aAttrTab);
    for (auto nCnt = sizeof(HTMLAttrTable) / sizeof(HTMLAttr*); nCnt--; ++pHTMLAttributes)
    {
        HTMLAttr *pAttr = *pHTMLAttributes;
        if( pAttr )
            PutEEPoolItem( aItemSet, pAttr->GetItem() );
    }

    if( bBGColor )
    {
        aItemSet.Put( XFillStyleItem(drawing::FillStyle_SOLID) );
        aItemSet.Put( XFillColorItem(aEmptyOUStr, aBGColor) );
    }

    // parse the styles (here, it only works for attributes that can also be
    // set for the Draw object)
    SfxItemSet aStyleItemSet( m_pDoc->GetAttrPool(),
                              m_pCSS1Parser->GetWhichMap() );
    SvxCSS1PropertyInfo aPropInfo;
    if( HasStyleOptions( aStyle, aId, aClass )  &&
        ParseStyleOptions( aStyle, aId, aClass, aStyleItemSet, aPropInfo ) )
    {
        SfxItemIter aIter( aStyleItemSet );

        const SfxPoolItem *pItem = aIter.FirstItem();
        while( pItem )
        {
            PutEEPoolItem( aItemSet, *pItem );
            pItem = aIter.NextItem();
        }
    }

    // set the size
    Size aTwipSz( bPrcWidth ? 0 : nWidth, nHeight );
    if( (aTwipSz.Width() || aTwipSz.Height()) && Application::GetDefaultDevice() )
    {
        aTwipSz = Application::GetDefaultDevice()
                    ->PixelToLogic( aTwipSz, MapMode( MAP_TWIP ) );
    }

    if( SVX_CSS1_LTYPE_TWIP== aPropInfo.eWidthType )
    {
        aTwipSz.Width() = aPropInfo.nWidth;
        nWidth = 1; // != 0;
        bPrcWidth = false;
    }
    if( SVX_CSS1_LTYPE_TWIP== aPropInfo.eHeightType )
        aTwipSz.Height() = aPropInfo.nHeight;

    m_bFixMarqueeWidth = false;
    if( !nWidth || bPrcWidth )
    {
        if( m_pTable )
        {
            if( !pCurTable )
            {
                // The scrolling text is in a table, but not in a cell.
                // Here, we adapt the width to the width of the content of
                // the scrolling text, because, it is not possible here to
                // assign it to a cell in a meaningful way.
                m_bFixMarqueeWidth = true;
            }
            else if( !nWidth )
            {
                // Because we know which cell contains the scrolling text,
                // we can adapt the width. If there is no value given, we
                // handle it as if it was 100%.
                nWidth = 100;
                bPrcWidth = true;
            }
            aTwipSz.Width() = MINLAY;
        }
        else
        {
            long nBrowseWidth = GetCurrentBrowseWidth();
            aTwipSz.Width() = !nWidth ? nBrowseWidth
                                      : (nWidth*nBrowseWidth) / 100;
        }
    }

    // The height is only a minimum height.
    if( aTwipSz.Height() < MINFLY )
        aTwipSz.Height() = MINFLY;
    aItemSet.Put( makeSdrTextMinFrameHeightItem( aTwipSz.Height() ) );

    m_pMarquee->SetMergedItemSetAndBroadcast(aItemSet);

    if( aTwipSz.Width() < MINFLY )
        aTwipSz.Width() = MINFLY;
    m_pMarquee->SetLogicRect( Rectangle( 0, 0, aTwipSz.Width(), aTwipSz.Height() ) );

    // and insert the object in the document
    InsertDrawObject( m_pMarquee, aSpace, eVertOri, eHoriOri, aStyleItemSet,
                      aPropInfo );

    // Make the Draw object of the table known. This is a bit complicated,
    // because we need to invoke the parser, although the table is known, but
    // otherwise one would have to make the table public and that isn't very
    // elegant, either. By the way, the global pTable cannot be used either,
    // because sometimes the scrolling text may also be in a sub-table.
    if( pCurTable && bPrcWidth)
        RegisterDrawObjectToTable( pCurTable, m_pMarquee, (sal_uInt8)nWidth );
}

void SwHTMLParser::EndMarquee()
{
    OSL_ENSURE( m_pMarquee && OBJ_TEXT==m_pMarquee->GetObjIdentifier(),
            "not marquee, or wrong type" );

    if( m_bFixMarqueeWidth )
    {
        // Because there is no fixed height, create the text object wider
        // than necessary, so that no line break happens.
        const Rectangle& rOldRect = m_pMarquee->GetLogicRect();
        m_pMarquee->SetLogicRect( Rectangle( rOldRect.TopLeft(),
                                           Size( USHRT_MAX, 240 ) ) );
    }

    // insert the collected text
    static_cast<SdrTextObj*>(m_pMarquee)->SetText( m_aContents );
    m_pMarquee->SetMergedItemSetAndBroadcast( m_pMarquee->GetMergedItemSet() );

    if( m_bFixMarqueeWidth )
    {
        // adapt the size to the text
        static_cast<SdrTextObj*>(m_pMarquee)->FitFrameToTextSize();
    }

    m_aContents.clear();
    m_pMarquee = nullptr;
}

void SwHTMLParser::InsertMarqueeText()
{
    OSL_ENSURE( m_pMarquee && OBJ_TEXT==m_pMarquee->GetObjIdentifier(),
            "not marquee, or wrong type" );

    // add the current text piece to the text
    m_aContents += aToken;
}

void SwHTMLParser::ResizeDrawObject( SdrObject* pObj, SwTwips nWidth )
{
    OSL_ENSURE( OBJ_TEXT==pObj->GetObjIdentifier(),
            "not marquee, or wrong type" );

    if( OBJ_TEXT!=pObj->GetObjIdentifier() )
        return;

    // the old size
    const Rectangle& rOldRect = pObj->GetLogicRect();
    Size aNewSz( nWidth, rOldRect.GetSize().Height() );
    pObj->SetLogicRect( Rectangle( rOldRect.TopLeft(), aNewSz ) );
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
