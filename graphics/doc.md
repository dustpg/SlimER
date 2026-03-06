### RHI = Rendering Hardware Interface
因为打算写一个2D的渲染器, 功能有限, 本打算采用用各个API直接实现 Rendering Logic Layer. 之后发现了RHI层后, 发现实现一个RHI层非常不错. 最主要就是非常直观了解各个API的差异(不然只是用不同的扳手拧螺丝), 其他的什么 减少逻辑层代码, 扩展性强, 都是顺带的.
在学习D3D12/Vulkan时只是机械性地知道(甚至不知道)这个API需要怎么用, 在实现RHI的过程中, 几乎每个接口都在思考: 为什么D3D/Vulkan要这么设计? 对于学习来说很深刻, 否则只有一个印象: 需要一千行代码来渲染一个三角形

### 别学习OpenGL了
调试层实在是太重要了, 没有的话我估计第一步都不知道怎么处理. OpenGL没有自带的调试层实在不打算学习.

### Vulkan指南错了?
代码编写期间出现了一个有趣的问题, 使用新的VulkanSDK时, 验证层抓了一个问题, 很巧reddit出现同样问题的人
https://www.reddit.com/r/vulkan/comments/1o866s7/new_validation_error_after_updating_lunarg_sdk/
简单地说就是这段文本在编写时, Vulkan指南中资源分配是这样的:
 - renderFinishedSemaphore(one per frame in flight)
 - imageAvailableSemaphore(one per frame in flight)
 - inFlightFence(one per frame in flight)
但是实际应该这样:
 - renderFinishedSemaphore(one per swapchain image)
 - imageAvailableSemaphore(one per frame in flight)
 - inFlightFence(one per frame in flight)

只要 飞行帧数量 < 交换链图像数 验证层就会报告这个
 
 ref: https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html

### 动态渲染
在大致完成一个不需要顶点信息的hello triangle之后, 越发觉得renderpass对象过于负担. 查阅相关资料后, 发现如果不用子通道, Vulkan的动态渲染基本和RenderPass没有性能上的区别. 所以在hello triangle之后砍掉render pass对象(顺带劈掉frame buffer), 有时间再做fallback(意思就是不打算做了), 思路是有的, 大致就是维护一个Vulkan renderpass缓存.


### VertexBindingDesc / VertexAttributeDesc

因为Vulkan的接口更加严格, 所以更靠近Vulkan一点. 
 - VertexBindingDesc 顶点数据可能来着不同的Buffer, 一个来源就是一个.
 - VertexAttributeDesc 需要标明每个来源中数据布局

### 句柄VS指针
针对Buffer以及Image/Texture对象来说, 使用句柄表示的话更具有优势, 特别是固定环境中. 但是使用句柄的话就要维持一套句柄管理代码, 作为一个轻量级封装RHI来说过于复杂, 只是放在了TODO里面

### 飞行帧

飞行帧在 Slim RHI 层中拥有三个数值：编译时常量、创建时最大数量，以及当前使用的数量。三者之间的关系为：编译时常量 ≥ 创建时最大数量 ≥ 当前使用的数量。

#### 编译时常量

使用编译时常量的原因是可以直接使用编译期数组，避免向堆中申请几十个字节的空间，从而提升性能并简化内存管理。

#### 创建时最大数量

创建交换链时需要指定所需的飞行帧数量，但这个数量不会超过缓冲帧数量以及编译时常量的大小。创建后需要调用 `IRHISwapChain::GetDesc` 获取实际创建的数量。

#### 当前使用的数量

当前使用的数量支持动态切换状态。Slim RHI 的设计初衷是为了 GUI 的显示，GUI 的特性是可能很久都不会更新画面，但一旦更新就可能会在一秒内连续更新（典型的场景就是动画播放）。因此，通过动态调整当前使用的飞行帧数量，可以在低负载时节省资源，在高负载时保证流畅性。


### 资源绑定

可以说是RHI层最复杂的部分了, D3D12/Vulkan绑定模型差异实在太大了.

1. vulkan不分类型, d3d12分类型
```hlsl
[[vk::binding(0)]] Texture2D input1 : register(t0);
[[vk::binding(1)]] Texture2D input2 : register(t1);
[[vk::binding(2)]] cbuffer Matrix : register(b2)
{
     float4x4 model;
     float4x4 view;
     float4x4 proj;
};
```
这样编写的话存在明显的空洞, 可能会造成性能问题. 目前的解决方案是不管, 因为主要使用bindless绑定, 提供传统绑定方法只是至少要"能"吧?

或者使用偏移(dcx), 让空洞从d3d12移动到vulkan, 而vulkan的需要指定binding来看天然支持空洞. 
```
-fvk-t-shift 0 0 
-fvk-b-shift 100 0 
-fvk-u-shift 200 0 
```

经过较高强度的编码, 大概一个月才显示出了三角形, 再过了一个多月才显示纹理.

### Resource Barrier

根据搜索, 主流的解决方案是自动追踪状态, 自动插入屏障. 但是作为轻量级RHI需要这个吗. 先打上TODO吧


### RHI Pipeline Layout



#### DRHI绑定模型
 - 一个推送常量
 - 若干个推送描述符, 只能推送Buffer类型
 - 一个描述符Buffer
 - 静态采样器(只推荐测试用, 不推荐使用正式使用)
```

[Static Sampler]


[Push Constant] [Push Descriptor#0] [Push Descriptor#1] ... [Descriptor Buffer]
(Max 128bytes)                                                
                        |                    |            
                        v                    v          
                   Descriptor#0         Descriptor#1     
                                                  
```


#### Push Constant

Push Constant需要在D3D绑定槽位(甚至可以多个), Vulkan则只有一个(但是允许分段指定可见性). 
所以本RHI中一般使用(b0,space0)绑定Push Constant.

#### Push Descriptor


主要是D3D12限制只能推送Buffer类型(没有Meta信息), 纹理也能解释为Buffer.
Vulkan可以正常推送纹理， 如果不支持Push Descriptor会处理fallback.


#### Descriptor Buffer
针对D3D12的Heap, Vulkan的Sets进一步的抽象化, 各截取一部分.
 - Vulkan可以在Set上绑定各个资源, Descriptor Buffer就得行, D3D12做fallback
 - D3D12只能有一个B/T/U和一个S的Heap. 所以只能有一个Descriptor Buffer


D3D12需要在Heap上通过直接创建视图(即描述符本身), 但是Vulkan需要现有视图然后绑定在描述符上.
 - 所以直接RHI层抛弃视图概念, 直接把资源绑定在Descriptor Buffer
 - 所以会让Descriptor Buffer持有资源得引用, 需要通过Unbind手动清理
 - 如果需要延迟释放(飞行帧必须), 需要通过编写一个帮助对象辅助Unbind


#### Descriptor Buffer Fixed


固定绑定, 理论上只要中途会变化, 就必须per frame创建一个Descriptor Buffer.

具体上可让D3D12存在空洞(方便); 或者使用dxc的偏移, 空洞转移到vulkan(性能?). 

```
[Descriptor Buffer]
[Slot0] [Slot1] [Slot2] [Slot3] .... [SlotN]
  |      |         |      |            |
  V      v         v      V            V
 t0      b1        b2     t3          sN           
```

```
[Descriptor Buffer]
[Slot0] [Slot1] [Slot2] [Slot3] .... [SlotN]
  |      |         |      |            |
  V      v         v      V            V
 t0      b0        b1     t2          s0     
 +0      +100    +100    +0           +200    +SHIFT        
```

#### Descriptor Buffer Bindless

无绑定方案, 可以全局就一个Descriptor Buffer.

搜索了很多资料都推荐无绑定方案, 虽然可能多了一次间接访问, 但是避免了非常麻烦的绑定.
可以动态修改未使用的部分, 但是需要自己内部处理飞行帧的per frame数据.

```
[Descriptor Buffer]
[B] [T] [U] [S] 
 |   |   |   |
 v   v   v   v
[o] [o] [o] [o]
 |   |   |   |
 v   v   v   v
[X] [Y] [Z] [W]
```

#### D3D12 Descriptor

`GetDescriptorHandleIncrementSize`在自己的机器上拿到的大部分是32, 可能是微软当前所谓的"根描述符"使用受限制的原因.

#### Vulkan Descriptor

### 侵入式对象池

### Vulkan Timeline Chain

```
BAD:

|Frame#0|Frame#1|Frame#2|
|-------|-------|-------|
|SwapChain|

|wait:  |
|flight0|flight1|
|acquire|
|ava#0  |ava#1  |
|-------|-------|-------|
|Queue  |

|ava#0  |ava#1  |
|-------|-------|-------|
|chain@1|chain@2|

|chain@1|chain@2|
|-------|-------|-------|
|fin#0  |fin#1  |
|flight0|flight1


|SwapChain  |
|-------|-------|-------|
|wait   |
|fin#0  |fin#1  |
```

```
FINE:

|Frame#0|Frame#1|Frame#2|
|-------|-------|-------|
|SwapChain|

|wait:  |
|flight0|flight1|
|acquire|
|ava#0  |ava#1  |
|-------|-------|-------|
|Queue  |

|chain@0|chain@1|        <------
|ava#0  |ava#1  |
|-------|-------|-------|
|chain@1|chain@2|

|chain@1|chain@2|
|-------|-------|-------|
|fin#0  |fin#1  |
|flight0|flight1


|SwapChain  |
|-------|-------|-------|
|wait   |
|fin#0  |fin#1  |
```

```
FINE:

|Frame#0|Frame#1|Frame#2|
|-------|-------|-------|
|SwapChain|

|wait:  |
|flight0|flight1|
|acquire|
|ava#0  |ava#1  |
|-------|-------|-------|
|Queue  |

|ava#0  |ava#1  |
|-------|-------|-------|
|c#0@1  |c#1@2  |        <------

|c#0@1  |c#1@2  |        <------
|-------|-------|-------|
|fin#0  |fin#1  |
|flight0|flight1


|SwapChain  |
|-------|-------|-------|
|wait   |
|fin#0  |fin#1  |
```

