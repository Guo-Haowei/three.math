#ifdef D3D11_RESOURCES_INCLUDED
#error DO NOT INCLUDE THIS FILE IN HEADER
#endif
#define D3D11_RESOURCES_INCLUDED

#include <d3d11.h>
#include <wrl/client.h>

#include "rendering/gpu_resource.h"
#include "rendering/render_graph/draw_pass.h"

namespace my {

struct D3d11Texture : public GpuTexture {
    using GpuTexture::GpuTexture;

    uint64_t GetResidentHandle() const override { return 0; }
    uint64_t GetHandle() const override { return (uint64_t)srv.Get(); }

    Microsoft::WRL::ComPtr<ID3D11Resource> texture;

    // @TODO: refactor render target and texture entirely
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    // @TODO: this shouldn't be here
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
};

struct D3d11DrawPass : public DrawPass {
    std::vector<Microsoft::WRL::ComPtr<ID3D11RenderTargetView>> rtvs;
    std::vector<Microsoft::WRL::ComPtr<ID3D11DepthStencilView>> dsvs;
};

struct D3d11UniformBuffer : public ConstantBufferBase {
    using ConstantBufferBase::ConstantBufferBase;

    Microsoft::WRL::ComPtr<ID3D11Buffer> internalBuffer;
    mutable const char* data;
};

struct D3d11StructuredBuffer : GpuStructuredBuffer {
    using GpuStructuredBuffer::GpuStructuredBuffer;

    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
};

}  // namespace my