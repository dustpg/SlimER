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
#include <d3dcompiler.h>

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
};

void remove_cpp11_attr(char buf[], size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        if (i + 1 < len && buf[i] == '[' && buf[i + 1] == '[') {
            // Found start of C++11 attribute [[
            size_t start = i;
            size_t depth = 1;
            i += 2;
            
            while (i < len && depth > 0) {
                if (i + 1 < len && buf[i] == ']' && buf[i + 1] == ']') {
                    depth--;
                    if (depth == 0) {
                        for (size_t j = start; j <= i + 1; ++j) {
                            buf[j] = ' ';
                        }
                        i++; 
                        break;
                    } else {
                        i += 2;
                    }
                } else if (i + 1 < len && buf[i] == '[' && buf[i + 1] == '[') {
                    depth++;
                    i += 2;
                } else {
                    i++;
                }
            }
        }
    }
}

bool dxbc_test(std::vector<char>& ps_data, std::vector<char>& vs_data) {
    auto shader = ReadFileGetData("shaders/shader.hlsl");
    remove_cpp11_attr(shader.data(), shader.size());

    UINT compileFlags = 0;
#ifndef DNEBUG
    compileFlags |= D3DCOMPILE_DEBUG;
    compileFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob* vs = NULL;
    ID3DBlob* ps = NULL;

    HRESULT hr = D3DCompile(shader.data(), shader.size(), NULL, NULL, NULL, "VSMain", "vs_4_0", compileFlags, 0, &vs, NULL);
    if (!SUCCEEDED(hr)) {
        printf("Failed to compile vertex shader: %u\n", hr);
        return false;
    }
    hr = D3DCompile(shader.data(), shader.size(), NULL, NULL, NULL, "PSMain", "ps_4_0", compileFlags, 0, &ps, NULL);
    if (!SUCCEEDED(hr)) {
        vs->Release();
        printf("Failed to compile pixel shader: %u\n", hr);
        return false;
    }

    ps_data.clear();
    vs_data.clear();

    ps_data.resize(ps->GetBufferSize());
    std::memcpy(ps_data.data(), ps->GetBufferPointer(), ps_data.size());

    vs_data.resize(vs->GetBufferSize());
    std::memcpy(vs_data.data(), vs->GetBufferPointer(), vs_data.size());

    vs->Release();
    ps->Release();
    return true;
}

pipe_t pipeline(RHI::API api,
    RHI::IRHIInstance* instance, 
    RHI::IRHIDevice* device) {
    using RHI::base_ptr;
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
        //dxbc_test(ps_data, vs_data);
    }
    else {
        assert(false && "Unsupported RHI API");
    }

    assert(vs_data.size() && ps_data.size());
    base_ptr<RHI::IRHIShader> ps_code, vs_code;
    device->CreateShader(ps_data.data(), ps_data.size(), ps_code.as_get(), "PSMain");
    device->CreateShader(vs_data.data(), vs_data.size(), vs_code.as_get(), "VSMain");
    base_ptr<RHI::IRHIPipeline> pipeline;
    //base_ptr<RHI::IRHIRenderPass> renderPass;
    base_ptr<RHI::IRHIPipelineLayout> pipelineLayout;

    {
        RHI::PipelineLayoutDesc desc{};
        device->CreatePipelineLayout(&desc, pipelineLayout.as_get());
    }
    //{
    //    RHI::RenderPassDesc desc{};
    //    desc.sampleCount = { 1 };
    //    desc.colorAttachmentCount = 1;
    //    desc.colorAttachment[0] = {
    //        RHI::RHI_FORMAT_RGBA8_UNORM,
    //        RHI::PASS_LOAD_OP_CLEAR,
    //        RHI::PASS_STORE_OP_STORE
    //    };
    //    device->CreateRenderPass(&desc, renderPass.as_get());
    //}
    {
        RHI::PipelineDesc desc{};
        desc.layout = pipelineLayout;
        //desc.renderPass = renderPass;
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

        desc.basicPass.colorFormatCount = 1;
        desc.basicPass.colorFormats[0] = RHI::RHI_FORMAT_RGBA8_UNORM;
        device->CreatePipeline(&desc, pipeline.as_get());
    }
    return { pipeline, /*renderPass,*/ pipelineLayout};
} 


void record(RHI::IRHICommandList* list, const pipe_t& pipe, RHI::IRHIImage* image, RHI::IRHIAttachment* attachment) {
    list->Begin();
    list->ResourceBarrier({ image, RHI::IMAGE_LAYOUT_PRESENT_DST, RHI::IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    RHI::BeginRenderPassDesc rp{};
    //rp.renderpass = pipe.renderPass;
    //rp.framebuffer = fb;
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
        list->Draw(3, 1, 0, 0);
    }
    list->EndRenderPass();
    list->ResourceBarrier({ image, RHI::IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, RHI::IMAGE_LAYOUT_PRESENT_SRC });
    list->End();
}


int main() {
    com_init com_init_obj;
    //test();
    auto hWnd = WindowCreate(::GetModuleHandleW(nullptr), SW_SHOW, WINDOW_WIDTH, WINDOW_HEIGHT);

    using RHI::base_ptr;

    base_ptr<RHI::IRHIInstance> instance;
    bool debug = false;
    auto api = RHI::API::D3D12;
    //api = RHI::API::VULKAN;
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
        //if (api == RHI::API::D3D12 && desc.vendorID != 0x1414) {
        //    index++;
        //    continue;
        //}
        if (desc.feature & RHI::ADAPTER_FEATURE_2D) {
            break;
        }
        adapter->Dispose();
        index++;
    }

    base_ptr<RHI::IRHIDevice> device;
    instance->CreateDevice(adapter, device.as_get());


    base_ptr<RHI::IRHISwapChain> swapchian;
    RHI::SwapChainDesc scDesc = {};
    scDesc.windowHandle = hWnd;
    scDesc.width = WINDOW_WIDTH;
    scDesc.height = WINDOW_HEIGHT;
    scDesc.format = RHI::RHI_FORMAT_RGBA8_UNORM;
    scDesc.bufferCount = 2;
    device->CreateSwapChain(&scDesc, swapchian.as_get());
    swapchian->GetDesc(&scDesc);
    assert(scDesc.bufferCount == 2);
    base_ptr<RHI::IRHIImage> swapchianImage0, swapchianImage1;
    {
        base_ptr<RHI::IRHIImage> swapchianImage2, swapchianImage3;
        swapchian->GetImage(0, swapchianImage0.as_get());
        swapchian->GetImage(1, swapchianImage1.as_get());
        swapchian->GetImage(0, swapchianImage2.as_get());
        swapchian->GetImage(1, swapchianImage3.as_get());

        swapchianImage0->SetDebugName("Swapchain Image #0");
        swapchianImage1->SetDebugName("Swapchain Image #1");
    }
    base_ptr<RHI::IRHIAttachment> swapchianAttachment0, swapchianAttachment1;
    {
        RHI::AttachmentDesc desc{};
        device->CreateAttachment(&desc, swapchianImage0, swapchianAttachment0.as_get());
        device->CreateAttachment(&desc, swapchianImage1, swapchianAttachment1.as_get());
    }
    //base_ptr<RHI::IRHIFrameBuffer> framebuffer0, framebuffer1;
    auto pipe = pipeline(api, instance, device);
    //{
    //    RHI::FrameBufferDesc desc{};
    //    desc.renderpass = pipe.renderPass;
    //    desc.colorViewCount = 1;
    //    desc.colorViews[0] = swapchianImageView0;
    //    device->CreateFrameBuffer(&desc, framebuffer0.as_get());
    //    desc.colorViews[0] = swapchianImageView1;
    //    device->CreateFrameBuffer(&desc, framebuffer1.as_get());
    //}


    std::atomic<bool> flag{ true };

    std::thread render_thd{ [=, &flag]() {
        uint64_t counter = 1;
        fps_display_ctx fps_ctx = {};
        RHI::CommandPoolDesc desc{};
        base_ptr<RHI::IRHIFence> gra_fence;
        base_ptr<RHI::IRHICommandPool> gra_pools[2];
        base_ptr<RHI::IRHICommandList> gra_lists[2];
        base_ptr<RHI::IRHICommandQueue> gra_queue;
        desc.queueType = RHI::QUEUE_TYPE_GRAPHICS;
        swapchian->GetBoundQueue(gra_queue.as_get());
        device->CreateCommandPool(&desc, gra_pools[0].as_get());
        device->CreateCommandPool(&desc, gra_pools[1].as_get());
        device->CreateFence(counter, gra_fence.as_get());
        gra_pools[0]->CreateCommandList(gra_lists[0].as_get());
        gra_pools[1]->CreateCommandList(gra_lists[1].as_get());


        base_ptr<RHI::IRHIImage> swapchianImage[] = { swapchianImage0, swapchianImage1 };
        base_ptr<RHI::IRHIAttachment> swapchianAttachment[] = { swapchianAttachment0, swapchianAttachment1 };
        //base_ptr<RHI::IRHIFrameBuffer> frames[] = { framebuffer0, framebuffer1 };
        while (flag) {
            //gra_fence->Wait(counter, RHI::WAIT_FOREVER);
            ++counter;
            RHI::FrameContext ctx;
            swapchian->AcquireFrame(ctx);
            gra_pools[ctx.indexInFlight]->Reset();

            record(gra_lists[ctx.indexInFlight], pipe, swapchianImage[ctx.indexBuffer], swapchianAttachment[ctx.indexBuffer]);

            RHI::SubmitSynchAuto synch{ RHI::FencePairSignal { gra_fence, counter } };
            gra_queue->Submit(1, gra_lists[ctx.indexInFlight].as_set(), &synch);
            swapchian->PresentFrame(1);

            fps_display(fps_ctx);

            //std::printf("%lld\n", gra_fence->GetCompletedValue());
        }
        gra_fence->Wait(counter, RHI::WAIT_FOREVER);

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

