# Copyright Epic Games, Inc. All Rights Reserved.

from enum import Enum
from pathlib import Path
import unrealcmd
import unreal

#-------------------------------------------------------------------------------
class PerfTestType(Enum):
    SEQUENCE    = 0x00
    REPLAY      = 0x01
    MATERIAL    = 0x02
    CAMERA      = 0x03

    def as_arg(self):
        match self:
            case PerfTestType.SEQUENCE:
                return "AutomatedPerfTest.SequenceTest"
            case PerfTestType.REPLAY:
                return "AutomatedPerfTest.ReplayTest"
            case PerfTestType.MATERIAL:
                return "AutomatedPerfTest.MaterialTest"
            case PerfTestType.CAMERA:
                return "AutomatedPerfTest.StaticCameraTest"


#-------------------------------------------------------------------------------
class PerfSubTestType(Enum):
    PERF     = 0x00
    LLM      = 0x01
    INSIGHTS = 0x02
    GPUPERF  = 0x03
    ALL      = 0x04

    def as_arg(self):
        match self:
            case PerfSubTestType.PERF:
                return "AutomatedPerfTest.DoPerf"
            case PerfSubTestType.LLM:
                return "AutomatedPerfTest.DoLLM"
            case PerfSubTestType.INSIGHTS:
                return "AutomatedPerfTest.DoInsightsTrace"
            case PerfSubTestType.GPUPERF:
                return "AutomatedPerfTest.DoGPUPerf"

    @staticmethod
    def parse(external:str) -> "PerfSubTestType":
        try: return PerfSubTestType[external.upper()]
        except: raise ValueError(f"Unknown Sub Test type '{external}'")


#-------------------------------------------------------------------------------
class PerfTestResult(Enum):
    SUCCESS     = 0
    ERROR       = 1

    def __get__(self, instance, owner):
        return self.value


#-------------------------------------------------------------------------------
class _PerfTestConfig(object):
    iterations  = {}
    resx        = 0
    resy        = 0
    ignore_log  = False

    def load_defaults(self, xml_text : str):
        import xml.etree.ElementTree as ET
        tree = ET.fromstring(xml_text)

        DEFAULT_VAL_KEY = 'DefaultValue'
        def _get_option(name:str, attrib_key = DEFAULT_VAL_KEY) -> str:
            # Currently only checks for options. Properties may need
            # variable evaluation and I don't think we want to go there
            # yet.
            try:
                return tree.find(".//{*}Option" + f"[@Name='{name}']").attrib[attrib_key]
            except:
                return None

        def _get_bool(name:str, attrib_key = DEFAULT_VAL_KEY) -> bool:
            val = _get_option(name, attrib_key)
            return True if val is not None and val.lower() == "true" else False

        # More default values can be extracted if needed.
        self.iterations = {
            PerfSubTestType.PERF     : _get_option("IterationsPerf"),
            PerfSubTestType.LLM      : _get_option("IterationsLLM"),
            PerfSubTestType.INSIGHTS : _get_option("IterationsInsights"),
            PerfSubTestType.GPUPERF  : _get_option("IterationsGPUPerf")
        }

        self.resx       = _get_option("ResX")
        self.resy       = _get_option("ResY")
        self.ignore_log = _get_bool("APTIgnoreTestBuildLogging")

    def get_iterations(self, subtest : PerfSubTestType) -> int:
        if subtest in self.iterations:
            if self.iterations[subtest] is not None:
                return self.iterations[subtest]

        # Return default values if a subtest iteration value
        # does not exist.
        DEFAULT_ITERATIONS  = 3
        PERF_ITERATIONS     = 6
        LLM_ITERATIONS      = 3
        INSIGHTS_ITERATIONS = 1
        GPUPERF_ITERATIONS  = 3

        match subtest:
            case PerfSubTestType.PERF:
                return PERF_ITERATIONS
            case PerfSubTestType.LLM:
                return LLM_ITERATIONS
            case PerfSubTestType.INSIGHTS:
                return INSIGHTS_ITERATIONS
            case PerfSubTestType.GPUPERF:
                return GPUPERF_ITERATIONS
        return DEFAULT_ITERATIONS


#-------------------------------------------------------------------------------
class PerfTestBaseCmd(unrealcmd.Cmd):
    """
    Automated Perf Test(APT) - This command runs the specified performance test on
    a given APT plugin-enabled project and generates a performance report. You can choose
    to run any or all of the following sub tests:

    - Perf:     This is the default perf run. This collects timing metrics such as
    GameThread time, RHIThread time, Average FPS etc.

    - LLM:      This subtest collects memory related metrics. The project needs LLM
    tracking enabled for this to work as expected.

    - GPUPerf:  This subtest collects GPU/Graphics related performance metrics and is
    suitable for testing shader performance etc.

    - Insights: Enables insights trace and runs the test. A utrace file is generated
    which can be used to further do performance investigations.
    """
    host        = unreal.Platform.get_host()
    platform    = unrealcmd.Arg(host,       "Platform to run the test on")
    subtest     = unrealcmd.Arg("all",      "Subtest Type")
    variant     = unrealcmd.Arg("test",     "Build variant to use for the test")
    target      = unrealcmd.Arg("game",     "Cooked target to use")
    repeat      = unrealcmd.Opt(0,          "Number of iterations to run the test")
    build       = unrealcmd.Opt("",         "Use an alternative staged directory")
    targetname  = unrealcmd.Opt("",         "Override target name")
    uatargs     = unrealcmd.Arg([str],      "Additional arguments to pass to UAT")
    # --- Miscellaneous
    resx        = unrealcmd.Opt(0,          "Res X")
    resy        = unrealcmd.Opt(0,          "Res Y")
    fps_chart   = unrealcmd.Opt(False,      "Enable FPS Chart")
    debug_mem   = unrealcmd.Opt(False,      "Enable debug memory on platform if available")
    testid      = unrealcmd.Opt("",         "Test ID for perf test")
    # --- Config
    config      = _PerfTestConfig()

    def run_perf_subtest(self, test_type: PerfTestType, subtest: PerfSubTestType, *args):
        self.print_info(f"Initializing {test_type.name.lower()} Test")
        print("Arguments: ", *args)

        ue_context      = self.get_unreal_context()
        platform_name   = self.args.platform.lower()
        target_type     = unreal.TargetType.parse(self.args.target)
        target          = ue_context.get_target_by_type(target_type)
        iterations      = self.args.repeat
        project         = ue_context.get_project()
        platform        = self.get_platform(platform_name)
        cook_form       = platform.get_cook_form(target_type.name.lower())
        stage_dir       = project.get_dir() / f"Saved/StagedBuilds/{cook_form}"

        # Load defaults from config if required.
        iterations  = iterations if iterations != 0 else self.config.get_iterations(subtest)
        resx        = self.args.resx if self.args.resx != 0 else self.config.resx
        resy        = self.args.resy if self.args.resy != 0 else self.config.resy
        debug_mem   = self.args.debug_mem or subtest == PerfSubTestType.LLM or subtest == PerfSubTestType.INSIGHTS
        fps_chart   = self.args.fps_chart

        if not project:
            self.print_error("No project found in context")
            return PerfTestResult.ERROR

        test_id = f"{project.get_name()}-{self.args.subtest}-autoperftest-ushell"
        test_id = test_id if self.args.testid == "" else self.args.testid

        if not target:
            self.print_error(f"'{project.get_name()}' does not appear to have a '{self.args.target}' target")
            return PerfTestResult.ERROR

        target_name = target.get_name()
        if self.args.targetname != "":
            target_name = self.args.targetname

        if self.args.build != "":
            build_dir = Path(self.args.build)
            if build_dir.exists() and (build_dir/ "Engine").exists():
                stage_dir = build_dir
                print(f"Using staged build path in args")
            else:
                self.print_error(f"Directory {build_dir} does not exist or expected 'Engine' folder not present")
                return PerfTestResult.ERROR

        self.print_info("Running Automated Perf Test")
        print("Sub Test:",      subtest.name.lower())
        print("Iterations:",    iterations)
        print("Variant:",       self.args.variant)
        print("Target:",        target_name)
        print("Platform:",      self.args.platform)
        print("Staged Build:",  stage_dir)
        print("------")

        perf_args = (
            f"-test={test_type.as_arg()}",
            f"-{subtest.as_arg()}",
            f"-platform={platform_name}",
            f"-configuration={self.args.variant}",
            f"-iterations={iterations}",
            f"-target={target_name}",
            f"-build={stage_dir}",
            f"-AutomatedPerfTest.TestID={test_id}",
            "-AutomatedPerfTest.IgnoreTestBuildLogging" if self.config.ignore_log else None,
            "-AutomatedPerfTest.UsePlatformExtraDebugMemory" if debug_mem else None,
            "-AutomatedPerfTest.DoCSVProfiler",
            "-AutomatedPerfTest.TraceChannels=default,screenshot,stats",
            "-AutomatedPerfTest.DoFPSChart" if fps_chart else None,
            f"-resX={resx}",
            f"-resY={resy}",
            "-LocalReports",
            *args
        )

        return self.run_perftest_uat_command(*perf_args)

    def run_perftest(self, test_type: PerfTestType, *args):
        self._load_defaults()
        subtest = PerfSubTestType.parse(self.args.subtest)
        if subtest == PerfSubTestType.ALL:
            results:list[int] = []
            subtests = [test for test in PerfSubTestType if test != PerfSubTestType.ALL]
            self.print_info(f"Running all available subtests")
            print([test.name.lower() for test in subtests])
            for perf_subtest in subtests:
                result = self.run_perf_subtest(test_type, perf_subtest, *args)
                results.append(result)
            return PerfTestResult.SUCCESS if all(ret == PerfTestResult.SUCCESS for ret in results) else PerfTestResult.ERROR
        else:
            return self.run_perf_subtest(test_type, subtest, *args)

    def run_perftest_uat_command(self, *args):
        dot_uat_cmd = "_uat"
        dot_uat_args = (
            "RunUnreal",
            "--",
            *args
        )
        exec_context = self.get_exec_context()
        cmd = exec_context.create_runnable(dot_uat_cmd, *dot_uat_args)
        return cmd.run()

    def _load_defaults(self):
        SETTINGS_XML = "AutomatedPerfTestCommonSettings.xml"
        ue_context   = self.get_unreal_context()
        engine_dir   = ue_context.get_engine().get_dir()
        engine_dir   = engine_dir.joinpath("Plugins", "Performance", "AutomatedPerfTesting")
        settings     = self._find_file(SETTINGS_XML, "xml", engine_dir)

        if not settings:
            self.print_warning(f"Could not find Perf Test Common Settings file.")
            return

        xml_text = settings.read_text()
        self.config.load_defaults(xml_text)

        self.print_info("Loaded config defaults")

    def _find_file(self, name, ext = "*", dir:Path=None) -> Path:
        name = name.lower()
        for path in self._find_files(f"**/*.{ext}", dir):
            if path.name.lower() == name:
                return path
        return None

    def _find_files(self, pattern, dir:Path=None):
        if dir is None:
            # Default to Project Directory
            ue_context = self.get_unreal_context()
            project = ue_context.get_project()
            if not project:
                return None
            dir = project.get_dir()
        return dir.glob(pattern)

    #--- Tab Auto Complete

    def complete_subtest(self, prefix):
        yield from (subtest.name.lower() for subtest in PerfSubTestType)

    def complete_target(self, prefix):
        yield from (target.name.lower() for target in unreal.TargetType)


#-------------------------------------------------------------------------------
class Sequence(PerfTestBaseCmd):
    """
    Sequence Test - Runs a configured sequence in the project while collecting
    performance metrics.
    """
    SequenceComboName   = unrealcmd.Arg(str,  "Sequence Combo Name")
    config_section      = "/Script/AutomatedPerfTesting.AutomatedSequencePerfTestProjectSettings"
    config_key          = "MapsAndSequencesToTest"

    def main(self):
        args = (f"-AutomatedPerfTest.SequencePerfTest.MapSequenceName={self.args.SequenceComboName}",)
        return self.run_perftest(PerfTestType.SEQUENCE, *args)

    def _get_available_sequences(self) -> list[str]:
        config = self.get_unreal_context().get_config()
        sequences = config.get("Engine", self.config_section, self.config_key)
        return [sequence.ComboName for sequence in sequences]

    #--- Tab Auto Complete

    def complete_SequenceComboName(self, prefix):
        yield from self._get_available_sequences()


#-------------------------------------------------------------------------------
class Replay(PerfTestBaseCmd):
    """
    Replay Test - Runs performance test with given replay file.
    """
    ReplayFile  = unrealcmd.Arg(str,  "Replay file")

    def _find_replay(self, name):
        path = Path(name)
        if path.is_file():
            return path
        self.print_info("Searching for replay file...")
        return self._find_file(name, "replay")

    def main(self):
        replay = self._find_replay(self.args.ReplayFile)
        if replay is None:
            self.print_error(f"Could not find '{self.args.ReplayFile}'")
            return 1
        args = (f"-AutomatedPerfTest.ReplayPerfTest.ReplayName={replay}",)
        return self.run_perftest(PerfTestType.REPLAY, *args)


#-------------------------------------------------------------------------------
class Material(PerfTestBaseCmd):
    """
    Material Test.
    """
    def main(self):
        args = (None,)
        return self.run_perftest(PerfTestType.MATERIAL, *args)


#-------------------------------------------------------------------------------
class StaticCamera(PerfTestBaseCmd):
    """
    Static Camera Test.
    """
    MapName  = unrealcmd.Arg(str,  "Static Camera Test Map Name")
    def main(self):
        args = (f"-AutomatedPerfTest.StaticCameraPerfTest.MapName={self.args.MapName}",)
        return self.run_perftest(PerfTestType.CAMERA, *args)