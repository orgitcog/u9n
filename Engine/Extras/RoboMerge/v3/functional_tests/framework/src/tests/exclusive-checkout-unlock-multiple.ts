// Copyright Epic Games, Inc. All Rights Reserved.
import { FunctionalTest, P4Client, P4Util, RobomergeBranchSpec, Stream } from '../framework'

const streams: Stream[] = [
	{name: 'Main', streamType: 'mainline'},
	{name: 'Release', streamType: 'release', parent: 'Main'}
]

export class ExclusiveCheckoutUnlockMultiple extends FunctionalTest {
	mainSpec: RobomergeBranchSpec
	releaseUser2Client: P4Client

	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces(streams, undefined, ['testuser1', 'testuser2'])

		const mainClient = this.getClient('Main', 'testuser1')
		await P4Util.addFileAndSubmit(mainClient, 'test.uasset', 'Dummy content', true)
        await P4Util.addFileAndSubmit(mainClient, 'test2.uasset', 'Dummy content', true)
		
		this.releaseUser2Client = this.getClient('Release', 'testuser2')
		await Promise.all([
			mainClient.edit('test.uasset'),
			mainClient.edit('test2.uasset'),
			this.p4.populate(this.getStreamPath('Release'), 'Initial branch of files from Main')
				.then(() => this.releaseUser2Client.sync())
		])
	}

	async run() {
		// second user commits change in Release while first user has file in main checked out
		await P4Util.editFile(this.releaseUser2Client, 'test.uasset', 'New content')
        await P4Util.editFile(this.releaseUser2Client, 'test2.uasset', 'New content')
        await P4Util.submit(this.releaseUser2Client, "Files")
	}

	async verify() {
		await this.ensureBlocked('Release', 'Main')
        await this.verifyAndPerformUnlock('Release', 'Main')
        await this.ensureNotBlocked('Release', 'Main')
	}

	getBranches() {
		return [
			this.makeForceAllBranchDef('Main', []),
			this.makeForceAllBranchDef('Release', ['Main'])
		]
	}
}
