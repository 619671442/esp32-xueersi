param(
    [string]$Action = "build"
)

$ProjectDir = Join-Path $PSScriptRoot "main_firmware"
$Port = "COM3"

switch ($Action.ToLower()) {
    "build" {
        Write-Host "========================================" -ForegroundColor Cyan
        Write-Host "  Xueersi Console - Build Firmware" -ForegroundColor Cyan
        Write-Host "========================================" -ForegroundColor Cyan
        Write-Host ""

        & platformio run --project-dir $ProjectDir

        if ($LASTEXITCODE -eq 0) {
            Write-Host ""
            Write-Host "[OK] Build success!" -ForegroundColor Green
        } else {
            Write-Host ""
            Write-Host "[ERROR] Build failed" -ForegroundColor Red
        }
    }
    "upload" {
        Write-Host "========================================" -ForegroundColor Cyan
        Write-Host "  Xueersi Console - Upload Firmware" -ForegroundColor Cyan
        Write-Host "========================================" -ForegroundColor Cyan
        Write-Host ""
        Write-Host "Port: $Port"
        Write-Host "Press RESET if stuck at 'Connecting...'"
        Write-Host ""

        & platformio run --project-dir $ProjectDir --target upload --upload-port $Port

        if ($LASTEXITCODE -eq 0) {
            Write-Host ""
            Write-Host "[OK] Upload success!" -ForegroundColor Green
        } else {
            Write-Host ""
            Write-Host "[ERROR] Upload failed" -ForegroundColor Red
        }
    }
    "all" {
        & $MyInvocation.MyCommand.Path "build"
        if ($LASTEXITCODE -eq 0) {
            & $MyInvocation.MyCommand.Path "upload"
        }
    }
    default {
        Write-Host "Usage: .\firmware.ps1 [build|upload|all]" -ForegroundColor Yellow
        Write-Host "  build   - compile only" -ForegroundColor Yellow
        Write-Host "  upload  - compile & upload" -ForegroundColor Yellow
        Write-Host "  all     - build + upload" -ForegroundColor Yellow
    }
}
