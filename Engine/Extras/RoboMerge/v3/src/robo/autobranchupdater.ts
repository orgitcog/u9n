// Copyright Epic Games, Inc. All Rights Reserved.

import * as fs from 'fs';
import * as path from 'path';
import { ContextualLogger } from '../common/logger';
import { P4_FORCE, PerforceContext, Workspace } from '../common/perforce';
import { Bot } from './bot-interfaces';
import { BranchDefs } from './branchdefs';
import { GraphBot } from './graphbot';
import * as p4util from '../common/p4util';

const DISABLE = false

type AutoBranchUpdaterConfig = {
	rootPath: string		//				process.env('ROBO_BRANCHSPECS_ROOT_PATH')
	workspace: Workspace	// { directory:	process.env('ROBO_BRANCHSPECS_DIRECTORY')
	devMode: boolean
}							// , name:		process.env('ROBO_BRANCHSPECS_WORKSPACE') }

type MirrorPaths = {
	name: string
	directory: string
	stream: string
	depotFolder: string
	depotpath: string
	realFilepath: string
	mirrorFilepath: string
}

export class AutoBranchUpdater implements Bot {

	private p4: PerforceContext
	private filePath: string
	private workspace: Workspace

	private readonly abuLogger: ContextualLogger
	
	lastCl: number
	lastModifiedTime: number // Only used in devmode

	// public: used by NodeBot
	isRunning = false
	isActive = false

	static config: AutoBranchUpdaterConfig
	static initialCl: number

	tickCount = 0

	constructor(private graphBot: GraphBot, parentLogger: ContextualLogger, private readonly previewMode: boolean) {
		this.p4 = PerforceContext.getServerContext(parentLogger)

		const config = AutoBranchUpdater.config
		this.filePath = `${config.rootPath}/${graphBot.filename}`
		this.workspace = config.workspace

		this.lastCl = AutoBranchUpdater.initialCl
		this.abuLogger = parentLogger.createChild('ABU')
	}

	static async init(p4: PerforceContext, config: AutoBranchUpdaterConfig, parentLogger: ContextualLogger) {
		this.config = config
		const logger = parentLogger.createChild('ABU')

		logger.info(`Finding most recent branch spec changelist from ${config.rootPath}`)
		var bsRoot = config.rootPath + '/...'
		const change = await p4.latestChange(bsRoot)
		if (change === null)
			throw new Error(`Unable to query for most recent branch specs CL`)

		this.initialCl = change.change

		if (DISABLE) {
			logger.warn('Auto branch update disabled!');
			return
		}

		logger.info(`Syncing branch specs at CL ${change.change}`);
		let workspaceDir : string = path.resolve(config.workspace.directory);
		if (!fs.existsSync(workspaceDir)) {
			logger.info(`Creating local branchspec directory ${workspaceDir}`)
			fs.mkdirSync(workspaceDir);
		}

		const opts = !config.devMode ? [P4_FORCE] : undefined 
		await p4.sync(config.workspace, `${bsRoot}@${change.change}`, {opts})
	}
	
	async start() {

		if (AutoBranchUpdater.config.devMode) {
			const stats = fs.statSync(`${this.workspace.directory}/${this.graphBot.filename}`);
			this.lastModifiedTime = new Date(stats.mtime).getTime();
		}

		this.isRunning = true;
		this.abuLogger.info(`Began monitoring ${this.graphBot.branchGraph.botname} branch specs at CL ${this.lastCl}`);
	}

	async tick() {
		if (DISABLE) {
			return false
		}

		let reloadBranchDefs = false;
		try {
			const change = await this.p4.latestChange(this.filePath);
			if (change !== null && change.change > this.lastCl) {
				await this.p4.sync(this.workspace, `${this.filePath}@${change.change}`);
	
				// set this to be the last changelist regardless of success - if it failed due to a broken
				// .json file, the file will have to be recommitted anyway
				this.lastCl = change.change;
				reloadBranchDefs = true;
			}
		}
		catch (err) {
			// if we're in devmode we support files open for add or not in perforce so absorb the exception
			if (!AutoBranchUpdater.config.devMode) {
				this.abuLogger.printException(err, 'Branch specs: error while querying P4 for changes');
				return false
			}
		}

		if (AutoBranchUpdater.config.devMode) {
			const stats = fs.statSync(`${this.workspace.directory}/${this.graphBot.filename}`);
			const newModifiedTime = new Date(stats.mtime).getTime();
			if (this.lastModifiedTime !== newModifiedTime) {
				this.lastModifiedTime = newModifiedTime
				reloadBranchDefs = true
			}
		}

		if (reloadBranchDefs) {
			await this._tryReloadBranchDefs();
		}
		return true
	}

	async _tryReloadBranchDefs() {
		let branchGraphText
		try {
			branchGraphText = require('fs').readFileSync(`${this.workspace.directory}/${this.graphBot.filename}`, 'utf8')
		}
		catch (err) {
			// @todo email author of changes!
			this.abuLogger.printException(err, 'ERROR: failed to reload branch specs file')
			return
		}

		const result = await BranchDefs.parseAndValidate(this.abuLogger, branchGraphText)
		if (!result.branchGraphDef) {
			// @todo email author of changes!
			let errText = 'failed to parse/validate branch specs file\n'
			for (const error of result.validationErrors) {
				errText += `${error}\n`
			}
			this.abuLogger.error(errText.trim())
			return
		}

		const botname = this.graphBot.branchGraph.botname
		if (this.graphBot.ensureStopping()) {
			this.abuLogger.info(`Ignoring changes to ${botname} branch specs, since bot already stopping`)
			return
		}

		this.abuLogger.info(`Stopped monitoring ${botname} branches, in preparation for reloading branch definitions`)

		// NOTE: not awaiting next tick. Waiters on this function carry on as soon as we return
		// this doesn't wait until all the branch bots have stopped, but that's ok - we're creating a new set of branch bots
		process.nextTick(async () => {
			await this.p4.sync(this.workspace, this.filePath, {okToFail:AutoBranchUpdater.config.devMode})

			this.abuLogger.info(`Branch spec change detected: reloading ${botname} from CL#${this.lastCl}`)

			await this.graphBot.reinitFromBranchGraphsObject(result.config, result.branchGraphDef!)

			const mirrorWorkspace = AutoBranchUpdater.getMirrorWorkspace(this.graphBot)
			if (!mirrorWorkspace) {
				return
			}

			await this.updateMirror(mirrorWorkspace)
		})
	}

	forceUpdate() {
		this.lastCl = -1
	}

	get fullName() {
		return `${this.graphBot.branchGraph.botname} auto updater`
	}

	get fullNameForLogging() {
		return this.abuLogger.context
	}

	private async updateMirror(workspace: MirrorPaths) {

		if (this.previewMode)
		{
			this.abuLogger.info("Skipping mirror update in Preview Mode")
			return
		}

		this.abuLogger.info("Updating branchmap mirror")

		const stream = this.graphBot.branchGraph.config.mirrorPath[0]

		// make sure the root directory exists
		fs.mkdirSync(workspace.directory, {recursive:true})

		const workspaceQueryResult = await this.p4.find_workspace_by_name(workspace.name, {includeUnloaded: true})
		if (workspaceQueryResult.length === 0) {
			await this.p4.newWorkspace(workspace.name, {
				Stream: stream,
				Root: AutoBranchUpdater.config!.workspace.directory
			})
		}
		else {
			if (workspaceQueryResult[0].IsUnloaded) {
				await this.p4.reloadWorkspace(workspace.name)
			}
			await p4util.cleanWorkspace(this.abuLogger, this.p4, this.graphBot.branchGraph.botname, workspace, workspace.depotpath)
		}

		const {depotpath, realFilepath, mirrorFilepath} = workspace

		const cl = await this.p4.new_cl(workspace, "Updating mirror file\n")
		const fstat = await this.p4.fstat(workspace, depotpath)
		if (fstat.length > 0) {
			await this.p4.sync(workspace, depotpath, {opts:[P4_FORCE]})
			await this.p4.edit(workspace, cl, depotpath)
		}
		else {
			fs.mkdirSync(path.dirname(mirrorFilepath), { recursive: true })
		}
		 fs.copyFileSync(realFilepath, mirrorFilepath)
		if (fstat.length == 0) {
			await this.p4.add(workspace, cl, depotpath)
		}
		await this.p4.submit(workspace, cl)
	}

	static getMirrorWorkspace(bot: GraphBot): MirrorPaths | null {
		const mirrorPathBits = bot.branchGraph.config.mirrorPath
		if (mirrorPathBits.length === 0) {
			return null
		}
		
		if (mirrorPathBits.length !== 3) {
			throw new Error('If mirrorPath is specified, expecting [stream, sub-path, filename]')
		}

		if (mirrorPathBits[0].length == 0 || mirrorPathBits[2].length == 0) {
			throw new Error('stream and filename must be supplied for mirrorPath')
		}

		const directory = AutoBranchUpdater.config!.workspace.directory
		const depotFolder = mirrorPathBits[1].length > 0 ? mirrorPathBits.slice(0, 2).join('/') : mirrorPathBits[0]
		const depotpath = `${depotFolder}/${mirrorPathBits[2]}`
		const realFilepath = path.join(directory, bot.filename)
		const mirrorFilepath = path.join(directory, ...mirrorPathBits.slice(1))
		return {
			name: `${AutoBranchUpdater.config!.workspace.name}-${bot.branchGraph.botname}-mirror`,
			stream: mirrorPathBits[0], depotFolder, depotpath, realFilepath, mirrorFilepath, directory
		}
	}
}
