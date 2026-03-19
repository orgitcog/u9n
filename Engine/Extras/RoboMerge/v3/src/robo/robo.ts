// Copyright Epic Games, Inc. All Rights Reserved.

import * as Sentry from '@sentry/node';
import { OnUncaughtException, OnUnhandledRejection } from '@sentry/node/dist/integrations';
import * as fs from 'fs';
import * as os from 'os';
import * as path from 'path';
import { Analytics } from '../common/analytics';
import { Arg, readProcessArgs } from '../common/args';
import { _setTimeout } from '../common/helper';
import { ContextualLogger } from '../common/logger';
import { Mailer } from '../common/mailer';
import { initializePerforce, PerforceContext } from '../common/perforce';
import { BuildVersion, VersionReader } from '../common/version';
import { CertFiles } from '../common/webserver';
import { addBranchGraph, Graph, GraphAPI } from '../new/graph';
import { AutoBranchUpdater } from './autobranchupdater';
import { Branch } from './branch-interfaces';
import { Gate } from './gate';
import { GraphBot } from './graphbot';
import { IPC, Message } from './ipc';
import { NodeBot } from './nodebot';
import { roboAnalytics, setGlobalAnalytics } from './roboanalytics';
import { RoboServer } from './roboserver';
import { notificationsInit } from './notifications'
import { roboArgsInit } from './roboargs'
import { settingsInit } from './settings'
import { Vault } from './vault'
import * as Preview from './preview'

/*************************
 * RoboMerge main process
 *************************/

// Begin by intializing our logger and version reader
const roboStartupLogger = new ContextualLogger('Robo Startup')
VersionReader.init(roboStartupLogger)

const COMMAND_LINE_ARGS: {[param: string]: Arg<any>} = {
	botname: {
		match: /^-botname=([a-zA-Z0-9_,]+)$/,
		parse: (str: string) => str.split(','),
		env: 'BOTNAME',
		dflt: ["TEST"]
	},
	runbots: {
		match: /^-runbots=(.+)$/,
		parse: (str: string) => {
			if (str === "yes")
				return true;
			if (str === "no")
				return false;
			throw new Error(`Invalid -runbots=${str}`);
		},
		env: 'ROBO_RUNBOTS',
		dflt: true
	},

	hostInfo: {
		match: /^-hostInfo=(.+)$/,
		env: 'ROBO_HOST_INFO',
		dflt: os.hostname()
	},

	externalUrl: {
		match: /^-externalUrl=(.+)$/,
		env: 'ROBO_EXTERNAL_URL',
		dflt: 'https://127.0.0.1'
	},

	branchSpecsRootPath: {
		match: /^-bs_root=(.*)$/,
		env: 'ROBO_BRANCHSPECS_ROOT_PATH',
		dflt: '//GamePlugins/Main/Programs/RoboMerge/data'
	},

	branchSpecsWorkspace: {
		match: /^-bs_workspace=(.+)$/,
		env: 'ROBO_BRANCHSPECS_WORKSPACE',
		dflt: 'robomerge-branchspec-' + os.hostname()
	},

	branchSpecsDirectory: {
		match: /^-bs_directory=(.+)$/,
		env: 'ROBO_BRANCHSPECS_DIRECTORY',
		dflt: './data'
	},

	gateUpdateWorkspacePrefix: {
		match: /^-gate_update_workspace_prefix=(.+)$/,
		env: 'ROBO_GATE_UPDATE_WORKSPACE_PREFIX',
		dflt: 'robomerge-gateupdate-' + os.hostname()
	},

	gateUpdatesDirectory: {
		match: /^-gate_updates_directory=(.+)$/,
		env: 'ROBO_GATE_UPDATES_DIRECTORY',
		dflt: './gateupdates'
	},

	noIPC: {
		match: /^(-noIPC)$/,
		parse: _str => true,
		env: 'ROBO_NO_IPC',
		dflt: false
	},

	noMail: {
		match: /^(-noMail)$/,
		parse: _str => true,
		env: 'ROBO_NO_MAIL',
		dflt: false
	},

	noTLS: {
		match: /^(-noTLS)$/,
		parse: _str => true,
		env: 'ROBO_NO_TLS',
		dflt: false
	},

	useSlackInDev: {
		match: /^(-useSlackInDev)$/,
		parse: _str => true,
		env: 'ROBO_USE_SLACK_IN_DEV',
		dflt: false
	},

	vault: {
		match: /^-vault_path=(.+)$/,
		env: 'ROBO_VAULT_PATH',
		dflt: '/vault'
	},

	devMode: {
		match: /^(-devMode)$/,
		parse: str => str === "false" ? false : true,
		env: 'ROBO_DEV_MODE',
		dflt: false
	},

	previewOnly: {
		match: /^(-previewOnly)$/,
		parse: str => str === "false" ? false : true,
		env: 'ROBO_PREVIEW_ONLY',
		dflt: false
	},

	exclusiveLockOpenedsToRun: {
		match: /^-exclusiveLockOpenedsToRun=([0-9]+)$/,
		env: 'ROBO_EXCLUSIVE_LOCK_OPENEDS_TO_RUN',
		parse: (str: string) => {
			return parseInt(str)
		},
		dflt: 100
	},

	// Sentry environment designation -- use 'PROD' to enable Sentry bug tracking
	epicEnv: {
		match: /^(-epicEnv)$/,
		env: 'EPIC_ENV',
		parse: (str: string) => {
			return str.toLowerCase()
		},
		dflt: 'dev'
	},
	nodeEnv: {
		match: /^(-nodeEnv)$/,
		env: 'NODE_ENV',
		parse: (str: string) => {
			return str.toLowerCase()
		},
		dflt: 'development'
	},
	epicDeployment: {
		match: /^(-epicDeployment)$/,
		env: 'EPIC_DEPLOYMENT',
		parse: (str: string) => {
			return str.toLowerCase()
		},
		dflt: 'unknown'
	},
	sentryDsn: {
		match: /^-sentryDsn=(.+)$/,
		env: 'SENTRY_DSN',
		dflt: 'https://f68a5bce117743a595d871f3ddac26bf@sentry.io/1432517' // Robomerge sentry project: https://sentry.io/organizations/to/issues/?project=1432517
	},
	slackDomain: {
		match: /^-slackDomain=(.+)$/,
		env: 'ROBO_SLACK_DOMAIN',
		dflt: 'https://slack.com'
	},
	persistenceDir: {
		match: /^-persistenceDir=(.+)$/,
		env: 'ROBO_PERSISTENCE_DIR',
		dflt: process.platform === 'win32' ? 'D:/ROBO' : path.resolve(process.env.HOME || '/root', '.robomerge')
	},
	persistenceBackupPath: {
		match: /^-persistenceBackupPath=(.*)$/,
		env: 'ROBO_PERSISTENCE_BACKUP_PATH',
		dflt: '//GamePlugins/RobomergePersistence'
	},
	persistenceBackupWorkspace: {
		match: /^-bs_workspace=(.+)$/,
		env: 'ROBO_PERSISTENCE_BACKUP_WORKSPACE',
		dflt: 'robomerge-persistence-' + os.hostname()
	},
	persistenceBackupFrequency: {
		match: /^-persistenceBackupFrequency=([0-9]+)$/,
		env: 'ROBO_PERSISTENCE_BACKUP_FREQUENCY',
		parse: (str: string) => {
			return parseInt(str)
		},
		dflt: 60
	}
};

const maybeNullArgs = readProcessArgs(COMMAND_LINE_ARGS, roboStartupLogger);
if (!maybeNullArgs) {
	process.exit(1)
}
const args = maybeNullArgs!

const vault = new Vault(args.vault)
notificationsInit(args, vault)
roboArgsInit(args)
settingsInit(args)
Preview.init(args.branchSpecsRootPath)

const env = args.epicEnv || (args.nodeEnv === "production" ? "prod" : "dev")
const sentryEnv = `${env}-${args.epicDeployment}`
if (env === 'prod' 
	&& args.sentryDsn) {
		roboStartupLogger.verbose(`(Robo) Sentry enabled for environment ${sentryEnv}. Uploading error/event reports to ${args.sentryDsn}`)
	Sentry.init({
		dsn: args.sentryDsn,
		release: VersionReader.getShortVersion(),
		environment: sentryEnv,
		serverName: args.hostInfo,
		integrations: [...Sentry.defaultIntegrations, new OnUnhandledRejection(), new OnUncaughtException() ]
	})
}

GraphBot.dataDirectory = args.branchSpecsDirectory;

export class RoboMerge {
	readonly roboMergeLogger = new ContextualLogger('RoboMerge')
	readonly graphBots = new Map<string, GraphBot>()
	graph: GraphAPI
	mailer: Mailer
	
	static VERSION : BuildVersion = VersionReader.getBuildVersionObj();

	readonly VERSION_STRING = VersionReader.toString();

	getAllBranches() : Branch[] {
		const branches: Branch[] = [];
		for (const graphBot of robo.graphBots.values()) {
			branches.push(...graphBot.branchGraph.branches);
		}
		return branches;
	}

	getBranchGraph(name: string) {
		const graphBot = this.graphBots.get(name.toUpperCase())
		return graphBot ? graphBot.branchGraph : null
	}

	dumpSettingsForBot(name: string) {
		const graphBot = this.graphBots.get(name.toUpperCase())
		return graphBot && graphBot.settings.object
	}

	stop() {
		roboAnalytics!.stop()
	}
}

// should be called after data directory has been synced, to get latest email template
function _initMailer(logger: ContextualLogger) {
	robo.mailer = new Mailer(roboAnalytics!, logger, args.noMail);
}

function _checkForAutoPauseBots(branches: Branch[], logger: ContextualLogger) {
	if (!args.runbots) {
		let paused = 0
		for (const branch of branches) {
			if (branch.bot) {
				// branch.bot.pause('Paused due to command line arguments or environment variables', 'robomerge')
				// ++paused

				// This should always be true
				if (branch.bot instanceof NodeBot) {
					paused += branch.bot.pauseAllEdges('Paused due to command line arguments or environment variables', 'robomerge')
				}
				else {
					logger.warn(`Encountered non-NodeBot when attempting to pause edges: ${branch.bot.fullNameForLogging}`)
				}
			}
		}

		if (paused !== 0) {
			logger.info(`Auto-pause: ${paused} branch bot${paused > 1 ? 's' : ''} paused`)
		}
	}
}

let specReloadEntryCount = 0
async function _onBranchSpecReloaded(graphBot: GraphBot, logger: ContextualLogger) {
	try {
		++specReloadEntryCount
	}
	finally {
		--specReloadEntryCount
	}

	graphBot.initBots(robo.graph, args.previewOnly)

	if (specReloadEntryCount === 0) {
		// regenerate ubergraph (last update to finish does regen if multiple in flight)
		const graph = new Graph

		// race condition when two or more branches get reloaded at the same time!
		//	- multiple bots await above, at which time new branch objects have no bots
		try {
			for (const graphBot of robo.graphBots.values()) {
				addBranchGraph(graph, graphBot.branchGraph)
			}
			robo.graph.reset(graph)
		}
		catch (err) {
			logger.printException(err, 'Caught error regenerating ubergraph')
		}
	}

	logger.info(`Restarting monitoring ${graphBot.branchGraph.botname} branches after reloading branch definitions`)
	graphBot.runbots()
}

async function init(logger: ContextualLogger) {

	let p4 = PerforceContext.getServerContext(logger)

	let lookupStream = async function(rootPath: string): Promise<string> {

		while (true) {
			try {
				const stream = await p4.getStreamName(rootPath);
				if (typeof stream === 'string') {
					if (await p4.stream(stream)) {
						return stream
					}
				}
				else {
					logger.warn(stream.message)
					return ""
				}
			}
			catch (err) {
			}

			const timeout = 5.0;
			logger.info(`Will look up stream from ${rootPath} again in ${timeout} sec...`);
			await _setTimeout(timeout*1000);
		}
	}

	if (args.branchSpecsRootPath) {
		const branchSpecsAbsPath = fs.realpathSync(args.branchSpecsDirectory)
		const autoUpdaterConfig = {
			rootPath: args.branchSpecsRootPath,
			workspace: {directory: branchSpecsAbsPath, name: args.branchSpecsWorkspace},
			devMode: args.devMode
		}

		// Ensure we have a workspace for branch specs
		const workspace: Object[] = await p4.find_workspace_by_name(args.branchSpecsWorkspace)
		if (workspace.length === 0) {
			logger.info(`Cannot find workspace ${args.branchSpecsWorkspace}, creating a new one.`)

			if ((await lookupStream(args.branchSpecsRootPath)).length > 0) {
				await p4.newBranchSpecWorkspace(autoUpdaterConfig.workspace, args.branchSpecsRootPath)
			}
		}

		// make sure we've got the latest branch specs
		logger.info('Syncing latest branch specs')
		await AutoBranchUpdater.init(p4, autoUpdaterConfig, robo.roboMergeLogger)

	} else {
		logger.warn('Auto brancher updater not configured!')
	}

	if (args.gateUpdateWorkspacePrefix && args.gateUpdatesDirectory) {
		Gate.init(args.gateUpdateWorkspacePrefix, args.gateUpdatesDirectory, logger)
	} else {
		logger.warn('Gate Update Workspace Prefix or Directory not specified. Gate updating via webpage may not work correctly.')
	}

	if (args.persistenceBackupFrequency > 0 && !args.previewOnly) {
		const workspace: Object[] = await p4.find_workspace_by_name(args.persistenceBackupWorkspace)
		if (workspace.length === 0) {
			logger.info(`Cannot find workspace ${args.persistenceBackupWorkspace}, creating a new one.`)

			const workspace = {directory: fs.realpathSync(args.persistenceDir), name: args.persistenceBackupWorkspace}
			const stream = await lookupStream(args.persistenceBackupPath)
			if (stream.length > 0) {
				let roots: string | string[] = '/app/data' // default linux path
				if (workspace.directory !== roots) {
					roots = [roots, workspace.directory] // specified directory
				}
				const params: any = {
					AltRoots: roots,
					Stream: stream
				}		
				await p4.newWorkspace(args.persistenceBackupWorkspace, params)
			}
		}
	} else {
		logger.warn('Persistence backup not configured!')
	}

	_initMailer(logger)
	await _initGraphBots(logger)

	const graph = new Graph
	robo.graph = new GraphAPI(graph)
	for (const graphBot of robo.graphBots.values()) {
		graphBot.initBots(robo.graph, args.previewOnly)
		addBranchGraph(graph, graphBot.branchGraph)
	}
}

function startBots(logger: ContextualLogger) {

	// start them up
	logger.info("Starting branch bots...");
	for (let graphBot of robo.graphBots.values()) {
		if (AutoBranchUpdater.config) {
			graphBot.autoUpdater = new AutoBranchUpdater(graphBot, logger, args.previewOnly)
		}
		graphBot.runbots()
	}

	if (!args.runbots) {
		_checkForAutoPauseBots(robo.getAllBranches(), logger)
	}
}

if (!args.noIPC) {
	roboStartupLogger.setCallback((message: string) => {
		process.send!({
			logmsg: message
		})
	})
	process.on('message', async (msg: Message) => {
		let result: any | null = null
		try {
			if (!ipc) {
				roboStartupLogger.warn(`"${msg.name}" message received, but the IPC isn't ready yet. Sending 503.`)
				result = { statusCode: 503, message: 'Robomerge still starting up' }
			} else {
				result = await ipc.handle(msg)
			}
		}
		catch (err) {
			roboStartupLogger.printException(err, `IPC error processing message ${msg.name}`)
			result = {
				statusCode: 500,
				message: 'Internal server error',
				error: {
					name: err.name,
					message: err.message,
					stack: err.stack
				}
			}
		}
		process.send!({cbid: msg.cbid, args: result})
	});
	process.on('uncaughtException', (err) => {
		// Send watchdog process the error information since we're about to die
		process.send!({
			sentry: true,
			error: {
				name: err.name,
				message: err.message,
				stack: err.stack
			}
		})
		throw err
	})
}
else {
	const sendMessage = (name: string, args?: any[]) => ipc.handle({name:name, args:args}).catch(err => console.log('IPC error! ' + err.toString()));

	const ws = new RoboServer(args.externalUrl, sendMessage, () => roboStartupLogger.getLogTail(),
		() => roboStartupLogger.info('Received: getLastCrash'),
		() => roboStartupLogger.info('Received: stopBot'),
		() => roboStartupLogger.info('Received: startBot'));


	// TODO: Please make this better
	const certFiles = {
		key: args.noTLS ? "" : fs.readFileSync(`${args.vault}/cert.key`, 'ascii'),
		cert: args.noTLS ? "" : fs.readFileSync('./certs/cert.pem', 'ascii')
	}

	const protocol = args.noTLS ? 'http' : 'https'
	const port = args.noTLS ? 8877 : 4433
	ws.open(port, protocol, certFiles as CertFiles).then(() =>
		roboStartupLogger.info(`Running in-process web server (${protocol}) on port ${port}`)
	);
}

// bind to shutdown
let is_shutting_down = false;
function shutdown(exitCode: number, logger: ContextualLogger) {
	Sentry.close()
	if (is_shutting_down) return;
	is_shutting_down = true;
	logger.info("Shutting down...");

	// record the exit code
	let finalExitCode = exitCode || 0;

	robo.stop()

	// figure out how many stop callbacks to wait for
	let callCount = 1;
	let callback = () => {
		if (--callCount === 0) {
			logger.info("... shutdown complete.");

			// force exit so we don't wait for anything else (like the webserver)
			process.exit(finalExitCode);
		}
		else if (callCount < 0) {
			throw new Error("shutdown weirdness");
		}
	}

	// stop all the branch bots
	for (let graphBot of robo.graphBots.values()) {
		++callCount;
		graphBot.stop(callback);
	}

	// make sure this gets called at least once (matches starting callCount at 1)
	callback();
}

process.once('SIGINT', () => { roboStartupLogger.error("Caught SIGINT"); shutdown(2, roboStartupLogger); });
process.once('SIGTERM', () => { roboStartupLogger.error("Caught SIGTERM"); shutdown(0, roboStartupLogger); });

async function _initGraphBots(logger: ContextualLogger) {
	for (const botname of args.botname)	{
		logger.info(`Initializing bot ${botname}`)
		const graphBot = await GraphBot.CreateAsync(botname, robo.mailer, args.externalUrl)
		robo.graphBots.set(graphBot.branchGraph.botname, graphBot)

		graphBot.reloadAsyncListeners.add(_onBranchSpecReloaded)
	}
}

async function main(logger: ContextualLogger) {
	while (true) {
		try {
			await initializePerforce(logger, vault, args.devMode);
			break;
		}
		catch (err) {
			logger.printException(err, 'P4 is not configured yet');

			const timeout = 15.0;
			logger.info(`Will check again in ${timeout} sec...`);
			await _setTimeout(timeout*1000);
		}
	}

	setGlobalAnalytics(new Analytics(args.hostInfo!))
	robo = new RoboMerge;
	ipc = new IPC(robo);

	await init(logger);

	startBots(logger);
}

let robo: RoboMerge
let ipc : IPC
main(roboStartupLogger);

