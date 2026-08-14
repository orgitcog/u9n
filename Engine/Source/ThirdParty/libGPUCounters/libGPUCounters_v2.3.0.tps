<?xml version="1.0" encoding="utf-8"?>
<TpsData xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <Name>libGPUCounters (formerly known as hwcpipe)</Name>
  <!-- Software Name and Version  -->
<!-- Software Name: libGPUCounters (formerly known as hwcpipe)
    Download Link: https://github.com/ARM-software/libGPUCounters/releases/tag/2.3.0
    Version: 2.3.0
    Notes: When a critical GPU crash occurs (Vulkan Device Lost error), we dump the information provided by this library to the log before the application terminates, hoping it will give us more information to understand what happened.
        -->
<Location>Engine/Source/ThirdParty/HWCPipe</Location>
<Function>This library gathers critical performance data for GPU's made by ARM, like the Mali series.</Function>
<Eula>https://github.com/ARM-software/libGPUCounters/blob/2.3.0/LICENSE.md</Eula>
  <RedistributeTo>
    <EndUserGroup>Licensee</EndUserGroup>
    <EndUserGroup>P4</EndUserGroup>
    <EndUserGroup>Git</EndUserGroup>
  </RedistributeTo>
  <LicenseFolder>/Engine/Source/ThirdParty/Licenses</LicenseFolder>
</TpsData>