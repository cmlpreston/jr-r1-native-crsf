void TransmitterUi::drawModelManagement()
{
    using Action = ModelManagementAction;
    using Control = ModelManagementControl;
    using Stage = ModelManagementStage;

    auto *screen = lv_scr_act();
    constexpr int inset = UiLayout::PageInset;
    constexpr int gap = UiLayout::Gap;
    const int width = kScreenWidth - 2*inset;
    const auto action = modelManagementAction_;
    const auto stage = modelManagementStage_;
    const char *title = action==Action::New ? "New model" :
        action==Action::Rename ? "Rename model" :
        action==Action::Copy ? "Copy model" : "Reset controls";
    auto text = [&](const char *value, int y, UiText::Style style, uint32_t tint) {
        auto *label = makeLabel(screen,value,inset,y,style,tint);
        lv_obj_set_width(label,width);
        lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
        return label;
    };
    auto button = [&](int x, int y, int w, int h, const char *label, Control item,
                      UiText::Style style = UiText::Secondary, uint32_t tint = cWhite) {
        return makePageActionButton(x,y,w,h,label,style,cPanel,tint,cLine,
            &TransmitterUi::controlModelManagement,uint8_t(item));
    };

    text(stage==Stage::Actions?"Manage models":title,46,UiText::Heading,cWhite);
    char line[64];
    if (stage == Stage::Actions) {
        snprintf(line,sizeof(line),"Current model: %s",modelNames_[activeModelSlot_]);
        text(line,78,UiText::Secondary,cDim);
        const int half = (width-gap)/2;
        button(inset,109,half,50,"New model",Control::New,UiText::Menu);
        button(inset+half+gap,109,half,50,"Rename model",Control::Rename,UiText::Menu);
        button(inset,167,half,50,"Copy model",Control::Copy,UiText::Menu);
        button(inset+half+gap,167,half,50,"Reset controls",Control::Reset,UiText::Menu);
        return;
    }

    if (stage == Stage::Slot) {
        if (action == Action::Copy) {
            snprintf(line,sizeof(line),"Copy from %s",modelNames_[activeModelSlot_]);
            text(line,76,UiText::Secondary,cDim);
        } else {
            text(action==Action::New?"Choose an inactive slot":"Choose a slot",
                76,UiText::Secondary,cDim);
        }
        const int columns = portraitUi?1:2;
        const int rows = (kModelMemoryCount+columns-1)/columns;
        const int pitch = portraitUi?34:42;
        const int cellWidth = (width-(columns-1)*gap)/columns;
        const int cellHeight = pitch-2;
        for (uint8_t slot = 0; slot < kModelMemoryCount; ++slot) {
            const bool active = slot==activeModelSlot_;
            const bool allowed = !active || action==Action::Rename || action==Action::Reset;
            const auto item = Control(uint8_t(Control::FirstSlot)+slot);
            const int x = inset+(slot/rows)*(cellWidth+gap);
            const int y = 99+(slot%rows)*pitch;
            auto *cell = allowed?button(x,y,cellWidth,cellHeight,modelNames_[slot],item,UiText::Menu):
                makeButton(x,y,cellWidth,cellHeight,modelNames_[slot],UiText::Menu,cPanel,cDim,cLine);
            auto *name = lv_obj_get_child(cell,0);
            const int nameInset = 36;
            const int roleWidth = active?46:0;
            boundedLabel(name,cellWidth-nameInset-roleWidth-gap);
            lv_obj_align(name,LV_ALIGN_LEFT_MID,nameInset,0);
            snprintf(line,sizeof(line),"%u",unsigned(slot)+1);
            auto *number = makeLabel(cell,line,0,0,UiText::Caption,cDim);
            lv_obj_align(number,LV_ALIGN_LEFT_MID,12,0);
            if (active) {
                auto *role = makeLabel(cell,"Active",0,0,UiText::Caption,cDim);
                lv_obj_align(role,LV_ALIGN_RIGHT_MID,-gap,0);
            }
        }
        return;
    }

    snprintf(line,sizeof(line),"Slot %u: %s",unsigned(modelManagementSlot_)+1,modelNames_[modelManagementSlot_]);
    if (stage == Stage::Name) {
        auto *slot = makeLabel(screen,line,inset,71,UiText::Caption,cDim);
        if (!portraitUi) lv_obj_align(slot,LV_ALIGN_TOP_RIGHT,-inset,76);
        auto *field = makePanel(inset,portraitUi?86:70,portraitUi?width:width/2-18,32,cPanel2,cLine);
        designDetails_[0] = makeLabel(field,modelManagementName_[0]?modelManagementName_:"Enter a name",
            gap,7,UiText::Menu,cWhite);
        boundedLabel(designDetails_[0],lv_obj_get_style_width(field,0)-2*gap);
        const int keyMargin = 5, keyGap = 3;
        const int keyWidth = kScreenWidth-2*keyMargin;
        const int keyTop = portraitUi?122:106;
        const int keyPitch = portraitUi?52:42;
        for (uint8_t row = 0; row < 4; ++row) {
            const char *keys = kWifiKeys[modelManagementKeyboard_][row];
            const int count = strlen(keys);
            const int left = (kScreenWidth-count*keyWidth/10)/2;
            for (int column = 0; column < count; ++column) {
                const char name[2] = {keys[column],0};
                const int x = left+column*keyWidth/10;
                const int right = left+(column+1)*keyWidth/10;
                button(x,keyTop+row*keyPitch,right-x-keyGap,keyPitch-4,name,
                    Control(uint8_t(Control::FirstKey)+row*10+column),UiText::Menu);
            }
        }
        const int actionsY = keyTop+4*keyPitch+2;
        const char *labels[] = {modelManagementKeyboard_==0?"ABC":modelManagementKeyboard_==1?"Symbols":"abc",
            "Space","Delete","Clear",action==Action::Rename?"Save name":"Next"};
        const Control controls[] = {Control::CycleKeyboard,Control::Space,Control::Delete,Control::Clear,Control::NameContinue};
        for (int i = 0; i < 5; ++i) {
            const int x = keyMargin+i*keyWidth/5;
            const int right = keyMargin+(i+1)*keyWidth/5;
            button(x,actionsY,right-x-keyGap,40,labels[i],controls[i],
                portraitUi?UiText::Caption:UiText::Secondary,i==4?cCyan:cWhite);
        }
        return;
    }

    text(line,77,UiText::Menu,cWhite);
    if (action != Action::Reset) {
        snprintf(line,sizeof(line),"New name: %s",modelManagementName_);
        text(line,101,UiText::Secondary,cWhite);
    }
    if (action == Action::Copy) {
        snprintf(line,sizeof(line),"Copy from: %s",modelNames_[activeModelSlot_]);
        text(line,122,UiText::Secondary,cWhite);
    }
    text("Replaces channel tuning, mixes, assignments and Walk limit. Clears ABS verification.",
        151,UiText::Secondary,cAmber);
    text("Keeps the slot's appearance, dashboard, alarms, timer settings and saved records.",
        portraitUi?215:193,UiText::Secondary,cDim);
    if (modelManagementSlot_ == activeModelSlot_) {
        text("Output stays CUT. Check controls before re-arming.",
            portraitUi?281:235,UiText::Secondary,cAmber);
    }
    const int actionsY = kScreenHeight-(portraitUi?73:57);
    const int half = (width-gap)/2;
    const char *apply = action==Action::New?"Create model":
        action==Action::Copy?"Copy controls":"Reset controls";
    button(inset,actionsY,half,42,"Cancel",Control::Cancel,UiText::Menu);
    button(inset+half+gap,actionsY,half,42,apply,Control::Apply,UiText::Menu,cAmber);
}

void TransmitterUi::backModelManagement()
{
    using Stage = ModelManagementStage;
    switch (modelManagementStage_) {
        case Stage::Actions: drawModelSelectPage(); return;
        case Stage::Slot: modelManagementStage_ = Stage::Actions; break;
        case Stage::Name: modelManagementStage_ = Stage::Slot; break;
        case Stage::Confirm:
            modelManagementStage_ = modelManagementAction_==ModelManagementAction::Reset?Stage::Slot:Stage::Name;
            break;
    }
    drawDesignPage(UiPage::ModelManagement);
}

void TransmitterUi::applyModelManagement()
{
    if (modelManagementRevision_ != modelRevision_) {
        snprintf(modelSelectStatus_,sizeof(modelSelectStatus_),"Models changed. Choose the slot again.");
    } else if (manageModelMemory(modelManagementAction_,modelManagementSlot_,modelManagementName_)) {
        modelSelectBank_ = modelManagementSlot_/5;
    }
    drawModelSelectPage();
}

void TransmitterUi::controlModelManagement(uint8_t item, PageControlEvent event)
{
    using Action = ModelManagementAction;
    using Control = ModelManagementControl;
    using Stage = ModelManagementStage;
    if (event != PageControlEvent::Activate) return;
    const auto control = Control(item);
    if (control == Control::Open && activePage_ == UiPage::ModelSelect &&
        !modelAppearanceEditing_ && modelSelectPendingSlot_ >= kModelMemoryCount) {
        if (!modelManagementAllowed()) {
            snprintf(modelSelectStatus_,sizeof(modelSelectStatus_),"CUT and centre throttle to manage models");
            drawModelSelectPage();
        } else {
            modelSelectStatus_[0] = 0;
            modelManagementStage_ = Stage::Actions;
            drawDesignPage(UiPage::ModelManagement);
        }
        return;
    }
    if (activePage_ != UiPage::ModelManagement) return;
    if (control == Control::Cancel) {
        drawModelSelectPage();
        return;
    }
    const auto stage = modelManagementStage_;
    if (stage == Stage::Actions && control >= Control::New && control <= Control::Reset) {
        modelManagementAction_ = Action(item-uint8_t(Control::New));
        modelManagementRevision_ = modelRevision_;
        modelManagementStage_ = Stage::Slot;
    } else if (stage == Stage::Slot && item >= uint8_t(Control::FirstSlot) &&
               item < uint8_t(Control::FirstSlot)+kModelMemoryCount) {
        const uint8_t slot = item-uint8_t(Control::FirstSlot);
        const auto action = modelManagementAction_;
        if (slot==activeModelSlot_ && (action==Action::Copy || action==Action::New)) return;
        modelManagementSlot_ = slot;
        modelManagementKeyboard_ = 1;
        if (action == Action::Copy) {
            snprintf(modelManagementName_,sizeof(modelManagementName_),"%.*s Copy",
                int(sizeof(modelManagementName_)-6),modelNames_[activeModelSlot_]);
        } else if (action == Action::New) {
            snprintf(modelManagementName_,sizeof(modelManagementName_),"Model %u",unsigned(slot)+1);
        } else {
            snprintf(modelManagementName_,sizeof(modelManagementName_),"%s",modelNames_[slot]);
        }
        modelManagementStage_ = action==Action::Reset?Stage::Confirm:Stage::Name;
    } else if (stage == Stage::Name) {
        if (control == Control::NameContinue) {
            if (!validModelBackupName(modelManagementName_)) {
                uiSetText(designDetails_[0],"Enter a name");
                playBuzzer(35);
                return;
            }
            if (modelManagementAction_ == Action::Rename) {
                applyModelManagement();
                return;
            }
            modelManagementStage_ = Stage::Confirm;
        } else if (control == Control::CycleKeyboard) {
            modelManagementKeyboard_ = (modelManagementKeyboard_+1)%3;
        } else {
            const size_t length = strlen(modelManagementName_);
            if (control == Control::Clear) modelManagementName_[0] = 0;
            else if (control == Control::Delete) {
                if (length) modelManagementName_[length-1] = 0;
            } else {
                char key = ' ';
                if (item >= uint8_t(Control::FirstKey) && item < uint8_t(Control::FirstKey)+40) {
                    const unsigned offset = item-uint8_t(Control::FirstKey);
                    const char *keys = kWifiKeys[modelManagementKeyboard_][offset/10];
                    if (offset%10 >= strlen(keys)) return;
                    key = keys[offset%10];
                } else if (control != Control::Space) return;
                if (length+1 >= sizeof(modelManagementName_)) {
                    playBuzzer(35);
                    return;
                }
                modelManagementName_[length] = key;
                modelManagementName_[length+1] = 0;
            }
            uiSetText(designDetails_[0],modelManagementName_[0]?modelManagementName_:"Enter a name");
            playKeyClick();
            return;
        }
    } else if (stage == Stage::Confirm && control == Control::Apply) {
        applyModelManagement();
        return;
    } else return;
    playKeyClick();
    drawDesignPage(UiPage::ModelManagement);
}
