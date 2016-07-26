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

#include "CommonSalLayout.hxx"

#include <vcl/unohelp.hxx>
#include <scrptrun.h>
#include <com/sun/star/i18n/CharacterIteratorMode.hpp>
#include <i18nlangtag/mslangid.hxx>
#include <limits>
#include <salgdi.hxx>

static hb_blob_t *getFontTable(hb_face_t* /*face*/, hb_tag_t nTableTag, void* pUserData)
{
    char pTagName[5];
    pTagName[0] = (char)(nTableTag >> 24);
    pTagName[1] = (char)(nTableTag >> 16);
    pTagName[2] = (char)(nTableTag >>  8);
    pTagName[3] = (char)(nTableTag);
    pTagName[4] = 0;

    sal_uLong nLength=0;
#if defined(_WIN32)
    const unsigned char* pBuffer = nullptr;
    HDC* phDC = static_cast<HDC*>(pUserData);
    nLength = WinFontFace::GetTable(pTagName, pBuffer, *phDC);
#elif defined(MACOSX) || defined(IOS)
    unsigned char* pBuffer = nullptr;
    CoreTextFontFace* pFont = static_cast<CoreTextFontFace*>(pUserData);
    nLength = pFont->GetFontTable(pTagName, nullptr);
    if( nLength>0 )
    {
        pBuffer = new unsigned char[nLength];
    }
    pFont->GetFontTable(pTagName, pBuffer);
#else
    const unsigned char* pBuffer = nullptr;
    ServerFont* pFont = static_cast<ServerFont*>(pUserData);
    pBuffer = pFont->GetTable(pTagName, &nLength);
#endif

    hb_blob_t* pBlob = nullptr;
    if (pBuffer != nullptr)
        pBlob = hb_blob_create(reinterpret_cast<const char*>(pBuffer), nLength, HB_MEMORY_MODE_READONLY, nullptr, nullptr);

    return pBlob;
}

static hb_unicode_funcs_t* getUnicodeFuncs()
{
    static hb_unicode_funcs_t* ufuncs = hb_unicode_funcs_create(hb_icu_get_unicode_funcs());
#if !HB_VERSION_ATLEAST(1, 1, 0)
    hb_unicode_funcs_set_decompose_compatibility_func(ufuncs, unicodeDecomposeCompatibility, nullptr, nullptr);
#endif
    return ufuncs;
}

#if defined(_WIN32)
CommonSalLayout::CommonSalLayout(HDC hDC, WinFontInstance& rWinFontInstance)
:   mhDC(hDC),
    mpHBFace(nullptr),
    maFontSelData(rWinFontInstance.maFontSelData)
{
    mpHBFace = hb_face_create_for_tables(getFontTable, &hDC, nullptr);
}

#elif defined(MACOSX) || defined(IOS)
CommonSalLayout::CommonSalLayout(const CoreTextStyle& rCoreTextStyle)
:   mpHBFace(nullptr),
    maFontSelData(rCoreTextStyle.maFontSelData),
    mrCoreTextStyle(rCoreTextStyle)
{
    mpHBFace = hb_face_create_for_tables(getFontTable, const_cast<CoreTextFontFace*>(rCoreTextStyle.mpFontData), nullptr);
}

#else
CommonSalLayout::CommonSalLayout(ServerFont& rServerFont)
:   mpHBFace(nullptr),
    maFontSelData(rServerFont.GetFontSelData()),
    mrServerFont(rServerFont)
{
    mpHBFace = hb_face_create_for_tables(getFontTable, &rServerFont, nullptr);
}
#endif

CommonSalLayout::~CommonSalLayout()
{
    hb_face_destroy(mpHBFace);
}

struct HbScriptRun
{
    int32_t mnMin;
    int32_t mnEnd;
    hb_script_t maScript;

    HbScriptRun(int32_t nMin, int32_t nEnd, UScriptCode aScript)
    : mnMin(nMin), mnEnd(nEnd),
      maScript(hb_icu_script_to_script(aScript))
    {}
};

typedef std::vector<HbScriptRun> HbScriptRuns;

namespace vcl {
    struct Run
    {
        int32_t nStart;
        int32_t nEnd;
        UScriptCode nCode;
        Run(int32_t nStart_, int32_t nEnd_, UScriptCode nCode_)
            : nStart(nStart_), nEnd(nEnd_), nCode(nCode_)
        {}
    };

    class TextLayoutCache
    {
    public:
        std::vector<vcl::Run> runs;
        TextLayoutCache(sal_Unicode const* pStr, sal_Int32 const nEnd)
        {
            vcl::ScriptRun aScriptRun(
                reinterpret_cast<const UChar *>(pStr),
                nEnd);
            while (aScriptRun.next())
            {
                runs.push_back(Run(aScriptRun.getScriptStart(),
                    aScriptRun.getScriptEnd(), aScriptRun.getScriptCode()));
            }
        }
    };
}

std::shared_ptr<vcl::TextLayoutCache> CommonSalLayout::CreateTextLayoutCache(OUString const& rString) const
{
    return std::make_shared<vcl::TextLayoutCache>(rString.getStr(), rString.getLength());
}

void CommonSalLayout::SetNeedFallback(ImplLayoutArgs& rArgs, sal_Int32 nCharPos, bool bRightToLeft)
{
    if (nCharPos < 0)
        return;

    using namespace ::com::sun::star;

    if (!mxBreak.is())
        mxBreak = vcl::unohelper::CreateBreakIterator();

    lang::Locale aLocale(rArgs.maLanguageTag.getLocale());

    //if position nCharPos is missing in the font, grab the entire grapheme and
    //mark all glyphs as missing so the whole thing is rendered with the same
    //font
    sal_Int32 nDone;
    sal_Int32 nGraphemeStartPos =
        mxBreak->previousCharacters(rArgs.mrStr, nCharPos+1, aLocale,
            i18n::CharacterIteratorMode::SKIPCELL, 1, nDone);
    sal_Int32 nGraphemeEndPos =
        mxBreak->nextCharacters(rArgs.mrStr, nCharPos, aLocale,
            i18n::CharacterIteratorMode::SKIPCELL, 1, nDone);

    rArgs.NeedFallback(nGraphemeStartPos, nGraphemeEndPos, bRightToLeft);
}

void CommonSalLayout::AdjustLayout(ImplLayoutArgs& rArgs)
{
    GenericSalLayout::AdjustLayout(rArgs);

    // apply asian kerning if the glyphs are not already formatted
    if( (rArgs.mnFlags & SalLayoutFlags::KerningAsian)
    && !(rArgs.mnFlags & SalLayoutFlags::Vertical) )
        if( (rArgs.mpDXArray != nullptr) || (rArgs.mnLayoutWidth != 0) )
            ApplyAsianKerning(rArgs.mrStr);
//XXX Kashida
}

void CommonSalLayout::DrawText( SalGraphics& rSalGraphics ) const
{
    //call platform dependent DrawText functions
    rSalGraphics.DrawSalLayout( *this );
}

bool CommonSalLayout::LayoutText(ImplLayoutArgs& rArgs)
{
    //XXX WinLayout object DOESN'T derive from GSL
    GenericSalLayout& rLayout = *this;

    hb_font_t* pHBFont = hb_font_create(mpHBFace);
    hb_font_set_ppem(pHBFont, maFontSelData.mnWidth? maFontSelData.mnWidth:maFontSelData.mnHeight , maFontSelData.mnHeight);
    hb_font_set_scale(pHBFont, (uint64_t)(maFontSelData.mnWidth? maFontSelData.mnWidth:maFontSelData.mnHeight) << 6
                             , (uint64_t)maFontSelData.mnHeight << 6);
    hb_ot_font_set_funcs(pHBFont);

    int nGlyphCapacity = 2 * (rArgs.mnEndCharPos - rArgs.mnMinCharPos);
    rLayout.Reserve(nGlyphCapacity);

    const int nLength = rArgs.mrStr.getLength();
    const sal_Unicode *pStr = rArgs.mrStr.getStr();

    std::unique_ptr<vcl::TextLayoutCache> pNewScriptRun;
    vcl::TextLayoutCache const* pTextLayout;
    if (rArgs.m_pTextLayoutCache)
    {
        pTextLayout = rArgs.m_pTextLayoutCache; // use cache!
    }
    else
    {
        pNewScriptRun.reset(new vcl::TextLayoutCache(pStr, rArgs.mnEndCharPos));
        pTextLayout = pNewScriptRun.get();
    }

    Point aCurrPos(0, 0);
    while (true)
    {
        int nBidiMinRunPos, nBidiEndRunPos;
        bool bRightToLeft;
        if (!rArgs.GetNextRun(&nBidiMinRunPos, &nBidiEndRunPos, &bRightToLeft))
            break;

        // Find script subruns.
        int nCurrentPos = nBidiMinRunPos;
        HbScriptRuns aScriptSubRuns;
        size_t k = 0;
        for (; k < pTextLayout->runs.size(); ++k)
        {
            vcl::Run const& rRun(pTextLayout->runs[k]);
            if (rRun.nStart <= nCurrentPos && nCurrentPos < rRun.nEnd)
            {
                break;
            }
        }

        while (nCurrentPos < nBidiEndRunPos && k < pTextLayout->runs.size())
        {
            int32_t nMinRunPos = nCurrentPos;
            int32_t nEndRunPos = std::min(pTextLayout->runs[k].nEnd, nBidiEndRunPos);
            HbScriptRun aRun(nMinRunPos, nEndRunPos, pTextLayout->runs[k].nCode);
            aScriptSubRuns.push_back(aRun);

            nCurrentPos = nEndRunPos;
            ++k;
        }

        // RTL subruns should be reversed to ensure that final glyph order is
        // correct.
        if (bRightToLeft)
            std::reverse(aScriptSubRuns.begin(), aScriptSubRuns.end());

        for (HbScriptRuns::iterator it = aScriptSubRuns.begin(); it != aScriptSubRuns.end(); ++it)
        {
            int nMinRunPos = it->mnMin;
            int nEndRunPos = it->mnEnd;
            int nRunLen = nEndRunPos - nMinRunPos;
            hb_script_t aHbScript = it->maScript;
            // hb_language_from_string() accept ISO639-3 language tag except for Chinese.
            LanguageTag &rTag = rArgs.maLanguageTag;
            OString sLanguage = OUStringToOString( MsLangId::isChinese(rTag.getLanguageType()) ? rTag.getBcp47():rTag.getLanguage() , RTL_TEXTENCODING_UTF8 );

            int nHbFlags = HB_BUFFER_FLAGS_DEFAULT;
            if (nMinRunPos == 0)
                nHbFlags |= HB_BUFFER_FLAG_BOT; /* Beginning-of-text */
            if (nEndRunPos == nLength)
                nHbFlags |= HB_BUFFER_FLAG_EOT; /* End-of-text */

            hb_buffer_t *pHbBuffer = hb_buffer_create();
            static hb_unicode_funcs_t* pHbUnicodeFuncs = getUnicodeFuncs();
#if !HB_VERSION_ATLEAST(1, 1, 0)
            hb_buffer_set_unicode_funcs(pHbBuffer, pHbUnicodeFuncs);
#endif
            hb_buffer_set_direction(pHbBuffer, bRightToLeft ? HB_DIRECTION_RTL: HB_DIRECTION_LTR);
            hb_buffer_set_script(pHbBuffer, aHbScript);
            hb_buffer_set_language(pHbBuffer, hb_language_from_string(sLanguage.getStr(), -1));
            hb_buffer_set_flags(pHbBuffer, (hb_buffer_flags_t) nHbFlags);
            hb_buffer_add_utf16(
                pHbBuffer, reinterpret_cast<uint16_t const *>(pStr), nLength,
                nMinRunPos, nRunLen);
#if HB_VERSION_ATLEAST(0, 9, 42)
            hb_buffer_set_cluster_level(pHbBuffer, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS);
#endif
            hb_shape(pHBFont, pHbBuffer, nullptr, 0);

            int nRunGlyphCount = hb_buffer_get_length(pHbBuffer);
            hb_glyph_info_t *pHbGlyphInfos = hb_buffer_get_glyph_infos(pHbBuffer, nullptr);
            hb_glyph_position_t *pHbPositions = hb_buffer_get_glyph_positions(pHbBuffer, nullptr);

            sal_Int32 nGraphemeStartPos = std::numeric_limits<sal_Int32>::max();
            sal_Int32 nGraphemeEndPos = std::numeric_limits<sal_Int32>::min();
            com::sun::star::lang::Locale aLocale(rArgs.maLanguageTag.getLocale());
            if (!mxBreak.is())
                mxBreak = vcl::unohelper::CreateBreakIterator();

            for (int i = 0; i < nRunGlyphCount; ++i) {
                int32_t nGlyphIndex = pHbGlyphInfos[i].codepoint;
                int32_t nCharPos = pHbGlyphInfos[i].cluster;

                // if needed request glyph fallback by updating LayoutArgs
                if (!nGlyphIndex)
                {
                    SetNeedFallback(rArgs, nCharPos, bRightToLeft);
                    if (SalLayoutFlags::ForFallback & rArgs.mnFlags)
                        continue;
                }

                sal_Int32 indexUtf16 = nCharPos;
                sal_UCS4 aChar = rArgs.mrStr.iterateCodePoints(&indexUtf16, 0);

                bool bInCluster = false;
                if(bRightToLeft && (nCharPos < nGraphemeStartPos))
                {
                    sal_Int32 nDone;
                    nGraphemeStartPos = mxBreak->previousCharacters(rArgs.mrStr, nCharPos+1, aLocale,
                                                  com::sun::star::i18n::CharacterIteratorMode::SKIPCELL, 1, nDone);
                }
                else if(!bRightToLeft && (nCharPos >= nGraphemeEndPos))
                {
                    sal_Int32 nDone;
                    nGraphemeEndPos = mxBreak->nextCharacters(rArgs.mrStr, nCharPos, aLocale,
                                                  com::sun::star::i18n::CharacterIteratorMode::SKIPCELL, 1, nDone);
                }
                else
                {
                    bInCluster = true;
                }

                long nGlyphFlags = 0;
                if (bRightToLeft)
                    nGlyphFlags |= GlyphItem::IS_RTL_GLYPH;

                if (bInCluster)
                    nGlyphFlags |= GlyphItem::IS_IN_CLUSTER;

                bool bDiacritic = false;
                if (hb_ot_layout_has_glyph_classes(mpHBFace))
                {
                    // the font has GDEF table
                    bool bMark = hb_ot_layout_get_glyph_class(mpHBFace, nGlyphIndex) == HB_OT_LAYOUT_GLYPH_CLASS_MARK;
                    if (bMark && pHbPositions[i].x_advance == 0)
                        bDiacritic = true;
                }
                else
                {
#if HB_VERSION_ATLEAST(0, 9, 42)
                    if(hb_unicode_general_category (pHbUnicodeFuncs, aChar) == HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK)
                        bDiacritic = true;
#else
                    // the font lacks GDEF table
                    if (pHbPositions[i].x_advance == 0)
                        bDiacritic = true;
#endif
                }

                if (bDiacritic)
                    nGlyphFlags |= GlyphItem::IS_DIACRITIC;

                int32_t nXOffset =  pHbPositions[i].x_offset >> 6;
                int32_t nYOffset =  pHbPositions[i].y_offset >> 6;
                int32_t nXAdvance = pHbPositions[i].x_advance >> 6;
                int32_t nYAdvance = pHbPositions[i].y_advance >> 6;
                Point aNewPos = Point(aCurrPos.X() + nXOffset, -(aCurrPos.Y() + nYOffset));
                // Definiton of glyphitem may have to change to support system graphics lib
                const GlyphItem aGI(nCharPos, nGlyphIndex, aNewPos, nGlyphFlags, nXAdvance, nXOffset);
                rLayout.AppendGlyph(aGI);

                aCurrPos.X() += nXAdvance;
                aCurrPos.Y() += nYAdvance;
            }

            hb_buffer_destroy(pHbBuffer);
        }
    }

    hb_font_destroy(pHBFont);

    rLayout.SortGlyphItems();

    /* XXX seems to be broken
    if((rArgs.mpDXArray || rArgs.mnLayoutWidth)
    && ((maHbScript == HB_SCRIPT_ARABIC) || (maHbScript == HB_SCRIPT_SYRIAC)))
        rArgs.mnFlags |= SalLayoutFlags::KashidaJustification;
    */
    return true;
}
