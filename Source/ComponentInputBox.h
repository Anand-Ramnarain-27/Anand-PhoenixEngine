#pragma once
#include "ComponentSelectable.h"
#include <algorithm>
#include <cctype>
#include <string>

// A single-line text field. It is editing whenever it has focus (click it, or Tab to it): typed characters go
// in at the caret, Backspace/Delete/Left/Right/Home/End edit, Ctrl+V pastes, Enter submits, Escape drops focus.
// Shift+click, drag, Shift+Left/Right and Shift+Home/End select a range; Ctrl+A selects all; typing, Backspace
// or Delete over a selection replaces/removes it; Ctrl+C/Ctrl+X copy or cut it.
//
// Text is printable ASCII for now (the baked UI font covers ASCII/Latin-1 and the caret works per character).
// Text wider than the box scrolls with the caret and is clipped to the box.
class ComponentInputBox : public ComponentSelectable {
public:
    enum class ContentType {
        Standard = 0,       // any printable character
        Integer = 1,        // digits, and a leading '-'
        Decimal = 2,        // digits, one '.', and a leading '-'
        Alphanumeric = 3,   // letters and digits
    };

    explicit ComponentInputBox(GameObject* owner) : ComponentSelectable(owner){}

    void onEditor() override;
    void onSave(std::string& outJson) const override;
    void onLoad(const std::string& json) override;
    Type getType() const override { return Type::InputBox; }

    // ---- authoring ----
    std::string text;
    std::string placeholder = "Enter text...";
    int maxLength = 64;            // 0 = unlimited
    ContentType contentType = ContentType::Standard;
    bool password = false;         // shows '*' instead of the text

    std::string fontName = "UIFont";
    float fontSize = 30.f;         // line height in canvas units
    Vector2 padding = Vector2(12.f, 6.f);

    Vector4 backgroundColor = Vector4(0.10f, 0.11f, 0.16f, 1.f);
    Vector4 focusColor = Vector4(0.35f, 0.6f, 1.f, 1.f);        // outline while focused
    Vector4 textColor = Vector4(1.f, 1.f, 1.f, 1.f);
    Vector4 placeholderColor = Vector4(0.55f, 0.58f, 0.65f, 1.f);
    Vector4 caretColor = Vector4(1.f, 1.f, 1.f, 1.f);
    Vector4 selectionColor = Vector4(0.3f, 0.5f, 0.9f, 0.45f);

    // ---- runtime state (written by ModuleUI) ----
    int caret = 0;                 // insertion index, 0..text.size()
    int selectionStart = -1;       // the other end of the selection; -1 = none. Caret is the active end.
    float scrollX = 0.f;           // how far the text is scrolled left, in canvas units
    double blinkStart = 0.0;       // caret blink restarts on every edit

    // ---- events ----
    // Raised when the user edits the text (not when game code calls setText).
    UIEvent<std::string> onValueChanged;
    // Raised when the user presses Enter.
    UIEvent<std::string> onSubmit;

    void clearListeners() override {
        ComponentSelectable::clearListeners();
        onValueChanged.clear();
        onSubmit.clear();
    }

    // ---- editing ----
    bool accepts(char c, int index) const {
        if (c < 32 || c > 126) return false;
        const bool digit = c >= '0' && c <= '9';
        switch (contentType){
        case ContentType::Integer:      return digit || (c == '-' && index == 0 && text.find('-') == std::string::npos);
        case ContentType::Decimal:      return digit || (c == '.' && text.find('.') == std::string::npos) ||
                                               (c == '-' && index == 0 && text.find('-') == std::string::npos);
        case ContentType::Alphanumeric: return std::isalnum(static_cast<unsigned char>(c)) != 0;
        default:                        return true;
        }
    }

    // ---- selection ----
    bool hasSelection() const { return selectionStart >= 0 && selectionStart != caret; }
    int selectionLo() const { return std::min(selectionStart, caret); }
    int selectionHi() const { return std::max(selectionStart, caret); }
    void clearSelection(){ selectionStart = -1; }
    void selectAll(){ selectionStart = 0; caret = (int)text.size(); }

    std::string selectedText() const { return hasSelection() ? text.substr(selectionLo(), selectionHi() - selectionLo()) : std::string(); }

    // Removes the selected range, if any. Caret ends up at the deletion point. Returns true if it deleted anything.
    bool eraseSelection(){
        if (!hasSelection()) return false;
        const int lo = selectionLo(), hi = selectionHi();
        text.erase(text.begin() + lo, text.begin() + hi);
        caret = lo;
        clearSelection();
        return true;
    }

    // Inserts the accepted characters of `s` at the caret, replacing any selection first. Returns true if
    // anything changed. `s` is empty on most frames (ModuleUI calls this unconditionally with whatever was
    // typed that frame), so bail out before touching the selection rather than silently clearing it every
    // frame a selection happens to be active for some other reason (e.g. mid-drag).
    bool insertText(const std::string& s){
        if (s.empty()) return false;
        bool changed = eraseSelection();
        for (char c : s){
            if (maxLength > 0 && (int)text.size() >= maxLength) break;
            if (!accepts(c, caret)) continue;
            text.insert(text.begin() + caret, c);
            ++caret;
            changed = true;
        }
        return changed;
    }

    // Backspace: removes the selection if there is one, otherwise the character before the caret.
    bool eraseBefore(){
        if (eraseSelection()) return true;
        if (caret <= 0) return false;
        text.erase(text.begin() + (caret - 1));
        --caret;
        return true;
    }

    // Delete: removes the selection if there is one, otherwise the character after the caret.
    bool eraseAfter(){
        if (eraseSelection()) return true;
        if (caret >= (int)text.size()) return false;
        text.erase(text.begin() + caret);
        return true;
    }

    // `extend` (Shift held, or an in-progress drag) grows or shrinks the selection from wherever it already
    // started; otherwise the caret just moves and any selection collapses.
    void moveCaret(int delta, bool extend = false){ setCaret(caret + delta, extend); }

    void setCaret(int index, bool extend = false){
        if (extend){ if (selectionStart < 0) selectionStart = caret; }
        else clearSelection();
        caret = std::clamp(index, 0, (int)text.size());
    }

    // Replaces the text (filtered and length-limited like typed input) and puts the caret at the end.
    void setText(const std::string& s){
        text.clear();
        caret = 0;
        clearSelection();
        insertText(s);
    }

    // What is drawn: the text, or one '*' per character for a password.
    std::string displayText() const { return password ? std::string(text.size(), '*') : text; }
};
