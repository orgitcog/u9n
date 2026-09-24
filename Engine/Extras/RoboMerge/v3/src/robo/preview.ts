import { BranchDefs } from './branchdefs';
import { BranchGraph } from './branchgraph';
import { ContextualLogger } from '../common/logger';
import { PerforceContext } from '../common/perforce';
import { Status } from './status';

const logger = new ContextualLogger('preview')

// let branchSpecsRootPath = ''
export function init(_root: string) {
	// branchSpecsRootPath = root
}

export async function getPreview(cl: number, singleBot?: string) {

	let p4 = PerforceContext.getServerContext(logger)

	const status = new Status(new Date(), `preview:${p4.swarmURL}/changes/${cl}`, logger)

	const bots: [string, string][] = []
	for (const entry of (await p4.describe(cl, undefined, true)).entries) {
		if (entry.action !== 'delete') {
			const match = entry.depotFile.match(/.*\/(.*)\.branchmap\.json$/)
			if (match) {
				bots.push([match[1], match[0]])
			}
		}
	}

	const errors = []
	for (const [bot, path] of bots) {
		if (singleBot && bot.toLowerCase() !== singleBot.toLowerCase()) {	
			continue
		}
		const fileText = await p4.print(`${path}@=${cl}`)

		const result = await BranchDefs.parseAndValidate(logger, fileText, true)

		const errorPrefix = `\n\t${bot} validation failed: `
		if (result.validationWarnings.length == 1) {
			status.addWarning(errorPrefix + result.validationWarnings[0])
		} else if (result.validationWarnings.length > 1) {
			status.addWarning(errorPrefix + result.validationWarnings.map(warn => `\n\t\t${warn}`).join(''))
		}

		if (!result.branchGraphDef) {
			if (result.validationErrors.length == 0) {
				errors.push(errorPrefix + 'unknown error')
			} else if (result.validationErrors.length == 1) {
				errors.push(errorPrefix + result.validationErrors[0])
			} else {
				errors.push(errorPrefix + result.validationErrors.map(err => `\n\t\t${err}`).join(''))
			}
			continue
		}

		const graph = new BranchGraph(bot)
		graph.config = result.config
		try {
			graph._initFromBranchDefInternal(result.branchGraphDef)
		}
		catch (err) {
			errors.push(errorPrefix + err.toString())
			continue
		}

		for (const branch of graph.branches) {
			status.addBranch(branch)
		}
	}

	if (errors.length > 0) {
		let error = errors.join('\n\n')
		const warnings = status.getWarnings()
		if (warnings.length > 0) {
			error += '\n\nWarning:'
			error += status.getWarnings().map(warning => `${warning}`).join('\n\n')
		}
		throw new Error(error)
	}
	return status
}
