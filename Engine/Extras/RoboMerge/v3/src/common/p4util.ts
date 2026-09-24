// Copyright Epic Games, Inc. All Rights Reserved.

import { _setTimeout } from './helper';
import { ContextualLogger } from './logger';
import { Change, ClientSpec, PerforceContext, Workspace } from './perforce';


export function makeRegexFromPerforcePath(path: string) {
	path = path.replace(/[/\-\\^$*+?.()|[\]{}]/g, '\\$&') // re.escape
	path = path.replace('\\*', '[^/]*')
	path = path.replace('\\.\\.\\.', '.*')
	return new RegExp(path)
}

const USER_WORKSPACE_EXCLUDE_PATTERNS: (RegExp | string)[] = [
	'horde-p4bridge-',
	'swarm-'
]

type WorkspaceID = string
function getWorkspaceID(serverAddress: string | undefined, workspaceName: string) {
	return JSON.stringify({serverAddress, workspaceName})
}
// true has been cleaned, false is being cleaned, undefined needs cleaning
let cleanedWorkspaces: Map<WorkspaceID, boolean> = new Map()
let workspacesByBot: Map<string, Set<WorkspaceID>> = new Map()

export async function clearCleanedWorkspaces(botName: string) {
	for (const workspaceID of (workspacesByBot.get(botName) || [])) {
		cleanedWorkspaces.delete(workspaceID)
	}
	workspacesByBot.delete(botName)
}

export async function cleanWorkspace(logger: ContextualLogger, p4: PerforceContext, botName: string, workspace: Workspace, workspaceRoot: string, edgeServerAddress?: string) {
	
	const workspaceID: WorkspaceID = getWorkspaceID(p4.serverAddress, workspace.name)

	// if the workspace is being cleaned, wait for it to finish
	let workspaceCleaned = cleanedWorkspaces.get(workspaceID)
	while (workspaceCleaned === false) {
		logger.info(`Waiting on ${workspace.name} being cleaned`)
		await _setTimeout(1000)
		workspaceCleaned = cleanedWorkspaces.get(workspaceID)
	}

	if (workspaceCleaned) {
		return
	}

	logger.info(`Cleaning ${workspace.name}`)
	cleanedWorkspaces.set(workspaceID, false)
	workspacesByBot.set(botName, (workspacesByBot.get(botName) || new Set<string>()).add(workspaceID))
	const changes = await p4.get_pending_changes(workspace.name) as Change[]

	// revert any pending changes left over from previous runs
	for (const change of changes) {
		const changeStr = `CL ${change.change}: ${change.desc}`
		if (change.shelved !== undefined) {
			logger.info(`Attempting to delete shelved files in ${changeStr}`)
			try {
				await p4.delete_shelved(workspace, change.change)
			}
			catch (err) {
				// ignore delete errors on startup (as long as delete works, we're good)
				logger.error(`Delete shelved failed. Will try revert anyway: ${err}`)
			}
		}
		logger.info(`Attempting to revert ${changeStr}`)
		try {
			await p4.revert(workspace, change.change, [], edgeServerAddress)
		}
		catch (err) {
			// ignore revert errors on startup (As long as delete works, we're good)
			logger.error(`Revert failed. Will try delete anyway: ${err}`)
		}

		await p4.deleteCl(workspace, change.change, edgeServerAddress)
	}

	logger.info(`Resetting workspace ${workspace.name} to revision 0`)
	await p4.sync(workspace, workspaceRoot + '#0', {edgeServerAddress})

	cleanedWorkspaces.set(workspaceID, true)
}

export async function getWorkspacesForUser(p4: PerforceContext, user: string) {
	return (await p4.find_workspaces(user))
		.filter(ws => !USER_WORKSPACE_EXCLUDE_PATTERNS.some(entry => ws.client.match(entry)))
}

function matchPrefix(a: string, b: string) {
	const len = Math.min(a.length, b.length)
	for (let i = 0; i < len; ++i) {
		if (a.charAt(i) !== b.charAt(i)) {
			return i
		}
	}
	return len
}

export async function chooseBestWorkspaceForUser(p4: PerforceContext, user: string, stream?: string) {
	const workspaces: ClientSpec[] = await getWorkspacesForUser(p4,user)

	if (workspaces.length > 0) {
		workspaces.sort((a,b) => b.Access - a.Access)
		// default to the first workspace
		let targetWorkspace = workspaces[0]

		// if this is a stream branch, do some better match-up
		if (stream) {
			const branch_stream = stream.toLowerCase()
			// find the stream with the closest match
			let target_match = 0
			for (let def of workspaces) {
				let stream = def.Stream
				if (stream) {
					const matchlen = matchPrefix(stream.toLowerCase(), branch_stream)
					if (matchlen == branch_stream.length) {
						return def
					}
					else if (matchlen > target_match) {
						target_match = matchlen
						targetWorkspace = def
					}
				}
			}
		}
		return targetWorkspace
	}

	return undefined
}