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
    constexpr float PAGE_X = 24.0f, PAGE_Y = 12.0f;
    constexpr float PAGE_WIDTH = 352.0f, PAGE_HEIGHT = 216.0f;
    constexpr float TEXT_X = 43.0f, TEXT_Y = 29.0f;
    constexpr float TEXT_SCALE = 0.48f, LINE_HEIGHT = 12.5f;
    constexpr float INDENT_WIDTH = 12.0f;
    constexpr int MAX_LINE_CHARACTERS = 43, VISIBLE_LINES = 15;
    constexpr int MAX_INDENT_LEVEL = 8;

    constexpr float KEY_WIDTH = 28.0f, KEY_HEIGHT = 20.0f;
    constexpr float KEY_GAP = 2.0f, KEY_START_X = 15.0f;
    constexpr float KEY_START_Y = 54.0f, KEY_ROW_STEP = 21.0f;

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
    constexpr u32 COLOR_TECLA_BORDE = C2D_Color32(38,38,35,255);
    constexpr u32 COLOR_TECLA = C2D_Color32(248,247,241,255);
    constexpr u32 COLOR_TECLA_SUPERIOR = C2D_Color32(255,255,252,255);
    constexpr u32 COLOR_TECLA_ACTIVA = C2D_Color32(220,231,222,255);
    constexpr u32 COLOR_SECUNDARIO = C2D_Color32(130,130,120,255);
    constexpr u32 COLOR_SELECCION = C2D_Color32(190,211,195,255);

    C3D_RenderTarget* topScreen = nullptr;
    C3D_RenderTarget* bottomScreen = nullptr;
    C2D_TextBuf textBuffer = nullptr;

    std::string document;
    std::size_t cursorPosition = 0, firstVisibleLine = 0;
    std::vector<std::pair<std::size_t,std::size_t>> visualLines;
    std::map<std::size_t,int> paragraphIndents;

    struct TextStyle
    {
        bool bold = false;
        bool italic = false;
        bool underline = false;
    };

    std::map<std::size_t,TextStyle> characterStyles;

    bool capsLock = false;
    std::string pendingDeadKey;
    bool hasSelection = false, selectionMode = false;
    std::size_t selectionAnchor = 0;
    std::string clipboard;

    bool boldEnabled = false, italicEnabled = false, underlineEnabled = false;
    bool showParagraphMarks = false, hudVisible = true;
    int alignmentMode = 0;

    struct EditorSnapshot
    {
        std::string document;
        std::size_t cursor, anchor;
        bool selection, selectionMode;
        std::map<std::size_t,int> paragraphIndents;
        std::map<std::size_t,TextStyle> characterStyles;
        bool bold, italic, underline;
    };

    std::vector<EditorSnapshot> undoHistory, redoHistory;

    u64 lastLPress = 0, nextRepeatTime = 0, nextPhysicalRepeat = 0;
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
        {{{"1","|","@",false},{"2","*","#",false},{"3","·","ª",false},
          {"4","$","+",false},{"5","%","¬",false},{"6","&","",false},
          {"7","/","º",false},{"8","(","<",false},{"9",")",">",false},{"0","=","^",false}}},
        {{{"q","Q","",true},{"w","W","",true},{"e","E","€",true},{"r","R","",true},
          {"t","T","",true},{"y","Y","",true},{"u","U","",true},{"i","I","",true},
          {"o","O","",true},{"p","P","",true}}},
        {{{"a","A","",true},{"s","S","",true},{"d","D","",true},{"f","F","",true},
          {"g","G","",true},{"h","H","",true},{"j","J","",true},{"k","K","",true},
          {"l","L","",true},{"ñ","Ñ","~",true}}},
        {{{"z","Z","",true},{"x","X","",true},{"c","C","ç",true},{"v","V","",true},
          {"b","B","",true},{"n","N","",true},{"m","M","",true},{",",",",",",false},
          {".",".",".",false},{"´","¨","`",false}}},
        {{{"«","«","«",false},{"¿","¿","¿",false},{"¡","¡","¡",false},
          {"(","[","{",false},{"-","–","–",false},{"‘","“","“",false},
          {")","]","}",false},{"?","?","?",false},{"!","!","!",false},{"»","»","»",false}}}
    }};

    std::size_t nextUTF8(const std::string& s,std::size_t p)
    {
        if(p>=s.size()) return s.size();
        ++p;
        while(p<s.size() && (static_cast<unsigned char>(s[p])&0xC0)==0x80) ++p;
        return p;
    }

    std::size_t previousUTF8(const std::string& s,std::size_t p)
    {
        if(!p) return 0;
        --p;
        while(p>0 && (static_cast<unsigned char>(s[p])&0xC0)==0x80) --p;
        return p;
    }

    std::size_t countCharacters(const std::string& s,std::size_t a,std::size_t b)
    {
        std::size_t n=0;
        while(a<b){a=nextUTF8(s,a);++n;}
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

    EditorSnapshot makeSnapshot()
    {
        return {document,cursorPosition,selectionAnchor,hasSelection,selectionMode,
                paragraphIndents,characterStyles,boldEnabled,italicEnabled,underlineEnabled};
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

            if(chars>=MAX_LINE_CHARACTERS && p<document.size() && !isSeparator(document[p]))
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
            if(cursorPosition>=visualLines[i].first && cursorPosition<=visualLines[i].second)
                return i;

        return visualLines.empty()?0:visualLines.size()-1;
    }

    void updateScroll()
    {
        const std::size_t line=findCursorLine();

        if(line<firstVisibleLine)
            firstVisibleLine=line;
        else if(line>=firstVisibleLine+VISIBLE_LINES)
            firstVisibleLine=line-VISIBLE_LINES+1;

        const std::size_t max=visualLines.size()>VISIBLE_LINES
            ?visualLines.size()-VISIBLE_LINES:0;

        firstVisibleLine=std::min(firstVisibleLine,max);
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

        std::map<std::size_t,TextStyle> styles;

        for(const auto& e:characterStyles)
        {
            if(e.first<a) styles[e.first]=e.second;
            else if(e.first>=b) styles[e.first-(b-a)]=e.second;
        }

        characterStyles.swap(styles);

        const int indent=getParagraphIndent(a);
        std::map<std::size_t,int> indents;

        for(const auto& e:paragraphIndents)
        {
            if(e.first<a) indents[e.first]=e.second;
            else if(e.first>=b) indents[e.first-(b-a)]=e.second;
        }

        indents[getParagraphStart(a)]=indent;

        document.erase(a,b-a);
        paragraphIndents.swap(indents);
        cursorPosition=a;
        clearSelection();
    }

    void restoreSnapshot(const EditorSnapshot& s)
    {
        document=s.document;
        cursorPosition=std::min(s.cursor,document.size());
        selectionAnchor=std::min(s.anchor,document.size());
        hasSelection=s.selection;
        selectionMode=s.selectionMode;
        paragraphIndents=s.paragraphIndents;
        characterStyles=s.characterStyles;
        boldEnabled=s.bold;
        italicEnabled=s.italic;
        underlineEnabled=s.underline;
        pendingDeadKey.clear();
        buildVisualLines();
        updateScroll();
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
        const int inherited=getParagraphIndent(p);

        std::map<std::size_t,int> indents;
        for(const auto& e:paragraphIndents)
            indents[e.first>=p?e.first+value.size():e.first]=e.second;

        std::map<std::size_t,TextStyle> styles;
        for(const auto& e:characterStyles)
            styles[e.first>=p?e.first+value.size():e.first]=e.second;

        document.insert(p,value);

        for(std::size_t i=0;i<value.size();)
        {
            if(value[i]=='\n') indents[p+i+1]=inherited;

            if(value[i]=='\n'||value[i]=='\v'||value[i]=='\f')
            {
                ++i;
                continue;
            }

            const std::size_t next=nextUTF8(value,i);
            styles[p+i]={boldEnabled,italicEnabled,underlineEnabled};
            i=next;
        }

        paragraphIndents.swap(indents);
        characterStyles.swap(styles);

        if(paragraphIndents.find(0)==paragraphIndents.end())
            paragraphIndents[0]=0;
    }

    void insertRaw(const std::string& value)
    {
        if(value.empty()) return;

        pushUndo();

        if(hasSelection) deleteSelection();

        insertDocumentText(cursorPosition,value);
        cursorPosition+=value.size();
        clearSelection();
        buildVisualLines();
        updateScroll();
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

        struct C{const char*d;const char*l;const char*r;};

        static const C combos[]={
            {"´","a","á"},{"´","e","é"},{"´","i","í"},{"´","o","ó"},{"´","u","ú"},
            {"´","A","Á"},{"´","E","É"},{"´","I","Í"},{"´","O","Ó"},{"´","U","Ú"},
            {"¨","u","ü"},{"¨","U","Ü"},{"`","a","à"},{"`","e","è"},{"`","i","ì"},
            {"`","o","ò"},{"`","u","ù"},{"^","a","â"},{"^","e","ê"},{"^","i","î"},
            {"^","o","ô"},{"^","u","û"}
        };

        std::string result=dead+value;

        for(const C& c:combos)
            if(dead==c.d&&value==c.l)
            {
                result=c.r;
                break;
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
            return;
        }

        if(!cursorPosition) return;

        const std::size_t p=previousUTF8(document,cursorPosition);
        pushUndo();

        std::map<std::size_t,TextStyle> styles;

        for(const auto&e:characterStyles)
        {
            if(e.first<p) styles[e.first]=e.second;
            else if(e.first>p) styles[e.first-(cursorPosition-p)]=e.second;
        }

        characterStyles.swap(styles);

        const std::size_t len=cursorPosition-p;
        std::map<std::size_t,int> indents;

        for(const auto&e:paragraphIndents)
        {
            if(e.first>p && e.first<=cursorPosition) continue;
            indents[e.first>cursorPosition?e.first-len:e.first]=e.second;
        }

        document.erase(p,len);
        paragraphIndents.swap(indents);
        cursorPosition=p;
        clearSelection();
        buildVisualLines();
        updateScroll();
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
        if(n<MAX_INDENT_LEVEL) ++n;
        buildVisualLines();
        updateScroll();
    }

    void resetIndent()
    {
        const std::size_t p=getParagraphStart(cursorPosition);
        pushUndo();

        for(auto&e:paragraphIndents)
            if(e.first>=p) e.second=0;

        buildVisualLines();
        updateScroll();
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
                if(!isWhitespace(document.substr(q,p-q))) break;
                p=q;
            }

            while(p>0)
            {
                std::size_t q=previousUTF8(document,p);
                if(isWhitespace(document.substr(q,p-q))) break;
                p=q;
            }

            if(p==cursorPosition) return;

            pushUndo();
            const std::size_t len=cursorPosition-p;

            std::map<std::size_t,TextStyle> styles;

            for(const auto&e:characterStyles)
            {
                if(e.first<p) styles[e.first]=e.second;
                else if(e.first>=cursorPosition) styles[e.first-len]=e.second;
            }

            characterStyles.swap(styles);

            std::map<std::size_t,int> indents;

            for(const auto&e:paragraphIndents)
            {
                if(e.first>p&&e.first<=cursorPosition) continue;
                indents[e.first>cursorPosition?e.first-len:e.first]=e.second;
            }

            document.erase(p,len);
            paragraphIndents.swap(indents);
            cursorPosition=p;
            clearSelection();
        }

        buildVisualLines();
        updateScroll();
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
                if(!isSeparator(document[a])) return;
                b=cursorPosition;
            }

            if(a==b) return;

            pushUndo();
            const std::size_t len=b-a;

            std::map<std::size_t,TextStyle> styles;

            for(const auto&e:characterStyles)
            {
                if(e.first<a) styles[e.first]=e.second;
                else if(e.first>=b) styles[e.first-len]=e.second;
            }

            characterStyles.swap(styles);

            std::map<std::size_t,int> indents;

            for(const auto&e:paragraphIndents)
            {
                if(e.first>a&&e.first<=b) continue;
                indents[e.first>b?e.first-len:e.first]=e.second;
            }

            document.erase(a,len);
            paragraphIndents.swap(indents);
            cursorPosition=a;
            clearSelection();
        }

        buildVisualLines();
        updateScroll();
    }

    void insertLineBreak(){insertCharacter("\v");}
    void insertParagraphBreak(){insertCharacter("\n");}
    void insertPageBreak(){insertCharacter("\f");}

    void moveCursorHorizontal(int dir,bool selecting)
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

        if(dir<0)
            cursorPosition=previousUTF8(document,cursorPosition);
        else if(dir>0)
            cursorPosition=nextUTF8(document,cursorPosition);

        if(selectionMode)
            hasSelection=(cursorPosition!=selectionAnchor);

        updateScroll();
    }

    void moveCursorVertical(int dir,bool selecting)
    {
        buildVisualLines();
        if(visualLines.empty()) return;

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

        const std::size_t line=findCursorLine();

        if((dir<0&&line==0)||(dir>0&&line+1>=visualLines.size()))
            return;

        const std::size_t col=countCharacters(document,visualLines[line].first,cursorPosition);
        const std::size_t target=dir<0?line-1:line+1;

        std::size_t p=visualLines[target].first,remain=col;

        while(p<visualLines[target].second&&remain>0)
        {
            p=nextUTF8(document,p);
            --remain;
        }

        cursorPosition=p;

        if(selectionMode)
            hasSelection=(cursorPosition!=selectionAnchor);

        updateScroll();
    }

    std::string getCurrentWord()
    {
        std::size_t a=cursorPosition,b=cursorPosition;

        while(a>0)
        {
            auto p=previousUTF8(document,a);
            if(isWhitespace(document.substr(p,a-p))) break;
            a=p;
        }

        while(b<document.size())
        {
            auto n=nextUTF8(document,b);
            if(isWhitespace(document.substr(b,n-b))) break;
            b=n;
        }

        std::string w=document.substr(a,b-a);
        if(!pendingDeadKey.empty()) w+=pendingDeadKey;
        return w;
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

            if(it==characterStyles.end()||!(it->second.*member))
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
    }

    void drawText(const char* s,float x,float y,float scale,u32 color)
    {
        C2D_Text t;
        C2D_TextParse(&t,textBuffer,s);
        C2D_TextOptimize(&t);
        C2D_DrawText(&t,C2D_WithColor,x,y,0.5f,scale,scale,color);
    }

    void drawItalicText(const char* s,float x,float y,float scale,u32 color)
    {
        C2D_Text t;
        C2D_TextParse(&t,textBuffer,s);
        C2D_TextOptimize(&t);

        C3D_Mtx previousMatrix;

        // Guardar la transformación actual de Citro2D.
        C2D_ViewSave(&previousMatrix);

        // Trasladar el origen al punto donde comienza el carácter.
        C2D_ViewTranslate(x,y);

        // Inclinar horizontalmente los glifos para representar cursiva.
        C2D_ViewShear(-0.22f,0.0f);

        // Devolver el origen a su posición original.
        C2D_ViewTranslate(-x,-y);

        C2D_DrawText(
            &t,
            C2D_WithColor,
            x,
            y,
            0.5f,
            scale,
            scale,
            color
        );

        // Asegurar que el texto transformado se procese antes de restaurar.
        C2D_Flush();

        // Restaurar la transformación previa para no afectar otros elementos.
        C2D_ViewRestore(&previousMatrix);
    }

    float measureTextWidth(const std::string& s)
    {
        if(s.empty()) return 0.0f;

        C2D_Text t;
        C2D_TextParse(&t,textBuffer,s.c_str());
        C2D_TextOptimize(&t);

        float w=0;
        C2D_TextGetDimensions(&t,TEXT_SCALE,TEXT_SCALE,&w,nullptr);
        return w;
    }

    void drawPage()
    {
        C2D_DrawRectSolid(PAGE_X,PAGE_Y,0.5f,PAGE_WIDTH,PAGE_HEIGHT,COLOR_PAPEL);
        C2D_DrawRectSolid(PAGE_X,PAGE_Y,0.6f,PAGE_WIDTH,1,COLOR_BORDE);
        C2D_DrawRectSolid(PAGE_X,PAGE_Y+PAGE_HEIGHT-1,0.6f,PAGE_WIDTH,1,COLOR_BORDE);
        C2D_DrawRectSolid(PAGE_X,PAGE_Y,0.6f,1,PAGE_HEIGHT,COLOR_BORDE);
        C2D_DrawRectSolid(PAGE_X+PAGE_WIDTH-1,PAGE_Y,0.6f,1,PAGE_HEIGHT,COLOR_BORDE);
    }

    void drawStyledLine(std::size_t start,std::size_t end,float x,float y)
    {
        std::size_t p=start;

        while(p<end)
        {
            const std::size_t q=nextUTF8(document,p);
            const std::string ch=document.substr(p,q-p);
            const auto it=characterStyles.find(p);
            const TextStyle st=it==characterStyles.end()?TextStyle{}:it->second;
            const float w=measureTextWidth(ch);

            if(st.italic)
                drawItalicText(ch.c_str(),x,y,TEXT_SCALE,COLOR_CARBON);
            else
                drawText(ch.c_str(),x,y,TEXT_SCALE,COLOR_CARBON);

            if(st.bold)
            {
                if(st.italic)
                    drawItalicText(ch.c_str(),x+0.45f,y,TEXT_SCALE,COLOR_CARBON);
                else
                    drawText(ch.c_str(),x+0.45f,y,TEXT_SCALE,COLOR_CARBON);
            }

            if(st.underline)
            {
                C2D_DrawRectSolid(
                    x,
                    y+11.5f,
                    0.7f,
                    std::max(1.0f,w),
                    0.55f,
                    COLOR_CARBON
                );
            }

            x+=w;
            p=q;
        }
    }

    void drawDocument()
    {
        buildVisualLines();
        updateScroll();

        for(int i=0;i<VISIBLE_LINES;++i)
        {
            const std::size_t li=firstVisibleLine+i;
            if(li>=visualLines.size()) break;

            const std::size_t a=visualLines[li].first,b=visualLines[li].second;
            const float x=TEXT_X+getParagraphIndent(a)*INDENT_WIDTH;
            const float y=TEXT_Y+i*LINE_HEIGHT;
            const std::string line=document.substr(a,b-a);

            if(hasSelection)
            {
                const std::size_t sa=std::max(a,selectionStart());
                const std::size_t sb=std::min(b,selectionEnd());

                if(sa<sb)
                {
                    const std::string before=document.substr(a,sa-a);
                    const std::string selected=document.substr(sa,sb-sa);

                    C2D_DrawRectSolid(
                        x+measureTextWidth(before),
                        y,
                        0.4f,
                        std::max(1.0f,measureTextWidth(selected)),
                        11,
                        COLOR_SELECCION
                    );
                }
            }

            drawStyledLine(a,b,x,y);

            if(showParagraphMarks&&b<document.size())
            {
                const char sep=document[b];
                const char* mark=sep=='\n'?"¶":(sep=='\v'?"↵":"");

                if(mark[0])
                    drawText(
                        mark,
                        x+measureTextWidth(line)+1,
                        y-0.5f,
                        0.48f,
                        COLOR_MUSGO
                    );
            }
        }

        const std::size_t line=findCursorLine();

        if(line>=firstVisibleLine&&line<firstVisibleLine+VISIBLE_LINES)
        {
            const std::size_t a=visualLines[line].first;
            const float x=TEXT_X+getParagraphIndent(a)*INDENT_WIDTH+
                          measureTextWidth(document.substr(a,cursorPosition-a));
            const float y=TEXT_Y+(line-firstVisibleLine)*LINE_HEIGHT;

            if((osGetTime()/CURSOR_BLINK_INTERVAL)%2==0)
                C2D_DrawRectSolid(x,y,0.7f,1.5f,10,COLOR_MUSGO);
        }
    }

    KeyRect getKeyRect(int r,int c)
    {
        const float total=10*KEY_WIDTH+9*KEY_GAP;

        return {KEY_START_X+(320-total)/2+c*(KEY_WIDTH+KEY_GAP),
                KEY_START_Y+r*KEY_ROW_STEP,KEY_WIDTH,KEY_HEIGHT};
    }

    void drawKey(const KeyRect&r,const char*label,bool active=false,float scale=0.43f)
    {
        C2D_DrawRectSolid(r.x,r.y,0.5f,r.width,r.height,COLOR_TECLA_BORDE);
        C2D_DrawRectSolid(r.x+1,r.y+1,0.6f,r.width-2,r.height-2,
                          active?COLOR_TECLA_ACTIVA:COLOR_TECLA);
        C2D_DrawRectSolid(r.x+2,r.y+2,0.7f,r.width-4,2,COLOR_TECLA_SUPERIOR);

        if(label&&label[0])
        {
            C2D_Text t;
            C2D_TextParse(&t,textBuffer,label);
            C2D_TextOptimize(&t);

            float w=0,h=0;
            C2D_TextGetDimensions(&t,scale,scale,&w,&h);

            C2D_DrawText(&t,C2D_WithColor,r.x+(r.width-w)/2,r.y+(r.height-h)/2,
                         0.8f,scale,scale,COLOR_CARBON);
        }
    }

    const char* getKeyLabel(const KeyDefinition&k,bool shift,bool alt,bool caps)
    {
        if(k.letter)
        {
            if(alt&&k.altGr[0]) return k.altGr;
            return (caps!=shift)?k.shift:k.base;
        }

        if(alt&&k.altGr[0]) return k.altGr;
        return shift?k.shift:k.base;
    }

    void drawToolbar(bool shift,bool alt)
    {
        const char* del=alt?"LINE":(shift?"WORD":"DEL");
        const char* labels[]={"N","K","S","Alin.","Aa","¶",del};
        const bool active[]={boldEnabled,italicEnabled,underlineEnabled,alignmentMode!=0,
                             false,showParagraphMarks,false};

        for(int i=0;i<7;++i)
            drawKey({4.0f+i*45.0f,3,43,19},labels[i],active[i],
                    i==3?0.34f:(i==6?0.32f:0.42f));
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
                    activeTouchKey==key,
                    0.40f
                );
            }
        }

        const char* label=(shift&&alt)?"RESET":
                          (alt?"INDENT":
                          (shift?"TAB":"ESPACIO"));

        drawKey(
            {49,174,222,27},
            label,
            activeTouchKey==101,
            0.43f
        );

        if(shift) drawText("SHIFT",7,207,0.28f,COLOR_MUSGO);
        if(alt) drawText("ZL",287,207,0.28f,COLOR_MUSGO);
    }

    void drawBottomScreen(bool shift,bool alt)
    {
        drawToolbar(shift,alt);

        C2D_DrawRectSolid(5,27,0.5f,310,21,COLOR_BORDE);
        C2D_DrawRectSolid(6,28,0.6f,308,19,COLOR_PAPEL);

        const std::string word=getCurrentWord();

        drawText(word.empty()?"Palabra actual":word.c_str(),11,32,
                 word.empty()?0.34f:0.48f,
                 word.empty()?COLOR_SECUNDARIO:COLOR_MUSGO);

        drawKeyboard(shift,alt);

        if(capsLock)
            drawText("MAY",283,34,0.30f,COLOR_MUSGO);
    }

    void drawEditor(bool shift,bool alt)
    {
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TextBufClear(textBuffer);

        C2D_TargetClear(topScreen,COLOR_CREMA);
        C2D_SceneBegin(topScreen);
        drawPage();
        drawDocument();

        C2D_TargetClear(bottomScreen,COLOR_CREMA);
        C2D_SceneBegin(bottomScreen);
        drawBottomScreen(shift,alt);

        C3D_FrameEnd(0);
    }

    int getTouchedKey(float x,float y)
    {
        if(y>=3&&y<=22)
        {
            if(x>=274&&x<=317) return 103;

            for(int i=0;i<6;++i)
                if(x>=4+i*45&&x<=47+i*45) return 110+i;
        }

        if(y>=174&&y<=201&&x>=49&&x<=271)
            return 101;

        for(int r=0;r<5;++r)
            for(int c=0;c<10;++c)
            {
                const KeyRect q=getKeyRect(r,c);

                if(x>=q.x&&x<=q.x+q.width&&y>=q.y&&y<=q.y+q.height)
                    return r*10+c;
            }

        return -1;
    }

    void activateToolbar(int n)
    {
        switch(n)
        {
            case 110:toggleStyle(&TextStyle::bold,boldEnabled);break;
            case 111:toggleStyle(&TextStyle::italic,italicEnabled);break;
            case 112:toggleStyle(&TextStyle::underline,underlineEnabled);break;
            case 113:alignmentMode=(alignmentMode+1)%3;break;
            case 114:break;
            case 115:showParagraphMarks=!showParagraphMarks;break;
        }
    }

    void activateKeyboardKey(int key,bool shift,bool alt)
    {
        if(key>=110&&key<=115)
        {
            activateToolbar(key);
            return;
        }

        if(key==103)
        {
            if(alt) deleteLine();
            else if(shift) deleteWord();
            else backspace();
            return;
        }

        if(key==101)
        {
            if(shift&&alt) resetIndent();
            else if(alt) increaseIndent();
            else if(shift) insertTab();
            else insertCharacter(" ");
            return;
        }

        if(key<0||key>=50) return;

        const int r=key/10,c=key%10;
        const KeyDefinition&k=keyboard[r][c];

        if(r==3&&c==9)
        {
            activateDeadKey(alt?"`":(shift?"¨":"´"));
            return;
        }

        if(r==0&&c==9&&alt)
        {
            activateDeadKey("^");
            return;
        }

        if(r==3&&c==2&&alt)
        {
            insertCharacter(shift?"Ç":"ç");
            return;
        }

        const char*v=getKeyLabel(k,shift,alt,capsLock);

        if(v&&v[0])
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
            if(key<100||key==103)
                activateKeyboardKey(key,shift,alt);

            nextRepeatTime=now+KEY_REPEAT_INTERVAL;
        }
    }
}

bool Editor_Init(C3D_RenderTarget* topTarget,C3D_RenderTarget* bottomTarget)
{
    if(!topTarget||!bottomTarget) return false;

    topScreen=topTarget;
    bottomScreen=bottomTarget;
    textBuffer=C2D_TextBufNew(4096);

    if(!textBuffer) return false;

    document.clear();
    cursorPosition=firstVisibleLine=0;

    paragraphIndents.clear();
    paragraphIndents[0]=0;
    characterStyles.clear();

    capsLock=false;
    pendingDeadKey.clear();
    hasSelection=selectionMode=false;
    selectionAnchor=0;
    clipboard.clear();

    boldEnabled=italicEnabled=underlineEnabled=false;
    showParagraphMarks=false;
    hudVisible=true;
    alignmentMode=0;

    undoHistory.clear();
    redoHistory.clear();

    lastLPress=nextRepeatTime=nextPhysicalRepeat=0;
    activeTouchKey=-1;

    buildVisualLines();
    return true;
}

void Editor_Run()
{
    while(aptMainLoop())
    {
        hidScanInput();

        const u32 pressed=hidKeysDown(),held=hidKeysHeld();
        const bool shift=(held&KEY_L)!=0,alt=(held&KEY_ZL)!=0;
        const u64 now=osGetTime();

        if(pressed&KEY_START) break;
        if(pressed&KEY_X) hudVisible=!hudVisible;

        if(pressed&KEY_L)
        {
            if(lastLPress&&now-lastLPress<=CAPS_DOUBLE_PRESS_MS)
            {
                capsLock=!capsLock;
                lastLPress=0;
            }
            else lastLPress=now;
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

        if((pressed&KEY_Y)&&shift)
        {
            if(hasSelection)
            {
                clipboard=document.substr(selectionStart(),selectionEnd()-selectionStart());
                pushUndo();
                deleteSelection();
                buildVisualLines();
                updateScroll();
            }
        }
        else if(pressed&KEY_Y&&hasSelection)
            clipboard=document.substr(selectionStart(),selectionEnd()-selectionStart());

        if((pressed&KEY_A)&&!clipboard.empty())
            insertRaw(clipboard);

        if((pressed&KEY_B)&&alt) redoAction();
        else if((pressed&KEY_B)&&shift) undoAction(5);
        else if(pressed&KEY_B) undoAction();

        if(pressed&KEY_LEFT) moveCursorHorizontal(-1,selectionMode);
        if(pressed&KEY_RIGHT) moveCursorHorizontal(1,selectionMode);
        if(pressed&KEY_UP) moveCursorVertical(-1,selectionMode);
        if(pressed&KEY_DOWN) moveCursorVertical(1,selectionMode);

        const bool pageCombo=(pressed&KEY_R)&&(held&KEY_ZR);

        if(pressed&KEY_R)
        {
            if(pageCombo) insertPageBreak();
            else if(shift) insertParagraphBreak();
            else insertLineBreak();
        }

        const bool erasePressed=(pressed&KEY_ZR)!=0;
        const bool eraseHeld=(held&KEY_ZR)!=0;

        if(erasePressed&&!pageCombo)
        {
            nextPhysicalRepeat=now+PHYSICAL_REPEAT_DELAY;

            if(alt) deleteLine();
            else if(shift) deleteWord();
            else backspace();
        }
        else if(eraseHeld&&!pageCombo&&now>=nextPhysicalRepeat)
        {
            if(alt) deleteLine();
            else if(shift) deleteWord();
            else backspace();

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
    paragraphIndents.clear();
    characterStyles.clear();
    undoHistory.clear();
    redoHistory.clear();
    clipboard.clear();
    pendingDeadKey.clear();
}
