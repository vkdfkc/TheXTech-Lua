/*
 * TheXTech - A platform game engine ported from old source code for VB6
 *
 * Copyright (c) 2009-2011 Andrew Spinks, original VB6 code
 * Copyright (c) 2020-2026 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <Logger/logger.h>
#include "sdl_proxy/sdl_stdinc.h"

#include "core/render.h"

#include "globals.h"
#include "graphics.h"
#include "fontman/font_manager.h"
#include "fontman/font_manager_private.h"


int SuperTextPixLen(int SuperN, const char* SuperChars, int Font)
{
    pLogDebug("SuperTextPixLen: ENTER N=%d chars='%.*s' Font=%d", SuperN, SuperN, SuperChars, Font);

    int dFont = FontManager::fontIdFromSmbxFont(Font);
    pLogDebug("SuperTextPixLen: fontIdFromSmbxFont(%d) = %d", Font, dFont);

    if(dFont < 0 && Font == 5)
    {
        Font = 4;
        pLogDebug("SuperTextPixLen: Font=5 fallback to 4");
        dFont = FontManager::fontIdFromSmbxFont(Font);
        pLogDebug("SuperTextPixLen: fontIdFromSmbxFont(4) = %d", dFont);
    }

    if(dFont < 0)
    {
        int len = 0;
        pLogDebug("SuperTextPixLen: Invalid font %d is specified", Font);

        for(int i = 0; i < SuperN; ++i)
        {
            len += 18;
            i += static_cast<size_t>(trailingBytesForUTF8[static_cast<UTF8>(SuperChars[i])]);
        }

        pLogDebug("SuperTextPixLen: returning fallback len=%d", len);
        return len;
    }

    uint32_t fontSize = FontManager::fontSizeFromSmbxFont(Font);
    int w = FontManager::textSize(SuperChars, SuperN, dFont, fontSize).w();
    pLogDebug("SuperTextPixLen: textSize returned w=%d, RETURN", w);
    return w;
}

void SuperPrintRightAlign(int SuperN, const char* SuperChars, int Font, int X, int Y, XTColor color)
{
    int RealFont = Font;
    bool outline = false;

    int dFont = FontManager::fontIdFromSmbxFont(Font);
    if(dFont < 0 && Font == 5)
    {
        Font = 4;
        outline = true;

        dFont = FontManager::fontIdFromSmbxFont(Font);
    }

    if(dFont >= 0)
    {
        X -= FontManager::textSize(SuperChars, SuperN, dFont, FontManager::fontSizeFromSmbxFont(Font)).w();
        FontManager::printText(SuperChars, SuperN, X, Y, dFont, color, FontManager::fontSizeFromSmbxFont(Font), outline);
        return;
    }

    X -= SuperTextPixLen(SuperN, SuperChars, RealFont);
    SuperPrint(SuperN, SuperChars, RealFont, X, Y, color);
}

void SuperPrintCenter(int SuperN, const char* SuperChars, int Font, int X, int Y, XTColor color)
{
    int RealFont = Font;
    bool outline = false;

    int dFont = FontManager::fontIdFromSmbxFont(Font);
    if(dFont < 0 && Font == 5)
    {
        Font = 4;
        outline = true;

        dFont = FontManager::fontIdFromSmbxFont(Font);
    }

    if(dFont >= 0)
    {
        X -= FontManager::textSize(SuperChars, SuperN, dFont, FontManager::fontSizeFromSmbxFont(Font)).w() / 2;
        FontManager::printText(SuperChars, SuperN, X, Y, dFont, color, FontManager::fontSizeFromSmbxFont(Font), outline);
        return;
    }

    X -= SuperTextPixLen(SuperN, SuperChars, RealFont) / 2;
    SuperPrint(SuperN, SuperChars, RealFont, X, Y, color);
}

void SuperPrintScreenCenter(int SuperN, const char* SuperChars, int Font, int Y, XTColor color)
{
    int RealFont = Font;
    bool outline = false;

    int dFont = FontManager::fontIdFromSmbxFont(Font);
    if(dFont < 0 && Font == 5)
    {
        Font = 4;
        outline = true;

        dFont = FontManager::fontIdFromSmbxFont(Font);
    }

    if(dFont >= 0)
    {
        int X = (XRender::TargetW / 2) - (FontManager::textSize(SuperChars, SuperN, dFont, FontManager::fontSizeFromSmbxFont(Font)).w() / 2);
        FontManager::printText(SuperChars, SuperN, X, Y, dFont, color, FontManager::fontSizeFromSmbxFont(Font), outline);
        return;
    }

    int X = (XRender::TargetW / 2) - (SuperTextPixLen(SuperN, SuperChars, RealFont) / 2);
    SuperPrint(SuperN, SuperChars, RealFont, X, Y, color);
}

void SuperPrint(int SuperN, const char* SuperChars, int Font, int X, int Y,
                XTColor color)
{
    bool outline = false;

    pLogDebug("SuperPrint: ENTER N=%d chars='%.*s' Font=%d X=%d Y=%d", SuperN, SuperN, SuperChars, Font, X, Y);

    int dFont = FontManager::fontIdFromSmbxFont(Font);
    pLogDebug("SuperPrint: fontIdFromSmbxFont(%d) = %d", Font, dFont);

    if(dFont < 0 && Font == 5)
    {
        Font = 4;
        outline = true;
        pLogDebug("SuperPrint: Font=5 fallback to 4");
        dFont = FontManager::fontIdFromSmbxFont(Font);
        pLogDebug("SuperPrint: fontIdFromSmbxFont(4) = %d", dFont);
    }

    if(dFont < 0)
    {
        pLogDebug("SuperPrint: Invalid font %d is specified", Font);
        return; // Invalid font specified
    }

    uint32_t fontSize = FontManager::fontSizeFromSmbxFont(Font);
    pLogDebug("SuperPrint: fontSize=%u, calling printText('%s', N=%d, x=%d, y=%d, dFont=%d, outline=%d)",
        fontSize, SuperChars, SuperN, X, Y, dFont, (int)outline);
    FontManager::printText(SuperChars, SuperN, X, Y, dFont, color, fontSize, outline);
    pLogDebug("SuperPrint: printText OK, RETURN");
}

// const char* versions

int SuperTextPixLen(const char* SuperChars, int Font)
{
    int len = (int)SDL_strlen(SuperChars);
    return SuperTextPixLen(len, SuperChars, Font);
}

void SuperPrintRightAlign(const char* SuperChars, int Font, int X, int Y, XTColor color)
{
    int len = (int)SDL_strlen(SuperChars);
    SuperPrintRightAlign(len, SuperChars, Font, X, Y, color);
}

void SuperPrintCenter(const char* SuperChars, int Font, int X, int Y, XTColor color)
{
    int len = (int)SDL_strlen(SuperChars);
    SuperPrintCenter(len, SuperChars, Font, X, Y, color);
}

void SuperPrintScreenCenter(const char* SuperChars, int Font, int Y, XTColor color)
{
    int len = (int)SDL_strlen(SuperChars);
    SuperPrintScreenCenter(len, SuperChars, Font, Y, color);
}

void SuperPrint(const char* SuperChars, int Font, int X, int Y, XTColor color)
{
    int len = (int)SDL_strlen(SuperChars);
    SuperPrint(len, SuperChars, Font, X, Y, color);
}


// const std::string& versions

int SuperTextPixLen(const std::string& SuperWords, int Font)
{
    return SuperTextPixLen((int)SuperWords.size(), SuperWords.c_str(), Font);
}

void SuperPrintRightAlign(const std::string& SuperWords, int Font, int X, int Y, XTColor color)
{
    SuperPrintRightAlign((int)SuperWords.size(), SuperWords.c_str(), Font, X, Y, color);
}

void SuperPrintCenter(const std::string& SuperWords, int Font, int X, int Y, XTColor color)
{
    SuperPrintCenter((int)SuperWords.size(), SuperWords.c_str(), Font, X, Y, color);
}

void SuperPrintScreenCenter(const std::string& SuperWords, int Font, int Y, XTColor color)
{
    SuperPrintScreenCenter((int)SuperWords.size(), SuperWords.c_str(), Font, Y, color);
}

void SuperPrint(const std::string& SuperWords, int Font, int X, int Y, XTColor color)
{
    SuperPrint((int)SuperWords.size(), SuperWords.c_str(), Font, X, Y, color);
}
