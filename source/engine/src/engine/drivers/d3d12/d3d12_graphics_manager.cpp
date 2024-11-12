#include "d3d12_graphics_manager.h"

#include <imgui/backends/imgui_impl_dx12.h>

#include "drivers/d3d12/d3d12_pipeline_state_manager.h"
#include "drivers/d3d_common/d3d_common.h"
#include "drivers/windows/win32_display_manager.h"

namespace my {

// @TODO: move to a common place
static constexpr DXGI_FORMAT SURFACE_FORMAT = DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;
static constexpr DXGI_FORMAT DEFAULT_DEPTH_STENCIL_FORMAT = DXGI_FORMAT_D32_FLOAT;

using Microsoft::WRL::ComPtr;

D3d12GraphicsManager::D3d12GraphicsManager() : EmptyGraphicsManager("D3d12GraphicsManager", Backend::D3D12) {
    m_pipelineStateManager = std::make_shared<D3d12PipelineStateManager>();
}

bool D3d12GraphicsManager::InitializeImpl() {
    bool ok = true;

    auto [w, h] = DisplayManager::GetSingleton().GetWindowSize();
    DEV_ASSERT(w > 0 && h > 0);

    ok = ok && CreateDevice();

    // @TODO: refactor
    bool m_enable_debug_layer = true;
    if (m_enable_debug_layer) {
        if (EnableDebugLayer()) {
            LOG("[GraphicsManager_DX12] Debug layer enabled");
        } else {
            LOG_ERROR("[GraphicsManager_DX12] Debug layer not enabled");
        }
    }

    ok = ok && CreateDescriptorHeaps();
    ok = ok && m_graphicsContext.initialize(this);
    ok = ok && m_copyContext.initialize(this);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvDescHeap.m_startCPU;
    for (int i = 0; i < NUM_BACK_BUFFERS; i++) {
        m_renderTargetDescriptor[i] = rtvHandle;
        rtvHandle.ptr += m_rtvDescHeap.m_incrementSize;
    }

    m_depthStencilDescriptor = m_dsvDescHeap.m_startCPU;

    ok = ok && CreateSwapChain(w, h);
    ok = ok && CreateRenderTarget(w, h);

    if (!ok) {
        // auto err = res.error();
        // report_error_impl(err.function, err.filepath, err.line, err.get_message());
        // return ERR_CANT_CREATE;
        return false;
    }

    LoadAssets();

    // Create debug buffer.
    {
        size_t bufferSize = sizeof(vec4) * 1000;  // hard code
        auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
        D3D_CALL(m_device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_debugVertexData)));
    }
    {
        size_t bufferSize = sizeof(uint32_t) * 3000;  // hard code
        auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
        D3D_CALL(m_device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_debugIndexData)));
    }

    ImGui_ImplDX12_Init(m_device.Get(),
                        NUM_FRAMES_IN_FLIGHT,
                        DXGI_FORMAT_R8G8B8A8_UNORM,
                        m_srvDescHeap.m_heap.Get(),
                        m_srvDescHeap.m_startCPU,
                        m_srvDescHeap.m_startGPU);

    ImGui_ImplDX12_NewFrame();

    SelectRenderGraph();
    return ok;
}

void D3d12GraphicsManager::Finalize() {
}

void D3d12GraphicsManager::Render() {
    // @TODO: refactor
    BeginFrame();

    ID3D12GraphicsCommandList* cmdList = m_graphicsContext.m_commandList.Get();
    // CommandList cmd = 0;
    // FrameContext* frameContext = m_currentFrameContext;
    // DEV_ASSERT(draw_data.batchConstants.size() <= 3000);
    // frameContext->perFrameBuffer->CopyData(&draw_data.frameCB, sizeof(PerFrameConstants));
    // frameContext->perBatchBuffer->CopyData(draw_data.batchConstants.data(), sizeof(PerBatchConstants) * draw_data.batchConstants.size());

    auto [width, height] = DisplayManager::GetSingleton().GetWindowSize();
    D3D12_RECT sissorRect{};
    sissorRect.left = 0;
    sissorRect.top = 0;
    sissorRect.right = width;
    sissorRect.bottom = height;

    D3D12_VIEWPORT vp{};
    vp.Width = float(width);
    vp.Height = float(height);
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;

    cmdList->RSSetViewports(1, &vp);
    cmdList->RSSetScissorRects(1, &sissorRect);

    // bind the frame buffer
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_depthStencilDescriptor;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_renderTargetDescriptor[m_backbufferIndex];

    // transfer resource state
    auto barriers = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_backbufferIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmdList->ResourceBarrier(1, &barriers);

    // set frame buffers
    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    // clear the back buffer to a deep blue
    constexpr float kGreenClearColor[4] = { 0.3f, 0.4f, 0.3f, 1.0f };
    cmdList->ClearRenderTargetView(rtvHandle, kGreenClearColor, 0, nullptr);
    cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    ID3D12DescriptorHeap* descriptorHeaps[] = { m_srvDescHeap.m_heap.Get() };

    cmdList->SetDescriptorHeaps(array_length(descriptorHeaps), descriptorHeaps);

    cmdList->SetGraphicsRootSignature(m_rootSignature.Get());

    // ID3D12Resource* perFrameBuffer = frameContext->perFrameBuffer->Resource();
    // ID3D12Resource* perBatchBuffer = frameContext->perBatchBuffer->Resource();
    // ID3D12Resource* boneConstantBuffer = frameContext->boneConstants->Resource();

    // cmdList->SetGraphicsRootConstantBufferView(1, perFrameBuffer->GetGPUVirtualAddress());
#if 0
    // draw objects
    for (size_t i = 0; i < draw_data.geometries.size(); ++i) {
        uint32_t objectIdx = draw_data.geometries[i];
        const ObjectComponent& obj = scene.get_component_array<ObjectComponent>()[objectIdx];
        const MeshComponent& meshComponent = *scene.get_component<MeshComponent>(obj.meshID);
        const MeshGeometry* geometry = reinterpret_cast<const MeshGeometry*>(meshComponent.gpuResource);

        auto CreateVBV = [&](const MeshComponent::VertexAttribute& attrib) {
            D3D12_VERTEX_BUFFER_VIEW vbv;
            vbv.BufferLocation = geometry->VertexBufferGPU->GetGPUVirtualAddress() + attrib.offsetInByte;
            vbv.SizeInBytes = attrib.sizeInByte;
            vbv.StrideInBytes = attrib.stride;
            return vbv;
        };

        D3D12_VERTEX_BUFFER_VIEW vbvs[] = {
            CreateVBV(meshComponent.attributes[MeshComponent::VertexAttribute::POSITION]),
            CreateVBV(meshComponent.attributes[MeshComponent::VertexAttribute::TEXCOORD_0]),
            CreateVBV(meshComponent.attributes[MeshComponent::VertexAttribute::NORMAL]),
            CreateVBV(meshComponent.attributes[MeshComponent::VertexAttribute::JOINTS_0]),
            CreateVBV(meshComponent.attributes[MeshComponent::VertexAttribute::WEIGHTS_0]),
        };

        D3D12_INDEX_BUFFER_VIEW ibv;
        ibv.BufferLocation = geometry->IndexBufferGPU->GetGPUVirtualAddress();
        ibv.Format = DXGI_FORMAT_R32_UINT;
        ibv.SizeInBytes = sizeof(uint32_t) * (uint32_t)meshComponent.indices.size();

        uint32_t numVertices = 3;

        if (meshComponent.armatureID.is_valid())
        {
            // upload bone matrices
            auto& armature = *scene.get_component<ArmatureComponent>(meshComponent.armatureID);

            frameContext->boneConstants->CopyData(armature.boneTransforms.data(), sizeof(mat4) * armature.boneTransforms.size());
            cmdList->SetGraphicsRootConstantBufferView(3, boneConstantBuffer->GetGPUVirtualAddress());

            SetPipelineState(g_boneMeshPSO, cmd);
            numVertices = 5;
        }
        else
        {
            SetPipelineState(g_meshPSO, cmd);
        }

        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmdList->IASetVertexBuffers(0, array_length(vbvs), vbvs);
        cmdList->IASetIndexBuffer(&ibv);

        D3D12_GPU_VIRTUAL_ADDRESS batchAddress = perBatchBuffer->GetGPUVirtualAddress();
        batchAddress += i * sizeof(PerBatchConstants);
        cmdList->SetGraphicsRootConstantBufferView(2, batchAddress);

        for (const MeshComponent::MeshSubset& subset : meshComponent.subsets)
        {
            // texture
            const MaterialComponent* mat = scene.get_component<MaterialComponent>(subset.materialID);
            DEV_ASSERT(mat);
            ImageHandle handle = nullptr;
            handle = mat->textures[MaterialComponent::BASE_COLOR_MAP].image;
            if (!handle)
            {
                // @TODO: cache it somewhere
                handle = g_assetManager->LoadImageSync("DefaultWhite");
            }

            D3D12_GPU_DESCRIPTOR_HANDLE tex{ handle->gpuHandle };
            cmdList->SetGraphicsRootDescriptorTable(0, tex);

            cmdList->DrawIndexedInstanced(subset.indexCount, 1, subset.indexOffset, 0, 0);
        }
    }

    // draw lines
    const uint32_t pointCount = static_cast<uint32_t>(draw_data.debugData.m_indices.size());
    if (pointCount)
    {
        DEV_ASSERT(pointCount % 2 == 0);

        D3D12_VERTEX_BUFFER_VIEW vbv;
        vbv.BufferLocation = m_debugVertexData->GetGPUVirtualAddress();
        vbv.StrideInBytes = static_cast<uint32_t>(sizeof(PosColor));
        vbv.SizeInBytes = SizeOfVector(draw_data.debugData.m_vertices);
        {
            UINT8* pVertexDataBegin;
            CD3DX12_RANGE readRange(0, 0);
            D3D_CALL(m_debugVertexData->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
            memcpy(pVertexDataBegin, draw_data.debugData.m_vertices.data(), vbv.SizeInBytes);
            m_debugVertexData->Unmap(0, nullptr);
        }
        D3D12_INDEX_BUFFER_VIEW ibv;
        ibv.BufferLocation = m_debugIndexData->GetGPUVirtualAddress();
        ibv.Format = DXGI_FORMAT_R32_UINT;
        ibv.SizeInBytes = SizeOfVector(draw_data.debugData.m_indices);
        {
            UINT8* pIndexDataBegin;
            CD3DX12_RANGE readRange(0, 0);
            D3D_CALL(m_debugIndexData->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin)));
            memcpy(pIndexDataBegin, draw_data.debugData.m_indices.data(), ibv.SizeInBytes);
            m_debugIndexData->Unmap(0, nullptr);
        }

        SetPipelineState(g_gridPSO, cmd);
        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

        cmdList->IASetVertexBuffers(0, 1, &vbv);
        cmdList->IASetIndexBuffer(&ibv);
        cmdList->DrawIndexedInstanced(pointCount, 1, 0, 0, 0);
    }
#endif

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);

    D3D12_RESOURCE_BARRIER barrier;
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_renderTargets[m_backbufferIndex];
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    cmdList->ResourceBarrier(1, &barrier);

    EndFrame();

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    D3D_CALL(m_swap_chain->Present(1, 0));  // Present with vsync
    m_graphicsContext.MoveToNextFrame();
}

bool D3d12GraphicsManager::CreateDevice() {
    bool m_enable_debug_layer = true;
    if (m_enable_debug_layer) {
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&m_debugController)))) {
            m_debugController->EnableDebugLayer();
        }
    }

    HRESULT hr = S_OK;
    D3D_FAIL_V(hr = CreateDXGIFactory1(IID_PPV_ARGS(&m_factory)), false);

    auto get_hardware_adapter = [](IDXGIFactory4* pFactory, IDXGIAdapter1** ppAdapter) {
        *ppAdapter = nullptr;
        for (UINT adapter_index = 0;; ++adapter_index) {
            IDXGIAdapter1* adapter = nullptr;
            if (DXGI_ERROR_NOT_FOUND == pFactory->EnumAdapters1(adapter_index, &adapter)) {
                // No more adapters to enumerate.
                break;
            }

            // Check to see if the adapter supports Direct3D 12, but don't create the
            // actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr))) {
                *ppAdapter = adapter;
                return;
            }
            adapter->Release();
        }
    };

    ComPtr<IDXGIAdapter1> hardwareAdapter;
    get_hardware_adapter(m_factory.Get(), &hardwareAdapter);

    if (FAILED(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)))) {
        ComPtr<IDXGIAdapter> warpAdapter;
        D3D_FAIL_V(hr = m_factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)), false);

        D3D_FAIL_V(hr = D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)), false);
    }

    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0 };

    D3D12_FEATURE_DATA_FEATURE_LEVELS featLevels = { array_length(featureLevels), featureLevels, D3D_FEATURE_LEVEL_12_0 };

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_12_0;
    D3D_CALL(m_device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featLevels, sizeof(featLevels)));
    featureLevel = featLevels.MaxSupportedFeatureLevel;
    switch (featureLevel) {
        case D3D_FEATURE_LEVEL_12_0:
            LOG("[DX12] Device Feature Level: 12.0");
            break;
        case D3D_FEATURE_LEVEL_12_1:
            LOG("[DX12] Device Feature Level: 12.1");
            break;
        default:
            CRASH_NOW();
            break;
    }

    return true;
}

ID3D12CommandQueue* D3d12GraphicsManager::CreateCommandQueue(D3D12_COMMAND_LIST_TYPE p_type) {
    D3D12_COMMAND_QUEUE_DESC desc;
    desc.Type = p_type;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;

    ID3D12CommandQueue* queue;

    D3D_CALL(m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&queue)));

    const wchar_t* debugName = nullptr;
    switch (p_type) {
        case D3D12_COMMAND_LIST_TYPE_DIRECT:
            debugName = L"GraphicsCommandQueue";
            break;
        case D3D12_COMMAND_LIST_TYPE_COMPUTE:
            debugName = L"ComputeCommandQueue";
            break;
        case D3D12_COMMAND_LIST_TYPE_COPY:
            debugName = L"CopyCommandQueue";
            break;
        default:
            CRASH_NOW();
            break;
    }

    NAME_DX12_OBJECT(queue, debugName);
    return queue;
}

bool D3d12GraphicsManager::EnableDebugLayer() {
    D3D_FAIL_V(D3D12GetDebugInterface(IID_PPV_ARGS(&m_debugController)), false);

    m_debugController->EnableDebugLayer();

    ComPtr<ID3D12Debug> debugDevice;
    m_device->QueryInterface(IID_PPV_ARGS(debugDevice.GetAddressOf()));

    ComPtr<ID3D12InfoQueue> d3dInfoQueue;

    D3D_FAIL_V(m_device->QueryInterface(IID_PPV_ARGS(&d3dInfoQueue)), false);

    d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
    d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
    d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);

#if 0
            D3D12_MESSAGE_ID blockedIds[] = {
                D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
                D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
                D3D12_MESSAGE_ID_COPY_DESCRIPTORS_INVALID_RANGES
            };

            D3D12_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.pIDList = blockedIds;
            filter.DenyList.NumIDs = array_length(blockedIds);
            d3dInfoQueue->AddRetrievalFilterEntries(&filter);
            d3dInfoQueue->AddStorageFilterEntries(&filter);
#endif

    return true;
}

bool D3d12GraphicsManager::CreateDescriptorHeaps() {
    bool ok = true;
    ok = ok && m_rtvDescHeap.initialize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 16, m_device.Get());
    ok = ok && m_dsvDescHeap.initialize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 16, m_device.Get());
    ok = ok && m_srvDescHeap.initialize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 100, m_device.Get(), true);
    return ok;
}

bool D3d12GraphicsManager::CreateSwapChain(uint32_t width, uint32_t height) {
    auto display_manager = dynamic_cast<Win32DisplayManager*>(DisplayManager::GetSingletonPtr());
    DEV_ASSERT(display_manager);

    // create a struct to hold information about the swap chain
    DXGI_SWAP_CHAIN_DESC1 scd{};

    // fill the swap chain description struct
    scd.Width = width;
    scd.Height = height;
    scd.Format = SURFACE_FORMAT;
    scd.Stereo = FALSE;
    scd.SampleDesc = { 1, 0 };
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;  // how swap chain is to be used
    scd.BufferCount = NUM_FRAMES_IN_FLIGHT;             // back buffer count
    scd.Scaling = DXGI_SCALING_STRETCH;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    scd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    IDXGISwapChain1* pSwapChain = nullptr;

    HRESULT hr = m_factory->CreateSwapChainForHwnd(
        m_graphicsContext.m_commandQueue.Get(),
        display_manager->GetHwnd(),
        &scd,
        NULL,
        NULL,
        &pSwapChain);

    D3D_FAIL_V_MSG(hr, false, "Failed to create swapchain");

    m_swap_chain.Attach(reinterpret_cast<IDXGISwapChain3*>(pSwapChain));

    m_swap_chain->SetMaximumFrameLatency(NUM_BACK_BUFFERS);
    m_swapChainWaitObject = m_swap_chain->GetFrameLatencyWaitableObject();

    return true;
}

bool D3d12GraphicsManager::CreateRenderTarget(uint32_t width, uint32_t height) {
    for (int32_t i = 0; i < NUM_FRAMES_IN_FLIGHT; i++) {
        ID3D12Resource* pBackBuffer = nullptr;
        D3D_CALL(m_swap_chain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer)));
        m_device->CreateRenderTargetView(pBackBuffer, nullptr, m_renderTargetDescriptor[i]);
        std::wstring name = std::wstring(L"Render Target Buffer") + std::to_wstring(i);
        pBackBuffer->SetName(name.c_str());
        m_renderTargets[i] = pBackBuffer;
    }

    D3D12_CLEAR_VALUE depthOptimizedClearValue{};
    depthOptimizedClearValue.Format = DEFAULT_DEPTH_STENCIL_FORMAT;
    depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
    depthOptimizedClearValue.DepthStencil.Stencil = 0;

    D3D12_HEAP_PROPERTIES prop{};
    prop.Type = D3D12_HEAP_TYPE_DEFAULT;
    prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    prop.CreationNodeMask = 1;
    prop.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = width;
    resourceDesc.Height = height;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DEFAULT_DEPTH_STENCIL_FORMAT;
    resourceDesc.SampleDesc = { 1, 0 };
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    HRESULT hr = m_device->CreateCommittedResource(
        &prop, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthOptimizedClearValue,
        IID_PPV_ARGS(&m_depthStencilBuffer));

    D3D_FAIL_V_MSG(hr, false, "Failed to create committed resource");

    m_depthStencilBuffer->SetName(L"Depth Stencil Buffer");
    m_device->CreateDepthStencilView(m_depthStencilBuffer, nullptr, m_depthStencilDescriptor);

    return true;
}

bool D3d12GraphicsManager::LoadAssets() {
    // Create a root signature consisting of a descriptor table with a single CBV.
    {
        CD3DX12_DESCRIPTOR_RANGE texTable;
        texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                      1,   // number of descriptors
                      0);  // register t0

        CD3DX12_ROOT_PARAMETER rootParams[4];
        rootParams[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
        rootParams[1].InitAsConstantBufferView(0);
        rootParams[2].InitAsConstantBufferView(1);
        rootParams[3].InitAsConstantBufferView(2);

        auto staticSamplers = GetStaticSamplers();

        // A root signature is an array of root parameters.
        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(array_length(rootParams),
                                                      rootParams,
                                                      (uint32_t)staticSamplers.size(),
                                                      staticSamplers.data(),
                                                      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        D3D_CALL(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
        D3D_CALL(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
    }

    return true;
}

void D3d12GraphicsManager::OnWindowResize(int p_width, int p_height) {
    if (m_swap_chain) {
        m_graphicsContext.Flush();

        CleanupRenderTarget();
        D3D_CALL(m_swap_chain->ResizeBuffers(0, p_width, p_height,
                                             DXGI_FORMAT_UNKNOWN,
                                             DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT));

        [[maybe_unused]] auto err = CreateRenderTarget(p_width, p_height);
    }
}

void D3d12GraphicsManager::CleanupRenderTarget() {
    m_graphicsContext.Flush();

    for (uint32_t i = 0; i < NUM_BACK_BUFFERS; ++i) {
        SafeRelease(m_renderTargets[i]);
    }
    SafeRelease(m_depthStencilBuffer);
}

void D3d12GraphicsManager::BeginFrame() {
    // @TODO: wait for swap chain
    FrameContext& frame = m_graphicsContext.BeginFrame();

    WaitForSingleObject(m_swapChainWaitObject, INFINITE);

    m_currentFrameContext = &frame;
    m_backbufferIndex = m_swap_chain->GetCurrentBackBufferIndex();
}

void D3d12GraphicsManager::EndFrame() {
    m_graphicsContext.EndFrame();
}

}  // namespace my
