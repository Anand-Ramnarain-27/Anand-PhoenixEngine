#pragma once
// Must be included AFTER windows.h (i.e. after Globals.h PCH in .cpp files).
#include <ole2.h>
#include <shellapi.h>

#include "DragDropManager.h"  // for DragDropManager::DropItem

#include <filesystem>
#include <vector>

// OLE IDropTarget implementation.
// DragEnter / DragOver / DragLeave update the DragDropManager hover state so
// ModuleEditor can draw a real-time overlay.  Drop expands any dropped
// folders, filters by supported extension, and hands the list to
// DragDropManager::QueueFiles for background processing.
//
// COM lifecycle: created by ModuleEditor::init() with refcount=1.
// RegisterDragDrop adds another reference; RevokeDragDrop releases it.
// ModuleEditor::cleanUp() calls Release() to drop the last reference.
class EngineDropTarget : public IDropTarget {
public:
    EngineDropTarget() = default;

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) override;
    ULONG   STDMETHODCALLTYPE AddRef()  override;
    ULONG   STDMETHODCALLTYPE Release() override;

    // IDropTarget
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* pDataObj,
                                        DWORD grfKeyState,
                                        POINTL pt,
                                        DWORD* pdwEffect) override;

    HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState,
                                       POINTL pt,
                                       DWORD* pdwEffect) override;

    HRESULT STDMETHODCALLTYPE DragLeave() override;

    HRESULT STDMETHODCALLTYPE Drop(IDataObject* pDataObj,
                                   DWORD grfKeyState,
                                   POINTL pt,
                                   DWORD* pdwEffect) override;

private:
    ULONG m_refCount = 1;
    bool  m_hasFiles = false;  // whether the dragged object contains CF_HDROP data

    // Extracts drop items from a CF_HDROP IDataObject.
    // Directories are passed through as DropItem{path, isFolder=true} WITHOUT
    // expansion — the worker thread copies the whole tree.  Individual files are
    // filtered by supported extension.
    bool tryExtractItems(IDataObject* pDataObj,
                         std::vector<DragDropManager::DropItem>& outItems) const;
};
