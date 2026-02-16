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
    m_selectedPath.clear();
    m_fileName.clear();
    m_selectedIndex = -1;

    // Set initial path
    if (!defaultPath.empty() && fs::exists(defaultPath))
    {
        m_currentPath = fs::absolute(defaultPath).string();
    }
    else
    {
        m_currentPath = fs::current_path().string();
    }

    refreshDirectory();
}

bool FileDialog::draw()
{
    if (!m_isOpen)
        return false;

    bool fileSelected = false;

    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(m_title.c_str(), &m_isOpen, ImGuiWindowFlags_NoCollapse))
    {
        // Current path display and navigation
        ImGui::Text("Location: %s", m_currentPath.c_str());
        ImGui::Separator();

        // Parent directory button
        if (ImGui::Button(".."))
        {
            fs::path parentPath = fs::path(m_currentPath).parent_path();
            if (fs::exists(parentPath))
            {
                m_currentPath = parentPath.string();
                refreshDirectory();
                m_selectedIndex = -1;
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Refresh"))
        {
            refreshDirectory();
        }

        ImGui::Separator();

        // File/Directory list
        ImGui::BeginChild("FileList", ImVec2(0, -70), true);

        for (int i = 0; i < m_entries.size(); ++i)
        {
            const FileEntry& entry = m_entries[i];

            // Display icon based on type
            const char* icon = entry.isDirectory ? "[DIR]" : "[FILE]";
            std::string label = std::string(icon) + " " + entry.name;

            bool isSelected = (m_selectedIndex == i);
            if (ImGui::Selectable(label.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick))
            {
                m_selectedIndex = i;

                if (entry.isDirectory)
                {
                    // Double-click to enter directory
                    if (ImGui::IsMouseDoubleClicked(0))
                    {
                        m_currentPath = (fs::path(m_currentPath) / entry.name).string();
                        refreshDirectory();
                        m_selectedIndex = -1;
                    }
                }
                else
                {
                    // File selected
                    m_fileName = entry.name;

                    // Double-click to confirm file
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

        // File name input (for save dialog)
        ImGui::Separator();
        ImGui::Text("File name:");
        ImGui::SameLine();

        char fileNameBuffer[256];
        strncpy(fileNameBuffer, m_fileName.c_str(), sizeof(fileNameBuffer) - 1);
        fileNameBuffer[sizeof(fileNameBuffer) - 1] = '\0';

        if (ImGui::InputText("##filename", fileNameBuffer, sizeof(fileNameBuffer)))
        {
            m_fileName = fileNameBuffer;
        }

        // Buttons
        ImGui::Separator();

        bool canConfirm = !m_fileName.empty();

        if (m_type == Type::Save)
        {
            if (ImGui::Button("Save") && canConfirm)
            {
                std::string fullPath = (fs::path(m_currentPath) / m_fileName).string();

                // Add extension if not present
                if (!m_extensionFilter.empty() &&
                    !fullPath.ends_with(m_extensionFilter))
                {
                    fullPath += m_extensionFilter;
                }

                m_selectedPath = fullPath;
                fileSelected = true;
                m_isOpen = false;
            }
        }
        else // Type::Open
        {
            if (ImGui::Button("Open") && canConfirm)
            {
                m_selectedPath = (fs::path(m_currentPath) / m_fileName).string();
                fileSelected = true;
                m_isOpen = false;
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            m_isOpen = false;
        }
    }
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
            std::string filename = entry.path().filename().string();

            // Skip hidden files (starting with .)
            if (filename[0] == '.')
                continue;

            FileEntry fileEntry;
            fileEntry.name = filename;
            fileEntry.isDirectory = entry.is_directory();

            // Filter files by extension if needed
            if (!fileEntry.isDirectory && !m_extensionFilter.empty())
            {
                if (!matchesFilter(filename))
                    continue;
            }

            m_entries.push_back(fileEntry);
        }

        // Sort: directories first, then files, both alphabetically
        std::sort(m_entries.begin(), m_entries.end(),
            [](const FileEntry& a, const FileEntry& b)
            {
                if (a.isDirectory != b.isDirectory)
                    return a.isDirectory; // Directories first
                return a.name < b.name;  // Alphabetically
            });
    }
    catch (const fs::filesystem_error& e)
    {
        LOG("FileDialog: Error reading directory: %s", e.what());
    }
}

bool FileDialog::matchesFilter(const std::string& filename) const
{
    if (m_extensionFilter.empty())
        return true;

    // Check if filename ends with the extension
    if (filename.length() >= m_extensionFilter.length())
    {
        return filename.compare(
            filename.length() - m_extensionFilter.length(),
            m_extensionFilter.length(),
            m_extensionFilter) == 0;
    }

    return false;
}