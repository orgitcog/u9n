<?xml version="1.0" encoding="utf-8"?>
<TpsData xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <Name>XED</Name>
  <!-- Software Name and Version  -->
<!-- Software Name: XED
    Download Link: https://github.com/intelxed/xed
    Version: 2024
    Notes: We will link in the import library which in turn will pull in the xed.dll
        -->
<Location>In perforce:
xed.dll under Engine/Binaries/Win64
x64_xed.lib under Engine/Source/Developer/Windows/LiveCoding/Private/LivePlusPlus</Location>
<Function>The library is used by Live++ to be able to do code modifications of running editor/game without restarting.</Function>
<Eula>https://github.com/intelxed/xed?tab=Apache-2.0-1-ov-file#readme</Eula>
  <RedistributeTo>
    <EndUserGroup>Licensee</EndUserGroup>
    <EndUserGroup>P4</EndUserGroup>
    <EndUserGroup>Git</EndUserGroup>
  </RedistributeTo>
  <LicenseFolder>/Engine/Source/ThirdParty/Licenses</LicenseFolder>
</TpsData>