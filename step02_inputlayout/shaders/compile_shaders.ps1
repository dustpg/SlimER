# Shader compilation script using DXC
# Compiles shader.hlsl to both Vulkan (.spv) and D3D12 (.cso) formats
# Separates Debug/Release and VS/PS variants

# Use VULKAN_SDK environment variable if available, otherwise fallback to common path
if ($env:VULKAN_SDK) {
    $DXC_PATH = Join-Path $env:VULKAN_SDK "Bin\dxc.exe"
} else {
    # Fallback: try to find Vulkan SDK in common location
    $vulkanSdkPath = Get-ChildItem "C:\VulkanSDK" -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending | Select-Object -First 1
    if ($vulkanSdkPath) {
        $DXC_PATH = Join-Path $vulkanSdkPath.FullName "Bin\dxc.exe"
    } else {
        Write-Host "Error: VULKAN_SDK environment variable not set and Vulkan SDK not found in C:\VulkanSDK" -ForegroundColor Red
        exit 1
    }
}
$SHADER_SRC = "shader.hlsl"
$OUTPUT_DIR = "."

# Check if DXC exists
if (-not (Test-Path $DXC_PATH)) {
    Write-Host "Error: DXC compiler not found at $DXC_PATH" -ForegroundColor Red
    exit 1
}

# Check if shader source exists
if (-not (Test-Path $SHADER_SRC)) {
    Write-Host "Error: Shader source not found: $SHADER_SRC" -ForegroundColor Red
    exit 1
}

Write-Host "Compiling shaders..." -ForegroundColor Green

# Vulkan SPIR-V compilation
Write-Host "`n=== Compiling Vulkan SPIR-V ===" -ForegroundColor Cyan

# VS Debug
Write-Host "Compiling VS Debug (SPIR-V)..." -ForegroundColor Yellow
& $DXC_PATH -spirv -T vs_6_0 -E VSMain -Od -Zi -Fo "shader_vs_debug.spv" $SHADER_SRC
if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to compile VS Debug (SPIR-V)" -ForegroundColor Red
}

# VS Release
Write-Host "Compiling VS Release (SPIR-V)..." -ForegroundColor Yellow
& $DXC_PATH -spirv -T vs_6_0 -E VSMain -O3 -Fo "shader_vs_release.spv" $SHADER_SRC
if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to compile VS Release (SPIR-V)" -ForegroundColor Red
}

# PS Debug
Write-Host "Compiling PS Debug (SPIR-V)..." -ForegroundColor Yellow
& $DXC_PATH -spirv -T ps_6_0 -E PSMain -Od -Zi -Fo "shader_ps_debug.spv" $SHADER_SRC
if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to compile PS Debug (SPIR-V)" -ForegroundColor Red
}

# PS Release
Write-Host "Compiling PS Release (SPIR-V)..." -ForegroundColor Yellow
& $DXC_PATH -spirv -T ps_6_0 -E PSMain -O3 -Fo "shader_ps_release.spv" $SHADER_SRC
if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to compile PS Release (SPIR-V)" -ForegroundColor Red
}

# D3D12 CSO compilation
Write-Host "`n=== Compiling D3D12 CSO ===" -ForegroundColor Cyan

# VS Debug
Write-Host "Compiling VS Debug (CSO)..." -ForegroundColor Yellow
& $DXC_PATH -T vs_6_0 -E VSMain -Od -Zi -Fo "shader_vs_debug.cso" $SHADER_SRC
if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to compile VS Debug (CSO)" -ForegroundColor Red
}

# VS Release
Write-Host "Compiling VS Release (CSO)..." -ForegroundColor Yellow
& $DXC_PATH -T vs_6_0 -E VSMain -O3 -Fo "shader_vs_release.cso" $SHADER_SRC
if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to compile VS Release (CSO)" -ForegroundColor Red
}

# PS Debug
Write-Host "Compiling PS Debug (CSO)..." -ForegroundColor Yellow
& $DXC_PATH -T ps_6_0 -E PSMain -Od -Zi -Fo "shader_ps_debug.cso" $SHADER_SRC
if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to compile PS Debug (CSO)" -ForegroundColor Red
}

# PS Release
Write-Host "Compiling PS Release (CSO)..." -ForegroundColor Yellow
& $DXC_PATH -T ps_6_0 -E PSMain -O3 -Fo "shader_ps_release.cso" $SHADER_SRC
if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to compile PS Release (CSO)" -ForegroundColor Red
}

Write-Host "`n=== Compilation Complete ===" -ForegroundColor Green
Write-Host "Generated files:" -ForegroundColor Cyan
$spvFiles = Get-ChildItem -Filter "shader_*.spv"
$csoFiles = Get-ChildItem -Filter "shader_*.cso"
$allFiles = $spvFiles + $csoFiles
$allFiles | ForEach-Object {
    Write-Host "  $($_.Name)" -ForegroundColor White
}

