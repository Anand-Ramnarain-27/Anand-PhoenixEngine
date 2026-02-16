#pragma once

#include <string>
#include <vector>
#include <functional>

// Simple File Dialog using ImGui
// Allows browsing directories and selecting files for save/load operations
class FileDialog
{
public:
    enum class Type
    {
        Open,   // Select existing file
        Save    // Save to new or existing file
    };

    FileDialog() = default;
    ~FileDialog() = default;

    // Open the dialog
    void open(Type type, const std::string& title, const std::string& defaultPath = "");

    // Draw the dialog (call every frame)
    // Returns true if file was selected
    bool draw();

    // Check if dialog is currently open
    bool isOpen() const { return m_isOpen; }

    // Get the selected file path (after dialog closes with selection)
    const std::string& getSelectedPath() const { return m_selectedPath; }

    // Set file extension filter (e.g., ".json", ".scene")
    void setExtensionFilter(const std::string& extension) { m_extensionFilter = extension; }

private:
    void refreshDirectory();
    bool matchesFilter(const std::string& filename) const;

private:
    bool m_isOpen = false;
    Type m_type = Type::Open;
    std::string m_title;
    std::string m_currentPath;
    std::string m_selectedPath;
    std::string m_fileName;
    std::string m_extensionFilter;

    struct FileEntry
    {
        std::string name;
        bool isDirectory;
    };

    std::vector<FileEntry> m_entries;
    int m_selectedIndex = -1;
};