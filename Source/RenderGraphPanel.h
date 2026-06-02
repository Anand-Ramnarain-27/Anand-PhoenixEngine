#pragma once
#include "EditorPanel.h"
#include <vector>
#include <string>

class RenderGraphPanel : public EditorPanel {
public:
    explicit RenderGraphPanel(ModuleEditor* editor);
    const char* getName() const override { return "Render Graph"; }

protected:
    void drawContent() override;
    bool noPadding() const override { return true; }

private:
    struct Port {
        std::string  name;
        bool         isOutput = false;
        ImVec4       color;
    };

    struct PassNode {
        int         id;
        std::string name;
        std::string badge;  // "GFX" or "COMP"
        ImVec4      dotColor;
        float       gpuMs = 0.f;
        std::vector<Port> ports;
    };

    struct Wire {
        int fromPass;  int fromPort;
        int toPass;    int toPort;
        ImVec4 color;
    };

    std::vector<PassNode> m_passes;
    std::vector<Wire>     m_wires;
    int                   m_selectedPass = -1;

    static constexpr float kNodeW   = 184.f;
    static constexpr float kNodeGap = 54.f;
    static constexpr float kNodeH   = 96.f;

    void buildPassData();
    void drawNode(const PassNode& node, ImVec2 origin, ImDrawList* dl, bool selected);
    void drawWires(ImDrawList* dl, ImVec2 graphOrigin);
    void drawDetailStrip(float stripY, float stripH);

    ImVec4 msColor(float ms) const;
};
