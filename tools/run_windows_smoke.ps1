param(
    [string]$BuildDir = "build-win",
    [string]$Configuration = "Release",
    [string]$VcpkgRoot = "",
    [int]$Samples = 3,
    [switch]$SkipConfigure,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Split-Path -Parent $ScriptDir
$ResolvedBuildDir = if ([System.IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $RepoRoot $BuildDir }

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command,
        [Parameter(ValueFromRemainingArguments = $true)]
        [string[]]$Arguments
    )

    Write-Host "> $Command $($Arguments -join ' ')"
    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Command failed with exit code $LASTEXITCODE"
    }
}

if ([string]::IsNullOrWhiteSpace($VcpkgRoot)) {
    if (-not [string]::IsNullOrWhiteSpace($env:VCPKG_ROOT)) {
        $VcpkgRoot = $env:VCPKG_ROOT
    }
    elseif (-not [string]::IsNullOrWhiteSpace($env:VCPKG_INSTALLATION_ROOT)) {
        $VcpkgRoot = $env:VCPKG_INSTALLATION_ROOT
    }
}

if (-not $SkipConfigure) {
    $ConfigureArgs = @(
        "-S", $RepoRoot,
        "-B", $ResolvedBuildDir,
        "-A", "x64",
        "-DHAKO_PDU_FOXGLOVE_BUILD_TESTS=ON",
        "-DHAKO_PDU_FOXGLOVE_BUILD_EXAMPLES=ON"
    )

    if (-not [string]::IsNullOrWhiteSpace($VcpkgRoot)) {
        $ToolchainFile = Join-Path $VcpkgRoot "scripts/buildsystems/vcpkg.cmake"
        if (-not (Test-Path -LiteralPath $ToolchainFile -PathType Leaf)) {
            throw "vcpkg toolchain file not found: $ToolchainFile"
        }
        $ConfigureArgs += "-DCMAKE_TOOLCHAIN_FILE=$ToolchainFile"
        $ConfigureArgs += "-DVCPKG_TARGET_TRIPLET=x64-windows"
        Write-Host "[run_windows_smoke.ps1] using vcpkg root: $VcpkgRoot"
    }
    else {
        Write-Warning "No vcpkg root was specified. CMake must be able to find Boost.Asio/Boost.Beast headers and Fast-CDR through its normal search paths."
    }

    Invoke-Checked cmake @ConfigureArgs
}

if (-not $SkipBuild) {
    Invoke-Checked cmake --build $ResolvedBuildDir --config $Configuration --parallel
}

Invoke-Checked ctest --test-dir $ResolvedBuildDir -C $Configuration --output-on-failure

$PublisherCandidates = @(
    (Join-Path $ResolvedBuildDir "$Configuration/cdr_publisher_example.exe"),
    (Join-Path $ResolvedBuildDir "cdr_publisher_example.exe")
)
$PublisherExe = $PublisherCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($PublisherExe)) {
    throw "cdr_publisher_example.exe was not found under $ResolvedBuildDir"
}

$EndpointConfig = Join-Path $RepoRoot "config/sample/endpoint_foxglove.json"
Write-Host "[run_windows_smoke.ps1] running publisher smoke with $Samples samples"
Invoke-Checked $PublisherExe $EndpointConfig $Samples

Write-Host "[run_windows_smoke.ps1] passed"
Write-Host "[run_windows_smoke.ps1] note: this smoke verifies build/tests and publisher lifecycle only; it does not prove a Foxglove Desktop client can connect through the local Windows network/firewall configuration."
