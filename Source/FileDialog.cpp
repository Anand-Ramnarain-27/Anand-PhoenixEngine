#include "Globals.h"
#include "FileDialog.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include <imgui.h>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

void FileDialog::open(Type type, const std::string& title, const std::string& defaultPath)
{
    m_isOpen = true;
    m_type = type;
    m_title = title;
    m_selectedIndex = -1;
    m_selectedPath.clear();
    m_fileName.clear();

    m_currentPath = (!defaultPath.empty() && fs::exists(defaultPath))
        ? fs::absolute(defaultPath).string()
        : fs::current_path().string();

    refreshDirectory();
}

bool FileDialog::draw()
{
    if (!m_isOpen) return false;

    bool fileSelected = false;

    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(m_title.c_str(), &m_isOpen, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return false;
    }

    ImGui::Text("Location: %s", m_currentPath.c_str());
    ImGui::Separator();

    if (ImGui::Button(".."))
    {
        fs::path parent = fs::path(m_currentPath).parent_path();
        if (fs::exists(parent))
        {
            m_currentPath = parent.string();
            m_selectedIndex = -1;
            refreshDirectory();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) refreshDirectory();
    ImGui::Separator();

    ImGui::BeginChild("FileList", ImVec2(0, -70), true);
    for (int i = 0; i < (int)m_entries.size(); ++i)
    {
        const FileEntry& entry = m_entries[i];
        std::string label = (entry.isDirectory ? "[DIR]  " : "[FILE] ") + entry.name;
        bool selected = (m_selectedIndex == i);

        if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick))
        {
            m_selectedIndex = i;
            if (entry.isDirectory)
            {
                if (ImGui::IsMouseDoubleClicked(0))
                {
                    m_currentPath = (fs::path(m_currentPath) / entry.name).string();
                    m_selectedIndex = -1;
                    refreshDirectory();
                }
            }
            else
            {
                m_fileName = entry.name;
                if (ImGui::IsMouseDoubleClicked(0))
                {
                    m_selectedPath = (fs::path(m_currentPath) / entry.name).string();
                    fileSelected = true;
                    m_isOpen = false;
                }
            }
        }
    }
    ImGui::EndChild();

    ImGui::Separator();
    ImGui::Text("File name:"); ImGui::SameLine();

    char buf[256];
    strncpy(buf, m_fileName.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    if (ImGui::InputText("##filename", buf, sizeof(buf)))
        m_fileName = buf;

    ImGui::Separator();

    if (!m_fileName.empty())
    {
        if (m_type == Type::Save)
        {
            if (ImGui::Button("Save"))
            {
                std::string path = (fs::path(m_currentPath) / m_fileName).string();
                if (!m_extensionFilter.empty() && !path.ends_with(m_extensionFilter))
                    path += m_extensionFilter;
                m_selectedPath = path;
                fileSelected = true;
                m_isOpen = false;
            }
        }
        else
        {
            if (ImGui::Button("Open"))
            {
                m_selectedPath = (fs::path(m_currentPath) / m_fileName).string();
                fileSelected = true;
                m_isOpen = false;
            }
        }
        ImGui::SameLine();
    }

    if (ImGui::Button("Cancel")) m_isOpen = false;

    ImGui::End();
    return fileSelected;
}

void FileDialog::refreshDirectory()
{
    m_entries.clear();

    try
    {
        if (!fs::exists(m_currentPath))
        {
            LOG("FileDialog: Directory does not exist: %s", m_currentPath.c_str());
            return;
        }

        for (const auto& entry : fs::directory_iterator(m_currentPath))
        {
            std::string name = entry.path().filename().string();
            if (name[0] == '.') continue;

            bool isDir = entry.is_directory();
            if (!isDir && !m_extensionFilter.empty() && !matchesFilter(name)) continue;

            m_entries.push_back({ name, isDir });
        }

        std::sort(m_entries.begin(), m_entries.end(), [](const FileEntry& a, const FileEntry& b)
            {
                if (a.isDirectory != b.isDirectory) return a.isDirectory;
                return a.name < b.name;
            });
    }
    catch (const fs::filesystem_error& e)
    {
        LOG("FileDialog: Error reading directory: %s", e.what());
    }
}

bool FileDialog::matchesFilter(const std::string& filename) const
{
    if (m_extensionFilter.empty() || filename.length() < m_extensionFilter.length())
        return m_extensionFilter.empty();

    return filename.compare(filename.length() - m_extensionFilter.length(),
        m_extensionFilter.length(), m_extensionFilter) == 0;
}