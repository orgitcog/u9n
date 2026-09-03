<?xml version="1.0" encoding="utf-8"?>
<TpsData xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <Name>ONNXRuntime</Name>
  <!-- Software Name and Version  -->
<!-- Software Name: ONNXRuntime
    Download Link: https://www.nuget.org/api/v2/package/Microsoft.ML.OnnxRuntime.DirectML/1.20.0
    Version: 1.20
    Notes: The library consists of header files and dynamically linked libraries. Both will be added and used in the engine. While the headers are compiled into the standalone application, the dynamic libraries are distributed along with it. The software is thus used internally, by licensees as well as end consumers.
       Please process this request also for UEFN and LiveLinkHub (not only UE5) as it will be needed by the MetaHuman (above 'Product' field does not allow mutli-select, but I noted it in the bulk request sheet).

        -->
<Location>/Engine/Plugins/NNE/NNERuntimeORT/Source/ThirdParty/Onnxruntime/</Location>
<Function>The software is used to run neural network inference through the onnx runtime backend and also to optimize ML models.</Function>
<Eula>https://www.nuget.org/packages/Microsoft.ML.OnnxRuntime.DirectML/1.20.0/License</Eula>
  <RedistributeTo>
    <EndUserGroup>Licensee</EndUserGroup>
    <EndUserGroup>P4</EndUserGroup>
    <EndUserGroup>Git</EndUserGroup>
  </RedistributeTo>
  <LicenseFolder>/Engine/Source/ThirdParty/Licenses</LicenseFolder>
</TpsData>
 