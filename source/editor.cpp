#include "editor.hpp"

#include <3ds.h>
#include <citro2d.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace
{
    constexpr float SCREEN_WIDTH = 400.0f;
    constexpr float SCREEN_HEIGHT = 240.0f;

    constexpr float PAGE_X = 24.0f;
    constexpr float PAGE_Y = 12.0f;
    constexpr float PAGE_WIDTH = 352.0f;
    constexpr float PAGE_HEIGHT = 216.0f;

    constexpr float TEXT_X = 43.0f;
    constexpr float TEXT_Y = 29.0f;
    constexpr float TEXT_SCALE = 0.48f;
    constexpr float LINE_HEIGHT = 12.5f;

    constexpr int MAX_LINE_CHARACTERS = 43;
    constexpr int VISIBLE_LINES = 15;

    constexpr u64 CURSOR_BLINK_INTERVAL = 500;

    constexpr u32 COLOR_CREMA =
        C2D_Color32(247, 245, 238, 255);

    constexpr u32 COLOR_PAPEL =
        C2D_Color32(252, 251, 247, 255);

    constexpr u32 COLOR_CARBON =
        C2D_Color32(30, 30, 30, 255);

    constexpr u32 COLOR_MUSGO =
        C2D_Color32(78, 122, 101, 255);

    constexpr u32 COLOR_BORDE =
        C2D_Color32(220, 218, 208, 255);

    constexpr u32 COLOR_SECUNDARIO =
        C2D_Color32(130, 130, 120, 255);

    C3D_RenderTarget* topScreen = nullptr;
    C3D_RenderTarget* bottomScreen = nullptr;

    C2D_TextBuf textBuffer = nullptr;

    std::string document;
    std::size_t cursorPosition = 0;
    std::size_t firstVisibleLine = 0;

    struct VisualLine
    {
        std::size_t start;
        std::size_t end;
        std::string text;
    };

    std::vector<VisualLine> visualLines;

    std::size_t nextUTF8(
        const std::string& text,
        std::size_t position)
    {
        if (position >= text.size())
            return text.size();

        ++position;

        while (position < text.size() &&
               (static_cast<unsigned char>(text[position]) & 0xC0) == 0x80)
        {
            ++position;
        }

        return position;
    }

    std::size_t previousUTF8(
        const std::string& text,
        std::size_t position)
    {
        if (position == 0)
            return 0;

        --position;

        while (position > 0 &&
               (static_cast<unsigned char>(text[position]) & 0xC0) == 0x80)
        {
            --position;
        }

        return position;
    }

    std::size_t countCharacters(
        const std::string& text,
        std::size_t start,
        std::size_t end)
    {
        std::size_t count = 0;

        while (start < end)
        {
            start = nextUTF8(text, start);
            ++count;
        }

        return count;
    }

    std::size_t positionAtColumn(
        const VisualLine& line,
        std::size_t column)
    {
        std::size_t position = line.start;

        while (position < line.end && column > 0)
        {
            position = nextUTF8(document, position);
            --column;
        }

        return position;
    }

    void buildVisualLines()
    {
        visualLines.clear();

        std::size_t lineStart = 0;
        std::size_t position = 0;
        int characterCount = 0;

        while (position < document.size())
        {
            if (document[position] == '\n')
            {
                visualLines.push_back({
                    lineStart,
                    position,
                    document.substr(lineStart, position - lineStart)
                });

                ++position;
                lineStart = position;
                characterCount = 0;

                continue;
            }

            position = nextUTF8(document, position);
            ++characterCount;

            if (characterCount >= MAX_LINE_CHARACTERS &&
                position < document.size() &&
                document[position] != '\n')
            {
                visualLines.push_back({
                    lineStart,
                    position,
                    document.substr(lineStart, position - lineStart)
                });

                lineStart = position;
                characterCount = 0;
            }
        }

        visualLines.push_back({
            lineStart,
            document.size(),
            document.substr(lineStart)
        });

        if (visualLines.empty())
        {
            visualLines.push_back({0, 0, ""});
        }
    }

    std::size_t findCursorLine()
    {
        for (std::size_t i = 0; i < visualLines.size(); ++i)
        {
            const VisualLine& line = visualLines[i];

            if (cursorPosition >= line.start &&
                cursorPosition <= line.end)
            {
                return i;
            }
        }

        return visualLines.size() - 1;
    }

    void updateScroll()
    {
        const std::size_t cursorLine = findCursorLine();

        if (cursorLine < firstVisibleLine)
        {
            firstVisibleLine = cursorLine;
        }
        else if (cursorLine >= firstVisibleLine + VISIBLE_LINES)
        {
            firstVisibleLine =
                cursorLine - VISIBLE_LINES + 1;
        }

        const std::size_t maximumScroll =
            visualLines.size() > VISIBLE_LINES
                ? visualLines.size() - VISIBLE_LINES
                : 0;

        firstVisibleLine =
            std::min(firstVisibleLine, maximumScroll);
    }

    void moveCursorHorizontal(int direction)
    {
        if (direction < 0)
        {
            cursorPosition =
                previousUTF8(document, cursorPosition);
        }
        else if (direction > 0)
        {
            cursorPosition =
                nextUTF8(document, cursorPosition);
        }
    }

    void moveCursorVertical(int direction)
    {
        const std::size_t currentLine = findCursorLine();

        if (direction < 0 && currentLine == 0)
            return;

        if (direction > 0 &&
            currentLine + 1 >= visualLines.size())
        {
            return;
        }

        const VisualLine& current =
            visualLines[currentLine];

        const std::size_t column =
            countCharacters(
                document,
                current.start,
                cursorPosition
            );

        const std::size_t targetLine =
            direction < 0
                ? currentLine - 1
                : currentLine + 1;

        const VisualLine& target =
            visualLines[targetLine];

        const std::size_t targetLength =
            countCharacters(
                document,
                target.start,
                target.end
            );

        cursorPosition = positionAtColumn(
            target,
            std::min(column, targetLength)
        );
    }

    void drawText(
        const char* content,
        float x,
        float y,
        float scale,
        u32 color)
    {
        C2D_Text text;

        C2D_TextParse(
            &text,
            textBuffer,
            content
        );

        C2D_TextOptimize(&text);

        C2D_DrawText(
            &text,
            C2D_WithColor,
            x,
            y,
            0.5f,
            scale,
            scale,
            color
        );
    }

    void drawCenteredText(
        const char* content,
        float screenWidth,
        float y,
        float scale,
        u32 color)
    {
        C2D_Text text;

        C2D_TextParse(
            &text,
            textBuffer,
            content
        );

        C2D_TextOptimize(&text);

        float width = 0.0f;

        C2D_TextGetDimensions(
            &text,
            scale,
            scale,
            &width,
            nullptr
        );

        C2D_DrawText(
            &text,
            C2D_WithColor,
            (screenWidth - width) / 2.0f,
            y,
            0.5f,
            scale,
            scale,
            color
        );
    }

    float measureTextWidth(const std::string& content)
    {
        if (content.empty())
            return 0.0f;

        C2D_Text text;

        C2D_TextParse(
            &text,
            textBuffer,
            content.c_str()
        );

        C2D_TextOptimize(&text);

        float width = 0.0f;

        C2D_TextGetDimensions(
            &text,
            TEXT_SCALE,
            TEXT_SCALE,
            &width,
            nullptr
        );

        return width;
    }

    void drawPage()
    {
        C2D_DrawRectSolid(
            PAGE_X,
            PAGE_Y,
            0.5f,
            PAGE_WIDTH,
            PAGE_HEIGHT,
            COLOR_PAPEL
        );

        // Borde discreto del área de escritura.
        C2D_DrawRectSolid(
            PAGE_X,
            PAGE_Y,
            0.6f,
            PAGE_WIDTH,
            1.0f,
            COLOR_BORDE
        );

        C2D_DrawRectSolid(
            PAGE_X,
            PAGE_Y + PAGE_HEIGHT - 1.0f,
            0.6f,
            PAGE_WIDTH,
            1.0f,
            COLOR_BORDE
        );

        C2D_DrawRectSolid(
            PAGE_X,
            PAGE_Y,
            0.6f,
            1.0f,
            PAGE_HEIGHT,
            COLOR_BORDE
        );

        C2D_DrawRectSolid(
            PAGE_X + PAGE_WIDTH - 1.0f,
            PAGE_Y,
            0.6f,
            1.0f,
            PAGE_HEIGHT,
            COLOR_BORDE
        );
    }

    void drawDocument()
    {
        buildVisualLines();
        updateScroll();

        C2D_TextBufClear(textBuffer);

        for (int i = 0; i < VISIBLE_LINES; ++i)
        {
            const std::size_t lineIndex =
                firstVisibleLine + i;

            if (lineIndex >= visualLines.size())
                break;

            const VisualLine& line =
                visualLines[lineIndex];

            const float y =
                TEXT_Y + static_cast<float>(i) * LINE_HEIGHT;

            if (!line.text.empty())
            {
                drawText(
                    line.text.c_str(),
                    TEXT_X,
                    y,
                    TEXT_SCALE,
                    COLOR_CARBON
                );
            }
        }

        // Cursor vertical parpadeante.
        const std::size_t cursorLine = findCursorLine();

        if (cursorLine >= firstVisibleLine &&
            cursorLine < firstVisibleLine + VISIBLE_LINES)
        {
            const VisualLine& line =
                visualLines[cursorLine];

            const std::string prefix =
                document.substr(
                    line.start,
                    cursorPosition - line.start
                );

            const float cursorX =
                TEXT_X + measureTextWidth(prefix);

            const float cursorY =
                TEXT_Y +
                static_cast<float>(
                    cursorLine - firstVisibleLine
                ) * LINE_HEIGHT;

            const bool cursorVisible =
                ((osGetTime() / CURSOR_BLINK_INTERVAL) % 2) == 0;

            if (cursorVisible)
            {
                C2D_DrawRectSolid(
                    cursorX,
                    cursorY,
                    0.7f,
                    1.5f,
                    10.0f,
                    COLOR_MUSGO
                );
            }
        }
    }

    void drawBottomScreen()
    {
        C2D_TextBufClear(textBuffer);

        drawCenteredText(
            "Laura",
            320.0f,
            25.0f,
            0.75f,
            COLOR_CARBON
        );

        drawCenteredText(
            "Documento sin titulo",
            320.0f,
            61.0f,
            0.42f,
            COLOR_MUSGO
        );

        C2D_DrawRectSolid(
            24.0f,
            91.0f,
            0.5f,
            272.0f,
            1.0f,
            COLOR_BORDE
        );

        drawCenteredText(
            "Navegacion del documento",
            320.0f,
            110.0f,
            0.40f,
            COLOR_CARBON
        );

        drawCenteredText(
            "Cruceta: mover cursor",
            320.0f,
            137.0f,
            0.38f,
            COLOR_SECUNDARIO
        );

        drawCenteredText(
            "START: salir del editor",
            320.0f,
            157.0f,
            0.38f,
            COLOR_SECUNDARIO
        );

        drawCenteredText(
            "Escritura y herramientas",
            320.0f,
            195.0f,
            0.34f,
            COLOR_MUSGO
        );

        drawCenteredText(
            "se incorporaran en la siguiente fase",
            320.0f,
            209.0f,
            0.30f,
            COLOR_SECUNDARIO
        );
    }

    void drawEditor()
    {
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        // Pantalla superior: documento.
        C2D_TargetClear(topScreen, COLOR_CREMA);
        C2D_SceneBegin(topScreen);

        drawPage();
        drawDocument();

        // Pantalla inferior: información provisional.
        C2D_TargetClear(bottomScreen, COLOR_CREMA);
        C2D_SceneBegin(bottomScreen);

        drawBottomScreen();

        C3D_FrameEnd(0);
    }
}

bool Editor_Init()
{
    textBuffer = C2D_TextBufNew(4096);

    if (textBuffer == nullptr)
        return false;

    document.clear();
    cursorPosition = 0;
    firstVisibleLine = 0;

    buildVisualLines();

    return true;
}

void Editor_Run()
{
    topScreen = C2D_CreateScreenTarget(
        GFX_TOP,
        GFX_LEFT
    );

    bottomScreen = C2D_CreateScreenTarget(
        GFX_BOTTOM,
        GFX_LEFT
    );

    while (aptMainLoop())
    {
        hidScanInput();

        const u32 pressedKeys = hidKeysDown();

        if (pressedKeys & KEY_START)
            break;

        if (pressedKeys & KEY_LEFT)
            moveCursorHorizontal(-1);

        if (pressedKeys & KEY_RIGHT)
            moveCursorHorizontal(1);

        if (pressedKeys & KEY_UP)
            moveCursorVertical(-1);

        if (pressedKeys & KEY_DOWN)
            moveCursorVertical(1);

                drawEditor();
    }
}

void Editor_Fini()
{
    if (textBuffer != nullptr)
    {
        C2D_TextBufDelete(textBuffer);
        textBuffer = nullptr;
    }
}
