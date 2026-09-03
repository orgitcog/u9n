# Release Notes

## 2025-04-10

* Run tests for experimental plugin in Dockerfile builds (41526895)
* Add agent property for opting out from server-initiated updates of agent software
  In the case of agent running on workstation, this ability allows for more control and avoids the agent process restarting itself. (41526788)
* Do not run matchers on partial json log line entries.
  This prevents us from trying to match unreliable log lines which were only partially written out e.g. due to the process crashing mid output. (41526045)
* Fixed warnings in LocalizationIssueHandler.cs (41525967)
* Improved Horde's LocalizationIssueHandler to get ready for many C++ localization warnings to change to structured logs (UE_LOGFMT) (41519652)
* Change GlobalConfig.PostLoad and IPluginConfig.PostLoad methods to async
  This is to allow, in the future, config's to use async APIs for example to fetch secrets.
  Some tests had setup in constructors changed these to use the TestInitialize attribute. (41483040)
* Make update session call more robust
  * Catch HttpRequestException in addition to RpcException
  * Implement exponential backoff starting at 1s (max 20s)
  * Increase max retries from 3 to 8
  * Improve logging slightly to explain attempt count and delay (41414189)
* Add topological sort of plugins by their dependencies to call the PostLoad Method in the correct order (41410626)
* Add admin claim resolution for Horde auth mode
  Admin claims are now properly resolved when users sign in using Horde auth mode, matching the existing behavior in OIDC/Okta auth handlers. (41410594)
* Replace display field with unit field for Horde Analytics, and add Bytes, BytesPerSecond, Seconds, and Percentage options (41366205)
* Display the year when not current to reduce confusion when looking at historical jobs (41362940)
* Improve data fetching error and surface network connection as likely cause on job page (41362331)
* Fix step ETA times sometimes being in the past (41355458)
* Add better indicator for inidividual data points in Horde Analytics (41352087)
* Oidc Token - Added ability to read encrypted oidc configuration from a url. Similar to the horde url but encrypted and not exactly the same configuration layout. Is intended to be able to replace the horde url as a more generic option. --AuthConfigUrl for OidcToken.exe
  This should also hopefully cover most of the cases for submitting a oidc-configuration.json into source control.
  Also added --ConfigPath as a way to explicitly set the path to oidc-configuration.json if redistributing this outside of the ue engine sync.
  Lastly I added source code generators for json serialization and configuration builder to enable AOT compilation of OidcToken (which is not used yet). This also fixes most trimming warnings. (41351365)
* Fix issue with artifacts not properly loading between step navigation (41327584)
* Set minimum value for the Package and Content charts in Horde Analytics to zero (41316971)

## 2025-04-03

* Horde Analytics: Fix issue with sum values updating without matching on group (41278970)
* Improve tracing spans in job scheduler (41276718)
* Dashboard and Metrics tweak. Used the wrong metrics for the Package Dashboard. (41273073)
* Add Commit Viewer link to Slack message (41240323)
* Add Show Telemetry option to view on Conforms (41238287)
* Added the ability to specify/override the HordeTelemetryApi when using a HordeAnalyticscProvider in StudioTelemetry from either the commandline or the environment variables.
  Use -HordeTelemetryApi='api/v1/telemetry/xxxx' on the commandline or set UE_HORDE_TELEMETRY_API='api/v1/telemetry/xxxx' in the build environment.
  Moved some reporting aroudn in Horde Analytics to accomodate the changes:
  Moved IoStoreChunks and VirtualAsset telemetry for Fortnite to the product specfic API in Horde Analytics.
  Moved the Fortnite specifc metrics for Packages from Engine to the Fortnite specifc metrics file and dashboard.
  Removed traces of product names from Engine metrics and dashboard. (41228862)
* Add Wine override setting to JobDriver
  Applies to all jobs executed by this driver process, and overrides the value set by JobOption (which is better used for per-job settings). (41198833)
* Aded Creative pack chunk metrics to Horde analytics (41196875)
* Use agent's self-reported tool ID for tool selection during upgrades
  Agents now send this information to provide correct hinting for what tool to use. For example multi-platform, self-contained, or even custom tools for workstation-based agents. (41193761)
* Added Horde_TemplateName as a variable in the Horde Analytics dashboard (41193329)
* Added package metrics for product names
  Added Per Product sizes to Horde Analytics dashboard (41149815)
* Add an agent mode distinguishing between dedicated and workstation agents (41139404)
* Added Package view to Horde Analytics Dashboard. 
  This view will display sizes per package and total package sizes per install type ( Installed, Optional, IAS, IAD ).
  Added InstalType variable
  Added a space to Hardware Platfrom variable name (41136887)
* Added Package Metrics to Horde Analytics (41119806)
* Added IoStore Telemetry to Horde Analytics
  Changed the Target attribute name to Platform so it aligns with other use cases in the analytics backend
  Bumped the IoStoreChunk event schema to 8 (41114074)
* Revert updateFilterItems optimisations (41106999)

## 2025-03-27

* Remove log warnings in agent happening during a normal, clean shutdown (41102934)
* Use status color user preferences for error and warning issue colors and controls (41076656)
* Use short template query param to avoid URL char limit in job search (41075171)
* Improve empty shelf error message on preflights (41070379)
* Report agent's tool ID as a property (41065929)
* Add support for filtering by status and showing cancelled builds in job search (41028739)
* Run agent's management server on dedicated thread (41027669)
* Use a shared ticker update for assigning Jira build health issues (41021755)
* Added support for WITH_TESTS (41021713)
* Reference scheduled downtime from config homepage (41019958)
* Add documentation for Scheduled Downtime (41018403)
* Fix issue with device health report bombing (40989153)
* Rate limit jira to horde issue sync for slack and other considerations (40985841)
* Improve performance of updateFilterItems (40933175)
* Fix param name in XML comment (40907450)
* Fixed badly formed XML warning (40906936)
* Add shortened alias for templates query param on api/v1/jobs endpoint (40905858)

## 2025-03-13

* Add retry policy for blob writes when getting an upload redirect (40683860)
* Add issue summary badges to job label overview, capped at 20 (40652838)
* Perforce server page visual fixes & linking from status page (40650335)
* Dashboard side of device cleaning suipport (40650241)
* Updated supportsPartitionedWorkspace doc (40580744)
* Constrain jupiterdownloads to UGS (40577651)
* Improve description of OIDC client secret config value to clarify public vs confidential client requirements (40575283)
* Add tooltips to issue navigation buttons (40572897)
* Lower log level for socket send buffer teardown from warning to info. This reduces log "pollution" until a better communication channel for Horde-specific diagnostics can be implemented. (40543971)
* Make parameter boolean checks case insensitive on new build dialog (40540848)

## 2025-03-06

* Update approved licenses (HTML changes) (40539666)
* Properly encode zen backend into uartifact files (40510907)
* Add parent/child job relationships, dashboard side (40508326)
* Adding zen backend to artifacts controller uartifact generation via metadata (40502152)
* Add device cleaning information to devices endpoint (40463300)
* Remove incorrect return type metadata for PUT /api/v2/devices/{deviceId} (40461854)
* Added ability to specify backends in uartifact files and added a prototype of how it could be used in the create cloud artifact task to generate a uartifact file that UGS could sync. (40451830)
* Update notes on Epic's deployment (40349855)

## 2025-02-20

* Explain GitHub image registry auth better in docs (40164690)
* Add -snapshot parameter to BuildCookRun, which will retrieve the most recent snapshot published to Jupiter and cook incrementally on top of the fetched data. (40156692)
* Auto assign Horde issues to Jira owner when possible (40147283)
* Add support for automated device cleaning on CI and shared kits, server side changes (40143179)
* Extend compute socket test timeouts to eliminate flakiness Tests were intermittently failing when CI resources were under load. (40137698)
* Allow service accounts to modify devices (40134987)
* Added verification that interfaces have matching native interface base classes (40091963)
* Warn when OIDC is configured with confidential client as this will prevent command-line auth (40060901)
* Cloud DDC - Fixed exception when a object id is encountered as we convert it to json (39986461)

## 2025-02-13

* Return explicit 500 status for VCS operation failures in create job REST endpoint (39941708)
* Downgrade Slack message failure from error to warning log level (39940773)
* Handle gRPC client disconnects more gracefully for log tail streams (39940732)
* AutomationHub, use ref time instead of CL time in summaries when available (39912687)
* Automation summaries, use latest test data fime for a CL (39911090)
* Fix incorrect IMDSv2 detection in agent (39903273)

## 2025-02-06

* Do not forward cancellation token to cleanup job driver as this will interrupt it (39755347)
* Fix Linux runs. (39727551)
* Add specialized status message support and render as Markdown (39721461)
* Add breadcrumbs to templates main menu, to indicate all or partial selection in submenus (39633740)
* Refactor job handler lease exceptions to run batch cleanup code (39633282)
* Add UX to log view to skip to next block of issue lines (39633254)
* make C# code compilation not require a .csproj in the project being tested.
  - UAT will now also recurse in Builds directories of plugins (if they happen to have them). (39601473)

## 2025-01-30

* Validate ServerUrl for auth modes and load balancers in settings
  - Add ServerUrl validation check for non-anonymous auth modes to prevent redirect issues
  - Add middleware to detect reverse proxy/load balancer deployments
  - Log warning when ServerUrl is not configured in proxy environments
  - Forgetting to set this has been a source of issues when deploying Horde (39586905)
* Fixed build warning in CbField json serialization (39586077)
* Fixed incorrect json serialization of cb fields that had fields without names (mostly encountered when serializing an array). (39585891)
* Add triage issue severity workflow setting to enable warning/error only workflows (39563253)
* Return non-200 status code when health check fails in agent
  Also improve logging when failing to bind agent HTTP server to all interfaces. (39550702)
* Improve where optional attributes can appear in enum, class, and struct definitions. (39548575)
* Re-enable and fix tests for agent's HTTP server (39546007)
* Add small HTTP server in agent for administration and health checks
  In this initial version, the health check endpoint returns a basic status whether it is connected to the Horde server or not. (39518264)
* OidcToken - Added missing copyright to tests and fixed other analyze errors in oidc token (39513944)
* Adding artifact type expiry information to dashboard config query (39513838)
* OidcToken - Refactored token manager to allow for tests and added some functional tests (39512897)
* Cloud Artifacts - Support uploading files larger then 2GB - implemented support for compressed buffer serialization in C# to read from a stream thus supporting compressing buffers larger then 2GB, still allocating this all in memory so will use a lot of memory for large buffers. (39512887)
* Run job executor cleanup when the job batch process fails or hangs (39494168)
* EpicGames.Perforce: Handle ... syntax with file extension in view mappings (four dots) (39478199)
* PR #11833: Add throw if Refresh token was not returned in response. (39477147)
* Display N/A Start and Stop Times for User Notices (39438421)
* Add Select All to small Templates submenus (39438351)
* Show partial elements selected on Templates context menu (39438288)
* [EpicGames.UBA] Fixed so uba logger log scope logic works (39429420)
* Add database schema migration tool for MongoDB (39407820)
* Fixed ToolTips not working on List type parameters in stream configs (39407062)
* Prevent exception swallowing during job batch execution (39371897)
* Reverse job audit logs to be top down (39356984)
* Add job audit view to dashboard (39356298)
* Add job audit logging for tracing of the state machine (39354365,39346324)
* EpicGames.Core: Fix unsafe type cast in LoggerScopeCollection.Scope constructor (39343576)
* Improve logging for job step log writer errors (39323447)

## 2025-01-14

* Set the "background": true flag when creating new indexes, and replace the deletion of unused indexes with a warning message. We don't want old pods to delete indexes created by new pods. (39182560)
* AutomationHub now loads all streams by default when none are selected (39089285)
* Improve preflight autosubmit messaging (39088492)
* Horde Log search improvements (39061326)
* [UE-192322] Job dialog: improve navigation of long dropdown lists
* Add a "Copy Artifact ID" to artifact button (39054641)
* Few small changes to handling of chunked data nodes.
  - ChunkedDataNodeRef now implements IHashedNodeRef, and extension methods now allow extracting chunked data to streams or files.
  - Adding a FileEntry no longer requires an explicit length parameter (this is already contained in the payload data).
  - Rename FileEntry.StreamHash to FileEntry.Hash (39004911)
* Add telemetry spans to IssueService (38934459)
* Include OidcToken app in Horde's .sln file (38920316)
* Include lease ID in returned properties for allocated compute tasks (38917576)
* Expose lease ID as an env var for jobs (38906012)
* Send final capabilities update before agent session termination (38886583)

## 2024-12-17

* Pass through OIDC scopes for API authorization.  Allows requesting custom delegated scopes for tools using OidcToken, like UGS interacting with Horde.
* EpicGames.Core: Add extension method for redacting sensitive values in request URIs (38781177)
* Added a flag to allow agents to auto restart on failure.  When installing the agent via HordeAgent service install, you can specify -AutoRestartService=true/false, defaults to true. It will set the three failure recovery options to Restart the Service (38780557)
* Use consts for LogicalCores and PhysicalCores values (38779497)
* Collect system telemetry on Linux (CPU/mem/disk usage) (38779426)
* Support multiple templates in job redirector queries (38755881)
* Make job redirector default to latest step regardless of state, with a complete query param option (38754699)
* Add job/step information to device issue audit logging (38754008)
* Cloud DDC - Added missing documentation on HttpContent extensions (38751527)
* Cloud DDC - Fixed issue were put blob responses did not correctly support being converted to compact binary. This involves avoiding anon types so that we can flag the fields for cb serialization. Also moved the tooling to read compact binary from a http client from Jupiter into EpicGames.Serialization for easier access. (38748909)
* Lower timeout for ComputeSocket send buffer detach (38748804)
* Analytics documentation update to consider new plugins setup. (38731447)
* Cleanup ResolveWarning API to remove buildconfiguration delegate, and instead apply target defaults in new API. (38674255)

## 2024-12-09

* Disable low cardinality indexes for Storage.Blobs collection (38662017)
* Moved Clear Selection and All Templates to a single checkbox, and added a Select All/None for each category with at least 3 options. (38577053)
* Add queryable Horde analytics meta data for better performance and to limit data set for top/bottom metrics (38571863)
* Cloud DDC - Fixed build warning from change 38567473 (38567875)
* Cloud DDC - Fixed an issue serializing a CbObject that was a part of a poco type. The collection of CbFields was picked up the serializer meaning the object was written as an array with fields (which is not valid json). (38567473)
* Add support for enumerating analytics metrics and providing multi-group filters (38551348)
* Improvements to analytics top/bottom N capture (38547327)
* Improve agent telemetry display (38545837)
* Make Preferences, user icon, and Tools middle-clickable. (38542886)
* Add a bit more information when can't resolve preflight stream (38541374)
* Cloud DDC - Fixed an issue were the json converted value of a object id was a base64 blob instead of a hex encoded string as we normally do. (38540397)
* Cloud DDC - Fixed some issues when converting from a json object to a compact binary object in the builds api (38534145)
* Make OIDC scopes and mappings overridable
  - Due to .NET config only merging array entries when combining config sources, hardcoded OIDC defaults (scopes and claim mappings) cannot be overridden.
  - Added OidcAddDefaultScopesAndMappings flag to control whether defaults are applied (38440554)
* Added Content Metrics to Horde Analytics
  Added Content vizualization to Horde Analytics
  Records the total sizes and virtualiszation level of all source content (38440278)
* EpicGames.Slack: Fix compiler warnings (38439811)
* CpuCount and CpuMultiplier in AgentSettings used in LogicalCores property
  - Limit the number of logical processors to CpuCount or number of logical processors reported by the OS, whichever is the smallest
  - Scale the number of logical processors by CpuMultiplier
  - Change CpuCount to a nullable value type to know when it is to be used
  - Find the number of logical processors on Windows
  - Set UE_HORDE_CPU_COUNT to CpuCount when CpuCount is set (38438211)
* Fix crash in UHT by adding a default include path in case none were found (38374613)
* Fix regex in mongo log/config check to be more inclusive (38335134)
* Cloud DDC - Fixed build warnings with the newly added CbObjectId type (38332782)
* Cloud DDC - Added initial version of a build api, this is intended to be used by cooked output snapshots and unsync builds. Its practically very similar to how snapshots already work but with a added index of which blocks exists so that it can be queried from the server rather then rely on input of a previous build which should help make better choices that increases dedup. 
  This api also provides simple searching which is better then what we had for the DDC api (which just had enumeration under special circumstances) which is needed to identify which snapshot to use.
  Lastly I added support for ObjectId in C# as this isn't something we have needed before but is used within objects for this API (is a 12 byte identifier that is valid within a object but not globally unique, it is similar to a uuid type 4).
  The build store currently only supports Scylla and Memory implementations (e.g. no base cassandra or mongo) as we have concerns about those dbs ability to perform for this workload. (38329536)
* Fixed Localisation warnings to correctly add a filepath which can be used for identifying suspected CL's which caused the issue. (38329203)
* Fix issue with failed batches not being able to retry steps (38299359)
* Merging Horde changes up to CL 38271598. (38274254)
* Merging NuGet package upgrades to 5.5.
  Source changes:
  - 36793185 (Update Blake3)
  - 36994680 (EpicGames.Core: Pin System.Text.Json to 8.0.5)
  - 37007077 (Update Logging nuget packages)
  - 37007445 (Missed a file from the logger nuget update)
  - 37011466 (Update more nuget packages)
  - 37033668 (Update more nuget packages (all system or ms that don't depend on System.Drawing.Common))
  - 37097577 (Update nuget packages)
  - 37753709 (Update nuget packages)
* Fix missing IClock for dependency injection This missing dependency caused agent startup failure when EC2 mode was enabled. (38259940)
* Add support for IMDSv2 for AWS EC2 in agent.  Used for detecting spot interruptions and auto-scaling changes on a running agent. (38251784)
* PR #12533: Fix Horde configPath for globals.json with perforce:// uri not working (38221010)
* Add additional full download size info to the current BuildDiff report (38220701)

## 2024-11-19

* Rename default GetSparseClassData accessor to GetMutableSparseClassData to make mutability more obvious and to encourage use of more explicit accessor with EGetSparseClassDataMethod parameter - deprecating old version (38208446)
* [UBA]
  * Changed caching code to use new uba path which shares logic with vfs
  * Removed last parts of OutputStatsThresholdMs
  * Removed code wrapping root path instance now when we use handles instead (38199398)
* Fix not being able to set a device to base model if another model is set (38193884)
* Add support for device audit logs, frontend (38192783)
* Improve device audit messages to include the user changing properties (38192679)
* [UBT]
  * Removed system roots and autosdk root now when all modules should provide the roots they need (38192101)
* Add device audit logging, server side (38189896)
* Decrease test time for AES transport test.  Removes the largest buffer size to reduce test time. (38186094)
* Changed the Context and LoadingNames in EditorTelemetry to be consistent with the Span Names (38183893)
* Add TopN/BottomN sampling constraints for Horde Analytics (38181517)
* Add download with Unreal Toolbox preference and generalized artifact button (38181432)
* Add CLI argument for loading additional config files.  Allows specifying custom config file paths, enabling more flexible configuration in buildfarm environments instead of relying solely on built-in default locations. (38180737)


## 2024-11-15

* Prevent exceeding open file handle limit on MacOS when uploading data. (38125783)
* Remove EC2 IMDS server availability check during startup (38123434)
* Fix an issue where blobs uploaded out of order can cause references to be dropped. Now creates a "shadow" blob info record for any referenced blobs that don't exist, and converts that to a regular record when data is sent. (38121174)
* Preserve type URLs in Protobuf messages for upgrades and conforms during serialization. (38119696)
* Fix test flakiness in ComputeSocketTests. Increase test timeouts and disable parallelization to handle CPU contention better during parallel test execution. (38088741)
* Ensure agent stopping state is reported to server (38085676)

## 2024-11-14

* Handle EndOfStreamException in RecvOptionalAsync. Treat EndOfStreamException same as zero bytes read, returning false to indicate no data available rather than propagating the exception. (38077833)
* Forward completion event to inner in IdleTimeoutTransport (38077823)
* Change back to 16 read tasks for batch reads from storage. (38057408)
* Set maximum size of channels used for buffering data read from storage. If part of this pipeline is particularly slow (eg. I/O, network, CPU), is can cause large backlogs of buffered data which can kill performance or cause machine instability. Applying some back pressure resolves the issue without any loss in performance. (38051395)
* Log encryption used for compute tasks in lease log (38031944)
* Make port allocation for tests thread-safe (38016954)
* Add explicit clear selection to template chooser (38007851)
* Enable parallel test execution for EpicGames.Horde.Tests. Fixed race conditions by adding unique suffixes to shared test directories. (38002324)
* Allow leases running in a stream to access artifacts in that stream by default, if not overridden elsewhere. (37996421)
* Replace AesTransport with a new implementation. Uses random nonces for each message instead of a base nonce. Add TCP transport integration tests (37992625)
* Add parameter for controlling inactivity timeout for compute executions. For UBA-based compute tasks this value needs to be increased as CPU or I/O can at times get starved, leading to a timeout. (37960155)
* Job search improvements, link to related jobs from operations bar, step callout optimizations (37920079)
* Improve symbol indexing time; perform hash calculations in parallel, and upload aliases in smaller batches. (37898457)
* Hide agent install console window and skip uninstall registration in Windows Registry (37874147)
* Fix agent uninstall directory path when registry path is not used (37874091)
* Fix CLI tool upload using named blobs (37873422)
* Show critical log levels as errors in the log view (37845567)
* Add an IStorageWriter interface, which allows buffering and batching metadata update requests. Use this to upload artifact aliases in a single batch. (37843288)

## 2024-11-06

* Fix cached commit source decrementing number of remaining results to return even if commits are filtered out of the resulting list by commit tags. Prevents issues with new jobs not being able to identify code changelist to run at. (37833532)
* Add support for parallel uploads of bundles. Bundle locators are now assigned by the client, allowing the next bundle to be assembled before the previous one has finished uploading. (37808051)
* Include a flag in template responses indicating whether the user can run it. (37792772)
* Show steps with issues inline when there is a label selected (37791398)
* Fix PerforceServiceCache.SubscribeAsync() stalling out when more than 10 commits are parsed in a single iteration, due to changes being enumerated in the incorrect order. Now uses a custom query to search for commits in the correct order, and only uses cached commit metadata. (37790919)
* Add flag for registering agent for uninstall in Windows. If the agent is installed via Unreal Toolbox but later uninstalled via Windows add/remove programs, Toolbox won't be notified. Until the uninstall command can be routed through Toolbox, this flag prevents Windows from handling uninstallation directly. (37743944)
* Do not try to parse portable PDB files. These are not supported by symstore. (37743471)
* [UBT] CppCompileEnvironment to use CppCompileWarning instead of raw properties (37696325)
* Add option for using uninstall location from registry (37693256)
* Remove EpicGames.Perforce dependency from agent (37693017)
* Include toolbox.json in agent with process elevation (37692524)
* Fix storage client initialization in tool upload command. (37650068)
* Fix DI error with Horde client tool commands (37646970)

## 2024-10-31

* Use lowercase for processor architecture in agent property names (37575294)
* Fix lease manager ignoring session termination requests when no lease is active (37575102)
* Make LeaseManager testable in agent. Previously used FakeHordeServer reinstantiated and session result now contain a reason leading to the outcome. (37574891)
* Right Click for extra Agent Info options isn't communicated in the UI, leads to support overhead. Improves UX communication by adding a Pencil Icon Button to column header which also displays the context menu when clicked. (37566738)
* Fix issue with password not being sent when creating a new account if it is provided (37514227)
* Add install/uninstall commands for agent (37499477)
* Add better error message when agent IPC server cannot start (37499449)
* Maintain order when selecting deselecting steps (37476215)
* Fix some step retry logic (37472690)
* Expose CPU and OS architecture as agent properties. Needed to distinguish between x86_64 and aarch64 for upcoming support in UBA. (37443665)
* Add cluster to perforce config check (37429576)
* Reduce size of self-contained agent. Removes unused libs from published directory, such as Linux and macOS versions accidentally making their way in. (37428045)

## 2024-10-24

* Fallback to normal change number for BuildGraph invocation if code change is missing (37388671)
* Prevent send and possible exceptions in background send for ComputeSocket (37387020)
* Fix undefined access in job vew (37350938)
* Update dashboard install docs for Vite (37350502)
* Device view fixes (37350192)
* Tweaks to compact views to support hiding/showing names (37350158)
* Slowed the read rate to avoid out of memory issues. (37323730)
* Fix hanging chrome when missing job template, also surface job error (37322037)
* Set agent's default setting for EC2 support to disabled
* Adding robomerge track change messages to slack messages. (37266636)

## 2024-10-17

* Delete ephemeral agents that have been offline for more than X hours. This ignores checking for any outdated sessions and simply uses the last online timestamp instead. (37172088)
* Show logs event data as generating for running steps when missing (37125109)
* Always show the reason for why compute resources may not be available (37114936)
* Do not retry HTTP requests which throw an HttpRequestException. These generally represent a non-transient error, unlike HTTP status responses. In the case of bad server addresses or ports, requests may take a long time to timeout. Retrying them does not make sense, and creates very long delays before the result is reported. (37093049)
* Cloud DDC - Changed server timing metrics into a concurrent bag instead of list with locks to reduce lock time a bit. (37085986)
* Fix for calling Remote UFUNCTIONs from other Remote UFUNCTIONs. (37066038)
* Instrument ManagedWorkspace with OpenTelemetry tracing (37059773)
* Remove use of resource.name in spans resource.name is Datadog concept and doesn't play well presenting traces from OpenTelemetry in the Web UI. (37051252)
* Fix bug with OpenTelemetry settings not being base64 encoded (37049676)
* Add Google Cloud Storage support (37048260)
* Pass OpenTelemetry settings to agent driver subprocess (37047438)

## 2024-10-10

* EpicGames.Perforce: Assume and mark workspace having untracked files prior to starting a sync (36972802)
* Allow HTTP for OIDC discovery endpoints
* Log errors when mongodb config/log files contain non-latin characters as mongodb.exe cannot handle these paths (36930465)
* Improve description text for several tools bundled with Horde. Mention that P4V needs to be restarted after installing P4VUtils. (36912499)
* Use P4 syntax on artifact modal to download current folder (36912473)
* Fix issues with theming and login/setup view for installer build (36907272)
* Don't add all files to filter when there is no selection in the artifact modal to avoid overflowing the max request length (36906929)
* Add Horde's built-in user ID claim when issuing OIDC userinfo for Horde accounts (36899176)
* Ensure auto-conforming is requested only when not already pending (36894611)
* Distinguish between no matching compute resources vs all resources in use (36857071)
* Allow resetting blob id for length scan. (36848920)
* Add schedule auditing (36825106)
* Prevent infinite loop when cancelling during agent registration (36819203)
* Allow clients to specify a locator when uploading blobs. The server will not allow overwriting any existing blob, but allowing the client to determine the locator opens the door to it being able to write multiple blobs in parallel. (36813875)
* Grant claims to leases identifying them as running certain projects, streams, and templates. (36807664)
* Support running jobs in streams that do not have the engine directly under the stream root. The "EnginePath" property in the stream config can be used to configure the path to the engine folder. (36804827)
* Include a Version.json file in Unreal Toolbox builds which can be used to detect upgrades/downgrades. When auto-updating from builds with this file, only upgrades will be allowed. (36789633)
* Unreal Toolbox: Always show the settings dialog on launch unless the -Quiet argument is passed on the command line. This makes a more sensible default than requiring a -Settings argument. (36785739)
* Remove agent enrollment server json upon uninstall (36784558)
* Fix agent store updates upon cache invalidation/deletes (36739556)
* Move UnrealBuildTool.Tests into Engine\Programs\Shared (36735944)
* Fix issues with agent enrollment and deletion latency (36735037)
* Fix bug where leases weren't immediately aborted when agent is marked as busy (36733795)
* Fix issue with artfifact log rendering when there is no artifact type (36730128)
* Fix AgentWorkspaceInfo objects constructed from RPC workspace messages having an empty string for 'method' instead of null, causing agents to get stuck in a conform loop. (36718257)
* Fix issue with enrolling agent from installer goes to default server (36710606)
* Fixed a bug where the same error/warning message using different slash directions would not match with an existing issue when handled by the hashed issue handler (36698600)
* Add more logging and tracing for updating agents via REST API (36697817)
* Allow specifying arbitary key/value metadata on tools, and add product ids for MSI installers. Unreal Toolbox will automatically install/uninstall MSIs using the given product id. (36694744)
* Unreal Toolbox: Various improvements with installation process (36678430)
* Fix bug where not all fields were included in Equals() check for AgentWorkspaceInfo (36667931)
* Add scratch and min conform space to REST API agent response (36667252)
* Fix issue with mixed ungrouped and grouped tool downloads (36647304)
* Use the correct token for writing artifacts. Adds an IArtifactBuilder interface returned from IArtifactCollection.CreateAsync() which can be used to upload artifact data with the correct permissions. (36645064)
* Resurrect option to explicitly update build health issues in advanced parameters (36644246)
* Use same logic for shutting down on MacOS as Linux. (36578059)

## 2024-09-25

* Add filter for agent properties in REST API (36563629)
* Re-add logical cores as an agent property. UBA uses this property for determining number of agents to allocate. (36532400)
* Rename sample stream project to 5.5, and add a ugs-pcb artifact type. (36508675)
* Fix path for constructing Azure storage backends. (36508415)
* Move agent sandbox to be independent of agent settings, not in a hidden folder, and to avoid tripping path length warning by default (36505143)
* Fix issue with conform threshold for workspaces not propagating (36502791)

## 2024-09-19

* Expose .NET runtime version in agent properties (36427479)
* Add a separate build of the Horde Agent MSI for the toolbox, which includes metadata for running the installer. (36425974)
* Remove tray app and idle behavior setting from agent installer (36410206)
* Add support for group and platforms to bundled tools, adjust the build accordingly (36407524)
* Rename HordeToolbox again. Now: "UnrealToolbox!" (36401901)
* Add a dedicated installer for the tray app, and include it in the server installer. (36394095)
* Fix infinite loop issue when querying for jobs (36392029)
* Fix bug where physical cores are counted twice in capability detection (36387918)
* Replace OpenTracing with OpenTelemetry in JobDriver (36386025)
* Document the location of the server config file. (36384366)
* Ensure all streams register default artifact types.  We recently introduced the requirement that artifact types are explicitly configured (CL 35531333). The job driver expects certain artifact types to be present, and jobs will fail if these types are not registered.  This issue affects new installations and older installations upgrading to 5.5. (36383858)
* Remove OpenTelemetry.Instrumentation.AspNetCore from agent (36383393)
* Add an option for automatically updating tools in the tray app. (36356951)
* Fixed a bug where issues would have their FixCommitId set to 0 rather than cleared after re-assignment to a new owner.  Note we set fixChange to 0 in the onAssign function in IssueViewV2.tsx.  I now check whether fixChange is == 0 and if so set FixCommitId to be empty, this causes the FIX CL to be cleared. (36345723)
* Add information about processes with a file locked when it cannot be overwritten when extracting data from a bundle. (36341246)
* Add a dataflow graph with multiple deletion workers to improve GC performance. (36340210)
* Upgrade Horde Agent to NET 8. (36340066)
* Add an exponential pause between iterations over the GC tick queue. This should reduce the time that we spend idle during blob deletion. (36321827)
* Fix changes not being enumerated in descending order by PerforceService, causing the minimum changelist number for ICommitCollection.SubscribeAsync() not being updated. This prevents issue fixed tags from being processed. (36321198)
* Add missing file. (36318745)
* Replace OpenTracing with OpenTelemetry in agent (36307242)
* Move OpenTelemetry settings to EpicGames.Horde for sharing with agent (36303013)
* Added an additional sanitise case for the PerforceMetadataLogger when trying to get file annotations.  We could potentially handle this in LogEventParser AddLine but I wanted to limit the impact of these changes. (36299319)
* Add a proxy server to the Horde tray app, allowing users to connect to the Horde server via an unauthenticated connection bound to localhost. (36292016)
* Add a flag for showing a tool in the Horde tray app. (36284430)
* Add settings menu to tray app, as well as functionality for downloading tools from Horde. (36265467)
* Handle .horde.json file not existing when configuring server url on Mac/Linux. (36264830)
* Changed code traversing workspace on disk to use EnumerateFileSystemInfos instead of directories and files separately. This reduces number of kernel calls and save 20% time on a machine with attached ssd (36149765)
* Add timeouts for available port checking in tests (36117480)
* Add server-defined properties for an agent
  - Allows setting properties that will overwrite and merge with properties reported by agent itself
  - Refactor parameters for agent creation into an options object
  - When a user/agent can create new agents outside enrollment process, it's marked as trusted for the time being (grandfathering existing registration in JobRpcService) (36114930)
* Ignore server-defined properties when sent by agent (36108891)
* Adding Horde installer custom actions, data directory selector, and fix server to be able to bootstrap using custom data directory (36082966)
* Add tooltips for step times (36043646)
* Additional logging for terminating sessions. (36041396)
* Set the max thread count for managed workspace operations to one less than the number of reported CPUs on the machine. (36038061)
* Add better error for failing to start a process from the Horde Agent. (36026676)
* Advertise a new OSFamilyCompatibility property from agents, indicating OSes that the system can emulate (for Linux agents with WINE to indicate Windows compatibility). (36018324)
* Allow specifying a list of properties required for agents to execute compute leases. (36012503)
* Re-enable dedupe for the tools bundled with Horde. The permissions affecting access to this data are now waived for bundled tools. (36012474)
* Fix installed server not correctly identifying code changes correctly. Server was incorrectly setting a flag indicating that all files for a change had been enumerated, preventing it scanning the entire set. (35977472)
* Add a symbol store plugin. Symbol stores use aliases in the storage system to map symbol store paths onto content streams from existing artifacts, allowing reuse of data already available in artifacts.
  - Symbols can be tagged with the appropriate metadata by setting the Symbols=true attribute on the CreateArtifact task. Referencing the namespace that the symbols will be uploaded to from the symbol store config will allow accessing them through the api/v1/symbols route.
  - Hashing for symbols is compatible with symstore.exe, but is handled by a custom implementation in SymStore.cs. (35971584)
* Fix linq expression for generation of alias index. (35967893)
* Pause the addition of new blobs for GC once the queue is longer than 50,000 entries. (35967249)
* Allow graceful draining of leases when yielding to local user activity
  - The termination signal file used for spot interruptions is now written to let a workload know about the lease being drained.
  - This requires workload to scan and respect the file for this to be effective, which UBA does for example. (35962694)
* Add agent version to compute resource class, used for exposing version in compute resource API. (35954995)
* Log Horde server and agent version for a UBA session (35954941)
* Fix name of aliases over blobs collection. (35950154)
* Remove code to read imports from uploaded blobs. These should now be set at upload time. (35949781)
* Support loading artifacts out of log which are not in the step artifacts (35931864)
* Improve issue button rendering to not obscure the structured logging eyeball, also change where we render the structured log (35931043)
* Include artifacts created through the CreateArtifact BuildGraph task in the list of artifacts for the job. (35924757)
* Catch compute cancellation exceptions and avoid flagging them as errors (35923687)
* Guard against socket errors during closing of compute socket, also ensure CloseAsync can't be invoked twice. (35903197)
* Set the UE_HORDE_STREAMID environment variable containing the current stream id when running under Horde. (35900707)
* Document the look up of P4TRUST file (35892976)
* Change tray app to use Avalonia rather than Winforms. (35883986)
* Dashboard side of bundled tool management (35877843)
* Add clear conform to agent context menu (35877280)
* Adding bundled information to tool responses (35876724)
* Fix broken ServiceAccountAuth test (35876245)
* Add name field to service accounts and use that in audit logs, previously, name of service accounts resolved to "unknown". (35873276)
* Keep track of the agents which are currently able to execute sessions.  Each server now caches the state of a session based on the last update time, allowing it to perform most operations with minimal context fetches in the common case where an agent is always being updated on the same server. (35872405)
* Fixed issue in recent UHT changes that prevented errors begin generated from the header file object. (35863940)
* Add agent settings for CPU count and multiplier.  Only provided as hinting to workloads, which can chose to respect these. The initial use-case is letting UBA limit number of CPUs in use, much how maxcpu/mulcpu works for UBT.  Later on these values could configure job objects (on Windows) for proper OS-enforced CPU limiting. (35861968)
* Document use of ssl: prefix for connecting to Perforce servers (35859151)
* Replace NuGet package with more explicit one to reduce dependency size (35822082)
* Fix description and creation time not being deserialized as part of artifact responses correctly. (35811531)
* Store session state in Redis rather than MongoDB. (35780092)
* Add HordeHTTPClient GetUgsMetadataAsync method (35775820)
* Fixed symbol table lookup to properly include the header file when walking up the outer chain. (35773746)
* Agent enrollment improvements (35770720)
* Add REST API endpoint for listing ACL permissions for current user (35770032)
* Fix invalid dep injection of ServerSettings in JwtHandler (35769300)
* Show permissions for ACL scopes on account page, useful for debugging permissions in Horde. (35768929)
* Improve OIDC configuration experience
  - Add debug mode for better explaining why a JWT bearer token is rejected
  - Validate required settings are set for auth mode OIDC/Okta
  - Improve docs for OIDC settings (35768735)

## 2024-08-22

* Lock fluent to fix upstream regression (35716215)
* Add customizable landing pages (35713203)
* Surface when log data is missing in log event (35712179)
* Address additional artifact search feedback (35710162)
* Make it more clear when a step was canceled by Horde vs a user (35709294)
* Add lock around invoking nftables CLI tool (35690511)

## 2024-08-19

* Add ability to override agent's compute IP used for incoming connections (35621653)
* Generate issues for completed job steps asynchronously. (35601740)
* Randomize port assignment for relay port mappings. There's a worry of re-using the same port leads to potential races with how nftables/conntrack cleans up entries. In previous impl, lowest available port number was always used which led to higher contention. (35594027)
* Check for invalid AWS region names in config (35589406)
* Allow configuring the namespace for artifact types, tools, and replicated Perforce data. (35568444, 35568869, 35570127)
* Skip expiration of artifacts if no retention policy is specified. Previous behavior was to purge all that weren't explicitly kept. (35543239)
* Add ushell to Horde installer. (35541567)
* Fix registration of bundled tools in installed builds. (35538391)
* Require artifact types to be declared in config files. Also allow setting expiration policies per-project and per-stream, and handle expiration of artifact types in deleted streams or of a type which are no longer declared. (35531333)
* Improve lease termination handling during AWS spot interruptions. Only terminate session once a lease has finished. (35529116)
* Ensure resolved issues are excluded from queries when resolved by timeout. (35449290)
* Fix rendering of message templates containing width and format specifiers. (35446849)

## 2024-07-29

* Fix changelist number not being returned in artifact responses. (35140759)
* Change artifact paths to Type/Stream/Commit/Name/Id, to reflect permissions hierarchy. (35129994)
* Add extension methods to allow extracting IBlobRef<DirectoryNode> directly into directories. (35094979)
* Preliminary support for VCS commit ids that aren't an integer. All endpoints still support passing changelist numbers for now. (35091676)

## 2024-07-25

* Add a separate flag for marking an issue as fixed as a systemic issue, rather than having to pass a negative value as a fix changelist. Still supports passing/returning negative fix changelists as well for now, but will be removed in future. (35029295)
* Serialize config for pool size strategies and fleet managers as actual json objects, rather than json objects embedded in strings. Supports reading from json objects embedded in strings for backwards compatibility. (35026676)
* Set output assembly name for the command line tool to "Horde". (34984531)
* Separate out log event errors and critical failures into a separate fingerprint when we exceed the 25 event limit in the Hashed Issue Handler. (34930826)
* Add support for tagging log events with issue fingerprints. The $issue property on a log event can contain a serialized IssueFingerprint, which will be parsed by the ExternalIssueHandler handler inside Horde. This property can be added from C# code using the ILogger.BeginIssueScope() extension method. (34913930)
* ParentView is optional per perforce documentation. Fixes Unhandled Exception (Missing 'ParentView' tag when parsing 'StreamRecord') for streams that does not have it set (34890081)

## 2024-07-16

* Support for disabling conform tasks through the build config file. (34836886)
* Allow setting access permissions for different artifact types on a per-type, per-project and per-stream basis. (34836758)
* Allow specifying a secondary object store to read objects from, allowing a migration from one store in the background without causing downtime. (34828731)

## 2024-07-15

* Split server functionality into plugins. Plugins are still currently statically configured, which will be changed in future. (34620916, many others)
* Fix threading issue causing block cache to always attempt to access element zero, causing tests to get stuck in an infinite loop. (34759616)
* Add a debug endpoint for writing memory mapped file cache stats to the log. (34757974)
* Fix resource leaks for blob data reads. (34756957, 34757889)
* Fix tracking of allocated size in MemoryMappedFileCache. (34747643)
* Fix binding of plugin server config instances. (34745408)
* Move build functionality into a plugin. (34744727)
* Respect Forwarded-For header for client's IP during compute cluster ID resolving. Also return a better error message when no network range or cluster is found. (34743435)
* Add support for automatic computer cluster assignment in Horde, based on internal and external IP. (34734153)
* Add endpoint for resolving a suitable compute cluster ID (34706120)
* Set agent property if Wine executable is configured. Needed for scheduling Wine-compatible compute tasks. (34675604)
* Do not log cancellation exception as a warning when checking if blob exists. (34672928)
* Remove legacy artifacts and log system. (34663531)
* Resolve HTTP client IP for Datadog trace enricher (34644555)
* Fix user ID/username not getting set in Datadog enricher for OpenTelemetry traces. Must be accessed once response has been sent. (34636046)
* Support for artifact searches (34605400)
* Log each configuration source during agent start for better visibility where the agent is reading config from. Also log the actual logs dir. (34603382)
* Log where agent registration file is saved to/loaded from. When the file exists, the agent provides no hinting it reads this file which can be confusing when trying to reset the agent. (34600505)
* Add documentation for setting up a secret store. (34569155)
* Add support for compression of all responses using gzip, brotli, and zstd. (34565711)
* Add creation time to artifact responses. (34559619)
* Change config reader to operate on JsonNode instances rather than writing directly into target objects. (34546472)

## 2024-06-20

* Compute a digest for each block stored in the block cache, and validate it before returning values. (34515388)
* Remove requirement to specify a stream or key to enumerate artifacts. (34513227)
* Downgrade log event about duplicate build products to information, since we don't have the list of duplicate build products to ignore in Horde. (34481396)
* Exclude blocks for empty files from the Unsync manifest. (34464454)
* Add IPoolSizeStrategyFactory interface, which is used for creating IPoolSizeStrategy instances. This allows us to remove the graph/job/stream collection interfaces from FleetService, allowing it to exist without any job handling code in the solution. (34457658)
* Do not shutdown disabled agents by default. Prior to this change, this value defaulted to 8 hours which assumes you want this behavior in the first place. Making this nullable allow for opt-in instead. (34455677)
* Remove tracing span from shared tickers. For long-running callbacks, this can create data which is difficult for Otel/DD to handle. (34444738)
* Add a debug endpoint which streams a random block of data to the caller. Usage is: /api/v1/debug/randomdata?size=1mb (34426596)

## 2024-06-17

* Add support for auto-assigning cluster ID during compute allocation requests (34423910)
* Increase MongoDB client wait queue size from 300 -> 5000 (34423018)
* Allow specifying cache sizes using standard binary suffixes (eg. "4gb") (34382251)
* Add the shutdown timeout for Kestrel server (34379789, 34379660)
* Add an AWS parameter store secret provider (34379595)

## 2024-06-13

* Add a pipelined blob read class for storage blobs. (34322019)
* Add a disk-based cache for compressed Unsync blocks. Uses memory mapped files to read/write to underlying storage, LRU eviction via random sampling. (34288152, 34292867)
* Tweak MongoDB client's config defaults to curb wait queue full errors (34276157)
* Resolving of capabilities has been known to stall at times. Adding trace spans to these should help see which part is causing slowdowns. (34232453)
* Add zstd as a compression format for bundles. Also add some basic compression tests, and fix an isuse with gzip streams being truncated. (34211536)
* Include rolling hashes in bundle archives to support unsync manifests. DirectoryNode/ChunkedDataNodeRef now use HordeApiVersion for versioning purposes, rather than their own custom versioning scheme. These blob types will serialize the most relevant Horde API version number they support into archives. (34207581)
* Ignore cancellations in AWS instance lifecycle (34207087)
* Include modification times for file entries in bundles by default, but do not write them unless the configured API version allows it. (34205938)
* Add compute endpoint that automatically finds the best cluster. Currently uses IP of requester together with networks from global config. (34201862)
* Fix some artifacts not showing up in job responses, due to default cap of 100 artifacts returned. (34200332)
* Check agent's compute cluster membership on assignment. Previously it was only checked by the task source. (34199317)
* Set user ID in compute assignment request track usage by user (34195976)
* Disable config updates when running locally in Redis read-only mode, preventing an exception on startup. (34169346)
* Make horde chunk boundary calculation consistent with unsync, add unit tests to validate determinism (34168267)

## 2024-06-06

* Add a retry policy to the Epic telemetry sink, to stop 502 errors causing server log entries. (34141477)
* Add a GC verification mode, which sets a flag in the DB rather than deleting an object from the store. (34135143)
* Fix issue where files will not be uploaded to temp storage if they were previously tagged as outputs in another step. Now allows output files to be produced by multiple steps, as long as their attributes match. (34132683)
* Adding support for optional cancellation reasons for jobs and steps (34131480)
* Forward all claims for access tokens minted for Horde accounts The role claim was missing which is used for resolving many permissions. (34119849)
* Log reasons when a lease is terminated due to the cancellation token being set. (34111700)
* Add a link to the producing job to .uartifact files, as well as other metadata and keys on the artifact object. (34095063)
* Allow user configured backends and namespaces to override the defaults in defaults.global.json. (34092465)
* Print a message to the batch log whenever a lease is cancelled or fails due to an unhandled exception. (34084201)
* Add a setting ("ReportWarnings") to exclude issues which are only warnings from summary reports. (34078316)
* Fix P4 server health not being updated when HTTP health check endpoint returns degraded. (34077493)
* Re-encode json log events which are downgraded from warning to information level. (34066649)
* Add more trace attributes to compute resource allocation (34064620)
* Record endpoint address in MongoDB command tracer, jelps differentiate between primary and secondary use (read-only ops) (34063808)
* Use cached agent data for JobTaskSource, this ticks every 5 seconds refreshing agent and pool data. The cached agent data is refreshed at the same rate. (34062377)

## 2024-05-31

* Use cached agent data for fleet and pool size handling (34029489)
* Fix conform commands using the incorrect access token to communicate with the server. (34012793)
* Allow registering multiple workspace materializer factories. (34010953)
* Add an agent document property for the combined list of pools, and upgrade existing agent documents on read to include it. This allows indexed searches for dynamic pools. (34009396)
* Add a log enricher which adds the server version. (34003927, 34005245, 34004705)

## 2024-05-30

* Update test projects to NET 8. (34002443)
* Refactor WorkspaceInfo to avoid keeping a long-lived connection. Keeping idle connections to the Perforce server after the initial workspace setup is unnecessary and wastes server resources. (33949575)
* Log in to the server when querying for config files from Perforce. (33945114)
* Fix http auth handler not using correct configuration for server when run locally. (33940162)
* Skip cleaning files for AutoSDK workspace. (33858727)
* Only enable debug controllers if enabled in settings (33857362)
* Add a fingerprint description as part of issue response (33833567)
* EpicGames.Perforce: Fix parsing of array fields from Perforce records. (33807413)
* EpicGames.Perforce: Fix parsing of records which have duplicate field names in child records. (33804970)
* Allow overriding the targets to execute for a job. The optional 'Targets' property can be specified when creating a job, and will replace any -Target= arguments in the command line if set. (33799426)
* Pass -Target arguments to individual steps to ensure that BuildGraph can correctly filter out which blocks to embed in temp storage manifests. (33799034)
* Remove server OS version info from anonymous endpoint responses. (#11909) (33793366)
* Allow overriding the driver for jobs through the JobOptions object. (33791494)
* Prevent AWS status queries from causing session update failures. (33782757)
* Move job driver into a separate executable. (33775640, 33782224)

## 2024-05-20

* Add grouping keys and platforms to tools. Intent is for the dashboard to only show one tool for each grouping key by default (the one with the closest matching platform based on the browser's user-agent), but users can manually expand the list of tools to show other platforms if desired.
  Platforms should be NET RIDs, eg. win-x64, osx-arm64, etc... (https://learn.microsoft.com/en-us/dotnet/core/rid-catalog). (33698418)
* Use native GrpcChannel instances for agent connection management, rather than IRpcConnection. (33660725, 33638856)
* Add an AdditionalArguments property to jobs, which can be used to append arbitrary arguments to those derived from a job's parameters. This field is preserved - but not appended - to the arguments list if arguments are specified explicitly. Needs hooking up to the dashboard. (33630321)
* Prevent default parameters being appended to jobs when an explicit argument list is specified. (33628649)
* Move Perforce/BuildGraph functionality into a new Horde.Agent.Driver application. Intent is to separate this from the core Agent application over time, making it easier to iterate on Job/BuildGraph related functionality outside of the core agent deployment. (33606684)
* Move settings for different executors into their own files. (33605105)
* Trap exceptions when parsing invalid workflow ids from node annotations. (33602812)
* Enable HTTP compression for server-sent responses (33600392)
* Add support for agent queries via replica read (33575058)
* Optimize agent assignment for compute tasks by filtering by pool (33488689)
* Add command-based tracer for MongoDB. Old tracer operates at the collection level, this instead listens for events emitted by the Mongo client. (33479889)
* Keep a cached list of current agents in AgentService (33454500)
* Add an explicit error when the server URL does not have a valid scheme or host name. (33427628)
* Move expired and ephemeral agent clean up to shared ticker (33424708)

## 2024-05-01

* Explicitly check that the expiry time is set when searching for refs to expire, so that DocDB will use an indexed query. (33367777)
* Add CheckConnectionAsync to HordeHttpClient. (33353008)
* Add an index to the refs collection to prevent collection scans when finding refs to expire. (33338629)
* Reduce how long ephemeral and deleted agents are kept in database. Heavy use of auto-scaling can lead to excessive history of ephemeral agents being kept. In the case of AWS, each new instance has a unique agent ID. Many of the queries for agents (and in turn indices) are not optimized for a large collection. (33317167)
* Disable file modified errors in job executor. (33299280)
* Remove legacy job methods from horde_rpc interface. (33298879)
* Remove support for legacy artifact uploads. (33298589)
* Enable agents sending CPU/RAM usage metrics to the server. (33290135, 33289584)

## 2024-04-25

* Allow setting project-wide workspace types. Will automatically be inserted to the workspace type list of each stream belonging to the project. This also allows defining base types which can be inherited from. (33213109)
* EpicGames.Perforce: Fix errors parsing change views from labels. (33207649)
* Delete step logs when deleting a job. (33206294)
* Use the shared registry location to configure the Horde command line tool on Windows, and use a .horde.json in the user folder on Mac and Linux. (33205861)
* Rename step -> job step, report -> job report, batch -> job batch in job responses. (33191560)
* Include graph information in job responses. (33189353)
* Add a service to expire jobs after a period of time. Jobs are kept forever by default, though this may be modified through the ExpireAfterDays parameter in the JobOptions object in streams or projects. (33109364)
* Add auto conform for agents with workspaces below a certain free disk space threshold (33102671)
* Use a standard message for server subsystems that are operating normally. (33100047)
* Add min scratch space to workspace config (33098369)
* Recognize tags in Perforce changelists of the form '#horde 123' as indication that a change fixes a particular issue. The specific tag can be configured in the globals.json file to support multiple deployments with the same Perforce server. (33089498)
* Clear the default ASPNETCORE_HTTP_PORTS environment variable in Docker images to prevent warnings about port overrides on server startup. We manage HTTP port configuration through Horde settings, and don't want the NET runtime image defaults. (33072683)

## 2024-04-18

* Add dashboard's time since last user activity to HTTP log (33068679)
* Fix agent registration not being invalidated when server is reinstalled. Server returns "unauthenticated" (401) response, not "forbidden" (403). Forbidden is only returned when the server deliberately invalidates an agent. (33060431)
* Send DMs to individual users rather than the notification channel when config updates succeed. (33060054)
* Prevent hypens before a period in a sanitized string id. (33044611)
* EpicGames.Perforce: Fix parsing of records with multiple arrays. (33038480)
* Do not allow multiple agent services to run at once. (33037301)
* Handle zero byte reads due to socket shutdown when reading messages into a compute buffer. (33034396)

## 2024-04-12

* Ensure blobs uploaded to the tools endpoint have the tool id as a prefix to the blob locator (32907981)
* Allow specifying a unique id for each parameter in a job template (32906466)
* Send Slack notifications whenever config changes are applied (32897388)
* Make log output directory configurable in agent (32886152)
* Use strongly typed identifiers for artifact log messages in agent. (32861971)
* Fix memory leak and incorrect usage of native P4API library (32798937, 32794622)
* Add debug endpoint for capturing a dotMemory snapshot (32790300)
* Refactor API message names, project locations and reduce gRPC dependency (32785690, 32783652)
* Allow clients to upload the list of references for each blob, removing the need for the server to do it later. (32768299)

## 2024-04-05

* Handle socket shutdown errors gracefully rather than throwing exceptions. (32756972)
* Add log message whenever native P4 connection buffer increases in size above 16mb. (32751407)
* Prevent log appearing empty if tailing is enabled but no new data has been received before the existing tail data is expired. Rpc log sink was waiting forever for new log tail data, so server was discarding the data already received. (32749401)
* Fix default settings for the analytics dashboard, and update documentation to specify the correct telemetry store name. (32741193)
* Update derived data for issues when they are closed by timeout. (32733991)
* Explicitly set parent context for telemetry spans in Perforce updates, to work around async context mismatches. (32732888)
* Rework commit metadata replication to run a single task for each replicated cluster, for a simpler code flow and better tracing data. (32730286)
* Instrument JobTaskSource ticker. We see occasional slow downs in agent assignment for jobs. Adding some additional tracing should help see what's causing it. (32727003)
* Do not assign compute resource to same machine as requester (32723450)

## 2024-04-01

* Fix decoding of UTF8 characters as part of unescaping Json strings. (32638382)
* Increase Jira timeout to 30 seconds, and improve cancellation handling. (32636702)
* Add a defaults.json file for backwards compatibility with older server installers. Just includes default.global.json. (32619739)
* Move tool update packages onto a separate "Internal" tab on the dashboard. (32619239)
* Only export nodes to Horde that are referenced by the initial job parameters, unless -AllowTargetChanges is specified. Prevents data being copied to temp storage that will never be used unless the target list changes. (32616197)
* Add notes for setting up a self-signed cert for testing. (32613289)
* Doc updates. (32595767)
* Comment out the placeholder ticket value in the default config file. (32585713)
* Improve ordering of nodes when writing large file trees to storage, such that the nodes which are read after each other are adjacent in the storage blobs. Nodes are read in the reverse order to which they are written, depth-first. Arranging nodes in the blob in this order prevents thrashing of the cache and improves performance. (32585474)

## 2024-03-28 (UE 5.4 Release)

* Remove /userinfo call when authenticating via JWT. If an access token is passed, it's not guaranteed it has permission to access /userinfo from OIDC. ID tokens during normal web-based login does on the other hand. (32539012)
* Fix parsing of true/false literals in condition expressions. (32518640)
* Add a default pool to include interactive agents, and map it to the TestWin64 agent type. (32514202)
* Add logging to trace when blobs are scanned for imports, to help debug some logs being expired while still referenced. (32506875)
* Normalize keys for artifacts to allow case-insensitive searching. (32495123)
* Support setting arbitrary artifact metadata. This cannot be queried, but will be returned in artifact responses. (32464180)
* Run a background task to monitor for hangs when executing Perforce commands. (32461961)
* Allow configuring different telemetry stores for each project and stream. (32460547)
* Fix slow queries in storage service garbage collection. (32460451)
* Add a [TryAuthorize] attribute which attempts authorization, but which does not return a challenge or forbid result on failure. Should fix tool download requests using service account tokens. (32447487)
* Fix OIDC issues when using Horde auth with UBA. (32406994)

## 2024-03-21

* Lower log level of failed AWS instance start (32397273)
* Assume any exceptions while checking for config files being out of date as meaning they need to be read again. (32388229)
* Use the first specified credentials for server P4 commands if the serviceAccount property is not set. (32383940)
* Update docs for deploying server on Mac. (32370724)
* Log a warning whenever an obsolete endpoint is used. (32329528)
* Fix separate RAM slots being reported separately in agent properties. (32328916)
* Fix use of auth tokens from environment variables when the server URL is also derived from environment variables. (32305895)
* Fix StorageService not being registered as a hosted service, preventing GC from running. (32291615)
* Fix artifacts not being added to graphs. (32286255)
* Add output artifacts to the graph from the exported BuildGraph definition. (32281784)

## 2024-03-15

* Get the userinfo endpoint from the OIDC discovery document rather than assuming it's a fixed path. (32250458)
* Fix serialization issue with global config path on Linux (32240996)
* Improve error reporting when exceptions are thrown during leases. (32233035)
* Prevent DeadlineExceeded exceptions in log rpc task from being logged as errors. (32232976)
* Update EpicGames.Perforce.Native to use the 2023.2 Perforce libraries. (32232242)
* Enable notification service on all server types, otherwise we keep queuing up new tasks without ever removing them. (32220252)
* Invalidate the current session when deleting an agent from the farm. (32212590)
* Invalidate registrations with a server if creating a session fails with a permission denied error. (32212298)
* Fix job costs displaying for all users. (32154404)
* Expose the bundle page size as a configurable option. Still defaults to 1mb. (32153263)
* Use BitFaster.Caching for the LRU bundle cache. (32153088)
* Pool connections in ManagedWorkspace rather than creating a new instance each time. (32144653)
* Exclude appsettings.Local.json files from publish output folders. (32140083)
* Add a UGS config file to command line tool. (32120883)
* Add endpoints to check permissions for certain ACL actions in different scopes. (32118158)
* Immediately cancel running leases if agent status gets set to busy (paused) (32102044)
* Include a separate file containing default values for Horde configuration from the a file copy into C:\ProgramData. (32098391)
* Add OAuth/OIDC support for internal Horde accounts. (32097065)

## 2024-03-07

* Store the URL of the default Horde server in the registry on Windows (31994865)
* Download UGS deployment settings from Horde parameters endpoint (32001513)
* Allow using partitioned workspaces for P4 replication (32041630)
* Allow only keeping a fixed number of artifacts rather than expiring by time (32043455)
* Include dashboard web UI files in public Docker image for Horde server (32054568)
* Turn off native Perforce client on non-Windows agents (31991203)

## 2024-02-29

* Add a back-off delay when RPC client gets interrupted in JsonRpcLogSink (31881762)
* Add a manual approval process for agents joining the farm. New EnrollmentRpc endpoint allows agents to connect and wait for approval without being assigned an agent id or being able to take on work. (31869942)
* Fix bug with tray app not setting agent in paused state during idle (31837900)
* Copy the tray app to a folder under C:\Users\xxx\AppData\Local before running, so the agent can update without having to restart the tray app (which may be running under a different user account). (31815878)
* Use the agent's reported UpdateChannel property to choose which tool to use for updating the installation. ()
* Write the name of the channel to use for agent updates into the agent config file, and report it to the server through the agent properties. (31806993, 31810088)
* Separate implementations for regular user accounts from service accounts. (31802961)
* Added endpoints for querying entitlements of the current user (or any other account). /account/entitlements will return entitlements for the logged in user, and /api/v1/accounts/xxx/entitlements will return entitlements for a Horde account. (31777667)
* Accounts now use the user's full name for the standard OIDC name claim. (https://openid.net/specs/openid-connect-core-1_0.html#StandardClaims) (31773722)

## 2024-02-23

* Fix regression in UGS metadata filtering, where metadata entries with an empty project string should be returned for any project. (31741737)
* Artifact expiry times are now retroactively updated whenever the configured expiry time changes. (31736576)
* Allow specifying a description string for artifacts. (31729492)
* Disable internal Horde account login by default. (31724108)
* Fix bundled tools not being handled correctly in installed builds. (31721936)
* Read registry config first so files and env vars can override (31720705)

## 2024-02-22

* Fixed bundled tools not being handled correctly in installed builds. (31721936)
* Read registry config first so files and env vars can override (31720705)
* Fix leases not being cancelled when a batch moves into the NoLongerNeeded state. (31709570)
* Prevent Windows service startup from completing until all Horde services have started correctly. (31691578)
* Add firewall exception in installer (31678384, 31677487)
* Always return a mime type of text/plain for .log files, allowing them to be viewed in-browser (31648543)
* Configure HTTP/2 port in server installer (31647511)
* Enable new temp storage backend by default. (31637506)
* Support for defining ACL profiles which can be shared between different ACL entries. Intent is to allow configuring some standard permission sets which can be inherited and modified by users. Two default profiles exist - 'generic-read' and 'generic-run' - which match how we have roles configured internally. New profiles can be created in the same scopes that ACL entries can be defined. (31618249)
* Add a new batch error code for agents that fail during syncing. (31576140)
