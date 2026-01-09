# Copyright Epic Games, Inc. All Rights Reserved.

$Working = [IO.Path]::Combine($env:LOCALAPPDATA, "ushell\.working")
if (![String]::IsNullOrEmpty($env:flow_working_dir)) {
    $Working = $env:flow_working_dir
}

$ProvisionBat = [IO.Path]::Combine($PSScriptRoot, "provision.bat")
& "cmd.exe" "/d/c" "`"$ProvisionBat`" $Working"

if (!$?) {
    throw "Ushell provisioning failed"
}


$PythonPath = [IO.Path]::Combine($Working, "python\current\flow_python.exe")
$Bootpy = [IO.Path]::Combine($PSScriptRoot, "..\core\system\boot.py")

$TempDir = [IO.Path]::Combine($env:TEMP, "ushell")

$Cookie = [IO.Path]::Combine($TempDir, "pwsh_boot_$(New-Guid).ps1")
& $PythonPath "-Xutf8" "-Esu" $Bootpy "--bootarg=pwsh,$Cookie" $Args | Out-Host
$PythonReturn = $?

if (!$PythonReturn) {
    throw "Python boot failed"
}

try {
    Import-Module $Cookie
}
finally {
    if (Test-Path $Cookie) {
        Remove-Item $Cookie
    }
}