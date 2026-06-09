#include "Globals.h"
#include "EngineDropTarget.h"
#include "DragDropManager.h"

#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// IUnknown
// ---------------------------------------------------------------------------
HRESULT STDMETHODCALLTYPE EngineDropTarget::QueryInterface(REFIID riid, void** ppvObj){
    if (!ppvObj) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IDropTarget) {
        *ppvObj = static_cast<IDropTarget*>(this);
        AddRef();
        return S_OK;
    }
    *ppvObj = nullptr;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE EngineDropTarget::AddRef(){
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE EngineDropTarget::Release(){
    ULONG ref = --m_refCount;
    if (ref == 0) delete this;
    return ref;
}

// ---------------------------------------------------------------------------
// IDropTarget
// ---------------------------------------------------------------------------
HRESULT STDMETHODCALLTYPE EngineDropTarget::DragEnter(
    IDataObject* pDataObj, DWORD /*grfKeyState*/, POINTL /*pt*/, DWORD* pdwEffect){
    // Check whether the dragged data contains files at all
    FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    m_hasFiles = (pDataObj && SUCCEEDED(pDataObj->QueryGetData(&fmt)));
    *pdwEffect = m_hasFiles ? DROPEFFECT_COPY : DROPEFFECT_NONE;
    DragDropManager::Get().SetDragging(m_hasFiles);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE EngineDropTarget::DragOver(
    DWORD /*grfKeyState*/, POINTL /*pt*/, DWORD* pdwEffect){
    *pdwEffect = m_hasFiles ? DROPEFFECT_COPY : DROPEFFECT_NONE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE EngineDropTarget::DragLeave(){
    m_hasFiles = false;
    DragDropManager::Get().SetDragging(false);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE EngineDropTarget::Drop(
    IDataObject* pDataObj, DWORD /*grfKeyState*/, POINTL /*pt*/, DWORD* pdwEffect){
    DragDropManager::Get().SetDragging(false);
    m_hasFiles = false;
    *pdwEffect = DROPEFFECT_COPY;

    std::vector<DragDropManager::DropItem> items;
    if (pDataObj && tryExtractItems(pDataObj, items)) {
        DragDropManager::Get().QueueItems(std::move(items));
    }
    return S_OK;
}

// ---------------------------------------------------------------------------
// Item extraction — directories are NOT expanded here; the worker copies them.
// ---------------------------------------------------------------------------
bool EngineDropTarget::tryExtractItems(IDataObject* pDataObj,
                                       std::vector<DragDropManager::DropItem>& outItems) const{
    FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM stg = {};
    if (FAILED(pDataObj->GetData(&fmt, &stg))) return false;

    HDROP hDrop = static_cast<HDROP>(GlobalLock(stg.hGlobal));
    if (!hDrop) { ReleaseStgMedium(&stg); return false; }

    UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
    for (UINT i = 0; i < count; ++i) {
        WCHAR buf[MAX_PATH];
        if (!DragQueryFileW(hDrop, i, buf, MAX_PATH)) continue;

        fs::path p(buf);
        std::error_code ec;

        if (fs::is_directory(p, ec)) {
            // Pass folder through intact — the worker copies the full tree so
            // .gltf/.bin/texture relative paths stay correct.
            outItems.push_back({ p, true });
        } else if (fs::is_regular_file(p, ec)) {
            std::string ext = p.extension().string();
            for (char& c : ext) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
            if (DragDropManager::IsSupportedExtension(ext)) {
                outItems.push_back({ p, false });
            } else {
                LOG("DragDrop: Skipping unsupported extension '%s': %s",
                    ext.c_str(), p.filename().string().c_str());
            }
        }
    }

    GlobalUnlock(stg.hGlobal);
    ReleaseStgMedium(&stg);
    return !outItems.empty();
}
