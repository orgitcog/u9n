// Copyright Epic Games, Inc. All Rights Reserved.

import { PerforceContext } from '../common/perforce'
import { ContextualLogger } from '../common/logger';

const jsonlint: any = require('jsonlint')

const RESERVED_BRANCH_NAMES = ['NONE', 'DEFAULT', 'IGNORE', 'DEADEND', ''];

export interface BotConfig {
	defaultStreamDepot: string | null
	isDefaultBot: boolean
	noStreamAliases: boolean
	globalNotify: string[]
	nagWhenBlocked: boolean | null
	nagSchedule: number[] | null
	nagAcknowledgedSchedule: number[] | null
	nagAcknowledgedLeeway: number | null
	changelistParser: string | null
	triager: string | null
	checkIntervalSecs: number
	excludeAuthors: string[]
	excludeDescriptions: string[]
	emailOnBlockage: boolean
	visibility: string[] | string
	slackChannel: string
	reportToBuildHealth: boolean
	mirrorPath: string[]
	aliases: string[] // alternative names for the bot, first in list used for incognito mode
	badgeUrlOverride: string
	branchNamesToIgnore: string[]

	macros: { [name: string]: string[] }
}

const branchBasePrototype = {
	name: '',

	rootPath: '',
	uniqueBranch: false,
	isDefaultBot: false,
	emailOnBlockage: false, // if present, completely overrides BotConfig

	notify: [''],
	flowsTo: [''],
	forceFlowTo: [''],
	defaultFlow: [''],
	macros: {} as { [name: string]: string[] } | undefined,
	changelistParser: null as string | null,
	resolver: '' as string | null,
	triager: '' as string | null,
	nagWhenBlocked: false as boolean | null,
	nagSchedule: [] as number[] | null,
	nagAcknowledgedSchedule: [] as number[] | null,
	nagAcknowledgedLeeway: 0 as number | null,
	aliases: [''],
	badgeProject: '' as string | null,
}

export type BranchBase = typeof branchBasePrototype

export class IntegrationMethod {
	static NORMAL = 'normal'
	static SKIP_IF_UNCHANGED = 'skip-if-unchanged'

	static all() {
		const cls: any = IntegrationMethod
		return Object.getOwnPropertyNames(cls)

			.filter(x => typeof cls[x] === 'string' && cls[x] !== cls.name)
			.map(x => cls[x])
	}
}

export const DAYS_OF_THE_WEEK = ['sun', 'mon', 'tue', 'wed', 'thu', 'fri', 'sat']
type DayOfTheWeek = 'sun' | 'mon' | 'tue' | 'wed' | 'thu' | 'fri' | 'sat'

export type IntegrationWindowPane = {
	// if days not specified, daily
	daysOfTheWeek?: DayOfTheWeek[]
	startHourUTC: number
	durationHours: number
}


export const commonOptionFieldsPrototype = {
	lastGoodCLPath: 0 as string | number,
	waitingForCISLink: "",
	pauseCISUnlessAtGate: false,

	initialCL: 0,
	forcePause: false,

	disallowSkip: false,
	incognitoMode: false,

	excludeAuthors: [] as string[], // if present, completely overrides BotConfig
	excludeDescriptions: [] as string[], // if present, completely overrides BotConfig

	// by default, specify when gate catch ups are allowed; can be inverted to disallow
	integrationWindow: [] as IntegrationWindowPane[],
	invertIntegrationWindow: false,

	// fake property
	_comment: ''
}

export type CommonOptionFields = typeof commonOptionFieldsPrototype

const nodeOptionFieldsPrototype = {
	...branchBasePrototype,
	...commonOptionFieldsPrototype,

	disabled: false,
	forceAll: false,
	visibility: '' as string[] | string,
	blockAssetFlow: [''],
	disallowDeadend: false,

	streamServer: undefined as string | undefined,
	streamDepot: '',
	streamName: '',
	streamSubpath: '',
	uniqueBranch: false,

	graphNodeColor: '',

	additionalSlackChannelForBlockages: '',
	postMessagesToAdditionalChannelOnly: false,
	ignoreBranchspecs: false,

	badgeUrlOverride: '',
}

const nodeOptionFieldNames: ReadonlySet<string> = new Set(Object.keys(nodeOptionFieldsPrototype));

type NodeOptionFields = typeof nodeOptionFieldsPrototype

// will eventually have all properties listed on wiki
const edgeOptionFieldsPrototype = {
	...commonOptionFieldsPrototype,

	branchspec: '',
	additionalSlackChannel: '',

	postOnlyToAdditionalChannel: false,

	resolver: '',
	triager: '',

	nagSchedule: [],
	nagAcknowledgedSchedule: [],
	nagAcknowledgedLeeway: 0,
	nagWhenBlocked: true,

	terminal: false, // changes go along terminal edges but no further
	integrationMethod: 'normal',

	implicitCommands: [''],

	ignoreInCycleDetection: false,

	// if set, still generate workspace but use this name
	workspaceNameOverride: '',

	approval: {
		description: '',
		channelId: '',
		block: true
	}
}

const edgeOptionFieldNames: ReadonlySet<string> = new Set(Object.keys(edgeOptionFieldsPrototype));

type EdgeOptionFields = typeof edgeOptionFieldsPrototype
export type ApprovalOptions = typeof edgeOptionFieldsPrototype.approval

export type NodeOptions = Partial<NodeOptionFields>
export type EdgeOptions = Partial<EdgeOptionFields>

export type EdgeProperties = EdgeOptions & {
	from: string
	to: string
}

export interface BranchSpecDefinition {
	from: string
	to: string
	name: string
}

export interface BranchGraphDefinition {
	branches: NodeOptions[]
	branchspecs?: BranchSpecDefinition[] 
	edges?: EdgeProperties[]
}

// for now, just validate branch file data, somewhat duplicating BranchGraph code
// eventually, switch branch map to using a separate class defined here for the branch definitions


async function validateCommonOptions(logger: ContextualLogger, options: Partial<CommonOptionFields>, isPreviewing?: boolean) {
	const errors: string[] = []
	const warnings: string[] = []
	if (options.integrationWindow) {
		for (const pane of options.integrationWindow) {
			if (pane.daysOfTheWeek) {
				for (let index = 0; index < pane.daysOfTheWeek.length; ++index) { 
					const dayStr = pane.daysOfTheWeek[index]
					const day = dayStr.slice(0, 3).toLowerCase()
					if (DAYS_OF_THE_WEEK.indexOf(day) < 0) {
						errors.push(`Unknown day of the week ${dayStr}`)
					}
					pane.daysOfTheWeek[index] = day as DayOfTheWeek
				}
			}
		}
	}
	if (typeof(options.lastGoodCLPath) === 'string') {
		const fstat = await PerforceContext.getServerContext(logger).fstat(null, options.lastGoodCLPath, true)
		if (fstat.length == 0) {
			if (isPreviewing) {
				warnings.push(`Unable to find lastGoodCLPath ${options.lastGoodCLPath}`)
			} else {
				errors.push(`Unable to find lastGoodCLPath ${options.lastGoodCLPath}`)
			}
		}
	}
	return {errors, warnings}
}


interface ParseResult {
	branchGraphDef: BranchGraphDefinition | null
	config: BotConfig
	validationErrors: string[],
	validationWarnings: string[]
}

export type StreamResult = {
	depot: string
	rootPath?: string
	stream?: string
}

/** expects either rootPath or all the other optional arguments */
export function calculateStream(nodeOrStreamName: string, rootPath?: string | null, depot?: string | null, streamSubpath?: string | null) {
	if (!rootPath) {
		if (!depot) {
			throw new Error(`Missing rootPath and no streamDepot defined for branch ${nodeOrStreamName}.`)
		}
		const stream = `//${depot}/${nodeOrStreamName}`
		return {depot, stream, rootPath: stream + (streamSubpath || '/...')}
	}

	if (!rootPath.startsWith('//') || !rootPath.endsWith('/...')) {
		throw new Error(`Branch rootPath not in '//<something>/...'' format: ${rootPath}`)
	}

	const depotMatch = rootPath.match(new RegExp('//([^/]+)/'))
	if (!depotMatch || !depotMatch[1]) {
		throw new Error(`Cannot find depotname in ${rootPath}`)
	}
	return {depot: depotMatch[1]}
}

export class BranchDefs {
	static checkName(name: string) {
		if (!name.match(/^[-a-zA-Z0-9_\.]+$/))
			return `Names must be alphanumeric, dash, underscore or dot: '${name}'`

		for (const reserved of RESERVED_BRANCH_NAMES) {
			if (name.toUpperCase() === reserved) {
				return `'${name}' is a reserved branch name`
			}
		}
		return undefined
	}

	private static checkValidIntegrationMethod(validationErrors: string[], method: string, name: string) {
		if (IntegrationMethod.all().indexOf(method.toLowerCase()) === -1) {
			validationErrors.push(`Unknown integrationMethod '${method}' in '${name}'`)
		}
	}

	static async parseAndValidate(logger: ContextualLogger, branchSpecsText: string, isPreviewing?: boolean): Promise<ParseResult> {
		const defaultConfigForWholeBot: BotConfig = {
			defaultStreamDepot: null,
			isDefaultBot: false,
			noStreamAliases: false,
			globalNotify: [],
			changelistParser: null,
			triager: null,
			nagSchedule: null,
			nagAcknowledgedSchedule: null,
			nagAcknowledgedLeeway: null,
			nagWhenBlocked: null,
			emailOnBlockage: true,
			checkIntervalSecs: 30.0,
			excludeAuthors: [],
			excludeDescriptions: [],
			visibility: ['fte'],
			slackChannel: '',
			reportToBuildHealth: false,
			mirrorPath: [],
			aliases: [],
			branchNamesToIgnore: [],
			macros: {},
			badgeUrlOverride: ''
		}

		let validationErrors: string[] = []
		let validationWarnings: string[] = []

		let branchGraphRaw: any
		try {
			branchGraphRaw = jsonlint.parse(branchSpecsText)

			if (!Array.isArray(branchGraphRaw.branches)) {
				throw new Error('expected "branches" array!')
			}
		}
		catch (err) {
			validationErrors.push(err)
			return {branchGraphDef: null, config: defaultConfigForWholeBot, validationErrors, validationWarnings}
		}

		if (branchGraphRaw.alias) {
			branchGraphRaw.aliases = [branchGraphRaw.alias, ...(branchGraphRaw.aliases || [])]
		}

		// copy config values
		for (let key of Object.keys(defaultConfigForWholeBot)) {
			let value = branchGraphRaw[key]
			if (value !== undefined) {
				if (key === 'macros') {
					let macrosLower: {[name:string]: string[]} | null = {}
					if (value === null && typeof value !== 'object') {
						macrosLower = null
					}
					else {
						const macrosObj = value as any
						for (const name of Object.keys(macrosObj)) {
							const lines = macrosObj[name]
							if (!Array.isArray(lines)) {
								macrosLower = null
								break
							}
							macrosLower[name.toLowerCase()] = lines
						}
					}
					if (!macrosLower) {
						validationErrors.push(`Invalid macro property: '${value}'`)
						return {branchGraphDef: null, config: defaultConfigForWholeBot, validationErrors, validationWarnings}
					}
					value = macrosLower
				}
				(defaultConfigForWholeBot as any)[key] = value
			}
		}

		const namesToIgnore = defaultConfigForWholeBot.branchNamesToIgnore.map(s => s.toUpperCase())
		defaultConfigForWholeBot.branchNamesToIgnore = namesToIgnore

		const names = new Map<string, string>()

		const branchGraph = branchGraphRaw as BranchGraphDefinition
		const branchesFromJSON = branchGraph.branches
		branchGraph.branches = []

		// Check for duplicate branch names
		await Promise.all(branchesFromJSON.map(async (def) => {
			if (!def.name) {
				validationErrors.push(`Unable to parse branch definition: ${JSON.stringify(def)}`)
				return
			}

			branchGraph.branches.push(def)

			const nameError = BranchDefs.checkName(def.name)
			if (nameError) {
				validationErrors.push(nameError)
				return
			}

			const upperName = def.name.toUpperCase()
			if (names.has(upperName)) {
				validationErrors.push(`Duplicate branch name '${upperName}'`)
			}
			else {
				names.set(upperName, upperName)
			}

			const streamResult = calculateStream(
				def.streamName || def.name,
				def.rootPath,
				def.streamDepot || defaultConfigForWholeBot.defaultStreamDepot,
				def.streamSubpath
			)

			if (streamResult.stream && !await PerforceContext.getServerContext(logger, def.streamServer).stream(streamResult.stream)) {
				if (isPreviewing) {
					validationWarnings.push(`Stream ${streamResult.stream} not found`)
				} else {
					validationErrors.push(`Stream ${streamResult.stream} not found`)
				}
			}
		}))

		// Check for duplicate aliases (and that branches/aliases are not in the ignore list)
		const addAlias = (upperBranchName: string, upperAlias: string) => {
			if (namesToIgnore.indexOf(upperBranchName) >= 0) {
				validationErrors.push(upperBranchName + ' branch is in branchNamesToIgnore')
			}

			if (namesToIgnore.indexOf(upperAlias) >= 0) {
				validationErrors.push(upperAlias + ' alias is in branchNamesToIgnore')
			}

			if (!upperAlias) {
				validationErrors.push(`Empty alias for '${upperBranchName}'`)
				return
			}

			const nameError = BranchDefs.checkName(upperAlias)
			if (nameError) {
				validationErrors.push(nameError)
				return
			}

			const existing = names.get(upperAlias)
			if (existing && existing !== upperBranchName) {
				validationErrors.push(`Duplicate alias '${upperAlias}' for '${existing}' and '${upperBranchName}'`)
			}
			else {
				names.set(upperAlias, upperBranchName)
			}
		}

		await Promise.all(branchGraph.branches.map(async (def) => {
			const upperName = def.name!.toUpperCase()
			if (def.aliases) {
				for (const alias of def.aliases) {
					addAlias(upperName, alias.toUpperCase())
				}
			}

			if (def.streamName && !def.streamSubpath && !defaultConfigForWholeBot.noStreamAliases) {
				addAlias(upperName, def.streamName.toUpperCase())
			}

			// check all properties are known (could make things case insensitive here)
			for (const keyName of Object.keys(def)) {
				if (!nodeOptionFieldNames.has(keyName)) {
					validationErrors.push(`Unknown property '${keyName}' specified for node ${def.name}`)
				}
			}

			const results = await validateCommonOptions(logger, def, isPreviewing)
			validationErrors.push(...results.errors)
			validationWarnings.push(...results.warnings)
		}))

		// Check edge properties
		if (branchGraph.edges) {
			await Promise.all(branchGraph.edges.map(async (edge) => {
				if (!names.get(edge.from.toUpperCase())) {
					validationErrors.push('Unrecognised source node in edge property ' + edge.from)
				}
				if (!names.get(edge.to.toUpperCase())) {
					validationErrors.push('Unrecognised target node in edge property ' + edge.to)
				}

				// check all properties are known (could make things case insensitive here)
				for (const keyName of Object.keys(edge)) {
					if (keyName !== 'from' && keyName !== 'to' && !edgeOptionFieldNames.has(keyName)) {
						throw new Error(`Unknown property '${keyName}' specified for edge ${edge.from}->${edge.to}`)
					}
				}

				if (edge.approval) {
					// should be replaced by generic handling of objects in the prototype
					if (!edge.approval.description || !edge.approval.channelId) {
						throw new Error(`Invalid approval settings for edge ${edge.from}->${edge.to}`)
					}
				}

				if (edge.integrationMethod) {
					BranchDefs.checkValidIntegrationMethod(validationErrors, edge.integrationMethod, `${edge.from}->${edge.to}`)
				}

				if (edge.branchspec && !await PerforceContext.getServerContext(logger).branchExists(edge.branchspec)) {
					if (isPreviewing) {
						validationWarnings.push(`Branchspec ${edge.branchspec} does not exist.`)
					} 
					else {
						validationErrors.push(`Branchspec ${edge.branchspec} does not exist.`)
					}
				}

				const results = await validateCommonOptions(logger, edge, isPreviewing)
				validationErrors.push(...results.errors)
				validationWarnings.push(...results.warnings)
			}))
		}

		// Check flow
		for (const def of branchGraph.branches) {
			const flowsTo = new Set()
			if (def.flowsTo) {
				if (!Array.isArray(def.flowsTo)) {
					validationErrors.push(`'${def.name}'.flowsTo is not an array`)
				}
				else for (const to of def.flowsTo) {
					const branchName = names.get(to.toUpperCase())
					if (branchName) {
						flowsTo.add(branchName)
					}
					else {
						validationErrors.push(`'${def.name}' flows to unknown branch/alias '${to}'`)
					}
				}
			}

			if (def.forceFlowTo) {
				if (!Array.isArray(def.forceFlowTo)) {
					validationErrors.push(`'${def.name}'.forceFlowTo is not an array`)
				}
				else for (const to of def.forceFlowTo) {
					const branchName = names.get(to.toUpperCase())
					if (!branchName) {
						validationErrors.push(`'${def.name}' force flows to unknown branch/alias '${to}'`)
					}
					else if (!flowsTo.has(branchName)) {
						validationErrors.push(`'${def.name}' force flows but does not flow to '${to}'`)
					}
				}
			}

			if (def.changelistParser) {
				try {
					await import(`./changelistparsers/${def.changelistParser}`)
				}
				catch {
					validationErrors.push(`'${def.name}' specifies custom parser '${def.changelistParser}' but unable to import it`)
				}
			}
		}

		// Check branchspecs for valid branches
		if (branchGraph.branchspecs) {
			await Promise.all(branchGraph.branchspecs.map(async (spec) => {
				let expectedFields = ['from', 'to', 'name']
				for (const [key, val] of Object.entries(spec)) {
					const efLength = expectedFields.length
					expectedFields = expectedFields.filter(f => f != key)
					if (efLength == expectedFields.length) {
						validationErrors.push('Unexpected branchspec property: ' + key)
					}
					if (typeof val !== 'string') {
						validationErrors.push(`Branchspec property ${key} is not a string`)
					}
				}

				if (expectedFields.length > 0) {
					validationErrors.push(`Branchspec missing field(s): ${expectedFields.join(', ')}`)
				}

				if (spec.from && !names.has(spec.from.toUpperCase())) {
					validationErrors.push(`From-Branch ${spec.from} not found in branchspec ${spec.name}`)
				}

				if (spec.to && !names.has(spec.to.toUpperCase())) {
					validationErrors.push(`To-Branch ${spec.to} not found in branchspec ${spec.name}`)
				}

				if (spec.name && !await PerforceContext.getServerContext(logger).branchExists(spec.name)) {
					if (isPreviewing) {
						validationWarnings.push(`Branchspec ${spec.name} does not exist.`)
					} 
					else {
						validationErrors.push(`Branchspec ${spec.name} does not exist.`)
					}
				}
			}))
		}

		if (validationErrors.length > 0) {
			console.log(validationErrors)
			return {branchGraphDef: null, config: defaultConfigForWholeBot, validationErrors, validationWarnings}
		}

		return {branchGraphDef: branchGraph, config: defaultConfigForWholeBot, validationErrors, validationWarnings}
	}
}
