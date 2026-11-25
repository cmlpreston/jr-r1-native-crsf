// Small shared shell for transient live feedback. Callers own the content and
// lifetime; this helper never changes page focus, input state or output safety.
namespace LiveFeedback {
constexpr int Height=112, Margin=24, Inset=12, RailInset=18, RailY=70, DetailY=88;
struct Panel {
    lv_obj_t *object=nullptr,*title=nullptr,*value=nullptr,*caption=nullptr;
    int width=0;
};
Panel create(const char *title,const char *value,const char *caption) {
    Panel result;result.width=kScreenWidth-2*Margin;
    result.object=makePanel(Margin,(kScreenHeight-Height)/2,result.width,Height,cPanel2,cCyan);
    result.title=makeLabel(result.object,title,Inset,6,UiText::Secondary,cWhite);
    lv_obj_set_width(result.title,result.width-(value?104:2*Inset));
    lv_label_set_long_mode(result.title,LV_LABEL_LONG_CLIP);
    if(value){
        result.value=makeLabel(result.object,value,result.width-80,6,UiText::Secondary,cCyan);
        lv_obj_set_width(result.value,68);
        lv_obj_set_style_text_align(result.value,LV_TEXT_ALIGN_RIGHT,0);
    }
    result.caption=makeLabel(result.object,caption,Inset,29,UiText::Caption,cDim);
    lv_obj_set_width(result.caption,result.width-2*Inset);
    lv_label_set_long_mode(result.caption,LV_LABEL_LONG_CLIP);
    return result;
}
lv_obj_t *detail(const Panel &panel,const char *text) {
    auto label=makeLabel(panel.object,text,Inset,DetailY,UiText::Caption,cWhite);
    lv_obj_set_width(label,panel.width-2*Inset);
    lv_label_set_long_mode(label,LV_LABEL_LONG_CLIP);
    return label;
}
void makePassive(lv_obj_t *object) {
    lv_obj_clear_flag(object,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICK_FOCUSABLE);
    for(uint32_t n=0;n<lv_obj_get_child_cnt(object);++n)makePassive(lv_obj_get_child(object,n));
}
void finish(const Panel &panel) {
    makePassive(panel.object);
    lv_obj_move_foreground(panel.object);
}
}
