// Copyright Epic Games, Inc. All Rights Reserved.
import { EdgeProperties, FunctionalTest, P4Util, Stream } from '../framework'

const streams: Stream[] = [
    {name: 'Main', streamType: 'mainline'},
    {name: 'Dev-Pootle', streamType: 'development', parent: 'Main'},
    {name: 'Dev-Perkin', streamType: 'development', parent: 'Main'}
]

export class SkipIfUnchanged extends FunctionalTest {

	async setup() {
        await this.p4.depot('stream', this.depotSpec())
        await this.createStreamsAndWorkspaces(streams)

		const mainClient = this.getClient('Main')
        await P4Util.addFileAndSubmit(mainClient, 'unchanged.txt', 'Initial content', true)
        await P4Util.addFileAndSubmit(mainClient, 'changed.txt', 'Initial content', true)
        await P4Util.addFileAndSubmit(mainClient, 'changed_filetype.txt', 'Initial content', true)

        const desc = 'Initial branch of files from Main'
        await Promise.all([
            this.p4.populate(this.getStreamPath('Dev-Perkin'), desc),
            this.p4.populate(this.getStreamPath('Dev-Pootle'), desc)
        ])
	}

	async run() {
		const mainClient = this.getClient('Main')
		await P4Util.editFileAndSubmit(mainClient, 'unchanged.txt', 'Initial content')
		await P4Util.editFileAndSubmit(mainClient, 'changed.txt', 'Changed content')
		await mainClient.edit('changed_filetype.txt', ['-t','text+w'])
			.then(() => mainClient.submit("Changed filetype"))
	}

	getBranches() {
		const mainDef = this.makeForceAllBranchDef('Main', ['Dev-Perkin', 'Dev-Pootle'])
		mainDef.blockAssetFlow = [this.fullBranchName('Dev-Perkin')]
		return [
			mainDef,
			this.makeForceAllBranchDef('Dev-Perkin', []),
			this.makeForceAllBranchDef('Dev-Pootle', [])
		]
	}

    getEdges() : EdgeProperties[] {
        return [{ 
            from: this.fullBranchName('Main'),
            to: this.fullBranchName('Dev-Perkin'),
            integrationMethod: "skip-if-unchanged"
          }]
    }

    verify() {
		return Promise.all([
			this.checkHeadRevision('Dev-Perkin', 'unchanged.txt', 1),
			this.checkHeadRevision('Dev-Pootle', 'unchanged.txt', 2),
			this.checkHeadRevision('Dev-Perkin', 'changed.txt', 2),
			this.checkHeadRevision('Dev-Pootle', 'changed.txt', 2),
			this.checkHeadRevision('Dev-Perkin', 'changed_filetype.txt', 2),
			this.checkHeadRevision('Dev-Pootle', 'changed_filetype.txt', 2),
			this.ensureNotBlocked('Main', 'Dev-Perkin')
		])
	}
}

