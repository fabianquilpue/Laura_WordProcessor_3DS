            else if(shift)insertTab();
            else insertCharacter(" ");
            return;
        }
        if(key<0||key>=50)return;
        const int r=key/10,c=key%10;const KeyDefinition&k=keyboard[r][c];
        if(r==3&&c==9){activateDeadKey(alt?"`":(shift?"¨":"´"));return;}
        if(r==0&&c==9&&alt){activateDeadKey("^");return;}
        if(r==3&&c==2&&alt){insertCharacter(shift?"Ç":"ç");return;}
        const char*v=getKeyLabel(k,shift,alt,capsLock);
        if(v&&v[0])insertCharacter(v);
    }
    void processTouch(bool shift,bool alt)
    {
        const u32 held=hidKeysHeld();
        if(!(held&KEY_TOUCH)){activeTouchKey=-1;nextRepeatTime=0;return;}
        touchPosition t;hidTouchRead(&t);
        const int key=getTouchedKey(static_cast<float>(t.px),static_cast<float>(t.py));
        if(key<0){activeTouchKey=-1;nextRepeatTime=0;return;}
        const u64 now=osGetTime();
        if(key!=activeTouchKey)
        {
            activeTouchKey=key;activateKeyboardKey(key,shift,alt);nextRepeatTime=now+KEY_REPEAT_DELAY;
        }
        else if(now>=nextRepeatTime)
        {
            if(key<100||key==103)activateKeyboardKey(key,shift,alt);
            nextRepeatTime=now+KEY_REPEAT_INTERVAL;
        }
    }
}

bool Editor_Init(C3D_RenderTarget* topTarget,C3D_RenderTarget* bottomTarget)
{
    if(!topTarget||!bottomTarget)return false;
    topScreen=topTarget;bottomScreen=bottomTarget;textBuffer=C2D_TextBufNew(4096);
    if(!textBuffer)return false;
    document.clear();cursorPosition=firstVisibleLine=0;
    paragraphIndents.clear();paragraphIndents[0]=0;characterStyles.clear();
    capsLock=false;pendingDeadKey.clear();hasSelection=selectionMode=false;selectionAnchor=0;
    clipboard.clear();boldEnabled=italicEnabled=underlineEnabled=false;
    showParagraphMarks=false;hudVisible=true;alignmentMode=0;
    undoHistory.clear();redoHistory.clear();lastLPress=nextRepeatTime=nextPhysicalRepeat=0;
    activeTouchKey=-1;buildVisualLines();return true;
}

void Editor_Run()
{
    while(aptMainLoop())
    {
        hidScanInput();
        const u32 pressed=hidKeysDown(),held=hidKeysHeld();
        const bool shift=(held&KEY_L)!=0,alt=(held&KEY_ZL)!=0;
        const u64 now=osGetTime();
        if(pressed&KEY_START)break;
        if(pressed&KEY_X)hudVisible=!hudVisible;
        if(pressed&KEY_L)
        {
            if(lastLPress&&now-lastLPress<=CAPS_DOUBLE_PRESS_MS){capsLock=!capsLock;lastLPress=0;}
            else lastLPress=now;
        }
        if(pressed&KEY_SELECT)
        {
            if(selectionMode){selectionMode=false;hasSelection=(cursorPosition!=selectionAnchor);}
            else{selectionMode=true;selectionAnchor=cursorPosition;hasSelection=false;}
        }
        if((pressed&KEY_Y)&&shift)
        {
            if(hasSelection){clipboard=document.substr(selectionStart(),selectionEnd()-selectionStart());
                pushUndo();deleteSelection();buildVisualLines();updateScroll();}
        }
        else if(pressed&KEY_Y&&hasSelection)
            clipboard=document.substr(selectionStart(),selectionEnd()-selectionStart());
        if((pressed&KEY_A)&&!clipboard.empty())insertRaw(clipboard);
        if((pressed&KEY_B)&&alt)redoAction();
        else if((pressed&KEY_B)&&shift)undoAction(5);
        else if(pressed&KEY_B)undoAction();

        if(pressed&KEY_LEFT)moveCursorHorizontal(-1,selectionMode);
        if(pressed&KEY_RIGHT)moveCursorHorizontal(1,selectionMode);
        if(pressed&KEY_UP)moveCursorVertical(-1,selectionMode);
        if(pressed&KEY_DOWN)moveCursorVertical(1,selectionMode);

        const bool pageCombo=(pressed&KEY_R)&&(held&KEY_ZR);
        if(pressed&KEY_R)
        {
            if(pageCombo)insertPageBreak();
            else if(shift)insertParagraphBreak();
            else insertLineBreak();
        }
        const bool erasePressed=(pressed&KEY_ZR)!=0;
        const bool eraseHeld=(held&KEY_ZR)!=0;
        if(erasePressed&&!pageCombo)
        {
            nextPhysicalRepeat=now+PHYSICAL_REPEAT_DELAY;
            if(alt)deleteLine();else if(shift)deleteWord();else backspace();
        }
        else if(eraseHeld&&!pageCombo&&now>=nextPhysicalRepeat)
        {
            if(alt)deleteLine();else if(shift)deleteWord();else backspace();
            nextPhysicalRepeat=now+PHYSICAL_REPEAT_INTERVAL;
        }
        processTouch(shift,alt);
        drawEditor(shift,alt);
    }
}
void Editor_Fini()
{
    if(textBuffer){C2D_TextBufDelete(textBuffer);textBuffer=nullptr;}
    topScreen=bottomScreen=nullptr;document.clear();visualLines.clear();
    paragraphIndents.clear();characterStyles.clear();undoHistory.clear();redoHistory.clear();
    clipboard.clear();pendingDeadKey.clear();
}
