# PowerShell build helper: compiles the C++ source into multimedia_steg.exe
param(
    [string]$Out = "yogeshwari_encrypter_kavi.exe",
    [string]$Src = "yogeshwari_encrypter_kavi.cpp"
)

Write-Host "Building $Src -> $Out"
$cmd = "g++ -std=c++17 -O2 `"$Src`" -o `"$Out`""
Write-Host $cmd
$proc = Start-Process -FilePath powershell -ArgumentList "-NoProfile -Command $cmd" -Wait -PassThru
if($proc.ExitCode -eq 0){ Write-Host "Build succeeded." -ForegroundColor Green } else { Write-Host "Build failed (exit $($proc.ExitCode))." -ForegroundColor Red; exit $proc.ExitCode }
