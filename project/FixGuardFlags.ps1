param(
	[Parameter(Mandatory = $true)]
	[string]$DllPath
)

$bytes = [IO.File]::ReadAllBytes($DllPath)
$peOffset = [BitConverter]::ToInt32($bytes, 0x3c)
$fileHeader = $peOffset + 4
$sectionCount = [BitConverter]::ToUInt16($bytes, $fileHeader + 2)
$optionalHeaderSize = [BitConverter]::ToUInt16($bytes, $fileHeader + 16)
$optionalHeader = $fileHeader + 20
$magic = [BitConverter]::ToUInt16($bytes, $optionalHeader)
$dataDirectory = if ($magic -eq 0x20b) { $optionalHeader + 0x70 } else { $optionalHeader + 0x60 }
$loadConfigDirectory = $dataDirectory + (10 * 8)
$loadConfigRva = [BitConverter]::ToUInt32($bytes, $loadConfigDirectory)

if ($loadConfigRva -eq 0) {
	exit 0
}

$sections = $optionalHeader + $optionalHeaderSize
$loadConfigRaw = -1

for ($i = 0; $i -lt $sectionCount; $i++) {
	$section = $sections + ($i * 40)
	$virtualSize = [BitConverter]::ToUInt32($bytes, $section + 8)
	$virtualAddress = [BitConverter]::ToUInt32($bytes, $section + 12)
	$rawSize = [BitConverter]::ToUInt32($bytes, $section + 16)
	$rawPointer = [BitConverter]::ToUInt32($bytes, $section + 20)
	$sectionSize = [Math]::Max($virtualSize, $rawSize)

	if ($loadConfigRva -ge $virtualAddress -and $loadConfigRva -lt ($virtualAddress + $sectionSize)) {
		$loadConfigRaw = $rawPointer + ($loadConfigRva - $virtualAddress)
		break
	}
}

if ($loadConfigRaw -lt 0) {
	throw "Unable to map Load Config RVA 0x$($loadConfigRva.ToString('X')) to a file offset."
}

$guardFlagsOffset = $loadConfigRaw + 0x58
if ($guardFlagsOffset + 4 -gt $bytes.Length) {
	throw "GuardFlags offset is outside of the file."
}

[Array]::Clear($bytes, $guardFlagsOffset, 4)
[IO.File]::WriteAllBytes($DllPath, $bytes)
