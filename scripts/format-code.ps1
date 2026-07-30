param(
    [switch]$Check
)

$ErrorActionPreference = "Stop"

function Find-ClangFormat {
    $fromPath = Get-Command clang-format -ErrorAction SilentlyContinue
    if ($fromPath) {
        return $fromPath.Source
    }

    $candidates = @(
        "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-format.exe",
        "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\ARM64\bin\clang-format.exe",
        "D:\Qt\Tools\llvm-mingw1706_64\bin\clang-format.exe",
        "D:\Qt\Tools\QtCreator\bin\clang\bin\clang-format.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    throw "clang-format.exe was not found. Install LLVM or Visual Studio LLVM tools, or add clang-format to PATH."
}

$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$clangFormat = Find-ClangFormat
$files = Get-ChildItem -Path (Join-Path $root "src"), (Join-Path $root "tests") -Recurse -File |
    Where-Object {
        $_.Extension -in ".cpp", ".h", ".hpp", ".c", ".cc", ".cxx" -or
        $_.Name -match "^Q[A-Za-z]"
    }

if ($files.Count -eq 0) {
    return
}

if ($Check) {
    & $clangFormat --dry-run --Werror --style=file @($files.FullName)
} else {
    & $clangFormat -i --style=file @($files.FullName)
}

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
