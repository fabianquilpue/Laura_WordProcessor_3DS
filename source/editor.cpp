#include "editor.hpp"

#include <3ds.h>
#include <citro2d.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace
{
    constexpr float PAGE_X = 24.0f, PAGE_Y = 12.0f;
    constexpr float PAGE_WIDTH = 352.0f, PAGE_HEIGHT = 216.0f;
    constexpr float TEXT_X = 43.0f, TEXT_Y = 29.0f;
    constexpr float TEXT_SCALE = 0.48f, LINE_HEIGHT = 12.5f;
    constexpr float INDENT_WIDTH = 12.0f;
    constexpr float TEXT_WIDTH = 310.0f;
    constexpr float TEXT_BOTTOM = 213.0f;
    constexpr int MAX_LINE_CHARACTERS = 43, VISIBLE_LINES = 15;
    constexpr int MAX_INDENT_LEVEL = 8;
    constexpr int PAGE_LINES = 55;

    constexpr float KEY_WIDTH = 25.0f, KEY_HEIGHT = 18.0f;
    constexpr float KEY_GAP = 0.0f, KEY_START_X = 22.0f;
    constexpr float KEY_START_Y = 88.0f, KEY_ROW_STEP = 18.0f;
    constexpr float KEY_SPECIAL_X = KEY_START_X + 10.0f * KEY_WIDTH;
    constexpr float KEY_SPECIAL_WIDTH = 28.0f;
    constexpr float SPACE_KEY_WIDTH = 278.0f;

    constexpr u64 CURSOR_BLINK_INTERVAL = 500;
    constexpr u64 CAPS_DOUBLE_PRESS_MS = 350;
    constexpr u64 KEY_REPEAT_DELAY = 350, KEY_REPEAT_INTERVAL = 65;
    constexpr u64 PHYSICAL_REPEAT_DELAY = 350, PHYSICAL_REPEAT_INTERVAL = 65;
    constexpr std::size_t MAX_HISTORY = 100;

    constexpr u32 COLOR_CREMA = C2D_Color32(247,245,238,255);
    constexpr u32 COLOR_PAPEL = C2D_Color32(252,251,247,255);
    constexpr u32 COLOR_CARBON = C2D_Color32(30,30,30,255);
    constexpr u32 COLOR_MUSGO = C2D_Color32(78,122,101,255);
    constexpr u32 COLOR_BORDE = C2D_Color32(220,218,208,255);
    constexpr u32 COLOR_TECLA_BORDE = C2D_Color32(105,105,100,255);
    constexpr u32 COLOR_TECLA = C2D_Color32(246,245,239,255);
    constexpr u32 COLOR_TECLA_SUPERIOR = C2D_Color32(255,255,253,255);
    constexpr u32 COLOR_TECLA_ACTIVA = C2D_Color32(133,164,141,255);
    constexpr u32 COLOR_TECLA_SOMBRA = C2D_Color32(185,184,176,255);
    constexpr u32 COLOR_TECLA_LATERAL = C2D_Color32(220,219,211,255);
    constexpr u32 COLOR_SECUNDARIO = C2D_Color32(130,130,120,255);

    constexpr u32 COLOR_SELECCION = C2D_Color32(65,190,185,255);

    constexpr u32 COLOR_BLANCO = C2D_Color32(255,255,255,255);
    constexpr u32 COLOR_INFO_BORDE = C2D_Color32(125,125,118,255);

    C3D_RenderTarget* topScreen = nullptr;
    C3D_RenderTarget* bottomScreen = nullptr;
    C2D_TextBuf textBuffer = nullptr;

    std::string document;
    std::size_t cursorPosition = 0, firstVisibleLine = 0;
    std::vector<std::pair<std::size_t,std::size_t>> visualLines;

    int preferredVerticalColumn = -1;

    std::map<std::size_t,int> paragraphIndents;
    std::map<std::size_t,int> paragraphAlignments;
    std::map<std::size_t,float> paragraphLineSpacing;

    struct TextStyle
    {
        bool bold = false;
        bool italic = false;
        bool underline = false;
    };

    std::map<std::size_t,TextStyle> characterStyles;

    struct ClipboardData
    {
        std::string text;
        std::map<std::size_t,TextStyle> styles;
    };

    ClipboardData clipboard;

    bool capsLock = false;
    std::string pendingDeadKey;
    bool hasSelection = false, selectionMode = false;
    std::size_t selectionAnchor = 0;

    bool boldEnabled = false, italicEnabled = false, underlineEnabled = false;
    bool showParagraphMarks = false, hudVisible = true;

    std::time_t lastModified = 0;

    struct EditorSnapshot
    {
        std::string document;
        std::size_t cursor, anchor;
        bool selection, selectionMode;
        std::map<std::size_t,int> paragraphIndents;
        std::map<std::size_t,int> paragraphAlignments;
        std::map<std::size_t,float> paragraphLineSpacing;
        std::map<std::size_t,TextStyle> characterStyles;
        bool bold, italic, underline;
    };

    std::vector<EditorSnapshot> undoHistory, redoHistory;

    u64 lastLPress = 0, nextRepeatTime = 0;
    u64 nextPhysicalRepeat = 0, nextUndoRepeat = 0;

    u64 nextNavigationRepeat = 0;
    u32 navigationKey = 0;
    bool navigationShift = false;

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
        float x,y,width,height;
    };

    const std::array<std::array<KeyDefinition,10>,5> keyboard = {{
        {{{"1","*","|",false},{"2","@","+",false},{"3","·","#",false},
          {"4","$","ª",false},{"5","%","",false},{"6","&","¬",false},
          {"7","/","\\",false},{"8","<","",false},{"9",">","^",false},
          {"0","=","º",false}}},
        {{{"q","Q","",true},{"w","W","",true},{"e","E","€",true},{"r","R","",true},
          {"t","T","",true},{"y","Y","",true},{"u","U","",true},{"i","I","",true},
          {"o","O","",true},{"p","P","",true}}},
        {{{"a","A","",true},{"s","S","",true},{"d","D","",true},{"f","F","",true},
          {"g","G","",true},{"h","H","",true},{"j","J","",true},{"k","K","",true},
          {"l","L","",true},{"ñ","Ñ","~",true}}},
        {{{"z","Z","",true},{"x","X","",true},{"c","C","ç",true},{"v","V","",true},
          {"b","B","",true},{"n","N","",true},{"m","M","",true},
          {",",";",",",false},{".",":","…",false},{"´","¨","`",false}}},
        {{{"«","«","«",false},{"¿","¿","¿",false},{"¡","¡","¡",false},
          {"(","[","{",false},{"-","–","–",false},{"‘","“","“",false},
          {")","]","}",false},{"?","?","?",false},{"!","!","!",false},{"»","»","»",false}}}
    }};

    std::size_t nextUTF8(const std::string& s,std::size_t p)
    {
        if(p>=s.size()) return s.size();

        ++p;

        while(p<s.size() && (static_cast<unsigned char>(s[p])&0xC0)==0x80)
            ++p;

        return p;
    }

    std::size_t previousUTF8(const std::string& s,std::size_t p)
    {
        if(!p) return 0;

        --p;

        while(p>0 && (static_cast<unsigned char>(s[p])&0xC0)==0x80)
            --p;

        return p;
    }

    std::size_t countCharacters(const std::string& s,std::size_t a,std::size_t b)
    {
        std::size_t n=0;

        while(a<b)
        {
            a=nextUTF8(s,a);
            ++n;
        }

        return n;
    }

    bool isSeparator(char c)
    {
        return c=='\n' || c=='\v' || c=='\f';
    }

    bool isWhitespace(const std::string& c)
    {
        return c==" " || c=="\t" || c=="\n" || c=="\v" || c=="\f";
    }

    std::size_t getParagraphStart(std::size_t p)
    {
        p=std::min(p,document.size());

        if(!p) return 0;

        const std::size_t n=document.rfind('\n',p-1);

        return n==std::string::npos ? 0 : n+1;
    }

    std::size_t getParagraphEnd(std::size_t p)
    {
        p=std::min(p,document.size());

        const std::size_t n=document.find('\n',p);

        return n==std::string::npos ? document.size() : n;
    }

    std::size_t getLogicalLineStart(std::size_t p)
    {
        p=std::min(p,document.size());

        while(p>0 && !isSeparator(document[p-1]))
            p=previousUTF8(document,p);

        return p;
    }

    int getParagraphIndent(std::size_t p)
    {
        auto it=paragraphIndents.upper_bound(getParagraphStart(p));

        if(it==paragraphIndents.begin()) return 0;

        return (--it)->second;
    }

    int getParagraphAlignment(std::size_t p)
    {
        auto it=paragraphAlignments.upper_bound(getParagraphStart(p));

        if(it==paragraphAlignments.begin()) return 0;

        return (--it)->second;
    }

    float getParagraphSpacing(std::size_t p)
    {
        auto it=paragraphLineSpacing.upper_bound(getParagraphStart(p));

        if(it==paragraphLineSpacing.begin()) return 1.0f;

        return (--it)->second;
    }

    void markModified()
    {
        lastModified=std::time(nullptr);
    }

    EditorSnapshot makeSnapshot()
    {
        return {
            document,cursorPosition,selectionAnchor,hasSelection,selectionMode,
            paragraphIndents,paragraphAlignments,paragraphLineSpacing,
            characterStyles,boldEnabled,italicEnabled,underlineEnabled
        };
    }

    void pushUndo()
    {
        undoHistory.push_back(makeSnapshot());

        if(undoHistory.size()>MAX_HISTORY)
            undoHistory.erase(undoHistory.begin());

        redoHistory.clear();
    }

    void buildVisualLines()
    {
        visualLines.clear();

        std::size_t start=0,p=0;
        int chars=0;

        while(p<document.size())
        {
            if(isSeparator(document[p]))
            {
                visualLines.push_back({start,p});
                ++p;
                start=p;
                chars=0;
                continue;
            }

            p=nextUTF8(document,p);
            ++chars;

            if(chars>=MAX_LINE_CHARACTERS &&
               p<document.size() &&
               !isSeparator(document[p]))
            {
                visualLines.push_back({start,p});
                start=p;
                chars=0;
            }
        }

        visualLines.push_back({start,document.size()});
    }

    std::size_t findCursorLine()
    {
        for(std::size_t i=0;i<visualLines.size();++i)
        {
            if(cursorPosition>=visualLines[i].first &&
               cursorPosition<=visualLines[i].second)
                return i;
        }

        return visualLines.empty()?0:visualLines.size()-1;
    }

    float getLineY(std::size_t line)
    {
        float y=TEXT_Y;

        for(std::size_t i=firstVisibleLine;
            i<line && i<visualLines.size();++i)
        {
            y+=LINE_HEIGHT*getParagraphSpacing(visualLines[i].first);
        }

        return y;
    }

    void updateScroll()
    {
        if(visualLines.empty())
            buildVisualLines();

        if(visualLines.empty())
        {
            firstVisibleLine=0;
            return;
        }

        const std::size_t line=findCursorLine();

        if(line<firstVisibleLine)
            firstVisibleLine=line;

        firstVisibleLine=std::min(firstVisibleLine,line);

        while(firstVisibleLine<line)
        {
            float y=TEXT_Y;

            for(std::size_t i=firstVisibleLine;i<line;++i)
                y+=LINE_HEIGHT*getParagraphSpacing(visualLines[i].first);

            if(y+11.0f<=TEXT_BOTTOM)
                break;

            ++firstVisibleLine;
        }

        if(firstVisibleLine>=visualLines.size())
            firstVisibleLine=visualLines.size()-1;
    }

    void clearSelection()
    {
        hasSelection=false;
        selectionMode=false;
        selectionAnchor=cursorPosition;
    }

    std::size_t selectionStart()
    {
        return std::min(cursorPosition,selectionAnchor);
    }

    std::size_t selectionEnd()
    {
        return std::max(cursorPosition,selectionAnchor);
    }

    void deleteSelection()
    {
        if(!hasSelection) return;

        const auto a=selectionStart(),b=selectionEnd();
        const std::size_t removed=b-a;

        const int indent=getParagraphIndent(a);
        const int alignment=getParagraphAlignment(a);
        const float spacing=getParagraphSpacing(a);

        std::map<std::size_t,TextStyle> styles;

        for(const auto& e:characterStyles)
        {
            if(e.first<a)
                styles[e.first]=e.second;
            else if(e.first>=b)
                styles[e.first-removed]=e.second;
        }

        std::map<std::size_t,int> indents;

        for(const auto& e:paragraphIndents)
        {
            if(e.first<a)
                indents[e.first]=e.second;
            else if(e.first>=b)
                indents[e.first-removed]=e.second;
        }

        std::map<std::size_t,int> alignments;

        for(const auto& e:paragraphAlignments)
        {
            if(e.first<a)
                alignments[e.first]=e.second;
            else if(e.first>=b)
                alignments[e.first-removed]=e.second;
        }

        std::map<std::size_t,float> spacings;

        for(const auto& e:paragraphLineSpacing)
        {
            if(e.first<a)
                spacings[e.first]=e.second;
            else if(e.first>=b)
                spacings[e.first-removed]=e.second;
        }

        document.erase(a,removed);

        characterStyles.swap(styles);
        paragraphIndents.swap(indents);
        paragraphAlignments.swap(alignments);
        paragraphLineSpacing.swap(spacings);

        const std::size_t paragraph=getParagraphStart(a);

        paragraphIndents[paragraph]=indent;
        paragraphAlignments[paragraph]=alignment;
        paragraphLineSpacing[paragraph]=spacing;

        cursorPosition=a;
        clearSelection();
        preferredVerticalColumn=-1;
    }

    void restoreSnapshot(const EditorSnapshot& s)
    {
        document=s.document;
        cursorPosition=std::min(s.cursor,document.size());
        selectionAnchor=std::min(s.anchor,document.size());

        hasSelection=s.selection;
        selectionMode=s.selectionMode;

        paragraphIndents=s.paragraphIndents;
        paragraphAlignments=s.paragraphAlignments;
        paragraphLineSpacing=s.paragraphLineSpacing;
        characterStyles=s.characterStyles;

        boldEnabled=s.bold;
        italicEnabled=s.italic;
        underlineEnabled=s.underline;

        pendingDeadKey.clear();
        preferredVerticalColumn=-1;

        buildVisualLines();
        updateScroll();
        markModified();
    }

    void undoAction(int amount=1)
    {
        while(amount-->0 && !undoHistory.empty())
        {
            redoHistory.push_back(makeSnapshot());

            EditorSnapshot s=undoHistory.back();
            undoHistory.pop_back();

            restoreSnapshot(s);
        }
    }

    void redoAction()
    {
        if(redoHistory.empty()) return;

        undoHistory.push_back(makeSnapshot());

        EditorSnapshot s=redoHistory.back();
        redoHistory.pop_back();

        restoreSnapshot(s);
    }

    void insertDocumentText(std::size_t p,const std::string& value)
    {
        if(value.empty()) return;

        p=std::min(p,document.size());

        const int inheritedIndent=getParagraphIndent(p);
        const int inheritedAlignment=getParagraphAlignment(p);
        const float inheritedSpacing=getParagraphSpacing(p);

        std::map<std::size_t,int> indents;

        for(const auto& e:paragraphIndents)
            indents[e.first>=p?e.first+value.size():e.first]=e.second;

        std::map<std::size_t,int> alignments;

        for(const auto& e:paragraphAlignments)
            alignments[e.first>=p?e.first+value.size():e.first]=e.second;

        std::map<std::size_t,float> spacings;

        for(const auto& e:paragraphLineSpacing)
            spacings[e.first>=p?e.first+value.size():e.first]=e.second;

        std::map<std::size_t,TextStyle> styles;

        for(const auto& e:characterStyles)
            styles[e.first>=p?e.first+value.size():e.first]=e.second;

        document.insert(p,value);

        for(std::size_t i=0;i<value.size();)
        {
            if(value[i]=='\n')
            {
                indents[p+i+1]=inheritedIndent;
                alignments[p+i+1]=inheritedAlignment;
                spacings[p+i+1]=inheritedSpacing;
            }

            if(isSeparator(value[i]))
            {
                ++i;
                continue;
            }

            const std::size_t next=nextUTF8(value,i);

            styles[p+i]={boldEnabled,italicEnabled,underlineEnabled};

            i=next;
        }

        paragraphIndents.swap(indents);
        paragraphAlignments.swap(alignments);
        paragraphLineSpacing.swap(spacings);
        characterStyles.swap(styles);

        if(paragraphIndents.find(0)==paragraphIndents.end())
            paragraphIndents[0]=0;

        if(paragraphAlignments.find(0)==paragraphAlignments.end())
            paragraphAlignments[0]=0;

        if(paragraphLineSpacing.find(0)==paragraphLineSpacing.end())
            paragraphLineSpacing[0]=1.0f;
    }

    void insertRaw(const std::string& value)
    {
        if(value.empty()) return;

        pushUndo();

        if(hasSelection)
            deleteSelection();

        insertDocumentText(cursorPosition,value);

        cursorPosition+=value.size();

        clearSelection();
        preferredVerticalColumn=-1;

        buildVisualLines();
        updateScroll();
        markModified();
    }

    void copySelection()
    {
        if(!hasSelection) return;

        const std::size_t a=selectionStart();
        const std::size_t b=selectionEnd();

        clipboard.text=document.substr(a,b-a);
        clipboard.styles.clear();

        for(const auto& e:characterStyles)
        {
            if(e.first>=a && e.first<b)
                clipboard.styles[e.first-a]=e.second;
        }
    }

    void pasteClipboard(bool plain)
    {
        if(clipboard.text.empty()) return;

        pushUndo();

        if(hasSelection)
            deleteSelection();

        const std::size_t start=cursorPosition;

        insertDocumentText(cursorPosition,clipboard.text);

        if(!plain)
        {
            for(const auto& e:clipboard.styles)
                characterStyles[start+e.first]=e.second;
        }

        cursorPosition+=clipboard.text.size();

        clearSelection();
        preferredVerticalColumn=-1;

        buildVisualLines();
        updateScroll();
        markModified();
    }

    void insertCharacter(const std::string& value)
    {
        if(value.empty()) return;

        if(pendingDeadKey.empty())
        {
            insertRaw(value);
            return;
        }

        const std::string dead=pendingDeadKey;
        pendingDeadKey.clear();

        struct C
        {
            const char*d;
            const char*l;
            const char*r;
        };

        static const C combos[]={
            {"´","a","á"},{"´","e","é"},{"´","i","í"},{"´","o","ó"},{"´","u","ú"},
            {"´","A","Á"},{"´","E","É"},{"´","I","Í"},{"´","O","Ó"},{"´","U","Ú"},
            {"¨","u","ü"},{"¨","U","Ü"},
            {"`","a","à"},{"`","e","è"},{"`","i","ì"},{"`","o","ò"},{"`","u","ù"},
            {"^","a","â"},{"^","e","ê"},{"^","i","î"},{"^","o","ô"},{"^","u","û"}
        };

        std::string result=dead+value;

        for(const C& c:combos)
        {
            if(dead==c.d && value==c.l)
            {
                result=c.r;
                break;
            }
        }

        insertRaw(result);
    }

    void activateDeadKey(const std::string& a)
    {
        if(!pendingDeadKey.empty())
            insertRaw(pendingDeadKey);

        pendingDeadKey=a;
    }

    void backspace()
    {
        if(!pendingDeadKey.empty())
        {
            pendingDeadKey.clear();
            return;
        }

        if(hasSelection)
        {
            pushUndo();
            deleteSelection();

            buildVisualLines();
            updateScroll();
            markModified();

            return;
        }

        if(!cursorPosition) return;

        const std::size_t p=previousUTF8(document,cursorPosition);

        pushUndo();

        std::map<std::size_t,TextStyle> styles;

        for(const auto&e:characterStyles)
        {
            if(e.first<p)
                styles[e.first]=e.second;
            else if(e.first>p)
                styles[e.first-(cursorPosition-p)]=e.second;
        }

        const std::size_t len=cursorPosition-p;

        std::map<std::size_t,int> indents;
        std::map<std::size_t,int> alignments;
        std::map<std::size_t,float> spacings;

        for(const auto&e:paragraphIndents)
        {
            if(e.first>p && e.first<=cursorPosition) continue;

            indents[e.first>cursorPosition?e.first-len:e.first]=e.second;
        }

        for(const auto&e:paragraphAlignments)
        {
            if(e.first>p && e.first<=cursorPosition) continue;

            alignments[e.first>cursorPosition?e.first-len:e.first]=e.second;
        }

        for(const auto&e:paragraphLineSpacing)
        {
            if(e.first>p && e.first<=cursorPosition) continue;

            spacings[e.first>cursorPosition?e.first-len:e.first]=e.second;
        }

        document.erase(p,len);

        characterStyles.swap(styles);
        paragraphIndents.swap(indents);
        paragraphAlignments.swap(alignments);
        paragraphLineSpacing.swap(spacings);

        cursorPosition=p;

        clearSelection();
        preferredVerticalColumn=-1;

        buildVisualLines();
        updateScroll();
        markModified();
    }

    void insertTab()
    {
        insertCharacter("\t");
    }

    void increaseIndent()
    {
        const std::size_t p=getParagraphStart(cursorPosition);

        pushUndo();

        int& n=paragraphIndents[p];

        if(n<MAX_INDENT_LEVEL)
            ++n;

        buildVisualLines();
        updateScroll();
        markModified();
    }

    void resetIndent()
    {
        const std::size_t p=getParagraphStart(cursorPosition);

        pushUndo();

        if(hasSelection)
        {
            const std::size_t a=getParagraphStart(selectionStart());
            const std::size_t b=getParagraphEnd(selectionEnd());

            for(auto& e:paragraphIndents)
            {
                if(e.first>=a && e.first<=b)
                    e.second=0;
            }
        }
        else
        {
            paragraphIndents[p]=0;
        }

        buildVisualLines();
        updateScroll();
        markModified();
    }

    void deleteWord()
    {
        if(hasSelection)
        {
            pushUndo();
            deleteSelection();
        }
        else
        {
            if(!cursorPosition) return;

            std::size_t p=cursorPosition;

            while(p>0)
            {
                std::size_t q=previousUTF8(document,p);

                if(!isWhitespace(document.substr(q,p-q)))
                    break;

                p=q;
            }

            while(p>0)
            {
                std::size_t q=previousUTF8(document,p);

                if(isWhitespace(document.substr(q,p-q)))
                    break;

                p=q;
            }

            if(p==cursorPosition) return;

            pushUndo();

            const std::size_t len=cursorPosition-p;

            std::map<std::size_t,TextStyle> styles;

            for(const auto&e:characterStyles)
            {
                if(e.first<p)
                    styles[e.first]=e.second;
                else if(e.first>=cursorPosition)
                    styles[e.first-len]=e.second;
            }

            characterStyles.swap(styles);

            std::map<std::size_t,int> indents;
            std::map<std::size_t,int> alignments;
            std::map<std::size_t,float> spacings;

            for(const auto&e:paragraphIndents)
            {
                if(e.first>p && e.first<=cursorPosition) continue;

                indents[e.first>cursorPosition?e.first-len:e.first]=e.second;
            }

            for(const auto&e:paragraphAlignments)
            {
                if(e.first>p && e.first<=cursorPosition) continue;

                alignments[e.first>cursorPosition?e.first-len:e.first]=e.second;
            }

            for(const auto&e:paragraphLineSpacing)
            {
                if(e.first>p && e.first<=cursorPosition) continue;

                spacings[e.first>cursorPosition?e.first-len:e.first]=e.second;
            }

            document.erase(p,len);

            paragraphIndents.swap(indents);
            paragraphAlignments.swap(alignments);
            paragraphLineSpacing.swap(spacings);

            cursorPosition=p;
            clearSelection();
        }

        preferredVerticalColumn=-1;

        buildVisualLines();
        updateScroll();
        markModified();
    }

    void deleteLine()
    {
        if(hasSelection)
        {
            pushUndo();
            deleteSelection();
        }
        else
        {
            if(!cursorPosition) return;

            const std::size_t start=getLogicalLineStart(cursorPosition);

            std::size_t a=start,b=cursorPosition;

            if(a==b)
            {
                a=previousUTF8(document,cursorPosition);

                if(!isSeparator(document[a]))
                    return;

                b=cursorPosition;
            }

            if(a==b) return;

            pushUndo();

            const std::size_t len=b-a;

            std::map<std::size_t,TextStyle> styles;

            for(const auto&e:characterStyles)
            {
                if(e.first<a)
                    styles[e.first]=e.second;
                else if(e.first>=b)
                    styles[e.first-len]=e.second;
            }

            characterStyles.swap(styles);

            std::map<std::size_t,int> indents;
            std::map<std::size_t,int> alignments;
            std::map<std::size_t,float> spacings;

            for(const auto&e:paragraphIndents)
            {
                if(e.first>a && e.first<=b) continue;

                indents[e.first>b?e.first-len:e.first]=e.second;
            }

            for(const auto&e:paragraphAlignments)
            {
                if(e.first>a && e.first<=b) continue;

                alignments[e.first>b?e.first-len:e.first]=e.second;
            }

            for(const auto&e:paragraphLineSpacing)
            {
                if(e.first>a && e.first<=b) continue;

                spacings[e.first>b?e.first-len:e.first]=e.second;
            }

            document.erase(a,len);

            paragraphIndents.swap(indents);
            paragraphAlignments.swap(alignments);
            paragraphLineSpacing.swap(spacings);

            cursorPosition=a;
            clearSelection();
        }

        preferredVerticalColumn=-1;

        buildVisualLines();
        updateScroll();
        markModified();
    }

    void insertLineBreak()
    {
        insertCharacter("\v");
    }

    void insertParagraphBreak()
    {
        insertCharacter("\n");
    }

    void insertPageBreak()
    {
        insertCharacter("\f");
    }

    void prepareCursorMovement(bool selecting)
    {
        if(selecting)
        {
            if(!selectionMode)
            {
                selectionMode=true;
                selectionAnchor=cursorPosition;
            }
        }
        else if(selectionMode)
        {
            selectionMode=false;
            hasSelection=(cursorPosition!=selectionAnchor);
        }
        else if(hasSelection)
        {
            clearSelection();
        }
    }

    void finishCursorMovement()
    {
        if(selectionMode)
            hasSelection=(cursorPosition!=selectionAnchor);

        updateScroll();
    }

    void moveCursorHorizontal(int dir,bool selecting)
    {
        prepareCursorMovement(selecting);

        if(dir<0)
            cursorPosition=previousUTF8(document,cursorPosition);
        else if(dir>0)
            cursorPosition=nextUTF8(document,cursorPosition);

        preferredVerticalColumn=-1;

        finishCursorMovement();
    }

    int getCursorColumn(std::size_t line)
    {
        if(line>=visualLines.size()) return 0;

        const std::size_t start=visualLines[line].first;
        const std::size_t end=std::min(cursorPosition,visualLines[line].second);

        return static_cast<int>(countCharacters(document,start,end));
    }

    std::size_t getPositionAtColumn(std::size_t line,int column)
    {
        if(line>=visualLines.size())
            return document.size();

        std::size_t p=visualLines[line].first;
        const std::size_t end=visualLines[line].second;

        while(column>0 && p<end)
        {
            p=nextUTF8(document,p);
            --column;
        }

        return p;
    }

    void moveCursorVertical(int direction,bool selecting)
    {
        buildVisualLines();

        if(visualLines.empty()) return;

        const std::size_t currentLine=findCursorLine();

        if(preferredVerticalColumn<0)
            preferredVerticalColumn=getCursorColumn(currentLine);

        prepareCursorMovement(selecting);

        std::size_t targetLine=currentLine;

        if(direction<0)
        {
            if(targetLine>0)
                --targetLine;
        }
        else
        {
            if(targetLine+1<visualLines.size())
                ++targetLine;
        }

        cursorPosition=getPositionAtColumn(targetLine,preferredVerticalColumn);

        finishCursorMovement();
    }

    void moveCursorToLineBoundary(bool toEnd,bool selecting)
    {
        buildVisualLines();
        prepareCursorMovement(selecting);

        const std::size_t line=findCursorLine();

        if(!visualLines.empty())
        {
            cursorPosition=toEnd
                ?visualLines[line].second
                :visualLines[line].first;
        }

        preferredVerticalColumn=-1;

        finishCursorMovement();
    }

    void moveCursorToParagraphBoundary(bool toEnd,bool selecting)
    {
        prepareCursorMovement(selecting);

        cursorPosition=toEnd
            ?getParagraphEnd(cursorPosition)
            :getParagraphStart(cursorPosition);

        preferredVerticalColumn=-1;

        finishCursorMovement();
    }

    void moveCursorWord(int dir,bool selecting)
    {
        prepareCursorMovement(selecting);

        if(dir<0)
        {
            while(cursorPosition>0)
            {
                const std::size_t p=previousUTF8(document,cursorPosition);

                if(!isWhitespace(document.substr(p,cursorPosition-p)))
                    break;

                cursorPosition=p;
            }

            while(cursorPosition>0)
            {
                const std::size_t p=previousUTF8(document,cursorPosition);

                if(isWhitespace(document.substr(p,cursorPosition-p)))
                    break;

                cursorPosition=p;
            }
        }
        else
        {
            while(cursorPosition<document.size())
            {
                const std::size_t p=nextUTF8(document,cursorPosition);

                if(!isWhitespace(document.substr(cursorPosition,p-cursorPosition)))
                    break;

                cursorPosition=p;
            }

            while(cursorPosition<document.size())
            {
                const std::size_t p=nextUTF8(document,cursorPosition);

                if(isWhitespace(document.substr(cursorPosition,p-cursorPosition)))
                    break;

                cursorPosition=p;
            }
        }

        preferredVerticalColumn=-1;

        finishCursorMovement();
    }

    std::string getCurrentWord()
    {
        std::size_t a=cursorPosition,b=cursorPosition;

        while(a>0)
        {
            auto p=previousUTF8(document,a);

            if(isWhitespace(document.substr(p,a-p)))
                break;

            a=p;
        }

        while(b<document.size())
        {
            auto n=nextUTF8(document,b);

            if(isWhitespace(document.substr(b,n-b)))
                break;

            b=n;
        }

        std::string w=document.substr(a,b-a);

        if(!pendingDeadKey.empty())
            w+=pendingDeadKey;

        return w;
    }

    void changeAlignment()
    {
        const int next=(getParagraphAlignment(cursorPosition)+1)%4;

        pushUndo();

        if(!hasSelection)
        {
            paragraphAlignments[getParagraphStart(cursorPosition)]=next;
        }
        else
        {
            const std::size_t a=getParagraphStart(selectionStart());
            const std::size_t b=getParagraphStart(selectionEnd());

            for(std::size_t p=a;p<=b;)
            {
                if(p==b && b>a && selectionEnd()==b)
                    break;

                paragraphAlignments[p]=next;

                const std::size_t end=getParagraphEnd(p);

                if(end>=document.size())
                    break;

                p=end+1;
            }
        }

        markModified();
    }

    void changeLineSpacing()
    {
        static const float values[]={1.0f,1.2f,1.5f,2.0f};

        const float current=getParagraphSpacing(cursorPosition);

        int index=0;

        for(int i=0;i<4;++i)
        {
            if(std::abs(values[i]-current)<0.001f)
                index=i;
        }

        const float next=values[(index+1)%4];

        pushUndo();

        if(!hasSelection)
        {
            paragraphLineSpacing[getParagraphStart(cursorPosition)]=next;
        }
        else
        {
            const std::size_t a=getParagraphStart(selectionStart());
            const std::size_t b=getParagraphStart(selectionEnd());

            for(std::size_t p=a;p<=b;)
            {
                if(p==b && b>a && selectionEnd()==b)
                    break;

                paragraphLineSpacing[p]=next;

                const std::size_t end=getParagraphEnd(p);

                if(end>=document.size())
                    break;

                p=end+1;
            }
        }

        buildVisualLines();
        updateScroll();
        markModified();
    }

    void toggleStyle(bool TextStyle::*member,bool& enabled)
    {
        if(!hasSelection)
        {
            enabled=!enabled;
            return;
        }

        const std::size_t a=selectionStart();
        const std::size_t b=selectionEnd();

        bool all=true;

        for(std::size_t p=a;p<b;)
        {
            auto it=characterStyles.find(p);

            if(it==characterStyles.end() || !(it->second.*member))
            {
                all=false;
                break;
            }

            p=nextUTF8(document,p);
        }

        pushUndo();

        for(std::size_t p=a;p<b;)
        {
            (characterStyles[p].*member)=!all;
            p=nextUTF8(document,p);
        }

        enabled=!all;
        markModified();
    }

    TextStyle getCursorStyle()
    {
        if(cursorPosition<document.size())
        {
            auto it=characterStyles.find(cursorPosition);

            if(it!=characterStyles.end())
                return it->second;
        }

        if(cursorPosition>=document.size())
            return {boldEnabled,italicEnabled,underlineEnabled};

        return {};
    }

    void drawText(const char* s,float x,float y,float scale,u32 color,
                  float depth=0.45f)
    {
        C2D_Text t;

        C2D_TextParse(&t,textBuffer,s);
        C2D_TextOptimize(&t);

        C2D_DrawText(
            &t,C2D_WithColor,x,y,depth,scale,scale,color
        );
    }

    void drawItalicText(const char* s,float x,float y,float scale,u32 color,
                        float depth=0.45f)
    {
        C2D_Text t;

        C2D_TextParse(&t,textBuffer,s);
        C2D_TextOptimize(&t);

        C3D_Mtx previousMatrix;

        C2D_ViewSave(&previousMatrix);

        C2D_ViewTranslate(x,y);
        C2D_ViewShear(-0.22f,0.0f);
        C2D_ViewTranslate(-x,-y);

        C2D_DrawText(
            &t,C2D_WithColor,x,y,depth,scale,scale,color
        );

        C2D_Flush();
        C2D_ViewRestore(&previousMatrix);
    }

    float measureTextWidthAtScale(const std::string& s,float scale)
    {
        if(s.empty()) return 0.0f;

        C2D_Text t;

        C2D_TextParse(&t,textBuffer,s.c_str());
        C2D_TextOptimize(&t);

        float w=0.0f;

        C2D_TextGetDimensions(&t,scale,scale,&w,nullptr);

        return w;
    }

    float measureTextWidth(const std::string& s)
    {
        if(s.empty()) return 0.0f;

        float width=0.0f;
        const float spaceWidth=measureTextWidthAtScale(" ",TEXT_SCALE);

        for(std::size_t p=0;p<s.size();)
        {
            const std::size_t q=nextUTF8(s,p);
            const std::string ch=s.substr(p,q-p);

            if(ch=="\t")
            {
                width+=spaceWidth*4.0f;
            }
            else if(ch!="\n" && ch!="\v" && ch!="\f")
            {
                width+=measureTextWidthAtScale(ch,TEXT_SCALE);
            }

            p=q;
        }

        return width;
    }

    void drawPage()
    {
        C2D_DrawRectSolid(
            PAGE_X,PAGE_Y,0.5f,PAGE_WIDTH,PAGE_HEIGHT,COLOR_PAPEL
        );

        C2D_DrawRectSolid(
            PAGE_X,PAGE_Y,0.6f,PAGE_WIDTH,1,COLOR_BORDE
        );

        C2D_DrawRectSolid(
            PAGE_X,PAGE_Y+PAGE_HEIGHT-1,0.6f,PAGE_WIDTH,1,COLOR_BORDE
        );

        C2D_DrawRectSolid(
            PAGE_X,PAGE_Y,0.6f,1,PAGE_HEIGHT,COLOR_BORDE
        );

        C2D_DrawRectSolid(
            PAGE_X+PAGE_WIDTH-1,PAGE_Y,0.6f,1,PAGE_HEIGHT,COLOR_BORDE
        );
    }

    void drawTabMark(float x,float y,float width)
    {
        const float cy=y+6.0f;
        const float end=x+std::min(width-2.0f,14.0f);

        if(end<=x+2.0f) return;

        C2D_DrawRectSolid(
            x,cy,0.30f,end-x,1.1f,COLOR_MUSGO
        );

        C2D_DrawRectSolid(
            end-3.0f,cy-2.0f,0.30f,1.1f,5.0f,COLOR_MUSGO
        );

        C2D_DrawRectSolid(
            end-3.0f,cy-2.0f,0.30f,3.0f,1.1f,COLOR_MUSGO
        );

        C2D_DrawRectSolid(
            end-3.0f,cy+2.0f,0.30f,3.0f,1.1f,COLOR_MUSGO
        );
    }

    void drawStyledLine(std::size_t start,std::size_t end,float x,float y,
                        float extraSpace=0.0f)
    {
        std::size_t p=start;

        while(p<end)
        {
            const std::size_t q=nextUTF8(document,p);
            const std::string ch=document.substr(p,q-p);

            const auto it=characterStyles.find(p);
            const TextStyle st=it==characterStyles.end()?TextStyle{}:it->second;

            const float w=measureTextWidth(ch);

            if(ch=="\t")
            {
                if(showParagraphMarks)
                    drawTabMark(x,y,w);

                x+=w;
                p=q;
                continue;
            }

            if(st.italic)
                drawItalicText(ch.c_str(),x,y,TEXT_SCALE,COLOR_CARBON);
            else
                drawText(ch.c_str(),x,y,TEXT_SCALE,COLOR_CARBON);

            if(st.bold)
            {
                if(st.italic)
                    drawItalicText(
                        ch.c_str(),x+0.45f,y,TEXT_SCALE,COLOR_CARBON
                    );
                else
                    drawText(
                        ch.c_str(),x+0.45f,y,TEXT_SCALE,COLOR_CARBON
                    );
            }

            if(st.underline)
            {
                C2D_DrawRectSolid(
                    x,y+14.0f,0.30f,std::max(1.0f,w),1.0f,COLOR_CARBON
                );
            }

            x+=w;

            if(ch==" ")
                x+=extraSpace;

            p=q;
        }
    }

    void drawParagraphMark(char separator,float x,float y)
    {
        const float cy=y+5.5f;

        if(separator=='\n')
        {
            const float t=1.25f;

            C2D_DrawRectSolid(x+1.0f,cy-5.0f,0.30f,t,11.0f,COLOR_MUSGO);
            C2D_DrawRectSolid(x+1.0f,cy-5.0f,0.30f,7.0f,t,COLOR_MUSGO);
            C2D_DrawRectSolid(x+7.0f,cy-5.0f,0.30f,t,5.5f,COLOR_MUSGO);
            C2D_DrawRectSolid(x+1.0f,cy+0.0f,0.30f,7.0f,t,COLOR_MUSGO);
            C2D_DrawRectSolid(x+5.0f,cy+0.0f,0.30f,t,6.0f,COLOR_MUSGO);
        }
        else if(separator=='\v')
        {
            const float t=1.3f;

            C2D_DrawRectSolid(x+1.0f,cy-4.0f,0.30f,9.0f,t,COLOR_MUSGO);
            C2D_DrawRectSolid(x+9.0f,cy-4.0f,0.30f,t,8.0f,COLOR_MUSGO);
            C2D_DrawRectSolid(x+5.0f,cy+2.8f,0.30f,4.5f,t,COLOR_MUSGO);
            C2D_DrawRectSolid(x+5.0f,cy+1.0f,0.30f,t,2.0f,COLOR_MUSGO);
            C2D_DrawRectSolid(x+4.0f,cy+2.0f,0.30f,t,1.0f,COLOR_MUSGO);
            C2D_DrawRectSolid(x+3.0f,cy+3.0f,0.30f,t,1.0f,COLOR_MUSGO);
        }
        else if(separator=='\f')
        {
            const float lineY=cy+1.0f;
            const float labelWidth=68.0f;
            const float labelX=TEXT_X+(TEXT_WIDTH-labelWidth)*0.5f;

            for(float px=TEXT_X;px<TEXT_X+TEXT_WIDTH;px+=6.0f)
            {
                if(px>=labelX-3.0f && px<labelX+labelWidth+3.0f)
                    continue;

                C2D_DrawRectSolid(
                    px,lineY,0.30f,3.0f,1.0f,COLOR_MUSGO
                );
            }

            C2D_DrawRectSolid(
                labelX-2.0f,cy-3.0f,0.25f,labelWidth+4.0f,9.0f,
                COLOR_PAPEL
            );

            drawText(
                "Salto de página",labelX,cy-2.5f,0.28f,COLOR_MUSGO
            );
        }
    }

    float getAlignedLineX(std::size_t a,float lineWidth)
    {
        const float baseX=TEXT_X+getParagraphIndent(a)*INDENT_WIDTH;
        const int alignment=getParagraphAlignment(a);

        if(alignment==1)
            return baseX+std::max(0.0f,TEXT_WIDTH-lineWidth);

        if(alignment==2)
            return baseX+std::max(0.0f,(TEXT_WIDTH-lineWidth)*0.5f);

        return baseX;
    }

    float getJustificationExtra(std::size_t a,std::size_t b,float lineWidth)
    {
        if(getParagraphAlignment(a)!=3)
            return 0.0f;

        const bool isLastLine=(b>=document.size() || isSeparator(document[b]));

        if(isLastLine)
            return 0.0f;

        std::size_t spaces=0;

        for(std::size_t p=a;p<b;)
        {
            const std::size_t q=nextUTF8(document,p);

            if(document.substr(p,q-p)==" ")
                ++spaces;

            p=q;
        }

        if(!spaces || lineWidth>=TEXT_WIDTH)
            return 0.0f;

        return (TEXT_WIDTH-lineWidth)/spaces;
    }

    float getCursorX(std::size_t line)
    {
        const std::size_t a=visualLines[line].first;
        const std::size_t b=visualLines[line].second;

        const std::string whole=document.substr(a,b-a);
        const float width=measureTextWidth(whole);

        float x=getAlignedLineX(a,width);

        const std::string before=document.substr(a,cursorPosition-a);

        x+=measureTextWidth(before);

        if(getParagraphAlignment(a)==3)
        {
            const float extra=getJustificationExtra(a,b,width);

            if(extra>0.0f)
            {
                std::size_t spaces=0;

                for(std::size_t p=a;p<cursorPosition;)
                {
                    const std::size_t q=nextUTF8(document,p);

                    if(document.substr(p,q-p)==" ")
                        ++spaces;

                    p=q;
                }

                x+=spaces*extra;
            }
        }

        return x;
    }

    void drawDocument()
    {
        buildVisualLines();
        updateScroll();

        for(int i=0;i<VISIBLE_LINES;++i)
        {
            const std::size_t li=firstVisibleLine+i;

            if(li>=visualLines.size())
                break;

            const std::size_t a=visualLines[li].first;
            const std::size_t b=visualLines[li].second;
            const float y=getLineY(li);

            if(y+11.0f>TEXT_BOTTOM)
                break;

            const std::string line=document.substr(a,b-a);
            const float lineWidth=measureTextWidth(line);
            const float x=getAlignedLineX(a,lineWidth);
            const float extraSpace=getJustificationExtra(a,b,lineWidth);

            if(hasSelection)
            {
                const std::size_t sa=std::max(a,selectionStart());
                const std::size_t sb=std::min(b,selectionEnd());

                if(sa<sb)
                {
                    const std::string before=document.substr(a,sa-a);
                    const std::string selected=document.substr(sa,sb-sa);

                    C2D_DrawRectSolid(
                        x+measureTextWidth(before),y,0.47f,
                        std::max(1.0f,measureTextWidth(selected)),11.0f,
                        COLOR_SELECCION
                    );
                }
            }

            drawStyledLine(a,b,x,y,extraSpace);

            if(showParagraphMarks &&
               b<document.size() &&
               isSeparator(document[b]))
            {
                drawParagraphMark(document[b],x+lineWidth+2.0f,y);
            }

            if(showParagraphMarks &&
               a==b &&
               a==document.size() &&
               !document.empty() &&
               document.back()=='\n')
            {
                drawParagraphMark('\n',x,y);
            }
        }

        const std::size_t line=findCursorLine();

        if(line>=firstVisibleLine &&
           line<firstVisibleLine+VISIBLE_LINES)
        {
            const float x=getCursorX(line);
            const float y=getLineY(line);

            if(y+10.0f<=TEXT_BOTTOM &&
               (osGetTime()/CURSOR_BLINK_INTERVAL)%2==0)
            {
                C2D_DrawRectSolid(
                    x,y,0.30f,1.5f,10.0f,COLOR_MUSGO
                );
            }
        }
    }

    KeyRect getKeyRect(int r,int c)
    {
        return {
            KEY_START_X+c*(KEY_WIDTH+KEY_GAP),
            KEY_START_Y+r*KEY_ROW_STEP,
            KEY_WIDTH,
            KEY_HEIGHT
        };
    }

    void drawKey(const KeyRect&r,const char*label,bool active=false,
                 float scale=0.48f)
    {
        C2D_DrawRectSolid(
            r.x,r.y,0.5f,r.width,r.height,COLOR_TECLA_BORDE
        );

        C2D_DrawRectSolid(
            r.x+1.0f,r.y+1.0f,0.6f,
            r.width-2.0f,r.height-2.0f,
            active?COLOR_TECLA_ACTIVA:COLOR_TECLA
        );

        C2D_DrawRectSolid(
            r.x+1.0f,r.y+1.0f,0.7f,
            r.width-2.0f,1.0f,COLOR_TECLA_SUPERIOR
        );

        C2D_DrawRectSolid(
            r.x+1.0f,r.y+1.0f,0.7f,
            1.0f,r.height-2.0f,COLOR_TECLA_SUPERIOR
        );

        C2D_DrawRectSolid(
            r.x+1.0f,r.y+r.height-2.0f,0.75f,
            r.width-2.0f,1.0f,COLOR_TECLA_SOMBRA
        );

        C2D_DrawRectSolid(
            r.x+r.width-2.0f,r.y+1.0f,0.75f,
            1.0f,r.height-2.0f,COLOR_TECLA_LATERAL
        );

        if(label && label[0])
        {
            C2D_Text t;

            C2D_TextParse(&t,textBuffer,label);
            C2D_TextOptimize(&t);

            float w=0,h=0;

            C2D_TextGetDimensions(&t,scale,scale,&w,&h);

            // Mantener las etiquetas delante de las caras y bordes.
            C2D_DrawText(
                &t,C2D_WithColor,
                r.x+(r.width-w)/2,r.y+(r.height-h)/2,
                0.45f,scale,scale,COLOR_CARBON
            );
        }
    }

    const char* getKeyLabel(const KeyDefinition&k,bool shift,bool alt,bool caps)
    {
        if(k.letter)
        {
            if(alt && k.altGr[0])
                return k.altGr;

            return (caps!=shift)?k.shift:k.base;
        }

        if(alt && k.altGr[0])
            return k.altGr;

        return shift?k.shift:k.base;
    }

    void drawAlignmentIcon(float x,float y)
    {
        const float widths[4][3]={
            {21,16,20},
            {13,20,16},
            {17,21,15},
            {21,21,21}
        };

        const int alignment=getParagraphAlignment(cursorPosition);

        for(int i=0;i<3;++i)
        {
            const float w=widths[alignment][i];

            const float px=alignment==0?x:
                           alignment==1?x+21-w:
                           alignment==2?x+(21-w)*0.5f:x;

            C2D_DrawRectSolid(
                px,y+i*4.0f,0.8f,w,1.5f,COLOR_CARBON
            );
        }
    }

    void drawToolbar()
    {
        const TextStyle cursorStyle=getCursorStyle();

        for(int i=0;i<7;++i)
        {
            const float x=4.0f+i*45.0f;

            const bool active=
                i==0?cursorStyle.bold:
                i==1?cursorStyle.italic:
                i==2?cursorStyle.underline:
                i==6?showParagraphMarks:false;

            char spacing[8];
            const char* label="";

            switch(i)
            {
                case 0: label="N"; break;
                case 1: label="K"; break;
                case 2: label="S"; break;
                case 3: label=""; break;

                case 4:
                    std::snprintf(
                        spacing,sizeof(spacing),"%.1f",
                        getParagraphSpacing(cursorPosition)
                    );
                    label=spacing;
                    break;

                case 5: label="Aa"; break;
                case 6: label="¶"; break;
            }

            drawKey(
                {x,3,43,19},label,active,
                i==5?0.42f:0.40f
            );

            if(i==3)
                drawAlignmentIcon(x+11.0f,6.0f);
        }
    }

    bool isDeadKeyActive(int key)
    {
        if(key==39)
        {
            return pendingDeadKey=="´" ||
                   pendingDeadKey=="¨" ||
                   pendingDeadKey=="`";
        }

        if(key==48)
            return pendingDeadKey=="^";

        return false;
    }

    void drawEnterKey(bool shift,bool alt)
    {
        const char* label=alt?"PAG":(shift?"PAR":"ENT");

        const KeyRect r={
            KEY_SPECIAL_X,
            KEY_START_Y+KEY_ROW_STEP,
            KEY_SPECIAL_WIDTH,
            KEY_HEIGHT*2.0f
        };

        drawKey(r,label,activeTouchKey==104,0.34f);
    }

    void drawKeyboard(bool shift,bool alt)
    {
        for(int r=0;r<5;++r)
        {
            for(int c=0;c<10;++c)
            {
                const int key=r*10+c;

                drawKey(
                    getKeyRect(r,c),
                    getKeyLabel(keyboard[r][c],shift,alt,capsLock),
                    activeTouchKey==key || isDeadKeyActive(key),
                    0.50f
                );
            }
        }

        const char* deleteLabel=alt?"LINE":(shift?"WORD":"DEL");

        drawKey(
            {KEY_SPECIAL_X,KEY_START_Y,KEY_SPECIAL_WIDTH,KEY_HEIGHT},
            deleteLabel,
            activeTouchKey==103,
            0.30f
        );

        drawEnterKey(shift,alt);

        const char* label=(shift&&alt)?"RESET":
                          (alt?"INDENT":
                          (shift?"TAB":"ESPACIO"));

        drawKey(
            {KEY_START_X,185.0f,SPACE_KEY_WIDTH,27.0f},
            label,
            activeTouchKey==101,
            0.48f
        );
    }

    void drawFormattedPreview(const std::string& word,float centerX,float y)
    {
        if(word.empty()) return;

        constexpr float PREVIEW_SCALE=0.82f;
        constexpr float MAX_WIDTH=300.0f;

        std::size_t wordStart=cursorPosition;

        while(wordStart>0)
        {
            const std::size_t prev=previousUTF8(document,wordStart);

            if(isWhitespace(document.substr(prev,wordStart-prev)))
                break;

            wordStart=prev;
        }

        const std::size_t documentWordLength=
            word.size()>=pendingDeadKey.size()
                ?word.size()-pendingDeadKey.size()
                :word.size();

        float naturalWidth=0.0f;

        for(std::size_t p=0;p<word.size();)
        {
            const std::size_t q=nextUTF8(word,p);
            naturalWidth+=measureTextWidthAtScale(
                word.substr(p,q-p),PREVIEW_SCALE
            );
            p=q;
        }

        const float scale=PREVIEW_SCALE*
            std::min(1.0f,MAX_WIDTH/std::max(1.0f,naturalWidth));

        float totalWidth=0.0f;

        for(std::size_t p=0;p<word.size();)
        {
            const std::size_t q=nextUTF8(word,p);
            totalWidth+=measureTextWidthAtScale(
                word.substr(p,q-p),scale
            );
            p=q;
        }

        float x=centerX-totalWidth*0.5f;

        for(std::size_t p=0;p<word.size();)
        {
            const std::size_t q=nextUTF8(word,p);
            const std::string ch=word.substr(p,q-p);

            const float w=measureTextWidthAtScale(ch,scale);

            TextStyle style{};

            if(p<documentWordLength)
            {
                const std::size_t absolutePosition=wordStart+p;
                auto it=characterStyles.find(absolutePosition);

                if(it!=characterStyles.end())
                    style=it->second;
            }
            else
            {
                style={boldEnabled,italicEnabled,underlineEnabled};
            }

            if(style.italic)
                drawItalicText(ch.c_str(),x,y,scale,COLOR_CARBON);
            else
                drawText(ch.c_str(),x,y,scale,COLOR_CARBON);

            if(style.bold)
            {
                if(style.italic)
                    drawItalicText(
                        ch.c_str(),x+0.55f,y,scale,COLOR_CARBON
                    );
                else
                    drawText(
                        ch.c_str(),x+0.55f,y,scale,COLOR_CARBON
                    );
            }

            if(style.underline)
            {
                C2D_DrawRectSolid(
                    x,y+17.0f,0.30f,std::max(1.0f,w),1.2f,COLOR_CARBON
                );
            }

            x+=w;
            p=q;
        }
    }

    void drawWordViewer()
    {
        C2D_DrawRectSolid(
            5,27,0.5f,310,49,COLOR_BORDE
        );

        C2D_DrawRectSolid(
            6,28,0.6f,308,47,COLOR_PAPEL
        );

        const std::string word=getCurrentWord();

        if(word.empty())
        {
            drawText(
                "Palabra actual",10,43,0.42f,COLOR_SECUNDARIO
            );
            return;
        }

        drawFormattedPreview(word,160.0f,41.0f);
    }

    std::size_t countWords()
    {
        std::size_t count=0;
        bool inside=false;

        for(std::size_t p=0;p<document.size();)
        {
            const std::size_t q=nextUTF8(document,p);
            const std::string ch=document.substr(p,q-p);
            const bool space=isWhitespace(ch);

            if(!space && !inside)
            {
                ++count;
                inside=true;
            }
            else if(space)
            {
                inside=false;
            }

            p=q;
        }

        return count;
    }

    void drawInfoBars()
    {
        const std::time_t now=std::time(nullptr);
        std::tm localTime{};

        std::tm* current=std::localtime(&now);

        if(current)
            localTime=*current;

        char clock[16]="--:--";

        std::strftime(
            clock,sizeof(clock),"%H:%M",&localTime
        );

        u8 batteryLevel=0;
        const Result batteryResult=PTMU_GetBatteryLevel(&batteryLevel);
        const bool batteryAvailable=R_SUCCEEDED(batteryResult);

        char batteryText[24];

        if(batteryAvailable)
        {
            const int batteryPercent=std::min(
                100,static_cast<int>(batteryLevel)*20
            );

            std::snprintf(
                batteryText,sizeof(batteryText),
                "Batería: %d%%",batteryPercent
            );
        }
        else
        {
            std::snprintf(
                batteryText,sizeof(batteryText),
                "Batería: --"
            );
        }

        C2D_DrawRectSolid(
            0,0,0.05f,400,14,COLOR_BLANCO
        );

        C2D_DrawRectSolid(
            0,226,0.05f,400,14,COLOR_BLANCO
        );

        C2D_DrawRectSolid(
            0,0,0.04f,400,1,COLOR_INFO_BORDE
        );

        C2D_DrawRectSolid(
            0,13,0.04f,400,1,COLOR_INFO_BORDE
        );

        C2D_DrawRectSolid(
            0,0,0.04f,1,14,COLOR_INFO_BORDE
        );

        C2D_DrawRectSolid(
            399,0,0.04f,1,14,COLOR_INFO_BORDE
        );

        C2D_DrawRectSolid(
            0,226,0.04f,400,1,COLOR_INFO_BORDE
        );

        C2D_DrawRectSolid(
            0,239,0.04f,400,1,COLOR_INFO_BORDE
        );

        C2D_DrawRectSolid(
            0,226,0.04f,1,14,COLOR_INFO_BORDE
        );

        C2D_DrawRectSolid(
            399,226,0.04f,1,14,COLOR_INFO_BORDE
        );

        drawText(
            batteryText,5,1,0.32f,COLOR_CARBON,0.0f
        );

        drawText(
            clock,365,1,0.32f,COLOR_CARBON,0.0f
        );

        const std::size_t words=countWords();

        const std::size_t characters=countCharacters(
            document,0,document.size()
        );

        const std::size_t pages=std::max<std::size_t>(
            1,(visualLines.size()+PAGE_LINES-1)/PAGE_LINES
        );

        char wordText[32];
        char characterText[32];
        char pageText[24];

        std::snprintf(
            wordText,sizeof(wordText),"Palabras: %lu",
            static_cast<unsigned long>(words)
        );

        std::snprintf(
            characterText,sizeof(characterText),"Caracteres: %lu",
            static_cast<unsigned long>(characters)
        );

        std::snprintf(
            pageText,sizeof(pageText),"Páginas: %lu",
            static_cast<unsigned long>(pages)
        );

        drawText(
            wordText,8,227,0.31f,COLOR_CARBON,0.0f
        );

        drawText(
            characterText,140,227,0.31f,COLOR_CARBON,0.0f
        );

        drawText(
            pageText,290,227,0.31f,COLOR_CARBON,0.0f
        );
    }

    void drawBottomScreen(bool shift,bool alt)
    {
        drawToolbar();
        drawWordViewer();
        drawKeyboard(shift,alt);

        if(capsLock)
            drawText("MAY",283,64,0.30f,COLOR_MUSGO);
    }

    void drawEditor(bool shift,bool alt)
    {
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        C2D_TextBufClear(textBuffer);

        C2D_TargetClear(topScreen,COLOR_CREMA);
        C2D_SceneBegin(topScreen);

        drawPage();

        buildVisualLines();
        updateScroll();

        drawDocument();

        if(hudVisible)
            drawInfoBars();

        C2D_TargetClear(bottomScreen,COLOR_CREMA);
        C2D_SceneBegin(bottomScreen);

        drawBottomScreen(shift,alt);

        C3D_FrameEnd(0);
    }

    int getTouchedKey(float x,float y)
    {
        if(y>=3 && y<=22)
        {
            for(int i=0;i<7;++i)
            {
                if(x>=4+i*45 && x<=47+i*45)
                    return 110+i;
            }
        }

        if(y>=KEY_START_Y &&
           y<=KEY_START_Y+KEY_HEIGHT &&
           x>=KEY_SPECIAL_X &&
           x<=KEY_SPECIAL_X+KEY_SPECIAL_WIDTH)
        {
            return 103;
        }

        if(y>=KEY_START_Y+KEY_ROW_STEP &&
           y<=KEY_START_Y+KEY_ROW_STEP+KEY_HEIGHT*2 &&
           x>=KEY_SPECIAL_X &&
           x<=KEY_SPECIAL_X+KEY_SPECIAL_WIDTH)
        {
            return 104;
        }

        if(y>=185 && y<=212 &&
           x>=KEY_START_X &&
           x<=KEY_START_X+SPACE_KEY_WIDTH)
        {
            return 101;
        }

        for(int r=0;r<5;++r)
        {
            for(int c=0;c<10;++c)
            {
                const KeyRect q=getKeyRect(r,c);

                if(x>=q.x && x<=q.x+q.width &&
                   y>=q.y && y<=q.y+q.height)
                {
                    return r*10+c;
                }
            }
        }

        return -1;
    }

    void activateToolbar(int n)
    {
        switch(n)
        {
            case 110:
                toggleStyle(&TextStyle::bold,boldEnabled);
                break;

            case 111:
                toggleStyle(&TextStyle::italic,italicEnabled);
                break;

            case 112:
                toggleStyle(&TextStyle::underline,underlineEnabled);
                break;

            case 113:
                changeAlignment();
                break;

            case 114:
                changeLineSpacing();
                break;

            case 115:
                break;

            case 116:
                showParagraphMarks=!showParagraphMarks;
                break;
        }
    }

    void activateKeyboardKey(int key,bool shift,bool alt)
    {
        if(key>=110 && key<=116)
        {
            activateToolbar(key);
            return;
        }

        if(key==103)
        {
            if(alt)
                deleteLine();
            else if(shift)
                deleteWord();
            else
                backspace();

            return;
        }

        if(key==104)
        {
            if(alt)
                insertPageBreak();
            else if(shift)
                insertParagraphBreak();
            else
                insertLineBreak();

            return;
        }

        if(key==101)
        {
            if(shift && alt)
                resetIndent();
            else if(alt)
                increaseIndent();
            else if(shift)
                insertTab();
            else
                insertCharacter(" ");

            return;
        }

        if(key<0 || key>=50)
            return;

        const int r=key/10;
        const int c=key%10;

        const KeyDefinition& k=keyboard[r][c];

        if(key==37 && shift)
        {
            insertCharacter(";");
            return;
        }

        if(key==38 && alt)
        {
            insertCharacter("…");
            return;
        }

        if(key==38 && shift)
        {
            insertCharacter(":");
            return;
        }

        if(r==3 && c==9)
        {
            activateDeadKey(alt?"`":(shift?"¨":"´"));
            return;
        }

        if(r==0 && c==8 && alt)
        {
            activateDeadKey("^");
            return;
        }

        if(r==3 && c==2 && alt)
        {
            insertCharacter(shift?"Ç":"ç");
            return;
        }

        const char* v=getKeyLabel(k,shift,alt,capsLock);

        if(v && v[0])
            insertCharacter(v);
    }

    void processTouch(bool shift,bool alt)
    {
        const u32 held=hidKeysHeld();

        if(!(held&KEY_TOUCH))
        {
            activeTouchKey=-1;
            nextRepeatTime=0;
            return;
        }

        touchPosition t;

        hidTouchRead(&t);

        const int key=getTouchedKey(
            static_cast<float>(t.px),
            static_cast<float>(t.py)
        );

        if(key<0)
        {
            activeTouchKey=-1;
            nextRepeatTime=0;
            return;
        }

        const u64 now=osGetTime();

        if(key!=activeTouchKey)
        {
            activeTouchKey=key;

            activateKeyboardKey(key,shift,alt);

            nextRepeatTime=now+KEY_REPEAT_DELAY;
        }
        else if(now>=nextRepeatTime)
        {
            if(key<100 || key==103 || key==104)
                activateKeyboardKey(key,shift,alt);

            nextRepeatTime=now+KEY_REPEAT_INTERVAL;
        }
    }

    void performNavigation(u32 key,bool shift)
    {
        if(key==KEY_LEFT)
        {
            if(shift)
                moveCursorWord(-1,selectionMode);
            else
                moveCursorHorizontal(-1,selectionMode);
        }
        else if(key==KEY_RIGHT)
        {
            if(shift)
                moveCursorWord(1,selectionMode);
            else
                moveCursorHorizontal(1,selectionMode);
        }
        else if(key==KEY_UP)
        {
            if(shift)
                moveCursorToParagraphBoundary(false,selectionMode);
            else
                moveCursorVertical(-1,selectionMode);
        }
        else if(key==KEY_DOWN)
        {
            if(shift)
                moveCursorToParagraphBoundary(true,selectionMode);
            else
                moveCursorVertical(1,selectionMode);
        }
    }

    void processPhysicalNavigation(u32 pressed,u32 held,bool shift,u64 now)
    {
        const u32 directions[]={KEY_LEFT,KEY_RIGHT,KEY_UP,KEY_DOWN};

        u32 pressedDirection=0;
        u32 heldDirection=0;

        for(u32 key:directions)
        {
            if(pressed&key)
            {
                pressedDirection=key;
                break;
            }
        }

        for(u32 key:directions)
        {
            if(held&key)
            {
                heldDirection=key;
                break;
            }
        }

        if(pressedDirection)
        {
            performNavigation(pressedDirection,shift);

            navigationKey=pressedDirection;
            navigationShift=shift;
            nextNavigationRepeat=now+PHYSICAL_REPEAT_DELAY;

            return;
        }

        if(!heldDirection)
        {
            navigationKey=0;
            navigationShift=shift;
            nextNavigationRepeat=0;
            return;
        }

        if(navigationKey!=heldDirection || navigationShift!=shift)
        {
            navigationKey=heldDirection;
            navigationShift=shift;
            nextNavigationRepeat=now+PHYSICAL_REPEAT_DELAY;

            return;
        }

        if(now>=nextNavigationRepeat)
        {
            performNavigation(heldDirection,shift);
            nextNavigationRepeat=now+PHYSICAL_REPEAT_INTERVAL;
        }
    }
}

bool Editor_Init(C3D_RenderTarget* topTarget,C3D_RenderTarget* bottomTarget)
{
    if(!topTarget || !bottomTarget)
        return false;

    topScreen=topTarget;
    bottomScreen=bottomTarget;

    textBuffer=C2D_TextBufNew(32768);

    if(!textBuffer)
        return false;

    document.clear();

    cursorPosition=firstVisibleLine=0;
    preferredVerticalColumn=-1;

    paragraphIndents.clear();
    paragraphIndents[0]=0;

    paragraphAlignments.clear();
    paragraphAlignments[0]=0;

    paragraphLineSpacing.clear();
    paragraphLineSpacing[0]=1.0f;

    characterStyles.clear();

    capsLock=false;
    pendingDeadKey.clear();

    hasSelection=selectionMode=false;
    selectionAnchor=0;

    clipboard.text.clear();
    clipboard.styles.clear();

    boldEnabled=italicEnabled=underlineEnabled=false;

    showParagraphMarks=false;
    hudVisible=true;

    lastModified=std::time(nullptr);

    undoHistory.clear();
    redoHistory.clear();

    lastLPress=nextRepeatTime=nextPhysicalRepeat=nextUndoRepeat=0;

    nextNavigationRepeat=0;
    navigationKey=0;
    navigationShift=false;

    activeTouchKey=-1;

    buildVisualLines();

    return true;
}

void Editor_Run()
{
    while(aptMainLoop())
    {
        hidScanInput();

        const u32 pressed=hidKeysDown();
        const u32 held=hidKeysHeld();

        const bool shift=(held&KEY_L)!=0;
        const bool alt=(held&KEY_ZL)!=0;

        const u64 now=osGetTime();

        if(pressed&KEY_START)
            break;

        if(pressed&KEY_X)
            hudVisible=!hudVisible;

        if(pressed&KEY_L)
        {
            if(lastLPress && now-lastLPress<=CAPS_DOUBLE_PRESS_MS)
            {
                capsLock=!capsLock;
                lastLPress=0;
            }
            else
            {
                lastLPress=now;
            }
        }

        if(pressed&KEY_SELECT)
        {
            if(selectionMode)
            {
                selectionMode=false;
                hasSelection=(cursorPosition!=selectionAnchor);

                if(!hasSelection)
                    selectionAnchor=cursorPosition;
            }
            else
            {
                selectionMode=true;
                selectionAnchor=cursorPosition;
                hasSelection=false;
            }
        }

        if(pressed&KEY_Y)
        {
            if(hasSelection)
            {
                copySelection();

                if(shift || alt)
                {
                    pushUndo();
                    deleteSelection();

                    buildVisualLines();
                    updateScroll();
                    markModified();
                }
            }
        }

        if(pressed&KEY_A)
            pasteClipboard(shift||alt);

        if(pressed&KEY_B)
        {
            if(shift || alt)
                redoAction();
            else
                undoAction();

            nextUndoRepeat=now+PHYSICAL_REPEAT_DELAY;
        }
        else if((held&KEY_B) && now>=nextUndoRepeat)
        {
            if(shift || alt)
                redoAction();
            else
                undoAction();

            nextUndoRepeat=now+PHYSICAL_REPEAT_INTERVAL;
        }

        if(!(held&KEY_B))
            nextUndoRepeat=0;

        processPhysicalNavigation(pressed,held,shift,now);

        const bool pageCombo=(pressed&KEY_R)&&(held&KEY_ZR);

        if(pressed&KEY_R)
        {
            if(pageCombo)
                insertPageBreak();
            else if(shift)
                insertParagraphBreak();
            else
                insertLineBreak();
        }

        const bool erasePressed=(pressed&KEY_ZR)!=0;
        const bool eraseHeld=(held&KEY_ZR)!=0;

        if(erasePressed && !pageCombo)
        {
            nextPhysicalRepeat=now+PHYSICAL_REPEAT_DELAY;

            if(alt)
                deleteLine();
            else if(shift)
                deleteWord();
            else
                backspace();
        }
        else if(eraseHeld && !pageCombo && now>=nextPhysicalRepeat)
        {
            if(alt)
                deleteLine();
            else if(shift)
                deleteWord();
            else
                backspace();

            nextPhysicalRepeat=now+PHYSICAL_REPEAT_INTERVAL;
        }

        processTouch(shift,alt);

        drawEditor(shift,alt);
    }
}

void Editor_Fini()
{
    if(textBuffer)
    {
        C2D_TextBufDelete(textBuffer);
        textBuffer=nullptr;
    }

    topScreen=bottomScreen=nullptr;

    document.clear();
    visualLines.clear();

    preferredVerticalColumn=-1;

    paragraphIndents.clear();
    paragraphAlignments.clear();
    paragraphLineSpacing.clear();
    characterStyles.clear();

    undoHistory.clear();
    redoHistory.clear();

    clipboard.text.clear();
    clipboard.styles.clear();

    pendingDeadKey.clear();

    navigationKey=0;
    nextNavigationRepeat=0;
}
