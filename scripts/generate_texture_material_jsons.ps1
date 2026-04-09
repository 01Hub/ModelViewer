param(
    [string]$Root = "D:\work\progs\Qt6\vcpkg\ModelViewer-Qt\textures\materials",
    [switch]$Overwrite
)

$ErrorActionPreference = 'Stop'

function Test-TokenMatch {
    param(
        [string]$Name,
        [string]$Token
    )
    $pattern = "(^|[^a-z0-9]){0}([^a-z0-9]|$)" -f [regex]::Escape($Token.ToLower())
    return [regex]::IsMatch($Name.ToLower(), $pattern)
}

function Get-BestMatch {
    param(
        [System.IO.FileInfo[]]$Files,
        [string[]]$Tokens,
        [string]$Type,
        [string]$PackedPath
    )

    $best = $null
    $bestTokenIndex = [int]::MaxValue
    $bestTokenPos = -1
    $bestExtIndex = [int]::MaxValue
    $exts = @("png", "jpg", "jpeg", "tga", "bmp", "dds", "hdr", "exr", "ktx2")

    foreach ($file in $Files) {
        if ($PackedPath -and $file.FullName -eq $PackedPath) { continue }
        $fname = [System.IO.Path]::GetFileNameWithoutExtension($file.Name).ToLower()
        $ext = $file.Extension.TrimStart('.').ToLower()
        $extIndex = [array]::IndexOf($exts, $ext)
        if ($extIndex -lt 0) { $extIndex = $exts.Count }

        for ($i = 0; $i -lt $Tokens.Count; $i++) {
            $tok = $Tokens[$i].ToLower()
            if (Test-TokenMatch -Name $fname -Token $tok) {
                $tokenPos = $fname.LastIndexOf($tok)
                $better = $false
                if ($i -lt $bestTokenIndex) {
                    $better = $true
                } elseif ($i -eq $bestTokenIndex) {
                    if ($tokenPos -gt $bestTokenPos) {
                        $better = $true
                    } elseif ($tokenPos -eq $bestTokenPos -and $extIndex -lt $bestExtIndex) {
                        $better = $true
                    }
                }

                if ($better) {
                    $best = $file
                    $bestTokenIndex = $i
                    $bestTokenPos = $tokenPos
                    $bestExtIndex = $extIndex
                }
                break
            }
        }
    }

    return $best
}

function New-MapEntry {
    param([string]$FileName)
    return [ordered]@{
        file = $FileName
        magFilter = "linear"
        minFilter = "linear_mipmap_linear"
        offset = @(0, 0)
        packing = [ordered]@{
            bias = 0
            channel = 0
            invert = $false
            scale = 1
        }
        rotation = 0
        scale = @(1, 1)
        texCoord = 0
        wrapS = "repeat"
        wrapT = "repeat"
    }
}

$tokens = @{
    albedo = @("alb", "albedo", "basecolor", "base_color", "base-color", "base", "diffuse", "diff", "col", "color")
    metallic = @("spec", "specular", "metallic", "metalness", "metal", "m")
    roughness = @("roughness", "rough", "r")
    normal = @("normal", "normalgl", "normalmap", "normal_map", "nor", "nrm", "nm", "_n", "-n", "n")
    ao = @("ao", "ambientocclusion", "ambient_occlusion", "occ", "occlusion")
    emissive = @("emissive", "emit", "emission", "glow", "light")
    opacity = @("opacity", "alpha", "transparency", "mask", "opa")
    height = @("height", "disp", "displacement", "bump")
    transmission = @("transmission", "trans", "glass")
    ior = @("ior", "indexofrefraction", "refract")
    sheenColor = @("sheen", "sheen_color")
    sheenRoughness = @("sheenrough", "sheen_rough", "sheenroughness")
    clearcoatColor = @("clearcoat", "cc_color")
    clearcoatRoughness = @("cc_rough", "clearcoatroughness", "clearcoat_rough")
    clearcoatNormal = @("cc_normal", "clearcoatnormal", "clearcoat_normal")
}

$packedAormTokens = @(
    "aorm", "aormap", "aor", "ormap", "orm",
    "occlusion_roughness_metallic", "occlusionroughnessmetallic",
    "occlusion_roughness_metal", "occlusion_roughness_metalmap",
    "occlusion-roughness-metallic", "ao_rm", "ao_rm_map", "ao_r_m",
    "ao-rm", "aor_map", "orm_map", "arm"
)

$defaultMaterial = [ordered]@{
    albedoColor = @(0.9, 0.9, 0.9)
    albedoTint = [ordered]@{
        grayThreshold = 0.02
        maskChannel = 0
        mode = 1
        strength = 1
        useVertexColor = $false
    }
    anisotropyStrength = 0
    clearcoat = 0
    clearcoatNormalScale = 1
    clearcoatRoughness = 0
    diffuseTransmissionColor = @(1, 1, 1)
    diffuseTransmissionFactor = 0
    emissiveColor = @(0, 0, 0)
    emissiveStrength = 1
    heightScale = 0.02
    ior = 1.5
    iridescenceFactor = 0
    iridescenceIor = 1.3
    iridescenceThicknessMax = 400
    iridescenceThicknessMin = 100
    metalness = 0
    normalScale = 1
    roughness = 0.45
    sheenColor = @(0, 0, 0)
    sheenRoughness = 0
    specularColor = @(1, 1, 1)
    specularFactor = 1
    thicknessFactor = 0
    transmission = 0
}

$dirs = Get-ChildItem -Path $Root -Directory | Sort-Object Name
$created = 0
$skipped = 0

foreach ($dir in $dirs) {
    $jsonPath = Join-Path $dir.FullName "material.json"
    if ((Test-Path $jsonPath) -and -not $Overwrite) {
        $skipped++
        continue
    }

    $files = Get-ChildItem -Path $dir.FullName -File | Where-Object {
        @(".png", ".jpg", ".jpeg", ".tga", ".bmp", ".dds", ".hdr", ".exr", ".ktx2") -contains $_.Extension.ToLower()
    }

    $maps = [ordered]@{}
    $packedPath = $null

    foreach ($file in $files) {
        $fname = [System.IO.Path]::GetFileNameWithoutExtension($file.Name).ToLower()
        foreach ($packedToken in $packedAormTokens) {
            if (Test-TokenMatch -Name $fname -Token $packedToken) {
                $packedPath = $file.FullName
                break
            }
        }
        if ($packedPath) { break }
    }

    if ($packedPath) {
        $packedFile = Split-Path -Leaf $packedPath
        foreach ($key in @("ao", "roughness", "metallic")) {
            $entry = New-MapEntry -FileName $packedFile
            switch ($key) {
                "ao" { $entry.packing.channel = 0 }
                "roughness" { $entry.packing.channel = 1 }
                "metallic" { $entry.packing.channel = 2 }
            }
            $maps[$key] = $entry
        }
    }

    foreach ($type in $tokens.Keys) {
        if ($packedPath -and @("ao", "roughness", "metallic") -contains $type) { continue }
        $match = Get-BestMatch -Files $files -Tokens $tokens[$type] -Type $type -PackedPath $packedPath
        if ($match) {
            $maps[$type] = New-MapEntry -FileName $match.Name
        }
    }

    $rootObject = [ordered]@{
        maps = $maps
        material = $defaultMaterial
        name = $dir.Name
        version = 1
    }

    $json = $rootObject | ConvertTo-Json -Depth 8
    Set-Content -Path $jsonPath -Value $json -Encoding UTF8
    $created++
}

Write-Host ("Generated {0} material.json files; skipped {1} existing files." -f $created, $skipped)

