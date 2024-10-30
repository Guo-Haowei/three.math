#include "d3d11_helpers.h"

#include "drivers/d3d11/d3d11_graphics_manager.h"

namespace my::d3d {

void SetDebugName(ID3D11DeviceChild* p_resource, const std::string& p_name) {
    unused(p_resource);
    unused(p_name);
#if USING(DEBUG_BUILD)
    std::wstring w_name(p_name.begin(), p_name.end());

    p_resource->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)w_name.size() * sizeof(wchar_t), w_name.c_str());
#endif
}

}  // namespace my::d3d
