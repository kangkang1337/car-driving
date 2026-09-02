# Smart Car Dashboard - PowerShell startup script
# Usage: powershell -ExecutionPolicy Bypass -File start.ps1

$ErrorActionPreference = "Stop"

# Kill existing Java processes
Write-Host "Killing any running Java processes..."
Get-Process java -ErrorAction SilentlyContinue | Stop-Process -Force

$projectDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $projectDir

Write-Host ""
Write-Host "Starting CarCloudServer..." -ForegroundColor Green
Write-Host ""

# Check Maven installation
$mavenPath = "C:\Program Files\apache-maven-3.9.16\bin\mvn.cmd"
if (-not (Test-Path $mavenPath)) {
    Write-Error "Maven not found at $mavenPath"
    exit 1
}

# Check environment variables
if (-not $env:HUAWEICLOUD_SDK_AK) {
    Write-Warning "HUAWEICLOUD_SDK_AK not set. Server will use mock data mode."
    Write-Warning "For live data, set environment variables as described in README_WEB.md"
}

# Run server
& $mavenPath "exec:java" "-Dexec.mainClass=CarCloudServer" "-q"

if ($LASTEXITCODE -ne 0) {
    Write-Error "Server startup failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}
