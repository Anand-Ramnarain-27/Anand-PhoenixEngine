#pragma once
#include "ComponentSelectable.h"
#include <algorithm>
#include <cctype>
#include <string>

// A single-line text field. It is editing whenever it has focus (click it, or Tab to it): typed characters go
// in at the caret, Backspace/Delete/Left/Right/Home/End edit, Ctrl+V pastes, Enter submits, Escape drops focus.
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

    // ---- runtime state (written by ModuleUI) ----
    int caret = 0;                 // insertion index, 0..text.size()
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

    // Inserts the accepted characters of `s` at the caret. Returns true if anything was inserted.
    bool insertText(const std::string& s){
        bool changed = false;
        for (char c : s){
            if (maxLength > 0 && (int)text.size() >= maxLength) break;
            if (!accepts(c, caret)) continue;
            text.insert(text.begin() + caret, c);
            ++caret;
            changed = true;
        }
        return changed;
    }

    bool eraseBefore(){
        if (caret <= 0) return false;
        text.erase(text.begin() + (caret - 1));
        --caret;
        return true;
    }

    bool eraseAfter(){
        if (caret >= (int)text.size()) return false;
        text.erase(text.begin() + caret);
        return true;
    }

    void moveCaret(int delta){ caret = std::clamp(caret + delta, 0, (int)text.size()); }
    void setCaret(int index){ caret = std::clamp(index, 0, (int)text.size()); }

    // Replaces the text (filtered and length-limited like typed input) and puts the caret at the end.
    void setText(const std::string& s){
        text.clear();
        caret = 0;
        insertText(s);
    }

    // What is drawn: the text, or one '*' per character for a password.
    std::string displayText() const { return password ? std::string(text.size(), '*') : text; }
};
