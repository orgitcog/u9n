// Copyright Epic Games, Inc. All Rights Reserved.
import { execFile, ExecFileOptions } from 'child_process';
import * as path from 'path';
// should really separate out RM-specific operations, but no real gain at the moment
import { isUserAKnownBot, isUserSlackGroup, postToRobomergeAlerts } from '../robo/notifications';
import { roboAnalytics } from '../robo/roboanalytics';
import { ContextualLogger, NpmLogLevel } from './logger';
import { Vault } from '../robo/vault'
import { VersionReader } from './version';
import { makeRegexFromPerforcePath } from './p4util';

type Type = 'string' | 'integer' | 'boolean'

type ParseOptions = {
	expected?: {[field: string]: Type}
	optional?: {[field: string]: Type}
}
const p4exe = process.platform === 'win32' ? 'p4.exe' : 'p4';
const RETRY_ERROR_MESSAGES = [
	'socket: Connection reset by peer',
	'socket: Broken pipe'
]

const INTEGRATION_FAILURE_REGEXES: [RegExp, string][] = [
	[/^(.*[\\\/])(.*) - can't \w+ exclusive file already opened/, 'partial_integrate'],
	[/Move\/delete\(s\) must be integrated along with matching move\/add\(s\)/, 'split_move'],
]

export const EXCLUSIVE_CHECKOUT_REGEX = INTEGRATION_FAILURE_REGEXES[0][0]

const REVERT_FAILURE_DUE_TO_MOVE_REGEX: RegExp = /(.*)#[0-9]+ - has been moved, not reverted/

const changeResultExpectedShape: ParseOptions = {
	expected: {change: 'integer', client: 'string', user: 'string', desc: 'string', time: 'integer', status: 'string', changeType: 'string'},
	optional: {oldChange: 'integer'}
}

const describeEntryExpectedShape: ParseOptions = {
	optional: { depotFile: 'string', action: 'string', rev: 'integer', type: 'string' }
}

const ztag_group_rex = /\n\n\.\.\.\s/;
const ztag_field_rex = /(?:\n|^)\.\.\.\s/;
const newline_rex = /\r\n|\n|\r/g;
const integer_rex = /^[1-9][0-9]*\s*$/;
const resolveGarbage_rex = /^(Branch resolve:|at: branch|ay: ignore)$/gm;

export interface BranchSpec {
	name: string;
	reverse: boolean;
}

export interface IntegrationSource {
	branchspec?: BranchSpec;
	changelist: number;
	depot: string;
	path_from: string;
	stream?: string;
}

export interface IntegrationTarget {
	depot: string;
	path_to: string;
	stream?: string;
}

export interface OpenedFileRecord {
	depotFile: string
	clientFile: string
	rev: string
	haveRev: string
	action: string
	type: string
	user: string
	client: string

	movedFile?: string
}

export interface ExclusiveFileDetails {
	depotPath: string
	name: string
	user: string
	client: string
}

interface DescribeEntry {
	depotFile: string
	action: string
	rev: number
	type: string // Perforce filetypes -- 'text', 'binary', 'binary+l', etc.
}

export interface DescribeResult {
	user: string
	status: string
	description: string
	path: string
	entries: DescribeEntry[]
	date: Date | null
}

export interface EdgeServer {
	id: string, 
	address: string
}

/**
	Example tracing output

	2020/09/12 19:40:33 576199000 pid 23850: <-  NetTcpTransport 10.200.65.101:63841 closing 10.200.21.246:1667
 */

const TRACE_OUTPUT_REGEX = /(\d\d\d\d\/\d\d\/\d\d \d\d:\d\d:\d\d \d+) pid (\d+): /

function parseTrace(response: string): [string, string] {
	const trace: string[] = []
	const nonTrace: string[] = []
	for (const line of response.split('\n')) {
		(line.match(TRACE_OUTPUT_REGEX) ? trace : nonTrace).push(line)
	}
	return [trace.join('\n'), nonTrace.join('\n')]
}


// parse the perforce tagged output format into an array of objects
// TODO: probably should switch this to scrape Python dictionary format (-G) since ztag is super inconsistent with multiline fields
export function parseZTag(buffer: string, opts?: ExecZtagOpts) {
	let output = [];

	// check for error lines ahead of the first ztag field
	let ztag_start = buffer.indexOf('...');
	if (ztag_start > 0) {
		// split the start off the buffer, then split it into newlines
		let preamble = buffer.substring(0, ztag_start).trim();
		output.push(preamble.split(newline_rex));
		buffer = buffer.substring(ztag_start);
	}
	else if (ztag_start < 0) {
		let preamble = buffer.trim();
		if (preamble.length > 0)
			output.push(preamble.split(newline_rex));
		buffer = "";
	}

	// resolve ztag can have some garbage that causes issues with
	// parsing the groups, so we're just going to strip them out
	if (opts && opts.resolve) {
		buffer = buffer.replaceAll(resolveGarbage_rex,"")
	}

	// split into groups
	let groups = buffer.split(ztag_group_rex);
	for (let i = 0; i < groups.length; ++i) {
		// make an object for each group
		let group: any = {};
		let text: string[] = [];

		// split fields
		let pairs = groups[i].split(ztag_field_rex);
		if (pairs[0] === "") {
			pairs.shift();
		}

		let setValue = false;
		for (let j = 0; j < pairs.length; ++j) {
			// each field is a key-value pair
			let pair = pairs[j].trim();
			if (pair === "")
				continue;

			let key, value;
			let s = pair.indexOf(' ');
			if (s >= 0) {
				key = pair.substring(0, s);
				value = pair.substring(s + 1);
				if (value.indexOf('\n') >= 0 && !(opts && opts.multiline)) {
					let lines = value.split('\n');
					value = lines.shift();
					text = text.concat(lines.filter((str) => { return str !== ""; }));
				}

				// if it's an integer, convert
				if (value!.match(integer_rex))
					value = parseInt(value!);
			}
			else {
				key = pair;
				value = true;
			}

			// set it on the group
			group[key] = value;
			setValue = true;
		}

		// if we have no values, omit this output
		if (!setValue)
			continue;

		// set to output
		output.push(group);

		// if we have raw text, add it at the end
		if (text.length > 0)
			output.push(text);
	}

	if (opts && opts.reduce) {
		return output.reduce((accumulator: any, value: any) => { accumulator = {...accumulator, ...value}; return accumulator; }, {})
	}
	return output;
}

class CommandRecord {
	constructor(public logger: ContextualLogger, public cmd: string, public start: Date = new Date()) {
	}
}


export interface Change {
	// from Perforce (ztag)
	change: number;
	client: string;
	user: string;
	desc: string;
	shelved?: number;
	time?: number;

	// hacked in
	isUserRequest?: boolean;
	ignoreExcludedAuthors?: boolean
	forceCreateAShelf?: boolean
	sendNoShelfNotification?: boolean // Used for requesting stomps for internal RM usage, such as stomp changes
	commandOverride?: string
	accumulateCommandOverride?: boolean

	// Stomp Changes support
	forceStompChanges?: boolean
	additionalDescriptionText?: string // Used for '#fyi' string to notify stomped authors
}


// After TS conversion, tidy up workspace usage
// Workspace and RoboWorkspace are fudged parameter types for specifying a workspace
// ClientSpec is a (partial) Perforce definition of a workspace

export interface Workspace {
	name: string,
	directory: string
}

// temporary fudging of workspace string used by main Robo code
export type RoboWorkspace = Workspace | string | null | undefined;

export interface ClientSpec {
	client: string
	Access: number
	Stream?: string
	IsUnloaded?: boolean
}

export type StreamSpec = {
	stream: string
	name: string
	parent: string
	desc: string
}

export type StreamSpecs = Map<string, StreamSpec>

export function isExecP4Error(err: any): err is [Error, string] {
	return Array.isArray(err) && err.length === 2 && err[0] instanceof Error && typeof err[1] === "string"
}

interface ExecOpts {
	stdin?: string
	quiet?: boolean
	noCwd?: boolean
	numRetries?: number
	trace?: boolean
	serverAddress?: string
}

interface ExecZtagOpts extends ExecOpts {
	format?: string; // if specified will be passed as the -F argument and the results returned without ztag parsing
	multiline?: boolean;
	resolve?: boolean; // hacky solution to clear certain problematic lines out of resolve ztags
	reduce?: boolean; // collapse the multiple entries to a single object, useful for problem parses that end up in multiple entries despite being a single result
}

export interface EditChangeOpts {
	newOwner?: string
	newWorkspace?: string
	newDescription?: string
	changeSubmitted?: boolean
	edgeServerAddress?: string
}

export interface IntegrateOpts {
	edgeServerAddress?: string
	virtual?: boolean
}

export interface IntegratedOpts {
	intoOnly?: boolean
	startCL?: number
	quiet?: boolean
}

export interface SyncParams {
	opts?: string[]
	quiet?: boolean
	edgeServerAddress?: string
	okToFail?: boolean // default is false
}

export interface ConflictedResolveNFile {
	clientFile: string // Workspace path on local disk
	targetDepotFile?: string // Target Depot path in P4 Depot
	fromFile: string // Source Depot path in P4 Depot
	startFromRev: number
	endFromRev: number
	resolveType: string // e.g. 'content', 'branch'
	resolveFlag: string // 'c' ?
	contentResolveType?: string  // e.g. '3waytext', '2wayraw' -- not applicable for delete/branch merges
}

interface ServerConfig {
	username: string
	serverID: string
	serverAddress?: string
	serverVersion: number
	multiServerEnvironment: boolean
	swarmURL?: string
}

export class ResolveResult {
	private rResultLogger: ContextualLogger
	private resolveOutput: any[] | null
	private dashNOutput: any[]
	private resolveDashNRan: boolean
	private remainingConflicts: ConflictedResolveNFile[] = []
	private hasUnknownConflicts = false
	private parsingError: string = ""
	private successfullyParsedRemainingConflicts = false

	constructor(_resolveOutput: any[] | null, resolveDashNOutput: string | any[], parentLogger: ContextualLogger) {
		this.rResultLogger = parentLogger.createChild('ResolveResult')
		this.resolveOutput = _resolveOutput

		// Non ztag resolve
		if (typeof resolveDashNOutput === 'string') {
			this.dashNOutput = [ resolveDashNOutput ]
			this.hasUnknownConflicts = resolveDashNOutput !== 'success'
			this.resolveDashNRan = false
		}
		// ZTag format
		else {
			this.dashNOutput = resolveDashNOutput
			this.resolveDashNRan = true
			if (this.dashNOutput.length > 0 && this.dashNOutput[0] !== 'success') {
				this.parseConflictsFromDashNOutput()
			}
		}
	}

	/* resolve -N will display the remaining files to have conflicts. It will display the following information in this order:
	 	... clientFile
		... fromFile 
		... startFromRev 
		... endFromRev 
		... resolveType
		... resolveFlag
		... contentResolveType

		Sometimes it will NOT have contentResolveType, but will have "Branch Resolve" information in the form of something similar to this:
			Branch resolve:
			at: branch
			ay: ignore
		
		We will allow these to be stomped
	*/
	private parseConflictsFromDashNOutput() {
		this.successfullyParsedRemainingConflicts = true
		try {
			for (let ztagGroup of this.dashNOutput) {
				if (ztagGroup[0] === "Branch resolve:" || ztagGroup[0] === "Delete resolve:") {
					if (!this.remainingConflicts[this.remainingConflicts.length - 1]) {
						throw new Error(`Encountered branch/delete resolve information, but could not find applicable conflict file: ${ztagGroup.toString()}`)
					}
					continue
				}

				["clientFile", "fromFile", "startFromRev", "endFromRev", "resolveType", "resolveFlag"].forEach(function (keyValue) {
					if (!ztagGroup[keyValue]) {
						throw new Error(`Resolve output missing ${keyValue} in ztag: ${JSON.stringify(ztagGroup)}`)
					}
				})

				let remainingConflict: ConflictedResolveNFile = {
					clientFile: ztagGroup["clientFile"],
					fromFile: ztagGroup["fromFile"],
					// This handles the use case where only one revision is conflicting and it's the first one
					startFromRev: ztagGroup["startFromRev"] === "none" ? 1 : parseInt(ztagGroup["startFromRev"]),
					endFromRev: parseInt(ztagGroup["endFromRev"]),
					resolveType: ztagGroup["resolveType"],
					resolveFlag: ztagGroup["resolveFlag"]
				}

				if (ztagGroup["contentResolveType"]) {
					remainingConflict.contentResolveType = ztagGroup["contentResolveType"]
				}

				// Create new remaining conflict
				this.remainingConflicts.push(remainingConflict)
			}
		}
		catch (err) {
			this.rResultLogger.printException(err, 'Error processing \'resolve -N\' output')
			this.parsingError = err.toString()
			this.successfullyParsedRemainingConflicts = false
			return
		}
	}

	hasConflict() {
		return this.remainingConflicts.length > 0 || this.hasUnknownConflicts
	}

	getResolveOutput() {
		return this.resolveOutput
	}

	getConflictsText(): string {
		const lines: string[] = this.dashNOutput.map(entry => typeof entry === 'string' ? entry : JSON.stringify(entry))
		return lines.join('\n')
	}

	successfullyParsedConflicts() {
		return this.successfullyParsedRemainingConflicts
	}

	getParsingError() {
		return this.parsingError
	}

	getConflicts() {
		return this.remainingConflicts
	}

	hasDashNResult() {
		return this.resolveDashNRan
	}
}

export type ChangelistStatus = 'pending' | 'submitted' | 'shelved'

const runningPerforceCommands = new Set<CommandRecord>();
export const getRunningPerforceCommands = () => [...runningPerforceCommands]
export const P4_FORCE = '-f';
const robomergeVersion = `v${VersionReader.getBuildNumber()}`

const MIN_SERVER_VERSION: number = 2020.1

/**
 * This method must succeed before PerforceContext can be used. Otherwise retrieving the Perforce context through
 * getServerContext() will error.
 */
export async function initializePerforce(logger: ContextualLogger, vault: Vault, devMode: boolean) {
	setInterval(() => {
		const now = Date.now()
		for (const command of runningPerforceCommands) {
			const durationMinutes = (now - command.start.valueOf()) / (60*1000)
			if (durationMinutes > 10) {
				command.logger.info(`Command still running after ${Math.round(durationMinutes)} minutes: ` + command.cmd)
			}
		}
	}, 5*60*1000)

	PerforceContext.devMode = devMode
	const servers = vault.valid && vault.perforceServers

	if (servers) {
		await Promise.all(Array.from(servers).map(([server, serverDetails]) => PerforceContext.initServer(logger, server, serverDetails)))
	}
	else {
		logger.info("Robomerge running in single server mode")
		await PerforceContext.initServer(logger, "default")
	}

}

/**
 * This is how our app interfaces with Perforce. All Perforce operations act through an
 * internal serialization object.
 */
export class PerforceContext {

	// Use same logger as instance owner, to cut down on context length
	private constructor(private readonly logger: ContextualLogger, private _serverConfig: ServerConfig) {
	}

	static devMode: boolean
	static readonly servers: Map<string, ServerConfig> = new Map()

	static async initServer(logger: ContextualLogger, serverID: string, serverDetails?: any) {
		
		const serverAddress = serverDetails && serverDetails.port

		let opts: ExecOpts = { serverAddress }
		let loginArgs = ["login"]
		if (serverDetails && serverDetails.user) {
			loginArgs = ["-u", serverDetails.user, ...loginArgs]
		}
		if (serverDetails && serverDetails.password) {
			opts.stdin = serverDetails.password
		}
		else {
			loginArgs.push('-s')
		}

		const output = await PerforceContext.execAndParse(logger, null, loginArgs, opts);
		let resp = output[0];
	
		if (resp && resp.User) {

			const [rawServerVersion, serversOutput, swarmOutput] = await Promise.all([
				PerforceContext._execP4(logger, null, ["-u", resp.User, "-ztag","-F","%serverVersion%","info"], { serverAddress} ),
				PerforceContext.execAndParse(logger, null, ["-u", resp.User, "servers"], { serverAddress }),
				PerforceContext._execP4(logger, null, ["-u", resp.User, "-ztag", "-F", "%value%", "property", "-l", "-n", "P4.Swarm.URL"], { serverAddress })
			])

			const match = rawServerVersion.match(/.*\/(\d+\.\d+)\/.*/)
			if (!match) {
				throw new Error(`Unable to parse server version from ${rawServerVersion}`)
			}
			let serverVersion = parseFloat(match[1])
			if (serverVersion < MIN_SERVER_VERSION) {
				throw new Error(`Robomerge requires a minimum server version of ${MIN_SERVER_VERSION}`)
			}
	
			let serverConfig: ServerConfig = {
				username: resp.User,
				serverID,
				serverAddress,
				serverVersion,
				multiServerEnvironment: serversOutput.length > 1,
				swarmURL: swarmOutput.length > 0 ? swarmOutput.trim() : undefined
			}
			this.servers.set(serverID, serverConfig)

			if (serverDetails && serverDetails.password) {
				// We're going to refresh our credentials every hour
				setInterval(() =>  {
					PerforceContext.execAndParse(logger, null, loginArgs, opts)
					.then((output) => {
						let resp = output[0];
						if (!resp.User) {
							logger.warn(`Failed to refresh credentials for ${serverID}: ${serverAddress}. Will try again in 1 hour.`)
						}
					})
					.catch(reason => {
						if (!isExecP4Error(reason)) {
							throw reason
						}

						const [_, output] = reason
						
						logger.warn(`Failed to refresh credentials for ${serverID}: ${serverAddress}.\nError: ${output}\n\nWill try again in 1 hour.`)
					})
			
				}, 60*60*1000)
			}

			if (serverAddress) {
				logger.info(`P4 server ${serverID}: ${serverAddress} initialized. User=${resp.User}`)
			}
			else {
				logger.info(`P4 initialized. User=${resp.User}`)
			}
		}
		else {
			throw new Error(`Failed to login to ${serverAddress || "server"}`)
		}
	}

	static sameServerID(aID?: string, bID?: string) {
		return (aID || 'default') === (bID || 'default')
	}

	static getServerContext(logger: ContextualLogger, serverID?: string) {
		serverID = serverID || 'default'
		const server = this.servers.get(serverID)
		if (!server) {
			throw new Error(`Unknown server ${serverID}`)
		}
		return new PerforceContext(logger, server)
	}

	static getServerConfig(serverID?: string) {
		return this.servers.get(serverID || 'default')
	}

	get username() {
		return this._serverConfig.username
	}

	get serverID() {
		return this._serverConfig.serverID
	}

	get serverAddress() {
		return this._serverConfig.serverAddress
	}

	get serverVersion() {
		return this._serverConfig.serverVersion
	}

	get multiServerEnvironment() {
		return this._serverConfig.multiServerEnvironment
	}

	get swarmURL() {
		return this._serverConfig.swarmURL
	}

	// get a list of all pending changes for this user
	async get_pending_changes(workspaceName?: string, edgeServerAddress?: string) {
		if (!this.username) {
			throw new Error("username not set");
		}

		let args = ['changes', '-u', this.username, '-s', 'pending']
		if (workspaceName) {
			args = [...args, '-c', workspaceName]
		}

		return this.execAndParse(null, args, { serverAddress: edgeServerAddress });
	}

	/** get a single change and return it in the format of changes() */
	async getChange(changenum: number) {
		let result = (await this.execAndParse(null, ['change', '-o', changenum.toString()]))[0]
		result.change = parseInt(result.Change)
		result.client = result.Client
		result.desc = result.Description
		result.user = result.User
		return result
	}

	/**
	 * Get a list of changes in a path since a specific CL
	 * @return Promise to list of changelists
	 */
	changes(path_in: string, since: number, limit?: number, status?: ChangelistStatus, quiet: boolean = true): Promise<Change[]> {
		const path = since > 0 ? `${path_in}@${since},now` : path_in;
		const args = ['changes', '-l',
			(status ? `-s${status}` : '-ssubmitted'),
			...(limit ? [`-m${limit}`] : []),
			path];

		return this.execAndParse(null, args, {quiet}, {
			expected: {change: 'integer', client: 'string', user: 'string', desc: 'string'},
			optional: {shelved: 'integer', oldChange: 'integer', IsPromoted: 'integer'}
		}) as Promise<unknown> as Promise<Change[]>
	}

	async latestChange(path: string, workspace?: RoboWorkspace): Promise<Change> {

		// temporarily waiting 30 seconds - filing ticket   - was: wait no longer than 5 seconds, retry up to 3 times
		const args = ['-vnet.maxwait=30', '-r3', 'changes', '-l', '-ssubmitted', '-m1', path]

		const startTime = Date.now()

		const result = await this.execAndParse(workspace, args, {quiet: true, trace: true}, changeResultExpectedShape)
		if (!result || result.length !== 1) {
			throw new Error(`Expected exactly one change. Got ${result ? result.length : 0}${result ? '' : '\n' + JSON.stringify(result)}`)
		}

		const durationSeconds = (Date.now() - startTime) / 1000;

		if (durationSeconds > 5.0) {
			this.logger.warn(`p4.latestChange took ${durationSeconds}s`)
		}

		return result[0] as Change
	}

	changesBetween(path: string, from: number, to: number) {
		const args = ['changes', '-l', '-ssubmitted', `${path}@${from},${to}`]
		return this.execAndParse(null, args, {quiet: true}, changeResultExpectedShape) as Promise<unknown> as Promise<Change[]>
	}

	async getDepot(depotName: string) {
		if ((await this.execAndParse(null, ['depots', '-e', depotName])).length > 0)
		{
			return (await this.execAndParse(null, ['depot', '-o', depotName]))[0]
		}
		return null
	}

	async getStreamName(path: string) {
		// Given the path, determine the depot and stream that a workspace needs to be created for
		let depotEndChar = path.indexOf('/',2)
		if (depotEndChar == -1) {
			return new Error(`Unable to determine depot from $(path)`)
		}

		const depot = await this.getDepot(path.substring(2,depotEndChar))
		if (!depot) {
			throw new Error(`Unable to find ${depot}`)
		}
		if (depot.Type == 'stream') {
			let streamDepth = depot.StreamDepth.match(/\//g).length - 2
			if (streamDepth < 1) {
				streamDepth = Number(depot.StreamDepth)
			}
			let streamNameEnd = 2;
			for (let i=0;i<streamDepth+1;i++) {
				let nextSlash = path.indexOf("/",streamNameEnd+1)
				if (nextSlash == -1) {
					if (i == streamDepth) {
						return path
					}
					else {
						return new Error(`Unable to determine stream from ${path}: not enough depth`)
					}
				}
				streamNameEnd = path.indexOf("/",streamNameEnd+1)
			}
			return path.substring(0,streamNameEnd)
		}
		return new Error(`Depot ${depot} is not of type stream`)
	}

	async streams() {
		const rawStreams = await this.execAndParse(null, ['streams'], {quiet: true})
		const streams = new Map<string, StreamSpec>()
		for (const raw of rawStreams) {
			const stream: StreamSpec = {
				stream: raw.Stream as string,
				name: raw.Name as string,
				parent: raw.Parent as string,
				desc: raw.desc as string,
			}
			streams.set(stream.stream, stream)
		}
		return streams
	}

	async stream(streamName: string) {
		try {
			const stream = (await this.execAndParse(null, ['streams', streamName]))[0]
			const streamSpec: StreamSpec = {
				stream: stream.Stream as string,
				name: stream.Name as string,
				parent: stream.Parent as string,
				desc: stream.desc as string,
			}
			return streamSpec
		}
		catch {
			return null
		}
	}

	async branchExists(branchName: string) {
		try {
			return (await this.execAndParse(null, ['branches','-e',branchName])).length == 1
		}
		catch {
			return false
		}
	}

	// find a workspace for the given user
	// output format is an array of workspace names
	async find_workspaces(user?: string, options?: {edgeServerAddress?: string, includeUnloaded?: boolean}) {

		const edgeServerAddress = options && options.edgeServerAddress
		const includeUnloaded = options && options.includeUnloaded

		let args = ['clients', '-u', user || this.username]

		let opts: ExecOpts = {}
		if (edgeServerAddress && edgeServerAddress !== 'commit') {
			opts.serverAddress = edgeServerAddress
		}
		else {
			// -a to include workspaces on edge servers
			args.push('-a')
		}

		let workspaces = [];
		try {
			let parsedLoadedClients = this.execAndParse(null, args, opts);
			let parsedUnloadedClients = (includeUnloaded ? this.execAndParse(null, [...args, '-U'], opts) : null)
			for (let clientDef of await parsedLoadedClients) {
				if (clientDef.client) {
					workspaces.push(clientDef);
				}
			}
			if (includeUnloaded) {
				for (let clientDef of await parsedUnloadedClients!) {
					if (clientDef.client) {
						workspaces.push(clientDef);
					}
				}
			}
		}
		catch (reason) {
			if (!isExecP4Error(reason)) {
				throw reason
			}

			const errorMsg = `Attempted to find workspaces for invalid user ${user || this.username}`
			this.logger.error(errorMsg)
			postToRobomergeAlerts(errorMsg)
		}

		return workspaces as ClientSpec[];
	}

	async find_workspace_by_name(workspaceName: string, options?: {edgeServerAddress?: string, includeUnloaded?: boolean}) {

		const edgeServerAddress = options && options.edgeServerAddress
		const includeUnloaded = options && options.includeUnloaded

		let args = ['clients', '-E', workspaceName]

		let opts: ExecOpts = {}
		if (edgeServerAddress && edgeServerAddress !== 'commit') {
			opts.serverAddress = edgeServerAddress
		}
		else {
			// -a to include workspaces on edge servers
			args.push('-a')
		}

		let result = await this.execAndParse(null, args, opts);
		if (includeUnloaded && result.length == 0) {
			result = await this.execAndParse(null, [...args, '-U'], opts)
		}

		return result
	}

	async getWorkspaceEdgeServer(workspaceName: string): Promise<EdgeServer | null> {
		const serverIdLine = await this._execP4(null, ['-ztag', '-F', '%ServerID%', 'client', '-o', workspaceName]) 
		if (serverIdLine) {
			const serverId = serverIdLine.trim()
			const address = await this.getEdgeServerAddress(serverId)
			if (!address) {
				throw new Error(`Couldn't find address for edge server '${serverId}'`)
			}
			return {id: serverId, address: address.trim()}
		}
		return null
	}

	async reloadWorkspace(workspaceName: string, edgeServerAddress?: string) {
		let result
		try {
			 result = await this.execAndParse(null, ['reload', '-c', workspaceName], {serverAddress: edgeServerAddress});
		}
		catch (reason) {
			if (!isExecP4Error(reason)) {
				throw reason
			}
			const [err, output] = reason

			// reload succeeds with an error because ... sigh
			if (!output.trim().startsWith('Client') ||
				!output.trim().endsWith('reloaded.')) {
				throw err
			}
			result = output;			
		}
		return result
	}

	getEdgeServerAddress(serverId: string) {
		return this._execP4(null, ['-ztag', '-F', '%Address%', 'server', '-o', serverId])
	}

	async clean(roboWorkspace: RoboWorkspace, edgeServerAddress?: string) {
		let result;
		try {
			result = await this._execP4(roboWorkspace, ['clean', '-e', '-a'], {serverAddress: edgeServerAddress});
		}
		catch (reason) {
			if (!isExecP4Error(reason)) {
				throw reason
			}
			const [err, output] = reason

			// ignore this error
			if (!output.trim().startsWith('No file(s) to reconcile')) {
				throw err
			}
			result = output;
		}
		this.logger.info(`p4 clean:\n${result}`);
	}

	// sync the depot path specified
	async sync(roboWorkspace: RoboWorkspace, depotPath: string|string[], params?: SyncParams) {
		if (typeof depotPath === "string") {
			depotPath = [depotPath]
		}
		const args = ['sync', ...(params && params.opts || []), ...depotPath]
		try {
			return await this.execAndParse(roboWorkspace, args, {serverAddress: params?.edgeServerAddress, quiet: params?.quiet})
		}
		catch (reason) {
			if (!isExecP4Error(reason)) {
				throw reason
			}
			if (!params || !params.okToFail) {
				const [err, output] = reason
				// this is an acceptable non-error case for us
				if (!output || typeof output !== "string" || !output.trim().endsWith("up-to-date.")) {
					throw err
				}
			}
		}

		return []
	}

	newWorkspace(workspaceName: string, params: any, edgeServer?: EdgeServer) {
		params.Client = workspaceName
		if (!('Root' in params)) {
			params.Root = 'd:/ROBO/' + workspaceName // default windows path
		}

		if (!('Owner' in params)) {
			params.Owner = this.username
		}

		if (!('Options' in params)) {
			params.Options = 'noallwrite clobber nocompress nomodtime'
		}

		if (!('SubmitOptions' in params)) {
			params.SubmitOptions = 'submitunchanged'
		}

		if (!('LineEnd' in params)) {
			params.LineEnd = 'local'
		}

		let args = ['client', '-i']
		if (edgeServer) {
			params.ServerID = edgeServer.id
		}

		let workspaceForm = ''
		for (let key in params) {
			let val = params[key];
			if (Array.isArray(val)) {
				workspaceForm += `${key}:\n`;
				for (let item of val) {
					workspaceForm += `\t${item}\n`;
				}
			}
			else {
				workspaceForm += `${key}: ${val}\n`;
			}
		}

		// run the p4 client command
		this.logger.info(`Executing: 'p4 client -i' to create workspace ${workspaceName}`);
		return this._execP4(null, args, { stdin: workspaceForm, quiet: true, serverAddress: edgeServer?.address });
	}

	// Create a new workspace for Robomerge GraphBot
	newGraphBotWorkspace(name: string, extraParams: any, edgeServer?: EdgeServer) {
		return this.newWorkspace(name, {Root: getRootDirectoryForBranch(name), ...extraParams}, edgeServer);
	}

	// Create a new workspace for Robomerge to read branchspecs from
	newBranchSpecWorkspace(workspace: Workspace, bsDepotPath: string) {
		let roots: string | string[] = '/app/data' // default linux path
		if (workspace.directory !== roots) {
			roots = [roots, workspace.directory] // specified directory
		}

		const params: any = {
			AltRoots: roots
		}

		// Perforce paths are mighty particular 
		if (bsDepotPath.endsWith("/...")) {
			params.View = bsDepotPath + ` //${workspace.name}/...`
		} else if (bsDepotPath.endsWith("/")) {
			params.View = bsDepotPath + `... //${workspace.name}/...`
		} else {
			params.View = bsDepotPath + `/... //${workspace.name}/...`
		}

		return this.newWorkspace(workspace.name, params);
	}


	// create a new CL with a specific description
	// output format is just CL number
	async new_cl(roboWorkspace: RoboWorkspace, description: string, files?: string[], edgeServerAddress?: string) {
		const workspace = coercePerforceWorkspace(roboWorkspace);
		// build the minimal form
		let form = 'Change:\tnew\nStatus:\tnew\nType:\tpublic\n';
		if (workspace)
			form += `Client:\t${workspace.name}\n`;

		if (files) {
			form += 'Files:\n';
			for (const filename of files) {
				form += `\t${filename}\n`;
			}
		}
		form += 'Description:\n\t' + this._sanitizeDescription(description);

		// run the P4 change command
		this.logger.info("Executing: 'p4 change -i' to create a new CL");
		while (true) {
			try {
				const output = await this._execP4(workspace, ['change', '-i'], { stdin: form, quiet: true, serverAddress: edgeServerAddress });
				// parse the CL out of output
				const match = output.match(/Change (\d+) created./);
				if (!match) {
					throw new Error('Unable to parse new_cl output:\n' + output);
				}
				// return the changelist number
				return parseInt(match![1]);
			}
			catch (reason) {
				if (!isExecP4Error(reason)) {
					throw reason
				}

				let [err, output] = reason
				// If perforce timed out try again
				if (output.includes("Operation took too long")) {
					this.logger.info("p4 change -i timed out. Retrying.")
					continue
				}

				throw err;			
			}
		}
	}

	async transfer(targetP4: PerforceContext, roboWorkspace: RoboWorkspace, source: IntegrationSource, dest_changelist: number, target: IntegrationTarget, printIfFailures: boolean)
	: Promise<string[]> {

		// TODO: support branchspecs
		if (source.branchspec) {
			return ["Branchspecs not yet supported for transfers."]
		}

		const files = await this.files(`${source.path_from}@=${source.changelist}`)

		let failures: string[] = []

		const p4quiet = !PerforceContext.devMode
		const sourceStreamLength = source.stream!.length
		const targetFilter = `${target.stream!}/...` == target.path_to ? null : makeRegexFromPerforcePath(target.path_to)

		let sortedFiles = new Map<string, any[]>()
		const addSortedFile = (key: string, file: any) => {
			if (!sortedFiles.has(key)) {
				sortedFiles.set(key,[])
			}
			sortedFiles.get(key)!.push(file)
		}

		files.forEach(file => {
			const targetDepotFile = target.stream! + file.depotFile.substring(sourceStreamLength)
			if (targetFilter && !targetDepotFile.match(targetFilter)) {
				// filter out files that don't match the target.path_to
				return
			}
			switch(file.action) {
				case 'add':
				case 'branch': {
					addSortedFile(`add:${file.type}`, [file, targetDepotFile])
					break
				}
				case 'edit':
				case 'integrate': {
					addSortedFile(`edit:${file.type}`, [file, targetDepotFile])
					break
				}
				case 'delete': {
					addSortedFile('delete', [file, targetDepotFile])
					break
				}
				case 'move/delete': {
					addSortedFile('move', [file, targetDepotFile])
					break
				}
				case 'move/add': break // move/add is handled from the move/delete portion of the pair

				default: {
					failures.push(`Unhandled transfer action ${file.action} for ${file.depotFile} in CL# ${source.changelist}`)
				}
			}
		})

		let promises: Promise<any>[] = []
		let printCommands: any[] = []

		const FILES_PER_ACTION = 100

		const HandleCaughtError = (reason: any, filterOut?: (s: string) => boolean) => {
			if (!isExecP4Error(reason)) {
				failures.push(reason)
			}

			const [_, output] = reason
			let reasons = output.split('\n')
			if (filterOut) {
				reasons = reasons.filter((s: string) => s.length > 0 && !filterOut(s))
			}
			failures.push(...reasons)
		}

		sortedFiles.forEach((files, action) => {
			if (action.startsWith('add')) {
				const fileType = action.slice(4)
				for (let i=0; i < files.length; i += FILES_PER_ACTION) {
					const filesSlice = files.slice(i,i+FILES_PER_ACTION)
					const targetDepotFiles = filesSlice.map(([_, targetDepotFile]) => targetDepotFile)
					promises.push(
						targetP4.add(roboWorkspace, dest_changelist, targetDepotFiles, fileType, p4quiet)
						        .then(adds => adds.forEach((add, index) => printCommands.push([`${filesSlice[index][0].depotFile}@${source.changelist}`, add.clientFile])))
						        .catch(reason => HandleCaughtError(reason))
					)
				}
			}
			else if (action.startsWith('edit')) {
				const fileType = action.slice(5)
				for (let i=0; i < files.length; i += FILES_PER_ACTION) {
					const filesSlice = files.slice(i,i+FILES_PER_ACTION)
					const targetDepotFiles = filesSlice.map(([_, targetDepotFile]) => targetDepotFile)
					promises.push(
						targetP4.sync(roboWorkspace, targetDepotFiles, {opts: ['-k'], quiet: p4quiet})
						        .then(syncs => { 
									syncs.forEach((sync, index) => printCommands.push([`${filesSlice[index][0].depotFile}@${source.changelist}`, sync.clientFile]))
									return targetP4.edit(roboWorkspace, dest_changelist, targetDepotFiles, ['-k', '-t', fileType], p4quiet)
									               .catch(reason => HandleCaughtError(reason))
						        })
						        .catch(reason => HandleCaughtError(reason))
					)
				}
			}
			else if (action === 'delete') {
				for (let i=0; i < files.length; i += FILES_PER_ACTION) {
					promises.push(
						targetP4.delete(roboWorkspace, dest_changelist, files.slice(i,i+FILES_PER_ACTION).map(([_, targetDepotFile]) => targetDepotFile), p4quiet)
						.catch(reason => HandleCaughtError(reason, s => s.endsWith("file(s) not on client.")))
					)
				}
			}
			else if (action === 'move') {
				promises.push(...files.map(([file, targetDepotFile]) => {
					return this.integrated(null, file.depotFile, {intoOnly: true, startCL: source.changelist, quiet: p4quiet})
					.then(integrated => {
						const integ = integrated.find(integ => parseInt(integ.change) == source.changelist)
						if (integ) {
							const targetMoveAddDepotFile = target.stream! + integ.fromFile.substring(sourceStreamLength)
							return targetP4.sync(roboWorkspace, targetDepotFile, {quiet: p4quiet})
							.then(() => { return targetP4.edit(roboWorkspace, dest_changelist, targetDepotFile, undefined, p4quiet) })
							.then(() => { return targetP4.move(roboWorkspace, dest_changelist, targetDepotFile, targetMoveAddDepotFile, ['-f'], p4quiet) })
							.then(move => printCommands.push([`${integ.fromFile}@${source.changelist}`, move[0].path]))
							.catch(reason => HandleCaughtError(reason))
						}
						else {
							failures.push(`Unable to look up integrated record for ${file.depotFile} in CL# ${source.changelist}`)
							return
						}
					})
				}))
			}
			else {
				throw new Error(`Unexpected action ${action} in sortedFiles map`)
			}
		})

		await Promise.all(promises)

		const MAX_PRINT_FAILURES = 10
		let failedPrints = 0
		if (printIfFailures || failures.length == 0) {
			const PARALLEL_PRINTS = 25
			const doPrints = async (startIndex: number) => {
				for (let i=startIndex; i < printCommands.length; i += PARALLEL_PRINTS) {
					while (failedPrints < MAX_PRINT_FAILURES) {
						try {
							await this.print(printCommands[i][0],printCommands[i][1])
							break
						} catch(reason) {
							if (!isExecP4Error(reason)) {
								throw reason
							}
							const [err, _] = reason
							this.logger.warn(err)
							failedPrints++
						}
					}
				}
			}
			await Promise.all(Array.from(Array(PARALLEL_PRINTS).keys()).map(n => doPrints(n)))
		}
		if (failedPrints == MAX_PRINT_FAILURES) {
			failures.push(`Failed p4 print ${MAX_PRINT_FAILURES} times. Aborting. See log for failures.`)
		}

		return failures
	}

	// integrate a CL from source to destination, resolve, and place the results in a new CL
	// output format is true if the integration resolved or false if the integration wasn't necessary (still considered a success)
	// failure to resolve is treated as an error condition
	async integrate(roboWorkspace: RoboWorkspace, source: IntegrationSource, dest_changelist: number, target: IntegrationTarget, opts?: IntegrateOpts)
		: Promise<[string, (Change | string)[]]> {

		opts = opts || {};

		// build a command
		let cmdList = [
			"integrate",
			"-Ob",
			"-Or",
			"-c" + dest_changelist
		];
		const range = `@${source.changelist},${source.changelist}`

		if (opts.virtual) {
			cmdList.push('-v')
		}

		let noSuchFilesPossible = false // Helper variable for error catching
		// Branchspec -- takes priority above other possibilities
		if (source.branchspec) {
			if (this.serverVersion >= 2024.1) {
				// 2024.1 now prevents merges between streams by default
				cmdList.push("-F")
			}
			cmdList.push("-b");
			cmdList.push(source.branchspec.name);
			if (source.branchspec.reverse) {
				cmdList.push("-r");
			}
			cmdList.push(target.path_to + range);
			noSuchFilesPossible = true
		}
		// Stream integration
		else if (source.depot === target.depot && source.stream && target.stream) {
			cmdList.push("-S")
			cmdList.push(source.stream)
			cmdList.push("-P")
			cmdList.push(target.stream)
			cmdList.push(target.path_to + range)
			noSuchFilesPossible = true
		}
		// Basic branch to branch
		else {
			if (this.serverVersion >= 2024.1) {
				// 2024.1 now prevents merges between streams by default
				cmdList.push("-F")
			}
			cmdList.push(source.path_from + range);
			cmdList.push(target.path_to);
		}

		// execute the P4 command
		let changes;
		try {
			changes = await this.execAndParse(roboWorkspace, cmdList, { numRetries: 0, serverAddress: opts.edgeServerAddress });
		}
		catch (reason) {
			if (!isExecP4Error(reason)) {
				throw reason
			}

			let [err, output] = reason

			// if this change has already been integrated, this is a special return (still a success)
			if (output.match(/already integrated\.\n/)) {
				return ["already_integrated", []];
			}
			else if (output.match(/already integrated in pending changelist\./)) {
				return ["integration_already_in_progress", []];
			}
			else if (output.match(/No file\(s\) at that changelist number\.\n/)) {
				return ["no_files", []];
			}
			else if (noSuchFilesPossible && output.match(/No such file\(s\)\.\n/)) {
				return ["no_files", []];
			}
			else if (output.match(/no target file\(s\) in branch view\n/)) {
				return ["no_files", []];
			}
			else if (output.match(/resolve move to/)) {
				return ["partial_integrate", output.split('\n')];
			}
			else {
				const knownIntegrationFailure = this.parseIntegrationFailure(err.toString().split('\n'))
				if (knownIntegrationFailure) {
					return knownIntegrationFailure
				}
			}

			// otherwise pass on error
			this.logger.printException(err, "Error encountered during integrate: ")
			throw err;
		}

		// annoyingly, p4 outputs failure to integrate locked files here on stdout (not stderr)

		const failures: string[] = [];
		for (const change of changes) {
			if (Array.isArray(change)) {
				// P4 emitted some error(s) in the stdout
				failures.push(...change);
			}
		}

		// if there were any failures, return that
		if (failures.length > 0) {
			return ["partial_integrate", failures];
		}

		// everything looks good
		return ["integrated", <Change[]>changes];
	}

	async resolveHelper(workspace: Workspace | null, flag: string, changelist: number, edgeServerAddress?: string): Promise<any> {
		try {
			// Perform merge
			return await this._execP4Ztag(workspace, ['resolve', flag, `-c${changelist}`], {serverAddress: edgeServerAddress})
		}
		catch (reason) {
			if (!isExecP4Error(reason)) {
				throw reason
			}

			let [err, output] = reason
			let result: string | null = null

			if (output.startsWith('No file(s) to resolve.')) {
				result = 'success'
			}
			else if (
				output.match(/can't move \(open for delete\)/) ||
				output.match(/can't delete moved file;/) ||
				output.match(/resolve skipped/)) {
				result = output
			}

			if (!result) {
				throw err
			}

			return output
		}
	}


	// output format a list of conflicting files (p4 output)
	async resolve(roboWorkspace: RoboWorkspace, changelist: number, resolution: string, disallowDashN?: boolean, edgeServerAddress?: string)
		: Promise<ResolveResult> {
		const workspace = coercePerforceWorkspace(roboWorkspace)
		let flag = null
		switch (resolution) {
			case 'safe': flag = '-as'; break
			case 'clobber': // Clobber should perform a normal merge first, then resolve with '-at' afterwards
			case 'normal': flag = '-am'; break
			case 'null': flag = '-ay'; break
			default:
				throw new Error(`Invalid resultion type ${resolution}`)
		}

		// Perform merge
		let fileInfo = await this.resolveHelper(workspace, flag, changelist, edgeServerAddress)

		// Clobber remaining files after first merge, if requested
		if (resolution === 'clobber') {
			// If resolveHelper() returned a string, we have nothing to clobber due to no files remaining or an error being thrown.
			// Either case is a bad clobber request. Throw an error.
			if (typeof fileInfo === 'string') {
				if (fileInfo === 'success') {
					return new ResolveResult(null, fileInfo, this.logger)
				}
				throw new Error(`Cannot continue with clobber request -- merge attempt before clobber returned "${fileInfo}"`)
			}

			// Otherwise, fileInfo should be updated with the clobber result
			fileInfo = await this.resolveHelper(workspace, '-at', changelist, edgeServerAddress)
		}

		// If resolveHelper() returned a string, we do not need to return a dashNResult
		if (typeof fileInfo === 'string') {
			// If our error is just there are no files to resolve, that is a success
			if (fileInfo.startsWith('No file(s) to resolve.')) {
				return new ResolveResult(null, 'success', this.logger)
			}
			return new ResolveResult(null, fileInfo, this.logger)
		}

		if (disallowDashN) {
			this.logger.warn('Skipping resolve -N')
			return new ResolveResult(null, 'skipped resolve -N', this.logger)
		}

		let dashNresult: string[]
		try {
			dashNresult = await this._execP4Ztag(workspace, ['resolve', '-N', `-c${changelist}`], {serverAddress: edgeServerAddress, resolve: true})
		}
		catch (reason) {
			if (!isExecP4Error(reason)) {
				throw reason
			}

			let [err, output] = reason
			
			// If our error is just there are no files to resolve, that is a success
			if (output.startsWith('No file(s) to resolve.')) {
				dashNresult = ['success']
			} else {
				throw err
			}

		}

		return new ResolveResult(fileInfo, dashNresult, this.logger)
	}

	// submit a CL
	// output format is final CL number or false if changes need more resolution
	async submit(roboWorkspace: RoboWorkspace, changelist: number, edgeServerAddress?: string): Promise<number | string> {
		let rawOutput: string
		try {
			rawOutput = await this._execP4(roboWorkspace, ['-ztag', 'submit', '-f', 'submitunchanged', '-c', changelist.toString()], {serverAddress: edgeServerAddress})
		}
		catch ([errArg, output]) {
			const err = errArg.toString().trim()
			const out: string = output.trim()
			if (out.startsWith('Merges still pending --')) {
				// concurrent edits (try again)
				this.logger.info(`=== SUBMIT FAIL === \nERR:${err}\nOUT:${out}`);
				return 0
			}
			else if (out.startsWith('Out of date files must be resolved or reverted')) {
				// concurrent edits (try again)
				this.logger.info(`=== SUBMIT FAIL === \nERR:${err}\nOUT:${out}`);
				return 0
			}
			else if (out.search(/Change \d+ unknown/) >= 0 || out.search(/Change \d+ is already committed./) >= 0)
			{
				/* Sometimes due to intermittent internet issues, we can succeed in submitting but we'll try to submit
				 * again, and the pending changelist doesn't exist anymore.
 				 * By returning zero, RM will waste a little time resolving again but should find no needed changes,
				 * handling this issue gracefully.
				 */
				this.logger.info(`=== SUBMIT FAIL === \nERR:${err}\nOUT:${out}`);
				return 0
			}
			else if (out.indexOf('File(s) couldn\'t be locked.') >= 0)
			{
				this.logger.info(`=== SUBMIT FAIL === \nERR:${err}\nOUT:${out}`);
				return err.toString()
			}
			else if (out.startsWith('No files to submit.')) {
				this.logger.info(`=== SUBMIT FAIL === \nERR:${err}\nOUT:${out}`);
				await this.deleteCl(roboWorkspace, changelist)
				return 0
			}
			else if (out.startsWith('Submit validation failed')) {
				// unable to submit due to validation trigger
				this.logger.info(`=== SUBMIT FAIL === \nERR:${err}\nOUT:${out}`);
				return err.toString()
			}
			throw err
		}

		// success, parse the final CL
		const result: any[] = parseZTag(rawOutput)
		// expecting list of committed files, followed by submitted CL, optionally followed by list of refreshed files
		const numResults = result.length
		for (let index = numResults - 1; index >= 0; --index) {
			if (result[index].depotFile) {
				break
			}
			const finalCL = result[index].submittedChange
			if (finalCL) {
				if (typeof finalCL === 'number') {
					return finalCL
				}
				break
			}
		}

		this.logger.error(`=== SUBMIT FAIL === \nOUT:${rawOutput}`)
		throw new Error(`Unable to find submittedChange in P4 results:\n${rawOutput}`)
	}


	// SHOULDN'T REALLY LEAK [err, result] FORMAT IN REJECTIONS

	// 	- thinking about having internal and external exec functions, but, along
	// 	- with ztag variants, that's quite messy

	// should refactor so that:
	//	- exec never fails, instead returns a rich result type
	//	- ztag is an option
	//	- then can maybe have a simple wrapper


	// delete a CL
	// output format is just error or not
	async deleteCl(roboWorkspace: RoboWorkspace, changelist: number, edgeServerAddress?: string) {
		try {
			await this._execP4(roboWorkspace, ["change", "-d", changelist.toString()], {serverAddress: edgeServerAddress})
		}
		catch (reason) {
			if (!isExecP4Error(reason)) {
				throw reason
			}

			let [_, output] = reason
			if (!output.match(/Change (\d+) unknown\./)) {
				throw reason
			}
		}
	}

	async getOpenedDetails(paths: {depotPath: string, name: string}[]) {
		const results: ExclusiveFileDetails[] = []
		const openedRequests: [{depotPath: string, name: string}, Promise<OpenedFileRecord[]>, boolean][] = []
		for (const path of paths) {
			openedRequests.push([path, this.opened(null, path.depotPath, true), true])
		}

		const failedResults: ExclusiveFileDetails[] = []
		for (const [path, request, exclusive] of openedRequests) {
			const recs = await request
			if (recs.length > 0) {
				// should only be one, since we're looking for exclusive check-out errors
				results.push({depotPath: path.depotPath, name: path.name, user: recs[0].user, client: recs[0].client})
			} else if (exclusive) {
				// if we failed to find it as an exclusive check-out, try as non-exclusive which will find adds
				openedRequests.push([path, this.opened(null, path.depotPath), false])
			} else {
				this.logger.warn(`Failed to get the opened status of ${path.depotPath}`)
				failedResults.push({depotPath: path.depotPath, name: path.name, user: "", client: ""})
			}
		}
		results.push(...failedResults)

		return results
	}

	// run p4 'opened' command: lists files in changelist with details of edit state (e.g. if a copy, provides source path)
	opened(roboWorkspace: RoboWorkspace | null, arg: number | string, exclusive?: boolean) {
		const args = ['opened']
		if (typeof arg === 'number') {
			// see what files are open in the given changelist
			args.push('-c', arg.toString())
		}
		else {
			// see which workspace has a file checked out/added
			args.push(exclusive && this.multiServerEnvironment ? '-x' : '-a', arg)
		}
		return this.execAndParse(roboWorkspace, args) as Promise<OpenedFileRecord[]>
	}

	async revertFile(file: string) {
		let opened = await this.opened(null, file, true)
		if (opened.length == 0) {
			opened = await this.opened(null, file)
			if (opened.length == 0) {
				this.logger.warn(`${file} does not appear to be open, cannot revert`)
				return null
			}
		}
		const client = opened[0].client
		return this.revertFiles([file],client)
	}

	async revertFiles(files: string[], client: string) {
		const edgeServer = await this.getWorkspaceEdgeServer(client)
		const args = ['revert', '-C', client, ...files]
		try {
			const results = await this.execAndParse(null, args, {serverAddress: edgeServer?.address})

			// Files can fail to be reverted because of moves, lets try and resolve that
			const movedFiles = 
				results.filter((result: any) => !result.action)
					   .reduce((matches: string[], results: string[]) => 
					   		matches.concat(
								results.map(result => result.match(REVERT_FAILURE_DUE_TO_MOVE_REGEX))
								       .filter(match => match)
								       .map(match => match![1])
							), [])
			if (movedFiles.length > 0) {
				const openedFiles = await this.execAndParse(null, ['opened','-a', ...movedFiles])
				const pairedAdds: string[] = openedFiles.map((openedFile: any) => openedFile.movedFile)
				await this.revertFiles(pairedAdds, client)
			}
		}
		catch (reason) {
			if (!isExecP4Error(reason)) {
				throw reason
			}

			let [err, output] = reason
			// this happens if there's literally nothing in the CL. consider this a success
			if (!output.match(/file\(s\) not opened (?:on this client|in that changelist)\./)) {
				throw err;
			}
		}
	}

	// revert a CL deleting any files marked for add
	// output format is just error or not
	async revert(roboWorkspace: RoboWorkspace, changelist: number, opts?: string[], edgeServerAddress?: string) {
		const workspace = coercePerforceWorkspace(roboWorkspace);
		const path = workspace && workspace.name ? `//${workspace.name}/...` : ''
		try {
			await this._execP4(workspace, ['revert',
				/*"-w", seems to cause ENOENT occasionally*/ ...(opts || []), '-c', changelist.toString(), path],
				{serverAddress: edgeServerAddress});
		}
		catch (reason) {
			if (!isExecP4Error(reason)) {
				throw reason
			}

			let [err, output] = reason
			// this happens if there's literally nothing in the CL. consider this a success
			if (!output.match(/file\(s\) not opened (?:on this client|in that changelist)\./) &&
				!output.match(/Change (\d+) unknown\./)) {
				throw err;
			}
		}
	}

	revertAndDelete(roboWorkspace: string, cl: number, edgeServerAddress?: string) {
		return this.revert(roboWorkspace, cl, [], edgeServerAddress)
		.then(() => {
			return this.deleteCl(roboWorkspace, cl, edgeServerAddress);	
		})
	}

	// list files in changelist that need to be resolved
	async listFilesToResolve(roboWorkspace: RoboWorkspace, changelist: number) {
		try {
			return await this.execAndParse(roboWorkspace, ['resolve', '-n', '-c', changelist.toString()]);
		}
		catch (err) {
			if (err.toString().toLowerCase().includes('no file(s) to resolve')) {
				return [];
			}
			throw (err);
		}
	}

	async move(roboWorkspace: RoboWorkspace, cl: number, src: string, target: string, opts?: string[], quiet?: boolean) {
		return this.execAndParse(roboWorkspace, ['move', ...(opts || []), '-c', cl.toString(), src, target], {quiet});
	}

	/**
	 * Run a P4 command by name on one or more files (one call per file, run in parallel)
	 */
	run(roboWorkspace: RoboWorkspace, action: string, cl: number, filePaths: string[], opts?: string[]) {
		const args = [action, ...(opts || []), '-c', cl.toString()]
		return Promise.all(filePaths.map(
			path => this._execP4(roboWorkspace, [...args, path])
		));
	}

	// shelve a CL
	// output format is just error or not
	async shelve(roboWorkspace: RoboWorkspace, changelist: number, edgeServerAddress?: string) {
		try {
			await this._execP4(roboWorkspace, ['shelve', '-f', '-c', changelist.toString()],
				{ numRetries: 0, serverAddress: edgeServerAddress })
		}
		catch (reason) {
			if (!isExecP4Error(reason)) {
				throw reason
			}

			let [err, output] = reason

			if (output && output.match && output.match(/No files to shelve/)) {
				return false
			}
			throw err
		}
		return true
	}

	// shelve a CL
	// output format is just error or not
	async unshelve(roboWorkspace: RoboWorkspace, changelist: number) {
		try {
			await this._execP4(roboWorkspace, ['unshelve', '-s', changelist.toString(), '-f', '-c', changelist.toString()])
		}
		catch (reason) {
			if (!isExecP4Error(reason)) {
				throw reason
			}

			let [err, output] = reason
			if (output && output.match && output.match(/No files to unshelve/)) {
				return false
			}
			throw err
		}
		return true
	}

	// delete shelved files from a CL
	delete_shelved(roboWorkspace: RoboWorkspace, changelist: number) {
		return this._execP4(roboWorkspace, ["shelve", "-d", "-c", changelist.toString(), "-f"]);
	}

	async getUser(username: string) {
		if (username.length > 0 && !isUserSlackGroup(username) && !isUserAKnownBot(username)) {
			try {
				return (await this.execAndParse(null, ['users', username], {quiet:true}))[0]
			}
			catch (reason) {
				if (!isExecP4Error(reason)) {
					throw reason
				}
	
				let [err, output] = reason
				if (output.includes('no such user')) {
					return null
				}
				throw err
			}
		}
		return null
	}

	// get the email (according to P4) for a specific user
	async getEmail(username: string) {
		const user = await this.getUser(username)
		return user && user.Email
	}

	/** Check out a file into a specific changelist ** ASYNC ** */
	edit(roboWorkspace: RoboWorkspace, cl: number, filePath: string|string[], additionalArgs?: string[], quiet?: boolean) {
		if (typeof filePath === "string") {
			filePath = [filePath]
		}
		const args = [
			'edit', 
			'-c', cl.toString(), 
			...(additionalArgs || []),
			...filePath
		]
		return this.execAndParse(roboWorkspace, args, {quiet});
	}

	/** Add a file into a specific changelist ** ASYNC ** */
	add(roboWorkspace: RoboWorkspace, cl: number, filePath: string|string[], filetype?: string, quiet?: boolean) {
		if (typeof filePath === "string") {
			filePath = [filePath]
		}
		const args = [
			'add',
			'-f',
			'-c', cl.toString(), 
			...(filetype ? ['-t', filetype] : []),
			...filePath
		]
		return this.execAndParse(roboWorkspace, args, {quiet});
	}

	/** Mark a file for delete into a specific changelist ** ASYNC ** */
	delete(roboWorkspace: RoboWorkspace, cl: number, filePath: string|string[], quiet?: boolean) {
		if (typeof filePath === "string") {
			filePath = [filePath]
		}
		const args = [
			'delete', 
			'-c', cl.toString(),
			'-v',
			...filePath
		]
		return this.execAndParse(roboWorkspace, args, {quiet});
	}

	async describe(cl: number, maxFiles?: number, includeShelved?: boolean) {
		const args = ['describe',
			...(maxFiles ? ['-m', maxFiles.toString()] : []),
			...(includeShelved ? ['-S'] : []),
			cl.toString()]
		let result = (await this.execAndParseArray(null, args, undefined, undefined, describeEntryExpectedShape))[0]
		result.user = result.user || ''
		result.status = result.status || ''
		result.description = result.desc || ''
		result.date = result.time ? new Date(result.time * 1000) : null

		return result as DescribeResult;
	}

	async dirs(path: string) {
		if (path.endsWith("/...")) {
			path = path.substring(0,path.length-3) + "*"
		}
		return (await this.execAndParse(null, ['dirs', path])).map((dir: any) => dir.dir)
	}

	async files(path: string, maxFiles?: number) {
		const args = ['files',
			...(maxFiles ? ['-m', maxFiles.toString()] : []),
			path]
		try {
			return await this.execAndParse(null, args);
		}
		catch (reason) {
			if (!isExecP4Error(reason)) {
				throw reason
			}

			let [_, output] = reason
			// If perforce doesn't detect revisions in the given range, return an empty set of revisions
			if (output.trim().endsWith("no such file(s).")) {
				return []
			}
			throw reason		
		}
	}

	sizes(path: string, summary?: boolean) {
		const args = ['sizes',
			...(summary ? ['-s'] : []),
			path]
		return this.execAndParse(null, args);
	}

	// update the fields on an existing CL using p4 change
	// output format is just error or not
	async editChange(roboWorkspace: RoboWorkspace, changelist: number, opts?: EditChangeOpts) {
		const workspace = coercePerforceWorkspace(roboWorkspace);

		opts = opts || {}; // optional newWorkspace:string and/or changeSubmitted:boolean
		// get the current changelist description
		let form = await this._execP4(workspace, ['change', '-o', changelist.toString()],
			{serverAddress: opts.edgeServerAddress});

		if (opts.newOwner) {
			form = form.replace(/\nUser:\t[^\n]*\n/, `\nUser:\t${opts.newOwner}\n`);
		}
		if (opts.newWorkspace) {
			form = form.replace(/\nClient:\t[^\n]*\n/, `\nClient:\t${opts.newWorkspace}\n`);
		}
		if (opts.newDescription) {
			// replace the description
			let new_desc = '\nDescription:\n\t' + this._sanitizeDescription(opts.newDescription);
			form = form.replace(/\nDescription:\n(\t[^\n]*\n)*/, new_desc.replace(/\$/g, '$$$$'));
		}

		// run the P4 change command to update
		const changeFlag = opts.changeSubmitted ? '-f' : '-u';
		this.logger.info(`Executing: 'p4 change -i ${changeFlag}' to edit CL ${changelist}`);

		await this._execP4(workspace, ['change', '-i', changeFlag],
				{ stdin: form, quiet: true, serverAddress: opts.edgeServerAddress })
	}

	// change the description on an existing CL
	// output format is just error or not
	async editDescription(roboWorkspace: RoboWorkspace, changelist: number, description: string, edgeServerAddress?: string) {
		const opts: EditChangeOpts = {newDescription: description, edgeServerAddress}
		await this.editChange(roboWorkspace, changelist, opts)
	}

	// change the owner of an existing CL
	// output format is just error or not
	async editOwner(roboWorkspace: RoboWorkspace, changelist: number, newOwner: string, opts?: EditChangeOpts) {
		opts = opts || {}; // optional newWorkspace:string and/or changeSubmitted:boolean
		opts.newOwner = newOwner
		await this.editChange(roboWorkspace, changelist, opts)
	}

	where(roboWorkspace: RoboWorkspace, clientPath: string|string[]) {
		if (typeof clientPath === "string") {
			clientPath = [clientPath]
		}
		return this.execAndParse(roboWorkspace, ['where', ...clientPath], {quiet: !PerforceContext.devMode})
	}

	async filelog(roboWorkspace: RoboWorkspace, depotPath: string, beginRev: string, endRev: string, longOutput = false) {
		let args = ['filelog', '-i']
		if (longOutput) {
			args.push('-l')
		}
		args.push(`${depotPath}#${beginRev},${endRev}`)

		try {
			return await this.execAndParse(roboWorkspace, args, {quiet:true})
		}
		catch (reason) {
			if (!isExecP4Error(reason)) {
				throw reason
			}

			let [err, output] = reason
			// If perforce doesn't detect revisions in the given range, return an empty set of revisions
			if (output.match(/no revision\(s\) (?:below|above) that revision/)) {
				return []
			}

			throw err
		}
	}

	async hasDiff(roboWorkspace: RoboWorkspace, depotPath: string) {
		const result = await this._execP4(roboWorkspace, ['diff', '-sr', depotPath], {quiet:true})
		return result.length == 0
	}

	async fstat(roboWorkspace: RoboWorkspace, depotPath: string, quiet?: boolean) {
		try {
			return await this.execAndParse(roboWorkspace, ['fstat', depotPath], {quiet})
		}
		catch (reason) {
			if (!isExecP4Error(reason)) {
				throw reason
			}

			let [err, output] = reason
			if (output.includes('no such file')) {
				return []
			}
			throw err
		}
	}

	async integrated(roboWorkspace: RoboWorkspace, depotPath: string, opts?: IntegratedOpts)
	{
		let args = ['integrated']
		if (opts) {
			if (opts.intoOnly) {
				args.push('--into-only')
			}
			if (opts.startCL) {
				args.push('-s')
				args.push(opts.startCL.toString())
			}
		}
		args.push(depotPath)

		try {
			return await this.execAndParse(roboWorkspace, args, {quiet: opts?.quiet})
		}
		catch (reason) {
			if (!isExecP4Error(reason)) {
				throw reason
			}

			let [err, output] = reason
			// If perforce doesn't detect revisions in the given range, return an empty set of revisions
			if (output.includes("no file(s) integrated.") || output.includes("no permission for operation on file(s).")) {
				return []
			}
			throw err
		}
	}

	print(path: string, outputPath?: string) {
		let args = ['print', '-q']
		if (outputPath) {
			args.push('-o', outputPath)
		}
		args.push(path)
		return this._execP4(null, args, { noCwd: true, quiet: true })
	}

	private _sanitizeDescription(description: string) {
		return description.trim().replace(/\n\n\.\.\.\s/g, "\n\n ... ").replace(/\n/g, "\n\t");
	}

	private parseIntegrationFailure(errorLines: string[]): [string, string[]] | null {
		for (const [regex, tag] of INTEGRATION_FAILURE_REGEXES)
		{
			const matchingLines = errorLines.filter(line => line.match(regex))
			if (matchingLines.length > 0) {
				return [tag, matchingLines]
			}
		}
		return null
	}

	// execute a perforce command
	static _getP4Cmd(roboWorkspace: RoboWorkspace, args: string[], optsIn?: ExecOpts) {
		const workspace = coercePerforceWorkspace(roboWorkspace);
		// add the client explicitly if one is set (should be done at call time)

		const opts = optsIn || {}

		if (workspace && workspace.name) {
			args = ['-c', workspace.name, ...args]
		}

		if (opts.serverAddress) {
			args = ['-p', opts.serverAddress, ...args]
		}

		args = ['-zprog=robomerge', '-zversion=' + robomergeVersion, ...args]

		return args
	}

	static _execP4(logger: ContextualLogger, roboWorkspace: RoboWorkspace, args: string[], optsIn?: ExecOpts) {
		// We have some special behavior regarding Robomerge being in verbose mode (able to be set through the IPC) --
		// basically 'debug' is for local development and these messages are really spammy
		let logLevel : NpmLogLevel =
			ContextualLogger.getLogLevel() === "verbose" ? "verbose" :
			"silly"
			

		const workspace = coercePerforceWorkspace(roboWorkspace);
		// add the client explicitly if one is set (should be done at call time)

		const opts = optsIn || {}

		args = PerforceContext._getP4Cmd(roboWorkspace, args, optsIn)

		// log what we're running
		let cmd_rec = new CommandRecord(logger, 'p4 ' + args.join(' '));
		// 	logger.verbose("Executing: " + cmd_rec.cmd);
		runningPerforceCommands.add(cmd_rec);

		// we need to run within the workspace directory so p4 selects the correct AltRoot
		let options: ExecFileOptions = { maxBuffer: 500 * 1024 * 1024 };
		if (workspace && workspace.directory && !opts.noCwd) {
			options.cwd = workspace.directory;
		}

		const cmdForLog = `  cmd: ${p4exe} ${args.join(' ')} ` + (options.cwd ? `(from ${options.cwd})` : '')
		if (!opts.quiet) {
			logger.info(cmdForLog)
		}
		else {
			logger[logLevel](cmdForLog)
		}

		// darwin p4 client seems to need this
		if (options.cwd) {
			options.env = {};
			for (let key in process.env)
				options.env[key] = process.env[key];
			options.env.PWD = path.resolve(options.cwd.toString());
		}

		const doExecFile = function(retries: number): Promise<string> {
			return new Promise<string>((done, fail) => {
				const child = execFile(p4exe, args, options, (err, stdout, stderr) => {
					logger[logLevel]("Command Completed: " + cmd_rec.cmd);
					// run the callback
					if (stderr) {
						let errstr = "P4 Error: " + cmd_rec.cmd + "\n";
						errstr += "STDERR:\n" + stderr + "\n";
						errstr += "STDOUT:\n" + stdout + "\n";
						if (opts.stdin)
							errstr += "STDIN:\n" + opts.stdin + "\n";
						fail([new Error(errstr), stderr.toString().replace(newline_rex, '\n')]);
					}
					else if (err) {
						let errstr = "P4 Error: " + cmd_rec.cmd + "\n" + err.toString() + "\n";

						if (stdout || stderr) {
							if (stdout)
								errstr += "STDOUT:\n" + stdout + "\n";
							if (stderr)
								errstr += "STDERR:\n" + stderr + "\n";
						}

						if (opts.stdin)
							errstr += "STDIN:\n" + opts.stdin + "\n";

						fail([new Error(errstr), stdout ? stdout.toString() : '']);
					}
					else {
						const response = stdout.toString().replace(newline_rex, '\n')
						if (response.length > 10 * 1024) {
							logger.info(`Response size: ${Math.round(response.length / 1024.)}K`)
						}

						if (opts.trace) {
							const [traceResult, rest] = parseTrace(response)
							const durationSeconds = (Date.now() - cmd_rec.start.valueOf()) / 1000
							if (durationSeconds > 30) {
								logger.info(`Cmd: ${cmd_rec.cmd}, duration: ${durationSeconds}s\n` + traceResult)
							}
							done(rest)
						}
						else {
							done(response)
						}
					}
				});

				// write some stdin if requested
				if (opts.stdin) {
					try {
						logger[logLevel](`-> Writing to p4 stdin:\n${opts.stdin}`);

						const childStdin = child.stdin!
						childStdin.write(opts.stdin);
						childStdin.end();

						logger[logLevel]('<-');
					}
					catch (ex) {
						// usually means P4 process exited immediately with an error, which should be logged above
						logger.info(ex);
					}
				}
			}).catch((reason: any) => {
				if (!isExecP4Error(reason)) {
					logger.printException(reason, 'Caught unknown/malformed rejection while in execP4:')
					throw reason
				}
				const [err, output] = reason
				
				try {
					let retry = false
					for (const retryable of RETRY_ERROR_MESSAGES) {
						if (err.message.indexOf(retryable) >= 0) {
							retry = true
							break
						}	
					}
					if (retry) {
						const msg = `${logger.context} encountered connection reset issue, retries remaining: ${retries} `
						logger.warn(msg + `\nCommand: ${cmd_rec.cmd}`)
	
						if (roboAnalytics) {
							roboAnalytics.reportPerforceRetries(1)
						}
	
						if (retries === 1)  {
							postToRobomergeAlerts(msg + ` \`\`\`${cmd_rec.cmd}\`\`\``)
						}
	
						if (retries > 0)  {
							return doExecFile(--retries)
						}
					}
				}
				catch (err) {
					logger.printException(err, `Rejection array caught during execP4 seems to be malformed and caused an error during processing.\nArray: ${JSON.stringify(reason)}\nError:`)
				}
				throw [err, output]	
			}).finally( () => {
				runningPerforceCommands.delete(cmd_rec);
			})
		}

		return doExecFile(optsIn && typeof optsIn.numRetries === 'number' && optsIn.numRetries >= 0 ? optsIn.numRetries : 3)
	}

	private _execP4(roboWorkspace: RoboWorkspace, args: string[], optsIn?: ExecOpts) {
		optsIn = optsIn || {}
		optsIn.serverAddress = optsIn.serverAddress || this._serverConfig.serverAddress
		return PerforceContext._execP4(this.logger, roboWorkspace, ['-u', this.username, ...args], optsIn)
	}

	static async _execP4Ztag(logger: ContextualLogger, roboWorkspace: RoboWorkspace, args: string[], opts?: ExecZtagOpts) {
		if (opts && opts.format) {
			return PerforceContext._execP4(logger, roboWorkspace, ['-ztag', '-F', opts.format, ...args], opts)
		}
		else {
			return parseZTag(await PerforceContext._execP4(logger, roboWorkspace, ['-ztag', ...args], opts), opts);
		}
	}

	private async _execP4Ztag(roboWorkspace: RoboWorkspace, args: string[], opts?: ExecZtagOpts) {
		opts = opts || {}
		opts.serverAddress = opts.serverAddress || this._serverConfig.serverAddress
		return PerforceContext._execP4Ztag(this.logger, roboWorkspace, ['-u', this.username, ...args], opts)
	}

	static parseValue(key: string, value: any, parseOptions?: ParseOptions)
	{
		if (parseOptions) {
			const optionalType = parseOptions.optional && parseOptions.optional[key]
			const fieldType = optionalType || (parseOptions.expected && parseOptions.expected[key]) || 'string'
			if (fieldType === 'boolean') {
				if (!optionalType || value) {
					const valLower = value.toLowerCase()
					if (valLower !== 'true' && valLower !== 'false') {
						throw new Error(`Failed to parse boolean field ${key}, value: ${value}`)
					}
					return valLower === 'true'
				}
				return undefined
			}
			else if (fieldType === 'integer') {
				// ignore empty strings for optional fields (e.g. p4.changes can return a 'shelved' property with no value)
				if (!optionalType || value) {
					const num = parseInt(value)
					if (isNaN(num)) {
						throw new Error(`Failed to parse number field ${key}, value: ${value}`)
					}
					return num
				}
				return undefined
			}
		}
		return value		
	}

	static async execAndParse(logger: ContextualLogger, roboWorkspace: RoboWorkspace, args: string[], execOptions?: ExecOpts, parseOptions?: ParseOptions) {

		args = ['-ztag', '-Mj', ...args]
		let rawResult = await PerforceContext._execP4(logger, roboWorkspace, args, execOptions)

		let result = []
		let startIndex = 0;

		let reviver = (key: string, value: any) => {
			return PerforceContext.parseValue(key, value, parseOptions)
		}

		while(startIndex < rawResult.length) {
			const endIndex = rawResult.indexOf('}\n', startIndex)
			const parsedResult = JSON.parse(rawResult.slice(startIndex, endIndex != -1 ? endIndex+1 : undefined), reviver)
			for (const expected in ((parseOptions && parseOptions.expected) || []))
			{
				if (!parsedResult[expected])
				{
					throw new Error(`Expected field ${expected} not present in ${JSON.stringify(parsedResult)}`)
				}
			}
			result.push(parsedResult)
			startIndex = endIndex + 2
		}

		let error = result
						.filter((r) => Object.hasOwn(r,'data') && (Object.hasOwn(r,'generic') && Object.hasOwn(r,'severity')) || (Object.hasOwn(r,'level')))
						.map((r) => r.data)
						.join('')

		if (error.length > 0) {
			const cmd = `p4 ${PerforceContext._getP4Cmd(roboWorkspace, args, execOptions).join(' ')}`
			throw [new Error(`P4 Error: ${cmd}\n${error}`), error.replace(newline_rex, '\n')]
		}

		return result
	}

	async execAndParse(roboWorkspace: RoboWorkspace, args: string[], execOptions?: ExecOpts, parseOptions?: ParseOptions) {
		execOptions = execOptions || {}
		execOptions.serverAddress = execOptions.serverAddress || this._serverConfig.serverAddress
		return PerforceContext.execAndParse(this.logger, roboWorkspace, ['-u', this.username, ...args], execOptions, parseOptions)
	}

	static async execAndParseArray(logger: ContextualLogger, roboWorkspace: RoboWorkspace, args: string[], execOptions?: ExecOpts, headerOptions?: ParseOptions, arrayEntryOptions?: ParseOptions) {

		args = ['-ztag', '-Mj', ...args]
		let rawResult = await PerforceContext._execP4(logger, roboWorkspace, args, execOptions)

		let result = []
		let startIndex = 0;

		let reviver = (key: string, value: any) => {
			const arrayElementMatch = key.match(/^(.*?)\d+$/)
			if (arrayElementMatch) {
				return this.parseValue(arrayElementMatch[1], value, arrayEntryOptions)
			}
			return this.parseValue(key, value, headerOptions)
		}

		while(startIndex < rawResult.length) {
			const endIndex = rawResult.indexOf('}\n', startIndex)
			let parsedResult = JSON.parse(rawResult.slice(startIndex, endIndex != -1 ? endIndex+1 : undefined), reviver)
			for (let expected in headerOptions && headerOptions.expected || []) {
				if (!parsedResult[expected])
				{
					throw new Error(`Expected field ${expected} not present in ${JSON.stringify(parsedResult)}`)
				}
			}
			let organizedResult: {[key:string]:any} = {};
			organizedResult.entries = []
			let expectedCounts: number[] = []
			for (const field in parsedResult) {
				const arrayElementMatch = field.match(/^(.*?)(\d+)$/)
				if (arrayElementMatch) {
					const arrayField = arrayElementMatch[1]
					const arrayIndex = parseInt(arrayElementMatch[2])
					const curLength = expectedCounts.length
					if (arrayIndex >= curLength) {
						organizedResult.entries.length = arrayIndex+1
						expectedCounts.length = arrayIndex+1
						for (let i=curLength; i<organizedResult.entries.length; i++) {
							let newEntry: {[key:string]:any} = {};
							organizedResult.entries[i] = newEntry
						}
					}
					organizedResult.entries[arrayIndex][arrayField] = parsedResult[field]
					if (arrayEntryOptions && arrayEntryOptions.expected && arrayEntryOptions.expected[arrayField]) {
						expectedCounts[arrayIndex] += 1
					}
				}
				else {
					organizedResult[field] = parsedResult[field]
				}
			}
			if (arrayEntryOptions && arrayEntryOptions.expected) {
				const expectedCount = Object.keys(arrayEntryOptions.expected).length
				for (let index in expectedCounts) {
					if (expectedCounts[index] != expectedCount) {
						for (let expected in arrayEntryOptions.expected) {
							if (!organizedResult.entries[index][expected])
							{
								throw new Error(`Expected field ${expected} not present in ${JSON.stringify(organizedResult.entries[index])}`)
							}
						}
					}
				}
			}
			result.push(organizedResult)
			startIndex = endIndex + 2
		}

		if (result.length == 1 && Object.hasOwn(result[0],'data') && Object.hasOwn(result[0],'generic') && Object.hasOwn(result[0],'severity')) {
			const cmd = `p4 ${PerforceContext._getP4Cmd(roboWorkspace, args, execOptions).join(' ')}`
			throw [new Error(`P4 Error: ${cmd}\n${result[0]['data']}`), result[0]['data'].replace(newline_rex, '\n')]
		}

		return result
	}

	async execAndParseArray(roboWorkspace: RoboWorkspace, args: string[], execOptions?: ExecOpts, headerOptions?: ParseOptions, arrayEntryOptions?: ParseOptions) {
		execOptions = execOptions || {}
		execOptions.serverAddress = execOptions.serverAddress || this._serverConfig.serverAddress
		return PerforceContext.execAndParseArray(this.logger, roboWorkspace, ['-u', this.username, ...args], execOptions, headerOptions, arrayEntryOptions)
	}
}

export function getRootDirectoryForBranch(name: string): string {
	return process.platform === "win32" ? `d:/ROBO/${name}` : `/src/${name}`;
}

export function coercePerforceWorkspace(workspace: any): Workspace | null {
	if (!workspace)
		return null

	if (typeof (workspace) === "string") {
		workspace = { name: workspace };
	}

	if (!workspace.directory) {
		workspace.directory = getRootDirectoryForBranch(workspace.name);
	}
	return <Workspace>workspace
}
