#pragma once
#include <ole2.h>
#include <shellapi.h>

#include "DragDropManager.h"

#include <filesystem>
#include <vector>

class EngineDropTarget : public IDropTarget {
public:
    EngineDropTarget() = default;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

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
    bool m_hasFiles = false;

    bool tryExtractItems(IDataObject* pDataObj,
                         std::vector<DragDropManager::DropItem>& outItems) const;
};
