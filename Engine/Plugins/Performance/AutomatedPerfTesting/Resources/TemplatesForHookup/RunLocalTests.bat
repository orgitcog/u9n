call **RUN_UAT_RELATIVEPATH**\RunUAT.bat BuildGraph -Script=**REPLACE_PROJECTPATH**/Build/AutoPerfTests.xml -Target="BuildAndTest **REPLACE_PROJECTNAME**" -UseLocalBuildStorage -set:TargetPlatforms=Win64 -set:TargetConfigurations=Test -set:TargetBootTest=false -set:WithAPTSequenceTests=true -set:DoPerf=true -set:DoLLM=true -set:GenerateLocalReports=true -set:IterationsPerf=1 -set:IterationsLLM=1 -set:IterationsInsights=1
pause

