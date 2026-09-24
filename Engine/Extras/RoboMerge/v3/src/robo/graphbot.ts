// Copyright Epic Games, Inc. All Rights Reserved.

import * as Sentry from '@sentry/node';
import { ContextualLogger } from '../common/logger';
import { Mailer } from '../common/mailer';
import { PerforceContext } from '../common/perforce';
import { AutoBranchUpdater } from './autobranchupdater';
import { bindBadgeHandler } from './badges';
import { Bot } from './bot-interfaces';
import { Blockage, Branch, BranchArg, NodeOpUrlGenerator, resolveBranchArg } from './branch-interfaces';
import { BotConfig, BranchDefs, BranchGraphDefinition } from './branchdefs';
import { BranchGraph } from './branchgraph';
import { PersistentConflict } from './conflict-interfaces';
import { BotEventHandler, BotEventTriggers } from './events';
import { NodeBot } from './nodebot';
import { bindBotNotifications, BotNotifications, NOTIFICATIONS_PERSISTENCE_KEY } from './notifications';
import { postMessageToChannel, postToRobomergeAlerts, SlackMessages } from './notifications';
import { roboAnalytics } from './roboanalytics';
import { BlockageNodeOpUrls, OperationUrlHelper } from './roboserver';
import { Settings } from './settings';
import { Status } from './status';
import { GraphBotState } from "./status-types"
import { TickJournal } from './tick-journal';
import { GraphAPI } from '../new/graph';
import * as p4util from '../common/p4util';

// probably get the gist after 2000 characters
const MAX_ERROR_LENGTH_TO_REPORT = 2000

export type ReloadListeners = (graphBot: GraphBot, logger: ContextualLogger) => void
export class GraphBot implements BotEventHandler {
	static dataDirectory: string
	branchGraph: BranchGraph
	filename: string
	reloadAsyncListeners = new Set<ReloadListeners>()
	autoUpdater: AutoBranchUpdater | null

	private botLogger: ContextualLogger;
	private slackMessages?: SlackMessages

	// separate off into class that only exists while bots are running?
	private eventTriggers?: BotEventTriggers;

	private p4: PerforceContext;

	private constructor(private mailer: Mailer, private externalUrl: string) {
	}

	static async CreateAsync(botname: string, mailer: Mailer, externalUrl: string) {
		let graphBot = new GraphBot(mailer, externalUrl)

		if (!GraphBot.dataDirectory) {
			throw new Error('Data directory must be set before creating a BranchGraph')
		}

		graphBot.botLogger = new ContextualLogger(botname.toUpperCase())
		graphBot.p4 = PerforceContext.getServerContext(graphBot.botLogger)

		graphBot.branchGraph = new BranchGraph(botname)
		graphBot.filename = botname + '.branchmap.json'

		const branchSettingsPath = `${GraphBot.dataDirectory}/${graphBot.filename}`

		graphBot.botLogger.info(`Loading branch map from ${branchSettingsPath}`)
		const fileText = require('fs').readFileSync(branchSettingsPath, 'utf8')

		const result = await BranchDefs.parseAndValidate(graphBot.botLogger, fileText)
		if (!result.branchGraphDef) {
			throw new Error(result.validationErrors.length === 0 ? 'Failed to parse' : result.validationErrors.join('\n'))
		}

		graphBot.branchGraph.config = result.config

		let error: string | null = null
		try {
			graphBot.branchGraph._initFromBranchDefInternal(result.branchGraphDef)
		}
		catch (exc) {
			// reset - don't keep a partially configured bot around
			graphBot.branchGraph = new BranchGraph(botname)
			graphBot.branchGraph.config = result.config
			graphBot.branchGraph._initFromBranchDefInternal(null)
			error = exc.toString();
		}

		if (!result.branchGraphDef) {
			error = result.validationErrors.length === 0 ? 'Failed to parse' : result.validationErrors.join('\n');
		}

		// start empty bot on error - can be fixed up by branch definition check-in
		if (error) {
			graphBot.botLogger.error(`Problem starting up bot ${botname}: ${error}`);
		}

		graphBot._settings = new Settings(botname, graphBot.branchGraph, graphBot.botLogger, graphBot.p4)

		graphBot.externalUrl = externalUrl

		return graphBot
	}

	findNode(branchname: BranchArg): NodeBot | undefined {
		const branch = this.branchGraph.getBranch(resolveBranchArg(branchname))
		return branch && branch.bot ? branch.bot as NodeBot : undefined
	}

	initBots(ubergraph: GraphAPI, previewMode: boolean) {
		this.eventTriggers = new BotEventTriggers(this.branchGraph.botname, this.branchGraph.config)
		this.eventTriggers.registerHandler(this)
		const blockageUrlGenerator: NodeOpUrlGenerator = (blockage : Blockage | null) => { 
			if (!blockage) {
				return null
			}
			const sourceNode = this.findNode(blockage.change.branch)
			return sourceNode ? sourceNode.getBlockageUrls(blockage) : null
		}

		// bind handlers to bot events
		// doing it here ensures that we're using the most up-to-date config, e.g. after a branch spec reload

		// preprocess to get a list of all additional Slack channels
		const slackChannelOverrides: [Branch, Branch, string, boolean][] = []
		for (const branch of this.branchGraph.branches) {
			for (const [targetBranchName, edgeProps] of branch.edgeProperties) {
				const slackChannel = edgeProps.additionalSlackChannel
				if (slackChannel) {
					slackChannelOverrides.push([branch, this.branchGraph.getBranch(targetBranchName)!, slackChannel, (edgeProps.postOnlyToAdditionalChannel || false)])
				}
			}
		}

		this.slackMessages = bindBotNotifications(this.eventTriggers, slackChannelOverrides, this.settings.getContext(NOTIFICATIONS_PERSISTENCE_KEY), blockageUrlGenerator, this.externalUrl, this.botLogger)
		bindBadgeHandler(this.eventTriggers, this.branchGraph, this.externalUrl, this.botLogger)

		let hasConflicts = false
		for (const branch of this.branchGraph.branches) {
			if (branch.enabled) {
				const persistence = this.settings.getContext(branch.upperName)
				branch.bot = new NodeBot(branch, previewMode, this.mailer, this.slackMessages, this.externalUrl, this.eventTriggers, persistence, ubergraph,
					async () => {
						const errPair = await this.handleRequestedIntegrationsForAllNodes()
						if (errPair) {
							// can report wrong nodebot (this rather than one that errored)
							const [_, err] = errPair
							throw err
						}
					}
				)

				if (branch.bot.getNumConflicts() > 0) {
					hasConflicts = true
				}

				if (branch.config.forcePause) {
					branch.bot.pause('Pause forced in branchspec.json', 'branchspec')
				}
			}
		}

		// report initial conflict status
		this.eventTriggers.reportConflictStatus(hasConflicts)
	}

	runbots() {
		this.botlist = []
		for (const branch of this.branchGraph.branches) {
			if (branch.bot)
				this.botlist.push(branch.bot)
		}

		this.waitTime = 1000 * this.branchGraph.config.checkIntervalSecs
		this.startBotsAsync()
	}

	async restartBots(who: string) {
		if (this._runningBots) {
			throw new Error('Already running!')
		}

		delete this.lastError
		GraphBot.crashRequests.delete(this.branchGraph.botname)

		const msg = `${who} restarted bot ${this.branchGraph.botname}`
		this.botLogger.info(msg)
		postToRobomergeAlerts(msg)

		this.botLogger.info('Marking workspaces to be recleaned')
		await p4util.clearCleanedWorkspaces(this.branchGraph.botname)

		await this.startBotsAsync()
	}

	static checkCrashRequest(botname: string) {
		// crashMe API support - simulate a bot crashing and stopping the GraphBot instance
		const crashRequested = this.crashRequests.get(botname)
		if (crashRequested) {
			throw new Error(crashRequested)
		}
	}

	// Don't call this unless you want to bring down the entire GraphBot in a crash!
	async danger_crashGraphBot(who: string) {
		const msg = `${who} has requested a crash for bot ${this.branchGraph.botname}`
		this.botLogger.warn(msg)
		await postMessageToChannel(msg, this.branchGraph.config.slackChannel)
		GraphBot.crashRequests.set(this.branchGraph.botname, msg)
	}

	async handleRequestedIntegrationsForAllNodes(): Promise<[NodeBot, Error] | null> {
		for (const branchName of this.branchGraph.getBranchNames()) {
			const node = this.findNode(branchName)!
			for (;;) {
				const request = node.popRequestedIntegration()
				if (!request) {
					break
				}
				try {
					node.isActive = true
					await node.processQueuedChange(request)
					node.isActive = false
				}
				catch(err) {
					return [node, err]
				}
				node.persistQueuedChanges()
			}
		}
		return null
	}

	// mostly for nodebots, but could also be the auto reloader bot
	private handleNodebotError(bot: Bot, err: Error) {
		this._runningBots = false

		let errStr = err.toString()
		if (errStr.length > MAX_ERROR_LENGTH_TO_REPORT) {
			errStr = errStr.substring(0, MAX_ERROR_LENGTH_TO_REPORT) + ` ... (error length ${errStr.length})`
		}
		else {
			errStr += err.stack
		}
		this.lastError = {
			nodeBot: bot.fullName,
			error: errStr
		}

		Sentry.withScope((scope) => {
			scope.setTag('graphBot', this.branchGraph.botname)
			scope.setTag('nodeBot', bot.fullName)
			scope.setTag('lastCl', bot.lastCl.toString())

			Sentry.captureException(err);
		})
		const msg = `${this.lastError.nodeBot} fell over with error`
		this.botLogger.printException(err, msg)
		postToRobomergeAlerts(`@here ${msg}:\n\`\`\`${errStr}\`\`\``)
	}

	private async startBotsAsync() {
		if (!this.waitTime) {
			throw new Error('runbots must be called before startBots')
		}

		this._runningBots = true

		if (this.autoUpdater) {
			if (!this.autoUpdater.isRunning) {
				this.botLogger.debug(`Starting bot ${this.autoUpdater.fullNameForLogging}`)
				this.autoUpdater.start()
			}
		}

		for (const bot of this.botlist) {
			if (!bot.isRunning) {
				this.botLogger.debug(`Starting bot ${bot.fullNameForLogging}`)
				bot.start()
			}
		}

		while (true) {
			const activity = new Map<string, TickJournal>()

			const startTime = Date.now()

			let tickBot = async function(bot: Bot): Promise<[Bot,boolean|Error]> {		
				bot.isActive = true
				let ticked = false
				try {
					if (bot instanceof NodeBot) {
						GraphBot.checkCrashRequest(bot.branchGraph.botname)
					}

					ticked = await bot.tick()

					if (bot instanceof NodeBot) {
						GraphBot.checkCrashRequest(bot.branchGraph.botname)
					}
				}
				catch (err) {
					return [bot,err]
				}
				bot.isActive = false

				if (ticked) {
					++bot.tickCount
					if (bot.tickJournal) {
						const nodeBot = bot as NodeBot
						bot.tickJournal.monitored = nodeBot.branch.isMonitored
					}
				}
				return [bot,ticked]
			}

			// The autoupdater needs to fully run before we parallelize the remaining bots
			if (this.autoUpdater) {
				const [_, autoUpdateResult] = await tickBot(this.autoUpdater)
				if (typeof autoUpdateResult !== 'boolean') {
					this.handleNodebotError(this.autoUpdater, autoUpdateResult)
					return
				}
				if (this._shutdownCb) {
					this._shutdownCb()
					this._runningBots = false
					delete this.eventTriggers
					this._shutdownCb = null
					return
				}
			}

			const tickResults = await Promise.all(this.botlist.map(async (bot) => tickBot(bot)));
			
			let botCrashed = false
			for (const [bot, result] of tickResults)
			{
				if (typeof result === 'boolean') {
					if (result && bot.tickJournal) {
						const nodeBot = bot as NodeBot
						activity.set(nodeBot.branch.upperName, bot.tickJournal)
					}
				}
				else {
					botCrashed = true
					this.handleNodebotError(bot, result)
				}
			}
			if (botCrashed) {
				return
			}

			if (this._shutdownCb) {
				this._shutdownCb()
				this._runningBots = false
				delete this.eventTriggers
				this._shutdownCb = null
				return
			}

			const errPair = await this.handleRequestedIntegrationsForAllNodes()
			if (errPair) {
				const [bot, err] = errPair
				this.handleNodebotError(bot, err)
				return
			}

			roboAnalytics!.reportActivity(this.branchGraph.botname, activity)
			roboAnalytics!.reportMemoryUsage('main', process.memoryUsage().heapUsed)

			const duration = Date.now() - startTime;
			if (duration < this.waitTime!)
			{
				await new Promise(done => setTimeout(done, this.waitTime!-duration))
			}

			// reset tick journals to start counting all events, some of which may happen outside of the bot's tick
			for (const bot of this.botlist) {
				if (bot.tickJournal) {
					(bot as NodeBot).initTickJournal()
				}
			}
		}
	}

	stop(callback: Function) {
		if (this._shutdownCb)
			throw new Error("already shutting down")

		// set a shutdown callback
		this._shutdownCb = () => {
			this.botLogger.info(`Stopped monitoring ${this.branchGraph.botname}`)

			for (const branch of this.branchGraph.branches) {
				if (branch.bot) {
					// clear pause timer
					(branch.bot as NodeBot).pauseState.cancelBlockagePauseTimeout()
				}
			}

			callback()
		}

		// cancel the timeout if we're between checks
		if (this.timeout) {
			clearTimeout(this.timeout)
			this.timeout = null
			process.nextTick(this._shutdownCb)
		}
	}

	ensureStopping() {
		const wasAlreadyStopping = !!this._shutdownCb
		if (!wasAlreadyStopping) {
			this.stop(() => {})
		}
		return wasAlreadyStopping
	}

	onBlockage(_: Blockage) {
		// send a red badge if this is the only conflict, i.e. was green before
		if (this.getNumBlockages() === 1) {
			this.eventTriggers!.reportConflictStatus(true)
		}
	}

	onBranchUnblocked(conflict: PersistentConflict)
	{
		const numBlockages = this.getNumBlockages()
		this.botLogger.info(`${this.branchGraph.botname}: ${conflict.blockedBranchName} unblocked! ${numBlockages} blockages remaining`)
		if (numBlockages === 0) {
			this.eventTriggers!.reportConflictStatus(false)
		}
	}

	sendTestMessage(username: string) {
		this.botLogger.info(`Sending test DM to ${username}`)

		const fakeHelper = (_blockage : Blockage) => {
			const urls: BlockageNodeOpUrls = {
				acknowledgeUrl: OperationUrlHelper.createAcknowledgeUrl(this.externalUrl, 'botname', 'sourcebranch', '0'),
				createShelfUrl: OperationUrlHelper.createCreateShelfUrl(this.externalUrl, 'botname', 'sourcebranch', '0', 'targetbranch', 'targetstream'),
				skipUrl: OperationUrlHelper.createSkipUrl(this.externalUrl, 'botname', 'sourcebranch', '0', 'targetbranch'),
				stompUrl: OperationUrlHelper.createStompUrl(this.externalUrl, 'botname', 'sourcebranch', '0', 'targetbranch'),
				unlockUrl: OperationUrlHelper.createUnlockUrl(this.externalUrl, 'botname', 'sourcebranch', '0', 'targetbranch')
			}
			return urls
		}

		let botNotify = new BotNotifications(this.branchGraph.botname, this.branchGraph.config.slackChannel,
			this.externalUrl, fakeHelper, this.botLogger, this.slackMessages)

		return botNotify.sendTestMessage(username)
	}

	private getNumBlockages() {
		let blockageCount = 0
		for (const branch of this.branchGraph.branches) {
			if (branch.bot) {
				blockageCount += branch.bot.getNumConflicts()
			}
		}

		return blockageCount
	}

	async reinitFromBranchGraphsObject(config: BotConfig, branchGraphs: BranchGraphDefinition) {
		if (this._runningBots)
			throw new Error("Can't re-init branch specs while running")

		this.branchGraph.config = config
		this.branchGraph._initFromBranchDefInternal(branchGraphs)

		// inform listeners of reload (allows main.js to init workspaces)
		for (const listener of this.reloadAsyncListeners) {
			await listener(this, this.botLogger)
		}
	}

	isRunningBots() {
		return this._runningBots
	}

	applyStatus(out: Status) {
		const status: GraphBotState = {
			isRunningBots: this._runningBots,
			aliases: this.branchGraph.config.aliases
		}
		if (this.autoUpdater) {
			status.lastBranchspecCl = this.autoUpdater.lastCl
		}
		if (this.lastError) {
			status.lastError = this.lastError
		}

		out.reportBotState(this.branchGraph.botname, status)
	}

	forceBranchmapUpdate() {
		if (this.autoUpdater) {
			this.autoUpdater.forceUpdate()
		}
	}

	private _settings: Settings

	get settings() {
		return this._settings;
	}
	private botlist: Bot[] = []
	private waitTime?: number

	private _runningBots = false
	private lastError?: {nodeBot: string, error: string}
	private timeout: ReturnType<typeof setTimeout> | null = null
	private _shutdownCb: Function | null = null

	private static crashRequests: Map<string, string> = new Map()
}
