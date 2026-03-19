platform=""
if [ "$(uname)" = "Darwin" ]; then
	platform="Mac"
fi

if [ "$(uname)" = "Linux" ]; then
	platform="Linux"
fi

**RUN_UAT_RELATIVEPATH**/RunUAT.sh BuildGraph -Script=**REPLACE_PROJECTPATH**/Build/AutoPerfTests.xml -Target="BuildAndTest **REPLACE_PROJECTNAME**" -UseLocalBuildStorage -set:TargetPlatforms=$platform -set:WithAPTSequenceTests=true -set:TargetConfigurations=Test -set:DoPerf=true -set:DoLLM=true -set:GenerateLocalReports=true -set:GenerateLocalReports=true -set:IterationsPerf=1 -set:IterationsLLM=1 -set:IterationsInsights=1
