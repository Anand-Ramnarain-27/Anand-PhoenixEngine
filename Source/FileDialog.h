#pragma once

#include <string>
#include <vector>

class FileDialog
{
public:
    enum class Type { Open, Save };

    FileDialog() = default;
    ~FileDialog() = default;

    void open(Type type, const std::string& title, const std::string& defaultPath = "");
    bool draw();

    bool               isOpen()           const { return m_isOpen; }
    const std::string& getSelectedPath()  const { return m_selectedPath; }
    void setExtensionFilter(const std::string& ext) { m_extensionFilter = ext; }

private:
    void refreshDirectory();
    bool matchesFilter(const std::string& filename) const;

    struct FileEntry { std::string name; bool isDirectory; };

    bool        m_isOpen = false;
    Type        m_type = Type::Open;
    int         m_selectedIndex = -1;

    std::string m_title;
    std::string m_currentPath;
    std::string m_selectedPath;
    std::string m_fileName;
    std::string m_extensionFilter;

    std::vector<FileEntry> m_entries;
};