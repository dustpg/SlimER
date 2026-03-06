#pragma once
#include "er_base.h"

namespace Slim { namespace RHI {

    struct IRHIShader;
    struct IRHIPipeline;
    //struct IRHIRenderPass;
    struct IRHIPipelineLayout;
    struct IRHIPipelineCache;
    struct IRHIDescriptorBuffer;

    enum DESCRIPTOR_TYPE : uint32_t;

    // Shader visibility flags (which shader stages can access the resource)
    enum SHADER_VISIBILITY : uint32_t {
        SHADER_VISIBILITY_ALL = 0,
        SHADER_VISIBILITY_VERTEX,
        SHADER_VISIBILITY_HULL,
        SHADER_VISIBILITY_DOMAIN,
        SHADER_VISIBILITY_GEOMETRY,
        SHADER_VISIBILITY_PIXEL,
        SHADER_VISIBILITY_AMPLIFICATION,
        SHADER_VISIBILITY_MESH,
    };

    enum VOLATILITY : uint32_t {
        VOLATILITY_DEFAULT = 0,
        VOLATILITY_VOLATILE,                    // use this for per frame update data
        VOLATILITY_STATIC,                      // use this for static data
        VOLATILITY_STATIC_AT_EXECUTE,
    };

    // Sampler filter mode (Vulkan: VkFilter, D3D12: D3D12_FILTER)
    enum FILTER_MODE : uint32_t {
        FILTER_MODE_NEAREST = 0,                // Nearest neighbor filtering
        FILTER_MODE_LINEAR,                     // Linear filtering
    };

    // Sampler address mode (Vulkan: VkSamplerAddressMode, D3D12: D3D12_TEXTURE_ADDRESS_MODE)
    enum ADDRESS_MODE : uint32_t {
        ADDRESS_MODE_REPEAT = 0,                // Repeat (Vulkan: REPEAT, D3D12: WRAP)
        ADDRESS_MODE_MIRRORED_REPEAT,           // Mirrored repeat (Vulkan: MIRRORED_REPEAT, D3D12: MIRROR)
        ADDRESS_MODE_CLAMP_TO_EDGE,             // Clamp to edge (Vulkan: CLAMP_TO_EDGE, D3D12: CLAMP)
        ADDRESS_MODE_CLAMP_TO_BORDER,           // Clamp to border (Vulkan: CLAMP_TO_BORDER, D3D12: BORDER)
        ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE,      // Mirror clamp to edge (Vulkan: MIRROR_CLAMP_TO_EDGE, D3D12: MIRROR_ONCE)
    };

    // Sampler mipmap mode (Vulkan: VkSamplerMipmapMode, D3D12: part of D3D12_FILTER)
    enum MIPMAP_MODE : uint32_t {
        MIPMAP_MODE_NEAREST = 0,                // Nearest mipmap
        MIPMAP_MODE_LINEAR,                     // Linear mipmap
    };

    enum COMPARE_OP : uint32_t;  // Forward declaration

    enum BORDER_COLOR : uint32_t {
        BORDER_COLOR_TRANSPARENT_BLACK = 0,
        BORDER_COLOR_OPAQUE_BLACK,
        BORDER_COLOR_OPAQUE_WHITE,
    };

    struct SamplerDesc {
        FILTER_MODE         magFilter;          // Magnification filter
        FILTER_MODE         minFilter;          // Minification filter
        MIPMAP_MODE         mipmapMode;         // Mipmap filtering mode
        ADDRESS_MODE        addressModeU;       // Address mode for U coordinate
        ADDRESS_MODE        addressModeV;       // Address mode for V coordinate
        ADDRESS_MODE        addressModeW;       // Address mode for W coordinate
        float               mipLodBias;         // Mipmap LOD bias
        float               maxAnisotropy;      // Maximum anisotropy level
        bool                anisotropyEnable;   // Enable anisotropic filtering
        bool                compareEnable;      // Enable comparison mode
        COMPARE_OP          compareOp;          // Comparison function
        float               minLod;             // Minimum LOD
        float               maxLod;             // Maximum LOD
        BORDER_COLOR        borderColor;        // Border color
    };

    struct SampleCount {
        uint32_t            count;          // 1,2,4,8,....
    };

    // Static sampler (D3D12 static samplers, optional for Vulkan)
    struct StaticSamplerDesc {
        uint32_t            binding;        // Binding/slot index
        uint32_t            space;          // Register space / descriptor set index
        SHADER_VISIBILITY   visibility;     // Which shader stages can access this sampler

        SamplerDesc         sampler;

    };

    // Pipeline layout flags
    //enum PIPELINE_LAYOUT_FLAGS : uint32_t {
    //    PIPELINE_LAYOUT_FLAG_NONE = 0,
    //    PIPELINE_LAYOUT_FLAG_ALLOW_INPUT_ASSEMBLER = 1,  // D3D12: D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    //};

    // Root descriptor: (typeless)




    // Push constant(Vulkan bind=[[vk::push_constant]]) / root constant (D3D bind=(b0, space0))

    struct PushConstantDesc {
        uint32_t                    size;                   // Size in bytes (<= 128, typical)
        SHADER_VISIBILITY           visibility;             // Which shader stages can access this range
    };

    struct PushDescriptorDesc {
        DESCRIPTOR_TYPE             type;                   // Type of descriptor
        uint32_t                    baseBinding;            // Starting binding/slot index, start at 1 recommended
        SHADER_VISIBILITY           visibility;             // Which shader stages can access this push descriptor
        VOLATILITY                  volatility;             // Descriptor Volatility
    };

    // set/space: [push] [static] [buffer]
    struct PipelineLayoutDesc {
        // OPT
        const PushDescriptorDesc*   pushDescriptors;
        uint32_t                    pushDescriptorCount;

        // OPT
        uint32_t                    staticSamplerCount;     // Number of static samplers (optional)
        const StaticSamplerDesc*    staticSamplers;         // Array of static sampler descriptions

        // OPT
        IRHIDescriptorBuffer*       descriptorBuffer;

        // OPT
        PushConstantDesc            pushConstant;           // Only one here

        // OPT
        bool                        inputAssembler;         // D3D12: D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
        bool                        staticSamplerOnBack;    // set/space: [push] [buffer] [static]
    };

    enum PASS_LOAD_OP : uint32_t {
        PASS_LOAD_OP_LOAD = 0,
        PASS_LOAD_OP_CLEAR,
        PASS_LOAD_OP_DONT_CARE,
        PASS_LOAD_OP_NONE,
        PASS_LOAD_OP_PRESERVE_LOCAL,    // Tile-Based
    };

    enum PASS_STORE_OP : uint32_t {
        PASS_STORE_OP_STORE = 0,
        PASS_STORE_OP_DONT_CARE,
        PASS_STORE_OP_NONE,             // NO_ACCESS
        PASS_STORE_OP_PRESERVE_LOCAL,   // Tile-Based
        PASS_STORE_OP_RESOLVE,          // D3D12 MSAA resolve
    };

    struct ColorAttachmentDesc {
        RHI_FORMAT              format;
        PASS_LOAD_OP            load;
        PASS_STORE_OP           store;
    };

    struct DepthStencilAttachmentDesc {
        RHI_FORMAT              format;
        PASS_LOAD_OP            depthLoad;
        PASS_STORE_OP           depthStore;
        PASS_LOAD_OP            stencilLoad;
        PASS_STORE_OP           stencilStore;

    };
#if 0
    struct RenderPassDesc {
        SampleCount                     sampleCount; 
        uint32_t                        colorAttachmentCount;
        ColorAttachmentDesc             colorAttachment[8];
        DepthStencilAttachmentDesc      depthStencil;
    };
#endif

    enum VERTEX_INPUT_RATE : uint32_t {
        VERTEX_INPUT_RATE_VERTEX = 0,
        VERTEX_INPUT_RATE_INSTANCE
    };

    // D3D12 compatible vertex attribute semantics
    enum VERTEX_ATTRIBUTE_SEMANTIC : uint32_t {
        VERTEX_ATTRIBUTE_SEMANTIC_CUSTOM = 0,       // CUSTOM
        VERTEX_ATTRIBUTE_SEMANTIC_POSITION,         // POSITION - vertex position
        VERTEX_ATTRIBUTE_SEMANTIC_NORMAL,           // NORMAL - vertex normal
        VERTEX_ATTRIBUTE_SEMANTIC_COLOR,            // COLOR - vertex color
        VERTEX_ATTRIBUTE_SEMANTIC_TEXCOORD,         // TEXCOORD - texture coordinates
        VERTEX_ATTRIBUTE_SEMANTIC_TANGENT,          // TANGENT - tangent vector (for normal mapping)
        VERTEX_ATTRIBUTE_SEMANTIC_BINORMAL,         // BINORMAL/BITANGENT - binormal vector (for normal mapping)
        VERTEX_ATTRIBUTE_SEMANTIC_BLENDWEIGHT,      // BLENDWEIGHT - blend weights (for skeletal animation)
        VERTEX_ATTRIBUTE_SEMANTIC_BLENDINDICES,     // BLENDINDICES - blend indices (for skeletal animation)
        VERTEX_ATTRIBUTE_SEMANTIC_PSIZE,            // PSIZE - point size
        VERTEX_ATTRIBUTE_SEMANTIC_FOG,              // FOG - fog factor
        VERTEX_ATTRIBUTE_SEMANTIC_DEPTH,            // DEPTH - depth value
        VERTEX_ATTRIBUTE_SEMANTIC_MATRIX,           // MATRIX

        COUNT_OF_VERTEX_ATTRIBUTE_SEMANTIC
    };

    struct VertexBindingDesc {
        uint32_t             binding;       // D3D12: INPUT SLOT
        uint32_t             stride;        
        VERTEX_INPUT_RATE    inputRate;
    };

    // VIA reflection
    //enum : uint32_t { AUTO_LOCATION = ~uint32_t(0) };.
    // VIA binding + format
    //enum : uint32_t { AUTO_OFFSET = ~uint32_t(0) };

    enum PRIMITIVE_TOPOLOGY : uint32_t {
        PRIMITIVE_TOPOLOGY_POINT_LIST = 0,
        PRIMITIVE_TOPOLOGY_LINE_LIST,
        PRIMITIVE_TOPOLOGY_LINE_STRIP,
        PRIMITIVE_TOPOLOGY_TRIANGLE_LIST ,
        PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
        PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,
        PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY,
        PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY,
        PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY,
        PRIMITIVE_TOPOLOGY_PATCH_LIST,
    };

    struct VertexAttributeDesc {
        uint32_t                    binding;
        uint32_t                    location;       // 4 DWORDs = 1 location
        RHI_FORMAT                  format;
        uint32_t                    offset;
        // D3D12 compatible vertex attribute semantics
        VERTEX_ATTRIBUTE_SEMANTIC   semantic;
    };

    enum PRIMITIVE_RESTART_VALUE : uint32_t {
        PRIMITIVE_RESTART_VALUE_DISABLED = 0,
        PRIMITIVE_RESTART_VALUE_0xFFFF,
        PRIMITIVE_RESTART_VALUE_0xFFFFFFFF,

    };

    enum POLYGON_MODE : uint32_t {
        POLYGON_MODE_FILL = 0,              // Solid fill (D3D12: SOLID)
        POLYGON_MODE_LINE,                  // Wireframe (D3D12: WIREFRAME)
        POLYGON_MODE_POINT,                 // Point mode (D3D12: doesn't support)
    };

    enum CULL_MODE : uint32_t {
        CULL_MODE_NONE = 0,
        CULL_MODE_FRONT,
        CULL_MODE_BACK,
        //CULL_MODE_FRONT_AND_BACK,    // Vulkan only
    };

    enum FRONT_FACE : uint32_t {
        FRONT_FACE_CLOCKWISE = 0,
        FRONT_FACE_COUNTER_CLOCKWISE,
    };

    struct RasterizerDesc {
        POLYGON_MODE     polygonMode;                   // Fill mode (Vulkan: polygonMode, D3D12: FillMode)
        CULL_MODE        cullMode;                      // Cull mode (Vulkan: cullMode, D3D12: CullMode)
        FRONT_FACE       frontFace;                     // Front face winding (Vulkan: frontFace, D3D12: FrontCounterClockwise)
        
        bool             depthClampEnable;              // Enable depth clamping (Vulkan: depthClampEnable, D3D12: !DepthClipEnable)
        bool             rasterizerDiscardEnable;       // Discard all primitives (Vulkan: rasterizerDiscardEnable)
        
        bool             depthBiasEnable;               // Enable depth bias (Vulkan: depthBiasEnable)
        float            depthBiasConstantFactor;       // Depth bias constant factor (Vulkan: depthBiasConstantFactor, D3D12: DepthBias)
        float            depthBiasClamp;                // Depth bias clamp value (Vulkan: depthBiasClamp, D3D12: DepthBiasClamp)
        float            depthBiasSlopeFactor;          // Depth bias slope factor (Vulkan: depthBiasSlopeFactor, D3D12: SlopeScaledDepthBias)
        
        //float            lineWidth;                     // Line width (Vulkan: lineWidth, D3D12: fixed to 1.0)
    };

    // Depth/Stencil comparison operations (Vulkan: VkCompareOp, D3D12: D3D12_COMPARISON_FUNC)
    enum COMPARE_OP : uint32_t {
        COMPARE_OP_NEVER = 0,           // Never pass
        COMPARE_OP_LESS,                // Pass if incoming < stored
        COMPARE_OP_EQUAL,               // Pass if incoming == stored
        COMPARE_OP_LESS_OR_EQUAL,       // Pass if incoming <= stored
        COMPARE_OP_GREATER,             // Pass if incoming > stored
        COMPARE_OP_NOT_EQUAL,           // Pass if incoming != stored
        COMPARE_OP_GREATER_OR_EQUAL,    // Pass if incoming >= stored
        COMPARE_OP_ALWAYS,              // Always pass
    };

    // Stencil operations (Vulkan: VkStencilOp, D3D12: D3D12_STENCIL_OP)
    enum STENCIL_OP : uint32_t {
        STENCIL_OP_KEEP = 0,            // Keep the current value
        STENCIL_OP_ZERO,                // Set to 0
        STENCIL_OP_REPLACE,             // Replace with reference value
        STENCIL_OP_INCREMENT_AND_CLAMP,  // Increment and clamp to max
        STENCIL_OP_DECREMENT_AND_CLAMP,  // Decrement and clamp to 0
        STENCIL_OP_INVERT,               // Bitwise invert
        STENCIL_OP_INCREMENT_AND_WRAP,   // Increment and wrap
        STENCIL_OP_DECREMENT_AND_WRAP,   // Decrement and wrap
    };

    // Depth write mask (Vulkan: depthWriteEnable, D3D12: D3D12_DEPTH_WRITE_MASK)
    enum DEPTH_WRITE_MASK : uint32_t {
        DEPTH_WRITE_MASK_ZERO = 0,      // Disable depth writes
        DEPTH_WRITE_MASK_ALL,           // Enable depth writes
    };

    // Stencil operation state for front/back faces
    struct StencilOpState {
        STENCIL_OP          failOp;             // Operation when stencil test fails (Vulkan: failOp, D3D12: StencilFailOp)
        STENCIL_OP          passOp;             // Operation when stencil test passes (Vulkan: passOp, D3D12: StencilPassOp)
        STENCIL_OP          depthFailOp;        // Operation when depth test fails (Vulkan: depthFailOp, D3D12: StencilDepthFailOp)
        COMPARE_OP          compareOp;          // Comparison function (Vulkan: compareOp, D3D12: StencilFunc)
        uint32_t            reference;          // Stencil reference value (Vulkan: reference, D3D12: set via command)
    };

    struct DepthStencilDesc {
        // Depth testing
        COMPARE_OP          depthCompareOp;           // Depth comparison function (Vulkan: depthCompareOp, D3D12: DepthFunc)
        float               minDepthBounds;           // Minimum depth bound (Vulkan: minDepthBounds, D3D12: not directly supported)
        float               maxDepthBounds;           // Maximum depth bound (Vulkan: maxDepthBounds, D3D12: not directly supported)
        bool                depthTestEnable;          // Enable depth testing (Vulkan: depthTestEnable, D3D12: DepthEnable)
        bool                depthWriteEnable;         // Enable depth writes (Vulkan: depthWriteEnable, D3D12: DepthWriteMask)
        bool                depthBoundsTestEnable;    // Enable depth bounds testing (Vulkan: depthBoundsTestEnable, D3D12: not directly supported)

        // Stencil testing
        bool                stencilTestEnable;         // Enable stencil testing (Vulkan: stencilTestEnable, D3D12: StencilEnable)
        uint32_t            stencilReadMask;           // Stencil compare mask (Vulkan: compareMask, D3D12: StencilReadMask)
        uint32_t            stencilWriteMask;          // Stencil write mask (Vulkan: writeMask, D3D12: StencilWriteMask)
        StencilOpState      front;                     // Stencil operations for front faces (Vulkan: front, D3D12: FrontFace)
        StencilOpState      back;                      // Stencil operations for back faces (Vulkan: back, D3D12: BackFace)
    };

    // Blend factors (Vulkan: VkBlendFactor, D3D12: D3D12_BLEND)
    enum BLEND_FACTOR : uint32_t {
        BLEND_FACTOR_ZERO = 0,                        // 0, 0, 0, 0
        BLEND_FACTOR_ONE,                             // 1, 1, 1, 1
        BLEND_FACTOR_SRC_COLOR,                       // Rs, Gs, Bs, As
        BLEND_FACTOR_ONE_MINUS_SRC_COLOR,             // 1-Rs, 1-Gs, 1-Bs, 1-As
        BLEND_FACTOR_DST_COLOR,                       // Rd, Gd, Bd, Ad
        BLEND_FACTOR_ONE_MINUS_DST_COLOR,             // 1-Rd, 1-Gd, 1-Bd, 1-Ad
        BLEND_FACTOR_SRC_ALPHA,                       // As, As, As, As
        BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,             // 1-As, 1-As, 1-As, 1-As
        BLEND_FACTOR_DST_ALPHA,                       // Ad, Ad, Ad, Ad
        BLEND_FACTOR_ONE_MINUS_DST_ALPHA,             // 1-Ad, 1-Ad, 1-Ad, 1-Ad
        BLEND_FACTOR_CONSTANT_COLOR,                  // Rc, Gc, Bc, Ac (from blend constants)
        BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,        // 1-Rc, 1-Gc, 1-Bc, 1-Ac
        BLEND_FACTOR_CONSTANT_ALPHA,                  // Ac, Ac, Ac, Ac (from blend constants)
        BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,        // 1-Ac, 1-Ac, 1-Ac, 1-Ac
        BLEND_FACTOR_SRC_ALPHA_SATURATE,             // f, f, f, 1 (f = min(As, 1-Ad))
        BLEND_FACTOR_SRC1_COLOR,                      // Rs1, Gs1, Bs1, As1 (dual-source blending)
        BLEND_FACTOR_ONE_MINUS_SRC1_COLOR,            // 1-Rs1, 1-Gs1, 1-Bs1, 1-As1
        BLEND_FACTOR_SRC1_ALPHA,                      // As1, As1, As1, As1
        BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA,            // 1-As1, 1-As1, 1-As1, 1-As1
    };

    // Blend operations (Vulkan: VkBlendOp, D3D12: D3D12_BLEND_OP)
    enum BLEND_OP : uint32_t {
        BLEND_OP_ADD = 0,                             // src + dst
        BLEND_OP_SUBTRACT,                            // src - dst
        BLEND_OP_REVERSE_SUBTRACT,                    // dst - src
        BLEND_OP_MIN,                                 // min(src, dst)
        BLEND_OP_MAX,                                 // max(src, dst)
    };

    // Logic operations (Vulkan: VkLogicOp, D3D12: D3D12_LOGIC_OP)
    enum LOGIC_OP : uint32_t {
        LOGIC_OP_CLEAR = 0,                           // 0
        LOGIC_OP_AND,                                 // src & dst
        LOGIC_OP_AND_REVERSE,                         // src & ~dst
        LOGIC_OP_COPY,                                // src
        LOGIC_OP_AND_INVERTED,                        // ~src & dst
        LOGIC_OP_NO_OP,                               // dst
        LOGIC_OP_XOR,                                 // src ^ dst
        LOGIC_OP_OR,                                  // src | dst
        LOGIC_OP_NOR,                                 // ~(src | dst)
        LOGIC_OP_EQUIVALENT,                          // ~(src ^ dst)
        LOGIC_OP_INVERT,                              // ~dst
        LOGIC_OP_OR_REVERSE,                          // src | ~dst
        LOGIC_OP_COPY_INVERTED,                       // ~src
        LOGIC_OP_OR_INVERTED,                         // ~src | dst
        LOGIC_OP_NAND,                                // ~(src & dst)
        LOGIC_OP_SET,                                 // 1
    };

    // Color component flags (Vulkan: VkColorComponentFlags, D3D12: D3D12_COLOR_WRITE_ENABLE)
    enum COLOR_COMPONENT_FLAG : uint32_t {
        COLOR_COMPONENT_FLAG_R = 1 << 0,              // Red component
        COLOR_COMPONENT_FLAG_G = 1 << 1,              // Green component
        COLOR_COMPONENT_FLAG_B = 1 << 2,              // Blue component
        COLOR_COMPONENT_FLAG_A = 1 << 3,              // Alpha component
        COLOR_COMPONENT_FLAG_ALL = COLOR_COMPONENT_FLAG_R | COLOR_COMPONENT_FLAG_G | COLOR_COMPONENT_FLAG_B | COLOR_COMPONENT_FLAG_A,
    };

    struct COLOR_COMPONENT_FLAGS { uint32_t flag; };

    // Blend attachment state (Vulkan: VkPipelineColorBlendAttachmentState, D3D12: D3D12_RENDER_TARGET_BLEND_DESC)
    struct BlendAttachmentDesc {
        bool                blendEnable;                // Enable blending (Vulkan: blendEnable, D3D12: BlendEnable)
        BLEND_FACTOR        srcColorBlendFactor;        // Source color blend factor (Vulkan: srcColorBlendFactor, D3D12: SrcBlend)
        BLEND_FACTOR        dstColorBlendFactor;        // Destination color blend factor (Vulkan: dstColorBlendFactor, D3D12: DestBlend)
        BLEND_OP            colorBlendOp;               // Color blend operation (Vulkan: colorBlendOp, D3D12: BlendOp)
        BLEND_FACTOR        srcAlphaBlendFactor;        // Source alpha blend factor (Vulkan: srcAlphaBlendFactor, D3D12: SrcBlendAlpha)
        BLEND_FACTOR        dstAlphaBlendFactor;        // Destination alpha blend factor (Vulkan: dstAlphaBlendFactor, D3D12: DestBlendAlpha)
        BLEND_OP            alphaBlendOp;               // Alpha blend operation (Vulkan: alphaBlendOp, D3D12: BlendOpAlpha)
        COLOR_COMPONENT_FLAGS colorWriteMask;           // Color write mask (Vulkan: colorWriteMask, D3D12: RenderTargetWriteMask)
    };

    struct BlendDesc {
        // Global blend state (Vulkan: VkPipelineColorBlendStateCreateInfo, D3D12: D3D12_BLEND_DESC)
        LOGIC_OP            logicOp;                    // Logic operation (Vulkan: logicOp, D3D12: LogicOp)
        bool                logicOpEnable;              // Enable logic operation (Vulkan: logicOpEnable, D3D12: LogicOpEnable)
        bool                alphaToCoverageEnable;      // Enable alpha-to-coverage (D3D12: AlphaToCoverageEnable, Vulkan: via multisample state)
        bool                independentBlendEnable;     // Independent blend per attachment (D3D12: IndependentBlendEnable, Vulkan: per attachment)
        float               blendConstants[4];          // Blend constants (Vulkan: blendConstants, D3D12: set via command)

        // Per-attachment blend state (Vulkan: pAttachments, D3D12: RenderTarget)
        uint32_t            attachmentCount;            // Number of color attachments (Vulkan: attachmentCount, D3D12: up to 8)
        BlendAttachmentDesc attachments[8];             // Blend state for each attachment (Vulkan: pAttachments, D3D12: RenderTarget[8])
    };

    struct BasicRenderPassDesc {
        // color attachment
        uint32_t                    colorFormatCount;
        RHI_FORMAT                  colorFormats[8];
        RHI_FORMAT                  depthStencil;
    };

    struct PipelineDesc {
        IRHIPipelineLayout*         layout;
#if 0
        IRHIRenderPass*             renderPass;
#endif
        IRHIShader*                 vs;
        IRHIShader*                 ps;
        IRHIShader*                 ds;
        IRHIShader*                 hs;
        IRHIShader*                 gs;


        // BasicRenderPass
        BasicRenderPassDesc         basicPass;

        // VertexInputState
        uint32_t                    vertexBindingCount;
        uint32_t                    vertexAttributeCount;
        const VertexBindingDesc*    vertexBinding;
        const VertexAttributeDesc*  vertexAttribute;

        // InputAssemblyState
        PRIMITIVE_TOPOLOGY          topology;
        PRIMITIVE_RESTART_VALUE     primitiveRestart;

        // ViewportState
        uint32_t                    maxViewportCount;
        uint32_t                    maxScissorCount;

        // MultisampleState
        SampleCount                 sampleCount;
        uint32_t                    sampleMask;
        // alphaToCoverageEnable BlendState.AlphaToCoverageEnable

        // RasterizationState 
        RasterizerDesc              rasterization;

        // DepthStencilState
        DepthStencilDesc            depthStencil;

        // ColorBlendState
        BlendDesc                   blend;

        // TODO: IRHIPipelineCache

    };

    struct DRHI_NO_VTABLE IRHIShader : IRHIBase {

    };

    struct DRHI_NO_VTABLE IRHIPipelineLayout : IRHIBase {

    };

    struct DRHI_NO_VTABLE IRHIPipeline : IRHIBase {

    };

    //struct IRHIRenderPass : IRHIBase { };

}}

