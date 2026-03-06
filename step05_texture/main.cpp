#define _CRT_SECURE_NO_WARNINGS
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>
#include <atomic>
#include <thread>
#include <cassert>
#include <rhi/er_include_all.h>

#include <windows.h>
#include <wincodec.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


#pragma comment(lib, "windowscodecs.lib")

namespace detail {
    struct vec2 { float x, y; };
    struct vec3 { float x, y, z; };
}

struct BaseCBuffer {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

struct Vertex {
    detail::vec2 pos;
    detail::vec2 tex;
    detail::vec3 color;
};

auto ReadFileGetData(const char* filename) {
    std::vector<char> buffer;
    if (const auto file = std::fopen(filename, "rb")) {
        std::fseek(file, 0, SEEK_END);
        const auto size = std::ftell(file);
        std::fseek(file, 0, SEEK_SET);
        buffer.resize(size);
        std::fread(buffer.data(), 1, size, file);
        std::fclose(file);
    }
    return buffer;
}

using namespace Slim;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

HWND WindowCreate(HINSTANCE hInstance, int nCmdShow, int w, int h) {
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"Dust.Window.Normal";
    wc.hbrBackground = nullptr;

    ::RegisterClassExW(&wc);

    DWORD dwStyle = WS_OVERLAPPEDWINDOW;
    DWORD dwExStyle = 0;

    RECT clientRect = { 0, 0, w, h };

    ::AdjustWindowRectEx(&clientRect, dwStyle, FALSE, dwExStyle);

    const int windowWidth = clientRect.right - clientRect.left;
    const int windowHeight = clientRect.bottom - clientRect.top;

    HWND hwnd = ::CreateWindowExW(
        dwExStyle,
        wc.lpszClassName,
        L"Minimal Window",
        dwStyle,
        CW_USEDEFAULT, CW_USEDEFAULT,
        windowWidth, windowHeight,

        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (hwnd == NULL) return 0;

    ::ShowWindow(hwnd, nCmdShow);
    ::UpdateWindow(hwnd);
    return hwnd;
}

enum {
    WINDOW_WIDTH = 1280,
    WINDOW_HEIGHT = 720,
};

struct fps_display_ctx {
    uint32_t time;
    uint32_t counter;
    uint32_t counter_all;
    float fps;
};

void fps_display(fps_display_ctx&);

struct com_init {
    com_init() noexcept { if (FAILED(::CoInitialize(nullptr))) std::exit(1); }
    ~com_init() noexcept { ::CoUninitialize(); }
};

struct pipe_t {
    RHI::base_ptr<RHI::IRHIPipeline> pipeline;
    //RHI::base_ptr<RHI::IRHIRenderPass> renderPass;
    RHI::base_ptr<RHI::IRHIPipelineLayout> pipelineLayout;
    RHI::base_ptr<RHI::IRHIBuffer> bufferVertex;
    RHI::base_ptr<RHI::IRHIBuffer> bufferIndex;
    RHI::base_ptr<RHI::IRHIImage> bufferImage;
    //RHI::base_ptr<RHI::IRHIImageView> bufferImageView;
    RHI::base_ptr<RHI::IRHIBuffer> baseCBuffers[RHI::MAX_FRAMES_IN_FLIGHT];

};

void copyImage(RHI::IRHIBuffer* src
    , RHI::IRHIImage* dst
    , uint32_t pitch
    , uint32_t width
    , uint32_t height
    , RHI::IRHISwapChain* swapchian
    , RHI::IRHIDevice* device) {
    using RHI::base_ptr;
    RHI::CommandPoolDesc desc{};

    desc.queueType = RHI::QUEUE_TYPE_GRAPHICS;
    base_ptr<RHI::IRHICommandPool> pool;
    base_ptr<RHI::IRHICommandQueue> queue;
    base_ptr<RHI::IRHICommandList> list;
    device->CreateCommandPool(&desc, pool.as_get());
    swapchian->GetBoundQueue(queue.as_get());
    pool->CreateCommandList(list.as_get());
    list->Begin();
    list->ResourceBarriers({
        { dst, RHI::IMAGE_LAYOUT_UNDEFINED, RHI::IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL },
    });
    list->CopyBufferToImage({ dst, src, 0, pitch, 0, 0, width, height });
    list->ResourceBarrier({ dst, RHI::IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, RHI::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
    list->End();
    queue->Submit(1, list.as_set());
    queue->WaitIdle();
}

void copyBuffer(RHI::IRHIBuffer* from
    , RHI::IRHIBuffer* to
    , RHI::IRHISwapChain* swapchian
    , RHI::IRHIDevice* device
    , uint64_t size) {
    RHI::CommandPoolDesc desc{};
    using RHI::base_ptr;
    desc.queueType = RHI::QUEUE_TYPE_GRAPHICS;
    base_ptr<RHI::IRHICommandPool> pool;
    base_ptr<RHI::IRHICommandQueue> queue;
    base_ptr<RHI::IRHICommandList> list;
    device->CreateCommandPool(&desc, pool.as_get());
    swapchian->GetBoundQueue(queue.as_get());
    pool->CreateCommandList(list.as_get());
    list->Begin();
    list->CopyBuffer({ to, from, 0, 0, size });
    list->End();
    queue->Submit(1, list.as_set());
    queue->WaitIdle();

}

void buildBaseCBuffer(RHI::IRHIDevice* device
    , RHI::IRHISwapChain* swapchian
    , RHI::IRHIBuffer** buffer) {

    RHI::BufferDesc desc{};
    desc.size = sizeof(BaseCBuffer);
    desc.type = RHI::MEMORY_TYPE_UPLOAD;
    desc.usage.flags
        = RHI::BUFFER_USAGE_CONSTANT_BUFFER
        ;
    device->CreateBuffer(&desc, buffer);
    void* data = nullptr;
    const auto obj = *buffer;
    obj->SetDebugName("cbuffer #??");
}

void buildIndex(RHI::IRHIDevice* device
    , RHI::IRHISwapChain* swapchian
    , RHI::IRHIBuffer** buffer) {
    const std::vector<uint16_t> indices = {
        0, 1, 2, 2, 3, 0,
    };

    RHI::IRHIBuffer* staging = nullptr;
    RHI::IRHIBuffer* display = nullptr;
    {
        RHI::BufferDesc desc{};
        desc.size = indices.size() * sizeof(indices[0]);
        desc.type = RHI::MEMORY_TYPE_UPLOAD;
        desc.usage.flags
            = RHI::BUFFER_USAGE_INDEX_BUFFER
            | RHI::BUFFER_USAGE_TRANSFER_SRC
            ;
        device->CreateBuffer(&desc, &staging);
        void* data = nullptr;
        staging->SetDebugName("triangle idx staging");
        if (RHI::Success(staging->Map(&data))) {
            std::memcpy(data, indices.data(), desc.size);
            staging->Unmap();
        }
    }
    {
        RHI::BufferDesc desc{};
        desc.size = indices.size() * sizeof(indices[0]);
        desc.type = RHI::MEMORY_TYPE_DEFAULT;
        desc.usage.flags
            = RHI::BUFFER_USAGE_INDEX_BUFFER
            | RHI::BUFFER_USAGE_TRANSFER_DST
            ;
        device->CreateBuffer(&desc, &display);
        assert(display);
        display->SetDebugName("triangle idx display");
    }
    copyBuffer(staging, display, swapchian, device, indices.size() * sizeof(indices[0]));
    {
        *buffer = display;
        display = nullptr;
    }
    RHI::SafeDispose(display);
    RHI::SafeDispose(staging);
}

void create_create_image_rgba8(const wchar_t* filename
    , std::vector<uint8_t>& data
    , uint32_t& pitch
    , uint32_t& width
    , uint32_t& height);

void buildTexture(RHI::IRHIDevice* device
    , RHI::IRHISwapChain* swapchian
    , RHI::IRHIImage** image) {
    std::vector<uint8_t> buffer;
    RHI::ImageDesc desci = {};
    RHI::BufferDesc descb = {};
    uint32_t pitch = 0;
    create_create_image_rgba8(L"image/texture.jpg", buffer, pitch, desci.width, desci.height);
    assert(buffer.size());


    desci.format = RHI::RHI_FORMAT_RGBA8_UNORM;
    desci.usage.flags = RHI::IMAGE_USAGE_TRANSFER_SRC;

    RHI::IRHIBuffer* staging = nullptr;
    RHI::IRHIImage* display = nullptr;
    {
        RHI::BufferDesc desc{};
        desc.size = buffer.size();
        desc.type = RHI::MEMORY_TYPE_UPLOAD;
        desc.usage.flags = RHI::BUFFER_USAGE_TRANSFER_SRC;

        device->CreateBuffer(&desc, &staging);
        staging->SetDebugName("texture staging buffer");
        void* ptr = nullptr;
        if (RHI::Success(staging->Map(&ptr))) {
            std::memcpy(ptr, buffer.data(), buffer.size());
            staging->Unmap();
        }
    }

    desci.usage.flags = RHI::IMAGE_USAGE_TRANSFER_DST;
    //desc.initialLayout = RHI::IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    device->CreateImage(&desci, &display);
    display->SetDebugName("texture display");

    copyImage(staging, display, pitch, desci.width, desci.height, swapchian, device);

    std::swap(*image, display);
    //display->Dispose();
    staging->Dispose();
}

void buildVertex(RHI::IRHIDevice* device
    , RHI::IRHISwapChain* swapchian
    , RHI::IRHIBuffer** buffer) {
    const std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f}, {2.0f, 0.0f}, { 1.0f, 0.0f, 0.0f }},
        {{0.5f, -0.5f}, {0.0f, 0.0f},{0.0f, 1.0f, 0.0f}},
        {{0.5f, 0.5f}, {0.0f, 2.0f},{0.0f, 0.0f, 1.0f}},
        {{-0.5f, 0.5f}, {2.0f, 2.0f},{1.0f, 1.0f, 1.0f}}
    };
    RHI::IRHIBuffer* staging = nullptr;
    RHI::IRHIBuffer* display = nullptr;
    {
        RHI::BufferDesc desc{};
        desc.size = vertices.size() * sizeof(vertices[0]);
        desc.type = RHI::MEMORY_TYPE_UPLOAD;
        desc.usage.flags 
            = RHI::BUFFER_USAGE_VERTEX_BUFFER
            | RHI::BUFFER_USAGE_TRANSFER_SRC
            ;
        device->CreateBuffer(&desc, &staging);
        void* data = nullptr;
        staging->SetDebugName("triangle vtx staging");
        if (RHI::Success(staging->Map(&data))) {
            std::memcpy(data, vertices.data(), desc.size);
            staging->Unmap();
        }
    }
    {
        RHI::BufferDesc desc{};
        desc.size = vertices.size() * sizeof(vertices[0]);
        desc.type = RHI::MEMORY_TYPE_DEFAULT;
        desc.usage.flags
            = RHI::BUFFER_USAGE_VERTEX_BUFFER
            | RHI::BUFFER_USAGE_TRANSFER_DST
            ;
        device->CreateBuffer(&desc, &display);
        assert(display);
        display->SetDebugName("triangle vtx display");
    }
    copyBuffer(staging, display, swapchian, device, vertices.size() * sizeof(vertices[0]));
    {
        *buffer = display;
        display = nullptr;
    }
    RHI::SafeDispose(display);
    RHI::SafeDispose(staging);
}

pipe_t pipeline(RHI::API api,
    RHI::IRHIInstance* instance,
    RHI::IRHISwapChain* swapchian,
    RHI::IRHIDescriptorBuffer* descBuffer,
    RHI::IRHIDevice* device) {
    using RHI::base_ptr;
    pipe_t rv;
    std::vector<char> ps_data, vs_data;
    if (api == RHI::API::VULKAN) {
#ifndef NDEBUG
        vs_data = ReadFileGetData("shaders/shader_vs_debug.spv");
        ps_data = ReadFileGetData("shaders/shader_ps_debug.spv");
#else
        vs_data = ReadFileGetData("shaders/shader_vs_release.spv");
        ps_data = ReadFileGetData("shaders/shader_ps_release.spv");
#endif
    }
    else if (api == RHI::API::D3D12) {
#ifndef NDEBUG
        vs_data = ReadFileGetData("shaders/shader_vs_debug.cso");
        ps_data = ReadFileGetData("shaders/shader_ps_debug.cso");
#else
        vs_data = ReadFileGetData("shaders/shader_vs_release.cso");
        ps_data = ReadFileGetData("shaders/shader_ps_release.cso");
#endif
    }
    else {
        assert(false && "Unsupported RHI API");
    }

    assert(vs_data.size() && ps_data.size());
    base_ptr<RHI::IRHIShader> ps_code, vs_code;
    device->CreateShader(ps_data.data(), ps_data.size(), ps_code.as_get(), "PSMain");
    device->CreateShader(vs_data.data(), vs_data.size(), vs_code.as_get(), "VSMain");
    //base_ptr<RHI::IRHIPipeline> pipeline;
    //base_ptr<RHI::IRHIPipelineLayout> pipelineLayout;
    {
        RHI::PushDescriptorDesc pushDesc[] = {
            { RHI::DESCRIPTOR_TYPE_CONST_BUFFER     , 1, RHI::SHADER_VISIBILITY_VERTEX, RHI::VOLATILITY_VOLATILE  },
            //{ RHI::DESCRIPTOR_TYPE_SHADER_RESOURCE  , 2, RHI::SHADER_VISIBILITY_PIXEL, RHI::VOLATILITY_STATIC  },
        };

        RHI::StaticSamplerDesc baseSampler[1] = {};
        baseSampler[0].binding = 0;  
        baseSampler[0].space = 1;   
        baseSampler[0].visibility = RHI::SHADER_VISIBILITY_PIXEL; 

        RHI::PipelineLayoutDesc desc{};
        desc.pushDescriptors = pushDesc;
        desc.pushDescriptorCount = sizeof(pushDesc) / sizeof(pushDesc[0]);
        desc.inputAssembler = true;

        //desc.staticSamplerCount = sizeof(baseSampler) / sizeof(baseSampler[0]);
        //desc.staticSamplers = baseSampler;

        desc.descriptorBuffer = descBuffer;
        //desc.staticSamplerOnBack = true;


        device->CreatePipelineLayout(&desc, rv.pipelineLayout.as_get());
    }
    //base_ptr<RHI::IRHIBuffer> bufferVertex;
    buildVertex(device, swapchian, rv.bufferVertex.as_get());
    //base_ptr<RHI::IRHIBuffer> bufferIndex;
    buildIndex(device, swapchian, rv.bufferIndex.as_get());
    for (int i = 0; i != RHI::MAX_FRAMES_IN_FLIGHT; ++i) {
        buildBaseCBuffer(device, swapchian, rv.baseCBuffers[i].as_get());
    }

    buildTexture(device, swapchian, rv.bufferImage.as_get());
    //{
    //    RHI::ImageViewDesc desc{};
    //    device->CreateImageView(&desc, rv.bufferImage, rv.bufferImageView.as_get());
    //    assert(rv.bufferImageView);
    //}

    {
        RHI::PipelineDesc desc{};
        RHI::VertexBindingDesc bindings = { 0, sizeof(Vertex), RHI::VERTEX_INPUT_RATE_VERTEX };
        RHI::VertexAttributeDesc attributes[] = {
            { 0, 0,  RHI::RHI_FORMAT_RG32_FLOAT, offsetof(Vertex, pos), RHI::VERTEX_ATTRIBUTE_SEMANTIC_POSITION},
            { 0, 1,  RHI::RHI_FORMAT_RG32_FLOAT, offsetof(Vertex, tex), RHI::VERTEX_ATTRIBUTE_SEMANTIC_TEXCOORD},
            { 0, 2,  RHI::RHI_FORMAT_RGB32_FLOAT, offsetof(Vertex, color), RHI::VERTEX_ATTRIBUTE_SEMANTIC_COLOR},
        };
        std::vector<RHI::VertexAttributeDesc> vAttributes;
        desc.layout = rv.pipelineLayout;
        desc.ps = ps_code;
        desc.vs = vs_code;
        desc.topology = RHI::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        desc.maxViewportCount = 1;
        desc.maxScissorCount = 1;
        desc.sampleCount = { 1 };
        desc.sampleMask = ~uint32_t(0);
        desc.blend.logicOp = RHI::LOGIC_OP_COPY;
        desc.blend.attachmentCount = 1;
        desc.blend.attachments[0].colorWriteMask = RHI::COLOR_COMPONENT_FLAG_ALL;

        desc.vertexBindingCount = 1;
        desc.vertexBinding = &bindings;
        desc.vertexAttributeCount = sizeof(attributes) / sizeof(attributes[0]);
        desc.vertexAttribute = attributes;

        desc.basicPass.colorFormatCount = 1;
        desc.basicPass.colorFormats[0] = RHI::RHI_FORMAT_RGBA8_UNORM;

        desc.rasterization.cullMode = RHI::CULL_MODE_FRONT;
        device->CreatePipeline(&desc, rv.pipeline.as_get());
    }
    return rv;
}

void update_cbuffer(void* coherent_buffer) {

    //static int frame_id = 0;
    const auto current = ::timeGetTime();

    const auto raid = current % 100000;
    const auto frame = float(raid) * 0.001f * 0.1f;

    BaseCBuffer cb{};

    cb.model = glm::rotate(glm::mat4(1.0f), glm::radians(frame * 360.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    cb.view = glm::lookAt(glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    cb.proj = glm::perspective(glm::radians(45.0f), float(WINDOW_WIDTH) / float(WINDOW_HEIGHT), 0.1f, 10.0f);

    std::memcpy(coherent_buffer, &cb, sizeof(BaseCBuffer));
}


void record(RHI::IRHICommandList* list, const pipe_t& pipe, RHI::IRHIImage* image, RHI::IRHIAttachment* attachment, int flight, RHI::IRHIDescriptorBuffer* desc) {
    //static int counter = 0; counter++;
    //if (counter == 100) {
    //    desc->BindImage({
    //        pipe.bufferImage, 1, RHI::RHI_FORMAT_UNKNOWN
    //    });
    //}

    list->Begin();
    list->ResourceBarrier({ image, RHI::IMAGE_LAYOUT_PRESENT_DST, RHI::IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    RHI::BeginRenderPassDesc rp{};
    rp.width = WINDOW_WIDTH;
    rp.height = WINDOW_HEIGHT;
    rp.colorCount = 1;

    rp.colors[0].attachment = attachment;
    rp.colors[0].load = RHI::PASS_LOAD_OP_CLEAR;
    rp.colors[0].store = RHI::PASS_STORE_OP_STORE;
    rp.colors[0].clear[0] = 0;
    rp.colors[0].clear[1] = 0;
    rp.colors[0].clear[2] = 0;
    rp.colors[0].clear[3] = 1;

    list->BeginRenderPass(rp);
    {
        list->BindPipeline(pipe.pipeline);

        list->BindDescriptorBuffer(pipe.pipelineLayout, desc);

        RHI::Viewport viewport{};
        viewport.topLeftX = 0.0f;
        viewport.topLeftY = 0.0f;
        viewport.width = static_cast<float>(WINDOW_WIDTH);
        viewport.height = static_cast<float>(WINDOW_HEIGHT);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        list->SetViewport(viewport);
        RHI::Rect scissor{};
        scissor.left = 0;
        scissor.top = 0;
        scissor.right = WINDOW_WIDTH;
        scissor.bottom = WINDOW_HEIGHT;
        list->SetScissor(scissor);
        list->BindVertexBuffer({ 0, pipe.bufferVertex , sizeof(Vertex) });
        list->BindIndexBuffer(pipe.bufferIndex, 0, RHI::INDEX_TYPE_UINT16);
        RHI::PushDescriptorSetDesc descs[] = {  
            RHI::PushDescriptorConstBuffer(pipe.baseCBuffers[flight]),
            //RHI::PushDescriptorShaderResource(pipe.bufferImageView),
        };
        list->PushDescriptorSets(pipe.pipelineLayout, sizeof(descs)/sizeof(descs[0]), descs);
        list->DrawIndexed(6, 1, 0, 0, 0);
    }

    list->EndRenderPass();
    list->ResourceBarrier({ image, RHI::IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, RHI::IMAGE_LAYOUT_PRESENT_SRC });
    list->End();
}

//int WinMain( HINSTANCE hInstance,
//    HINSTANCE hPrevInstance,
//              LPSTR     lpCmdLine,
//               int       nShowCmd
//){ 

int main() {
    wchar_t path[1024];
    ::GetCurrentDirectoryW(1024, path);
    //::SetEnvironmentVariableW(L"VK_ADD_DRIVER_FILES", path);
    com_init com_init_obj;
    auto hWnd = WindowCreate(::GetModuleHandleW(nullptr), SW_SHOW, WINDOW_WIDTH, WINDOW_HEIGHT);


    using RHI::base_ptr;

    base_ptr<RHI::IRHIInstance> instance;
    bool debug = false;
    auto api = RHI::API::D3D12;
    api = RHI::API::VULKAN;
#ifndef NDEBUG
    debug = true;
#endif
    RHI::CreateInstance({ api, debug }, instance.as_get());
    base_ptr<RHI::IRHIAdapter> adapter;
    uint32_t index = 0;
    while (RHI::Success(instance->EnumAdapters(index, adapter.as_get()))) {
        RHI::AdapterDesc desc{};
        adapter->GetDesc(&desc);
        const auto mb = double(desc.vram / 1024 / 1024);
        std::printf("Device: %s (%.2f G)\n", desc.name, mb / 1024.f);
        //if (!(desc.feature & RHI::ADAPTER_FEATURE_CPU)) {
        //    index++;
        //    continue;
        //}
        if (desc.feature & RHI::ADAPTER_FEATURE_2D) {
            break;
        }
        adapter->Dispose();
        index++;
    }
    if (!adapter)
        RHI::Success(instance->GetDefaultAdapter(adapter.as_get()));

    base_ptr<RHI::IRHIDevice> device;
    instance->CreateDevice(adapter, device.as_get());
    base_ptr<RHI::IRHIDescriptorBuffer> descriptorBuffer;
    {
        RHI::DescriptorSlotDesc slots[] = {
            { RHI::DESCRIPTOR_TYPE_T, 0 , RHI::SHADER_VISIBILITY_PIXEL, RHI::VOLATILITY_STATIC_AT_EXECUTE },
            { RHI::DESCRIPTOR_TYPE_S, 1 , RHI::SHADER_VISIBILITY_PIXEL, RHI::VOLATILITY_STATIC },
            { RHI::DESCRIPTOR_TYPE_T, 2 , RHI::SHADER_VISIBILITY_PIXEL, RHI::VOLATILITY_STATIC_AT_EXECUTE },
            { RHI::DESCRIPTOR_TYPE_S, 3 , RHI::SHADER_VISIBILITY_PIXEL, RHI::VOLATILITY_STATIC },
        };
        RHI::DescriptorBufferDesc desc{};
        desc.slots = slots;
        desc.slotCount = sizeof(slots) / sizeof(slots[0]);
        device->CreateDescriptorBuffer(&desc, descriptorBuffer.as_get());
    }


    base_ptr<RHI::IRHISwapChain> swapchian;
    RHI::SwapChainDesc scDesc = {};
    scDesc.windowHandle = hWnd;
    scDesc.width = WINDOW_WIDTH;
    scDesc.height = WINDOW_HEIGHT;
    scDesc.format = RHI::RHI_FORMAT_RGBA8_UNORM;
    scDesc.bufferCount = 3;
    device->CreateSwapChain(&scDesc, swapchian.as_get());
    swapchian->GetDesc(&scDesc);
    std::vector<base_ptr<RHI::IRHIImage>> swapchianImage;
    swapchianImage.resize(scDesc.bufferCount);
    for (uint32_t i = 0; i != scDesc.bufferCount; ++i) {
        char buffer[80];
        std::snprintf(buffer, sizeof(buffer), "Swapchain Image #%d", i);
        swapchian->GetImage(i, swapchianImage[i].as_get());
        swapchianImage[i]->SetDebugName(buffer);
    }
    swapchianImage.resize(scDesc.bufferCount);
    std::vector<base_ptr<RHI::IRHIAttachment>> swapchianAttachment;
    swapchianAttachment.resize(swapchianImage.size());
    for (uint32_t i = 0; i != scDesc.bufferCount; ++i) {
        RHI::AttachmentDesc desc{};
        device->CreateAttachment(&desc, swapchianImage[i], swapchianAttachment[i].as_get());
    }
    auto pipe = pipeline(api, instance, swapchian, descriptorBuffer, device);


    RHI::SamplerDesc sd{};
    sd.addressModeU = RHI::ADDRESS_MODE_MIRRORED_REPEAT;
    sd.addressModeV = RHI::ADDRESS_MODE_MIRRORED_REPEAT;

    descriptorBuffer->BindImage({
        0,  pipe.bufferImage, RHI::RHI_FORMAT_UNKNOWN
    });
    
    descriptorBuffer->BindSampler(1, sd);
    descriptorBuffer->BindImage({
        2,  pipe.bufferImage, RHI::RHI_FORMAT_UNKNOWN
    });

    sd.addressModeU = RHI::ADDRESS_MODE_REPEAT;
    sd.addressModeV = RHI::ADDRESS_MODE_REPEAT;
    descriptorBuffer->BindSampler(3, sd);

    std::atomic<bool> flag{ true };

    std::thread render_thd{ [=, &flag, &swapchianImage, &swapchianAttachment]() {
        uint64_t counter = 1;
        fps_display_ctx fps_ctx = {};
        RHI::CommandPoolDesc desc{};
        base_ptr<RHI::IRHIFence> gra_fence;
        base_ptr<RHI::IRHICommandPool> gra_pools[RHI::MAX_FRAMES_IN_FLIGHT];
        base_ptr<RHI::IRHICommandList> gra_lists[RHI::MAX_FRAMES_IN_FLIGHT];
        base_ptr<RHI::IRHICommandQueue> gra_queue;
        desc.queueType = RHI::QUEUE_TYPE_GRAPHICS;
        swapchian->GetBoundQueue(gra_queue.as_get());
        for (int i =0; i != RHI::MAX_FRAMES_IN_FLIGHT; ++i)
            device->CreateCommandPool(&desc, gra_pools[i].as_get());
        device->CreateFence(counter, gra_fence.as_get());
        for (int i = 0; i != RHI::MAX_FRAMES_IN_FLIGHT; ++i)
            gra_pools[i]->CreateCommandList(gra_lists[i].as_get());

        void* coherent_buffer[RHI::MAX_FRAMES_IN_FLIGHT];
        for (int i = 0; i != RHI::MAX_FRAMES_IN_FLIGHT; ++i) {
            pipe.baseCBuffers[i]->Map(coherent_buffer + i);
        }

        while (flag) {
            //::Sleep((counter & 1) ? 5 : 0);
            //gra_fence->Wait(counter, RHI::WAIT_FOREVER);

            //for (auto& b : pipe.baseCBuffers) b->Unmap();
            //for (int i = 0; i != RHI::MAX_FRAMES_IN_FLIGHT; ++i)
            //    pipe.baseCBuffers[i]->Map(coherent_buffer + i);

            ++counter;
            RHI::FrameContext ctx;
            swapchian->AcquireFrame(ctx);

            gra_pools[ctx.indexInFlight]->Reset();

            update_cbuffer(coherent_buffer[ctx.indexInFlight]);

            record(gra_lists[ctx.indexInFlight]
                , pipe
                , swapchianImage[ctx.indexBuffer]
                , swapchianAttachment[ctx.indexBuffer]
                , ctx.indexInFlight
                , descriptorBuffer
            );

            RHI::SubmitSynchAuto synch{ RHI::FencePairSignal { gra_fence, counter } };
            gra_queue->Submit(1, gra_lists[ctx.indexInFlight].as_set(), &synch);
            swapchian->PresentFrame(1);

            fps_display(fps_ctx);

            std::printf("%lld\n", gra_fence->GetCompletedValue());
        }
        gra_fence->Wait(counter, RHI::WAIT_FOREVER);
        for (auto& b : pipe.baseCBuffers) b->Unmap();

    } };

    MSG msg;
    while (::GetMessageW(&msg, NULL, 0, 0) > 0) {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }
    flag = false;
    render_thd.join();
    {
        base_ptr<RHI::IRHICommandQueue> gra_queue;
        swapchian->GetBoundQueue(gra_queue.as_get());
        gra_queue->WaitIdle();
    }

    std::printf("Hello, world!\n");

    return 0;
}

void fps_display(fps_display_ctx& ctx) {
    const auto current = timeGetTime();
    if (!ctx.time) ctx.time = current;
    ctx.counter++;
    ctx.counter_all++;
    const auto delta = current - ctx.time;
    if (delta > 300) {
        ctx.fps = float((double)ctx.counter / (double)delta * 1000.0);
        ctx.time = current;
        ctx.counter = 0;

        char buffer[100];
        std::snprintf(buffer, sizeof(buffer), "counter: %d fps=%3.2f\n", ctx.counter_all, ctx.fps);
        ::OutputDebugStringA(buffer);
    }
}


void create_create_image_rgba8(const wchar_t* filename
    , std::vector<uint8_t>& data
    , uint32_t& pitch
    , uint32_t& width
    , uint32_t& height) {
    data.clear();
    width = 0;
    height = 0;

    // Create WIC factory
    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory)
    );
    if (FAILED(hr)) {
        std::printf("Failed to create WIC factory: 0x%08X\n", hr);
        return;
    }

    // Create decoder
    IWICBitmapDecoder* decoder = nullptr;
    hr = factory->CreateDecoderFromFilename(
        filename,
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        &decoder
    );
    if (FAILED(hr)) {
        std::printf("Failed to create decoder: 0x%08X\n", hr);
        factory->Release();
        return;
    }

    // Get first frame
    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) {
        std::printf("Failed to get frame: 0x%08X\n", hr);
        decoder->Release();
        factory->Release();
        return;
    }

    // Get frame dimensions
    UINT frameWidth = 0, frameHeight = 0;
    hr = frame->GetSize(&frameWidth, &frameHeight);
    if (FAILED(hr)) {
        std::printf("Failed to get frame size: 0x%08X\n", hr);
        frame->Release();
        decoder->Release();
        factory->Release();
        return;
    }

    // Create format converter
    IWICFormatConverter* converter = nullptr;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) {
        std::printf("Failed to create format converter: 0x%08X\n", hr);
        frame->Release();
        decoder->Release();
        factory->Release();
        return;
    }

    // Convert to RGBA8
    hr = converter->Initialize(
        frame,
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0f,
        WICBitmapPaletteTypeCustom
    );
    if (FAILED(hr)) {
        std::printf("Failed to initialize converter: 0x%08X\n", hr);
        converter->Release();
        frame->Release();
        decoder->Release();
        factory->Release();
        return;
    }

    // Allocate buffer for RGBA8 data
    const UINT stride = frameWidth * 4; // 4 bytes per pixel (RGBA)
    const UINT bufferSize = stride * frameHeight;
    data.resize(bufferSize);

    // Copy pixel data
    hr = converter->CopyPixels(
        nullptr,
        stride,
        bufferSize,
        data.data()
    );
    if (FAILED(hr)) {
        std::printf("Failed to copy pixels: 0x%08X\n", hr);
        data.clear();
    } else {
        width = frameWidth;
        height = frameHeight;
        pitch = stride;
    }

    // Cleanup
    converter->Release();
    frame->Release();
    decoder->Release();
    factory->Release();
}