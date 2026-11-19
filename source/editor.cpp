#include "editor.hpp"

#include <3ds.h>
#include <citro2d.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace
{
    constexpr float PAGE_X = 24.0f;
    constexpr float PAGE_Y = 12.0f;
    constexpr float PAGE_WIDTH = 352.0f;
    constexpr float PAGE_HEIGHT = 216.0f;

    constexpr float TEXT_X = 43.0f;
    constexpr float TEXT_Y = 29.0f;
    constexpr float TEXT_SCALE = 0.48f;
    constexpr float LINE_HEIGHT = 12.5f;
    constexpr float INDENT_WIDTH = 12.0f;

    constexpr int MAX_LINE_CHARACTERS = 43;
    constexpr int VISIBLE_LINES = 15;
    constexpr int MAX_INDENT_LEVEL = 8;

    constexpr float KEY_MARGIN = 5.0f;
    constexpr float KEY_GAP = 1.0f;
    constexpr float KEY_WIDTH = 30.0f;
    constexpr float KEY_HEIGHT = 22.0f;
    constexpr float KEY_START_Y = 53.0f;
    constexpr float KEY_ROW_STEP = 23.0f;

    constexpr u64 CURSOR_BLINK_INTERVAL = 500;
    constexpr u64 CAPS_DOUBLE_PRESS_MS = 350;
    constexpr u64 KEY_REPEAT_DELAY = 350;
    constexpr u64 KEY_REPEAT_INTERVAL = 65;

    constexpr u64 PHYSICAL_REPEAT_DELAY = 350;
    constexpr u64 PHYSICAL_REPEAT_INTERVAL = 65;

    constexpr std::size_t MAX_HISTORY = 100;

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

    constexpr u32 COLOR_TECLA_BORDE =
        C2D_Color32(38, 38, 35, 255);

    constexpr u32 COLOR_TECLA =
        C2D_Color32(248, 247, 241, 255);

    constexpr u32 COLOR_TECLA_SUPERIOR =
        C2D_Color32(255, 255, 252, 255);

    constexpr u32 COLOR_TECLA_ACTIVA =
        C2D_Color32(220, 231, 222, 255);

    constexpr u32 COLOR_SECUNDARIO =
        C2D_Color32(130, 130, 120, 255);

    constexpr u32 COLOR_SELECCION =
        C2D_Color32(190, 211, 195, 255);

    C3D_RenderTarget* topScreen = nullptr;
    C3D_RenderTarget* bottomScreen = nullptr;
    C2D_TextBuf textBuffer = nullptr;

    std::string document;
    std::size_t cursorPosition = 0;
    std::size_t firstVisibleLine = 0;

    std::vector<std::pair<std::size_t, std::size_t>> visualLines;

    /*
     * La sangría se almacena como metadato.
     *
     * Cada entrada relaciona la posición inicial de un párrafo
     * con su nivel de sangría. No se insertan espacios artificiales
     * en el texto del documento.
     */
    std::map<std::size_t, int> paragraphIndents;

    bool capsLock = false;
    std::string pendingDeadKey;

    bool hasSelection = false;
    std::size_t selectionAnchor = 0;

    std::string clipboard;

    bool boldEnabled = false;
    bool italicEnabled = false;
    bool underlineEnabled = false;
    bool showParagraphMarks = false;
    bool hudVisible = true;

    int alignmentMode = 0;

    struct EditorSnapshot
    {
        std::string document;
        std::size_t cursor;
        std::size_t anchor;
        bool selection;
        std::map<std::size_t, int> paragraphIndents;
    };

    std::vector<EditorSnapshot> undoHistory;
    std::vector<EditorSnapshot> redoHistory;

    u64 lastLPress = 0;
    u64 nextRepeatTime = 0;
    u64 nextPhysicalRepeat = 0;

    int activeTouchKey = -1;

    struct KeyDefinition
    {
        const char* base;
        const char* shift;
        const char* altGr;
        bool letter;
    };

    struct KeyRect
    {
        float x;
        float y;
        float width;
        float height;
    };

    const std::array<std::array<KeyDefinition, 10>, 5> keyboard = {{
        {{
            {"1", "|", "@", false},
            {"2", "*", "#", false},
            {"3", "·", "ª", false},
            {"4", "$", "+", false},
            {"5", "%", "¬", false},
            {"6", "&", "", false},
            {"7", "/", "º", false},
            {"8", "(", "<", false},
            {"9", ")", ">", false},
            {"0", "=", "^", false}
        }},
        {{
            {"q", "Q", "", true},
            {"w", "W", "", true},
            {"e", "E", "€", true},
            {"r", "R", "", true},
            {"t", "T", "", true},
            {"y", "Y", "", true},
            {"u", "U", "", true},
            {"i", "I", "", true},
            {"o", "O", "", true},
            {"p", "P", "", true}
        }},
        {{
            {"a", "A", "", true},
            {"s", "S", "", true},
            {"d", "D", "", true},
            {"f", "F", "", true},
            {"g", "G", "", true},
            {"h", "H", "", true},
            {"j", "J", "", true},
            {"k", "K", "", true},
            {"l", "L", "", true},
            {"ñ", "Ñ", "~", true}
        }},
        {{
            {"z", "Z", "", true},
            {"x", "X", "", true},
            {"c", "C", "ç", true},
            {"v", "V", "", true},
            {"b", "B", "", true},
            {"n", "N", "", true},
            {"m", "M", "", true},
            {",", ",", ",", false},
            {".", ".", ".", false},
            {"´", "¨", "`", false}
        }},
        {{
            {"«", "«", "«", false},
            {"¿", "¿", "¿", false},
            {"¡", "¡", "¡", false},
            {"(", "[", "{", false},
            {"-", "–", "–", false},
            {"‘", "“", "“", false},
            {")", "]", "}", false},
            {"?", "?", "?", false},
            {"!", "!", "!", false},
            {"»", "»", "»", false}
        }}
    }};

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

    /*
     * Devuelve el comienzo del párrafo que contiene una posición.
     */
    std::size_t getParagraphStart(std::size_t position)
    {
        if (position > document.size())
            position = document.size();

        if (position == 0)
            return 0;

        const std::size_t newline =
            document.rfind('\n', position - 1);

        return newline == std::string::npos ? 0 : newline + 1;
    }

    int getParagraphIndent(std::size_t position)
    {
        const std::size_t paragraphStart =
            getParagraphStart(position);

        auto it = paragraphIndents.upper_bound(paragraphStart);

        if (it == paragraphIndents.begin())
            return 0;

        --it;
        return it->second;
    }

    /*
     * Inserción centralizada: actualiza también las posiciones
     * de los metadatos de párrafo.
     */
    void insertDocumentText(
        std::size_t position,
        const std::string& value)
    {
        if (value.empty())
            return;

        if (position > document.size())
            position = document.size();

        const int inheritedIndent =
            getParagraphIndent(position);

        std::map<std::size_t, int> updated;

        for (const auto& entry : paragraphIndents)
        {
            std::size_t key = entry.first;

            if (key > position)
                key += value.size();

            updated[key] = entry.second;
        }

        document.insert(position, value);

        for (std::size_t i = 0; i < value.size(); ++i)
        {
            if (value[i] == '\n')
                updated[position + i + 1] = inheritedIndent;
        }

        paragraphIndents.swap(updated);
    }

    /*
     * Eliminación centralizada: elimina metadatos de párrafos
     * que desaparecen y desplaza los posteriores.
     */
    void eraseDocumentText(
        std::size_t position,
        std::size_t length)
    {
        if (length == 0 || position >= document.size())
            return;

        length = std::min(length, document.size() - position);

        const std::size_t end = position + length;

        std::map<std::size_t, int> updated;

        for (const auto& entry : paragraphIndents)
        {
            const std::size_t key = entry.first;

            if (key > position && key <= end)
                continue;

            if (key > end)
                updated[key - length] = entry.second;
            else
                updated[key] = entry.second;
        }

        document.erase(position, length);

        if (updated.find(0) == updated.end())
            updated[0] = 0;

        paragraphIndents.swap(updated);
    }

    void buildVisualLines()
    {
        visualLines.clear();

        std::size_t lineStart = 0;
        std::size_t position = 0;
        int characterCount = 0;

        while (position < document.size())
        {
            if (document[position] == '\n' ||
                document[position] == '\f')
            {
                visualLines.push_back({lineStart, position});

                ++position;
                lineStart = position;
                characterCount = 0;
                continue;
            }

            position = nextUTF8(document, position);
            ++characterCount;

            if (characterCount >= MAX_LINE_CHARACTERS &&
                position < document.size() &&
                document[position] != '\n' &&
                document[position] != '\f')
            {
                visualLines.push_back({lineStart, position});
                lineStart = position;
                characterCount = 0;
            }
        }

        visualLines.push_back({lineStart, document.size()});
    }

    std::size_t findCursorLine()
    {
        for (std::size_t i = 0; i < visualLines.size(); ++i)
        {
            if (cursorPosition >= visualLines[i].first &&
                cursorPosition <= visualLines[i].second)
            {
                return i;
            }
        }

        return visualLines.empty() ? 0 : visualLines.size() - 1;
    }

    void updateScroll()
    {
        const std::size_t cursorLine = findCursorLine();

        if (cursorLine < firstVisibleLine)
            firstVisibleLine = cursorLine;
        else if (cursorLine >= firstVisibleLine + VISIBLE_LINES)
            firstVisibleLine = cursorLine - VISIBLE_LINES + 1;

        const std::size_t maximumScroll =
            visualLines.size() > VISIBLE_LINES
                ? visualLines.size() - VISIBLE_LINES
                : 0;

        firstVisibleLine = std::min(firstVisibleLine, maximumScroll);
    }

    void clearSelection()
    {
        hasSelection = false;
        selectionAnchor = cursorPosition;
    }

    std::size_t selectionStart()
    {
        return std::min(cursorPosition, selectionAnchor);
    }

    std::size_t selectionEnd()
    {
        return std::max(cursorPosition, selectionAnchor);
    }

    void deleteSelection()
    {
        if (!hasSelection)
            return;

        const std::size_t start = selectionStart();
        const std::size_t end = selectionEnd();

        eraseDocumentText(start, end - start);
        cursorPosition = start;

        clearSelection();
    }

    void pushUndo()
    {
        undoHistory.push_back({
            document,
            cursorPosition,
            selectionAnchor,
            hasSelection,
            paragraphIndents
        });

        if (undoHistory.size() > MAX_HISTORY)
            undoHistory.erase(undoHistory.begin());

        redoHistory.clear();
    }

    void restoreSnapshot(const EditorSnapshot& snapshot)
    {
        document = snapshot.document;
        cursorPosition = std::min(snapshot.cursor, document.size());
        selectionAnchor = std::min(snapshot.anchor, document.size());
        hasSelection = snapshot.selection;
        paragraphIndents = snapshot.paragraphIndents;

        if (paragraphIndents.empty())
            paragraphIndents[0] = 0;

        pendingDeadKey.clear();

        buildVisualLines();
        updateScroll();
    }

    void undoAction(int amount = 1)
    {
        while (amount-- > 0 && !undoHistory.empty())
        {
            redoHistory.push_back({
                document,
                cursorPosition,
                selectionAnchor,
                hasSelection,
                paragraphIndents
            });

            const EditorSnapshot previous = undoHistory.back();
            undoHistory.pop_back();

            restoreSnapshot(previous);
        }
    }

    void redoAction()
    {
        if (redoHistory.empty())
            return;

        undoHistory.push_back({
            document,
            cursorPosition,
            selectionAnchor,
            hasSelection,
            paragraphIndents
        });

        const EditorSnapshot next = redoHistory.back();
        redoHistory.pop_back();

        restoreSnapshot(next);
    }

    void insertRaw(const std::string& value)
    {
        if (value.empty())
            return;

        pushUndo();

        if (hasSelection)
            deleteSelection();

        insertDocumentText(cursorPosition, value);
        cursorPosition += value.size();

        clearSelection();
        buildVisualLines();
        updateScroll();
    }

    void insertText(const std::string& value)
    {
        insertRaw(value);
    }

    void insertCharacter(const std::string& value)
    {
        if (value.empty())
            return;

        if (pendingDeadKey.empty())
        {
            insertRaw(value);
            return;
        }

        const std::string dead = pendingDeadKey;
        pendingDeadKey.clear();

        struct Composition
        {
            const char* dead;
            const char* letter;
            const char* result;
        };

        static const Composition combinations[] = {
            {"´", "a", "á"}, {"´", "e", "é"},
            {"´", "i", "í"}, {"´", "o", "ó"},
            {"´", "u", "ú"}, {"´", "A", "Á"},
            {"´", "E", "É"}, {"´", "I", "Í"},
            {"´", "O", "Ó"}, {"´", "U", "Ú"},
            {"¨", "u", "ü"}, {"¨", "U", "Ü"},
            {"`", "a", "à"}, {"`", "e", "è"},
            {"`", "i", "ì"}, {"`", "o", "ò"},
            {"`", "u", "ù"}, {"`", "A", "À"},
            {"`", "E", "È"}, {"`", "I", "Ì"},
            {"`", "O", "Ò"}, {"`", "U", "Ù"},
            {"^", "a", "â"}, {"^", "e", "ê"},
            {"^", "i", "î"}, {"^", "o", "ô"},
            {"^", "u", "û"}, {"^", "A", "Â"},
            {"^", "E", "Ê"}, {"^", "I", "Î"},
            {"^", "O", "Ô"}, {"^", "U", "Û"}
        };

        std::string result = dead + value;

        for (const Composition& item : combinations)
        {
            if (dead == item.dead && value == item.letter)
            {
                result = item.result;
                break;
            }
        }

        insertRaw(result);
    }

    void activateDeadKey(const std::string& accent)
    {
        if (!pendingDeadKey.empty())
            insertRaw(pendingDeadKey);

        pendingDeadKey = accent;
    }

    void backspace()
    {
        if (!pendingDeadKey.empty())
        {
            pendingDeadKey.clear();
            return;
        }

        if (hasSelection)
        {
            pushUndo();
            deleteSelection();
            buildVisualLines();
            updateScroll();
            return;
        }

        if (cursorPosition == 0)
            return;

        const std::size_t previous =
            previousUTF8(document, cursorPosition);

        pushUndo();

        eraseDocumentText(previous, cursorPosition - previous);
        cursorPosition = previous;

        clearSelection();
        buildVisualLines();
        updateScroll();
    }

    void insertTab()
    {
        insertCharacter("\t");
    }

    /*
     * Aumenta la sangría del párrafo completo.
     * No modifica el contenido textual.
     */
    void increaseIndent()
    {
        const std::size_t paragraphStart =
            getParagraphStart(cursorPosition);

        pushUndo();

        int& indent = paragraphIndents[paragraphStart];

        if (indent < MAX_INDENT_LEVEL)
            ++indent;

        buildVisualLines();
        updateScroll();
    }

    /*
     * RESET:
     * elimina la sangría del párrafo actual y de todos los
     * párrafos posteriores. Los párrafos anteriores no cambian.
     */
    void resetIndent()
    {
        const std::size_t paragraphStart =
            getParagraphStart(cursorPosition);

        pushUndo();

        for (auto& entry : paragraphIndents)
        {
            if (entry.first >= paragraphStart)
                entry.second = 0;
        }

        buildVisualLines();
        updateScroll();
    }

    void copySelection()
    {
        if (!hasSelection)
            return;

        clipboard = document.substr(
            selectionStart(),
            selectionEnd() - selectionStart()
        );
    }

    void cutSelection()
    {
        if (!hasSelection)
            return;

        copySelection();
        pushUndo();
        deleteSelection();
        buildVisualLines();
        updateScroll();
    }

    void pasteClipboard()
    {
        if (!clipboard.empty())
            insertText(clipboard);
    }

    /*
     * WORD:
     * primero atraviesa espacios, tabulaciones y saltos de línea
     * anteriores; después elimina la palabra precedente.
     *
     * Así, las repeticiones consecutivas no quedan detenidas
     * al encontrar un espacio.
     */
    void deleteWord()
    {
        if (hasSelection)
        {
            pushUndo();
            deleteSelection();
        }
        else
        {
            if (cursorPosition == 0)
                return;

            std::size_t start = cursorPosition;

            while (start > 0)
            {
                const std::size_t previous =
                    previousUTF8(document, start);

                const std::string character =
                    document.substr(previous, start - previous);

                if (character != " " &&
                    character != "\n" &&
                    character != "\t" &&
                    character != "\f")
                {
                    break;
                }

                start = previous;
            }

            while (start > 0)
            {
                const std::size_t previous =
                    previousUTF8(document, start);

                const std::string character =
                    document.substr(previous, start - previous);

                if (character == " " ||
                    character == "\n" ||
                    character == "\t" ||
                    character == "\f")
                {
                    break;
                }

                start = previous;
            }

            if (start == cursorPosition)
                return;

            pushUndo();

            eraseDocumentText(start, cursorPosition - start);
            cursorPosition = start;
            clearSelection();
        }

        buildVisualLines();
        updateScroll();
    }

    /*
     * LINE:
     * elimina hacia atrás hasta el comienzo de la línea.
     *
     * Si el cursor ya está al comienzo, elimina el salto de línea
     * anterior. De este modo, la siguiente repetición continúa
     * sobre la línea precedente, incluso si la línea actual
     * estaba vacía.
     */
    void deleteLine()
    {
        if (hasSelection)
        {
            pushUndo();
            deleteSelection();
        }
        else
        {
            if (cursorPosition == 0)
                return;

            const std::size_t lineStart =
                getParagraphStart(cursorPosition);

            if (cursorPosition > lineStart)
            {
                pushUndo();

                eraseDocumentText(
                    lineStart,
                    cursorPosition - lineStart
                );

                cursorPosition = lineStart;
                clearSelection();
            }
            else
            {
                /*
                 * El cursor está al comienzo del párrafo.
                 * Elimina el separador anterior para unirlo
                 * con el párrafo precedente.
                 */
                const std::size_t separator = lineStart - 1;

                if (document[separator] != '\n' &&
                    document[separator] != '\f')
                {
                    return;
                }

                pushUndo();

                eraseDocumentText(separator, 1);
                cursorPosition = separator;
                clearSelection();
            }
        }

        buildVisualLines();
        updateScroll();
    }

    void insertLineBreak()
    {
        insertCharacter("\n");
    }

    void insertParagraphBreak()
    {
        insertCharacter("\n\n");
    }

    void insertPageBreak()
    {
        insertCharacter("\f");
    }

    void moveCursorHorizontal(int direction, bool selecting)
    {
        if (selecting)
        {
            if (!hasSelection)
            {
                selectionAnchor = cursorPosition;
                hasSelection = true;
            }
        }
        else
        {
            clearSelection();
        }

        if (direction < 0)
            cursorPosition = previousUTF8(document, cursorPosition);
        else if (direction > 0)
            cursorPosition = nextUTF8(document, cursorPosition);

        if (hasSelection && cursorPosition == selectionAnchor)
            clearSelection();

        updateScroll();
    }

    void moveCursorVertical(int direction, bool selecting)
    {
        buildVisualLines();

        if (visualLines.empty())
            return;

        if (selecting)
        {
            if (!hasSelection)
            {
                selectionAnchor = cursorPosition;
                hasSelection = true;
            }
        }
        else
        {
            clearSelection();
        }

        const std::size_t currentLine = findCursorLine();

        if (direction < 0 && currentLine == 0)
            return;

        if (direction > 0 && currentLine + 1 >= visualLines.size())
            return;

        const std::size_t lineStart = visualLines[currentLine].first;

        const std::size_t column =
            countCharacters(document, lineStart, cursorPosition);

        const std::size_t targetLine =
            direction < 0 ? currentLine - 1 : currentLine + 1;

        const std::size_t targetStart = visualLines[targetLine].first;
        const std::size_t targetEnd = visualLines[targetLine].second;

        std::size_t targetPosition = targetStart;
        std::size_t remaining = column;

        while (targetPosition < targetEnd && remaining > 0)
        {
            targetPosition = nextUTF8(document, targetPosition);
            --remaining;
        }

        cursorPosition = targetPosition;

        if (hasSelection && cursorPosition == selectionAnchor)
            clearSelection();

        updateScroll();
    }

    std::string getCurrentWord()
    {
        std::size_t start = cursorPosition;
        std::size_t end = cursorPosition;

        while (start > 0)
        {
            const std::size_t previous =
                previousUTF8(document, start);

            const std::string character =
                document.substr(previous, start - previous);

            if (character == " " || character == "\n" ||
                character == "\t" || character == "\f")
            {
                break;
            }

            start = previous;
        }

        while (end < document.size())
        {
            const std::size_t next = nextUTF8(document, end);

            const std::string character =
                document.substr(end, next - end);

            if (character == " " || character == "\n" ||
                character == "\t" || character == "\f")
            {
                break;
            }

            end = next;
        }

        std::string word = document.substr(start, end - start);

        if (!pendingDeadKey.empty())
            word += pendingDeadKey;

        return word;
    }

    void drawText(
        const char* content,
        float x,
        float y,
        float scale,
        u32 color)
    {
        C2D_Text text;

        C2D_TextParse(&text, textBuffer, content);
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

    float measureTextWidth(const std::string& content)
    {
        if (content.empty())
            return 0.0f;

        C2D_Text text;

        C2D_TextParse(&text, textBuffer, content.c_str());
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
            PAGE_X, PAGE_Y, 0.5f,
            PAGE_WIDTH, PAGE_HEIGHT,
            COLOR_PAPEL
        );

        C2D_DrawRectSolid(
            PAGE_X, PAGE_Y, 0.6f,
            PAGE_WIDTH, 1.0f, COLOR_BORDE
        );

        C2D_DrawRectSolid(
            PAGE_X, PAGE_Y + PAGE_HEIGHT - 1.0f, 0.6f,
            PAGE_WIDTH, 1.0f, COLOR_BORDE
        );

        C2D_DrawRectSolid(
            PAGE_X, PAGE_Y, 0.6f,
            1.0f, PAGE_HEIGHT, COLOR_BORDE
        );

        C2D_DrawRectSolid(
            PAGE_X + PAGE_WIDTH - 1.0f, PAGE_Y, 0.6f,
            1.0f, PAGE_HEIGHT, COLOR_BORDE
        );
    }

    void drawDocument()
    {
        buildVisualLines();
        updateScroll();

        for (int i = 0; i < VISIBLE_LINES; ++i)
        {
            const std::size_t lineIndex = firstVisibleLine + i;

            if (lineIndex >= visualLines.size())
                break;

            const std::size_t start = visualLines[lineIndex].first;
            const std::size_t end = visualLines[lineIndex].second;

            const std::string line =
                document.substr(start, end - start);

            const int indentLevel =
                getParagraphIndent(start);

            const float lineX =
                TEXT_X + indentLevel * INDENT_WIDTH;

            const float lineY =
                TEXT_Y + static_cast<float>(i) * LINE_HEIGHT;

            if (hasSelection)
            {
                const std::size_t overlapStart =
                    std::max(start, selectionStart());

                const std::size_t overlapEnd =
                    std::min(end, selectionEnd());

                if (overlapStart < overlapEnd)
                {
                    const std::string prefix =
                        document.substr(start, overlapStart - start);

                    const std::string selected =
                        document.substr(
                            overlapStart,
                            overlapEnd - overlapStart
                        );

                    C2D_DrawRectSolid(
                        lineX + measureTextWidth(prefix),
                        lineY,
                        0.4f,
                        measureTextWidth(selected),
                        11.0f,
                        COLOR_SELECCION
                    );
                }
            }

            if (!line.empty())
            {
                drawText(
                    line.c_str(),
                    lineX,
                    lineY,
                    TEXT_SCALE,
                    COLOR_CARBON
                );
            }

            if (showParagraphMarks &&
                end < document.size() &&
                (document[end] == '\n' || document[end] == '\f'))
            {
                drawText(
                    document[end] == '\f' ? "¶" : "·",
                    lineX + measureTextWidth(line) + 1.0f,
                    lineY - 0.5f,
                    0.48f,
                    COLOR_MUSGO
                );
            }
        }

        const std::size_t cursorLine = findCursorLine();

        if (cursorLine >= firstVisibleLine &&
            cursorLine < firstVisibleLine + VISIBLE_LINES)
        {
            const std::size_t start = visualLines[cursorLine].first;

            const std::string prefix =
                document.substr(start, cursorPosition - start);

            const float lineX =
                TEXT_X + getParagraphIndent(start) * INDENT_WIDTH;

            const float cursorX =
                lineX + measureTextWidth(prefix);

            const float cursorY =
                TEXT_Y +
                static_cast<float>(cursorLine - firstVisibleLine) *
                LINE_HEIGHT;

            if ((osGetTime() / CURSOR_BLINK_INTERVAL) % 2 == 0)
            {
                C2D_DrawRectSolid(
                    cursorX, cursorY, 0.7f,
                    1.5f, 10.0f,
                    COLOR_MUSGO
                );
            }
        }
    }

    KeyRect getKeyRect(int row, int column)
    {
        return {
            KEY_MARGIN + column * (KEY_WIDTH + KEY_GAP),
            KEY_START_Y + row * KEY_ROW_STEP,
            KEY_WIDTH,
            KEY_HEIGHT
        };
    }

    void drawKey(
        const KeyRect& rect,
        const char* label,
        bool active = false,
        float scale = 0.48f)
    {
        C2D_DrawRectSolid(
            rect.x, rect.y, 0.5f,
            rect.width, rect.height,
            COLOR_TECLA_BORDE
        );

        C2D_DrawRectSolid(
            rect.x + 1.0f, rect.y + 1.0f, 0.6f,
            rect.width - 2.0f, rect.height - 2.0f,
            active ? COLOR_TECLA_ACTIVA : COLOR_TECLA
        );

        C2D_DrawRectSolid(
            rect.x + 2.0f, rect.y + 2.0f, 0.7f,
            rect.width - 4.0f, 2.0f,
            COLOR_TECLA_SUPERIOR
        );

        if (label != nullptr && label[0] != '\0')
        {
            C2D_Text text;

            C2D_TextParse(&text, textBuffer, label);
            C2D_TextOptimize(&text);

            float width = 0.0f;
            float height = 0.0f;

            C2D_TextGetDimensions(
                &text, scale, scale, &width, &height
            );

            C2D_DrawText(
                &text,
                C2D_WithColor,
                rect.x + (rect.width - width) / 2.0f,
                rect.y + (rect.height - height) / 2.0f,
                0.8f,
                scale,
                scale,
                COLOR_CARBON
            );
        }
    }

    const char* getKeyLabel(
        const KeyDefinition& key,
        bool shift,
        bool altGr,
        bool caps)
    {
        if (key.letter)
        {
            const bool uppercase = caps != shift;

            if (altGr && key.altGr[0] != '\0')
                return key.altGr;

            return uppercase ? key.shift : key.base;
        }

        if (altGr && key.altGr[0] != '\0')
            return key.altGr;

        return shift ? key.shift : key.base;
    }

    void drawToolbar(bool shift, bool altGr)
    {
        const char* deleteLabel =
            altGr ? "LINE" : (shift ? "WORD" : "DEL");

        const char* labels[] = {
            "N", "K", "S", "Alin.", "Aa", "¶", deleteLabel
        };

        const bool active[] = {
            boldEnabled,
            italicEnabled,
            underlineEnabled,
            alignmentMode != 0,
            false,
            showParagraphMarks,
            false
        };

        constexpr float xStart = 4.0f;
        constexpr float gap = 2.0f;
        constexpr float width = 43.0f;
        constexpr float height = 19.0f;

        for (int i = 0; i < 7; ++i)
        {
            drawKey(
                {
                    xStart + i * (width + gap),
                    3.0f,
                    width,
                    height
                },
                labels[i],
                active[i],
                i == 3 ? 0.34f : (i == 6 ? 0.32f : 0.42f)
            );
        }
    }

    void drawWordPreview()
    {
        C2D_DrawRectSolid(
            5.0f, 27.0f, 0.5f,
            310.0f, 21.0f,
            COLOR_BORDE
        );

        C2D_DrawRectSolid(
            6.0f, 28.0f, 0.6f,
            308.0f, 19.0f,
            COLOR_PAPEL
        );

        const std::string word = getCurrentWord();

        if (word.empty())
        {
            drawText(
                "Palabra actual",
                11.0f, 33.0f, 0.34f,
                COLOR_SECUNDARIO
            );
        }
        else
        {
            drawText(
                word.c_str(),
                11.0f, 32.0f, 0.48f,
                COLOR_MUSGO
            );
        }
    }

    void drawKeyboard(bool shift, bool altGr)
    {
        for (int row = 0; row < 5; ++row)
        {
            for (int column = 0; column < 10; ++column)
            {
                const KeyDefinition& key = keyboard[row][column];

                drawKey(
                    getKeyRect(row, column),
                    getKeyLabel(key, shift, altGr, capsLock),
                    false,
                    0.43f
                );
            }
        }

        drawKey(
            {5.0f, 174.0f, 43.0f, 27.0f},
            "->|", false, 0.40f
        );

        const char* spaceLabel = "ESPACIO";
        float spaceScale = 0.43f;

        if (shift && altGr)
        {
            spaceLabel = "RESET";
            spaceScale = 0.43f;
        }
        else if (altGr)
        {
            spaceLabel = "SANGRIA";
            spaceScale = 0.39f;
        }
        else if (shift)
        {
            spaceLabel = "TAB";
            spaceScale = 0.48f;
        }

        drawKey(
            {50.0f, 174.0f, 220.0f, 27.0f},
            spaceLabel, false, spaceScale
        );

        drawKey(
            {272.0f, 174.0f, 43.0f, 27.0f},
            "=->", false, 0.40f
        );

        if (shift)
            drawText("SHIFT", 7.0f, 207.0f, 0.28f, COLOR_MUSGO);

        if (altGr)
            drawText("ALT GR", 260.0f, 207.0f, 0.28f, COLOR_MUSGO);
    }

    void drawBottomScreen(bool shift, bool altGr)
    {
        drawToolbar(shift, altGr);
        drawWordPreview();
        drawKeyboard(shift, altGr);

        if (capsLock)
            drawText("MAY", 283.0f, 34.0f, 0.30f, COLOR_MUSGO);
    }

    void drawEditor(bool shift, bool altGr)
    {
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        C2D_TextBufClear(textBuffer);

        C2D_TargetClear(topScreen, COLOR_CREMA);
        C2D_SceneBegin(topScreen);

        drawPage();
        drawDocument();

        C2D_TargetClear(bottomScreen, COLOR_CREMA);
        C2D_SceneBegin(bottomScreen);

        drawBottomScreen(shift, altGr);

        C3D_FrameEnd(0);
    }

    int getTouchedKey(float x, float y)
    {
        if (y >= 3.0f && y <= 22.0f &&
            x >= 274.0f && x <= 317.0f)
        {
            return 103;
        }

        if (y >= 3.0f && y <= 22.0f)
        {
            if (x >= 4.0f && x <= 47.0f) return 110;
            if (x >= 49.0f && x <= 92.0f) return 111;
            if (x >= 94.0f && x <= 137.0f) return 112;
            if (x >= 139.0f && x <= 182.0f) return 113;
            if (x >= 184.0f && x <= 227.0f) return 114;
            if (x >= 229.0f && x <= 272.0f) return 115;
        }

        if (y >= 174.0f && y <= 201.0f)
        {
            if (x >= 5.0f && x <= 48.0f) return 100;
            if (x >= 50.0f && x <= 270.0f) return 101;
            if (x >= 272.0f && x <= 315.0f) return 102;
        }

        for (int row = 0; row < 5; ++row)
        {
            for (int column = 0; column < 10; ++column)
            {
                const KeyRect rect = getKeyRect(row, column);

                if (x >= rect.x && x <= rect.x + rect.width &&
                    y >= rect.y && y <= rect.y + rect.height)
                {
                    return row * 10 + column;
                }
            }
        }

        return -1;
    }

    void activateToolbar(int index)
    {
        switch (index)
        {
            case 110:
                boldEnabled = !boldEnabled;
                break;

            case 111:
                italicEnabled = !italicEnabled;
                break;

            case 112:
                underlineEnabled = !underlineEnabled;
                break;

            case 113:
                alignmentMode = (alignmentMode + 1) % 3;
                break;

            case 114:
                // El panel de ajustes se incorporará con el sistema
                // de configuración del documento.
                break;

            case 115:
                showParagraphMarks = !showParagraphMarks;
                break;

            default:
                break;
        }
    }

    void activateKeyboardKey(int keyIndex, bool shift, bool altGr)
    {
        if (keyIndex >= 110 && keyIndex <= 115)
        {
            activateToolbar(keyIndex);
            return;
        }

        if (keyIndex == 100)
        {
            insertTab();
            return;
        }

        if (keyIndex == 101)
        {
            if (shift && altGr)
                resetIndent();
            else if (altGr)
                increaseIndent();
            else if (shift)
                insertTab();
            else
                insertCharacter(" ");

            return;
        }

        if (keyIndex == 102)
        {
            increaseIndent();
            return;
        }

        if (keyIndex == 103)
        {
            if (altGr)
                deleteLine();
            else if (shift)
                deleteWord();
            else
                backspace();

            return;
        }

        if (keyIndex < 0 || keyIndex >= 50)
            return;

        const int row = keyIndex / 10;
        const int column = keyIndex % 10;

        const KeyDefinition& key = keyboard[row][column];

        if (row == 3 && column == 9)
        {
            const char* accent =
                altGr ? "`" : (shift ? "¨" : "´");

            activateDeadKey(accent);
            return;
        }

        if (row == 0 && column == 9 && altGr)
        {
            activateDeadKey("^");
            return;
        }

        if (row == 3 && column == 2 && altGr)
        {
            insertCharacter(shift ? "Ç" : "ç");
            return;
        }

        const char* value =
            getKeyLabel(key, shift, altGr, capsLock);

        if (value != nullptr && value[0] != '\0')
            insertCharacter(value);
    }

    void processTouch(bool shift, bool altGr)
    {
        const u32 held = hidKeysHeld();

        if (!(held & KEY_TOUCH))
        {
            activeTouchKey = -1;
            nextRepeatTime = 0;
            return;
        }

        touchPosition touch;
        hidTouchRead(&touch);

        const int touchedKey =
            getTouchedKey(
                static_cast<float>(touch.px),
                static_cast<float>(touch.py)
            );

        if (touchedKey < 0)
        {
            activeTouchKey = -1;
            nextRepeatTime = 0;
            return;
        }

        const u64 now = osGetTime();

        if (touchedKey != activeTouchKey)
        {
            activeTouchKey = touchedKey;

            activateKeyboardKey(
                activeTouchKey,
                shift,
                altGr
            );

            nextRepeatTime = now + KEY_REPEAT_DELAY;
        }
        else if (now >= nextRepeatTime)
        {
            /*
             * Las teclas de texto y DEL se repiten.
             * Los botones de alternancia de la barra no.
             */
            if (activeTouchKey < 100 || activeTouchKey == 103)
            {
                activateKeyboardKey(
                    activeTouchKey,
                    shift,
                    altGr
                );
            }

            nextRepeatTime = now + KEY_REPEAT_INTERVAL;
        }
    }
}

bool Editor_Init(
    C3D_RenderTarget* topTarget,
    C3D_RenderTarget* bottomTarget)
{
    if (topTarget == nullptr || bottomTarget == nullptr)
        return false;

    topScreen = topTarget;
    bottomScreen = bottomTarget;

    textBuffer = C2D_TextBufNew(4096);

    if (textBuffer == nullptr)
        return false;

    document.clear();
    cursorPosition = 0;
    firstVisibleLine = 0;

    paragraphIndents.clear();
    paragraphIndents[0] = 0;

    capsLock = false;
    pendingDeadKey.clear();

    hasSelection = false;
    selectionAnchor = 0;

    clipboard.clear();

    boldEnabled = false;
    italicEnabled = false;
    underlineEnabled = false;
    showParagraphMarks = false;
    hudVisible = true;
    alignmentMode = 0;

    undoHistory.clear();
    redoHistory.clear();

    lastLPress = 0;
    nextRepeatTime = 0;
    nextPhysicalRepeat = 0;
    activeTouchKey = -1;

    buildVisualLines();

    return true;
}

void Editor_Run()
{
    while (aptMainLoop())
    {
        hidScanInput();

        const u32 pressed = hidKeysDown();
        const u32 held = hidKeysHeld();

        const bool shift = (held & KEY_L) != 0;
        const bool altGr = (held & KEY_ZL) != 0;
        const bool selecting = (held & KEY_SELECT) != 0;

        const u64 now = osGetTime();

        if (pressed & KEY_START)
            break;

        if (pressed & KEY_X)
            hudVisible = !hudVisible;

        if (pressed & KEY_L)
        {
            if (lastLPress != 0 &&
                now - lastLPress <= CAPS_DOUBLE_PRESS_MS)
            {
                capsLock = !capsLock;
                lastLPress = 0;
            }
            else
            {
                lastLPress = now;
            }
        }

        if ((pressed & KEY_Y) && shift)
            cutSelection();
        else if (pressed & KEY_Y)
            copySelection();

        if (pressed & KEY_A)
            pasteClipboard();

        if ((pressed & KEY_B) && altGr)
            redoAction();
        else if ((pressed & KEY_B) && shift)
            undoAction(5);
        else if (pressed & KEY_B)
            undoAction(1);

        if (pressed & KEY_LEFT)
            moveCursorHorizontal(-1, selecting);

        if (pressed & KEY_RIGHT)
            moveCursorHorizontal(1, selecting);

        if (pressed & KEY_UP)
            moveCursorVertical(-1, selecting);

        if (pressed & KEY_DOWN)
            moveCursorVertical(1, selecting);

        if (pressed & KEY_R)
        {
            if (altGr)
                insertPageBreak();
            else if (shift)
                insertParagraphBreak();
            else
                insertLineBreak();
        }

        const bool eraseHeld = (held & KEY_ZR) != 0;
        const bool erasePressed = (pressed & KEY_ZR) != 0;

        if (erasePressed)
        {
            nextPhysicalRepeat = now + PHYSICAL_REPEAT_DELAY;

            if (altGr)
                deleteLine();
            else if (shift)
                deleteWord();
            else
                backspace();
        }
        else if (eraseHeld && now >= nextPhysicalRepeat)
        {
            if (altGr)
                deleteLine();
            else if (shift)
                deleteWord();
            else
                backspace();

            nextPhysicalRepeat = now + PHYSICAL_REPEAT_INTERVAL;
        }

        processTouch(shift, altGr);

        drawEditor(shift, altGr);
    }
}

void Editor_Fini()
{
    if (textBuffer != nullptr)
    {
        C2D_TextBufDelete(textBuffer);
        textBuffer = nullptr;
    }

    topScreen = nullptr;
    bottomScreen = nullptr;

    document.clear();
    visualLines.clear();
    paragraphIndents.clear();

    undoHistory.clear();
    redoHistory.clear();

    clipboard.clear();
    pendingDeadKey.clear();
}
