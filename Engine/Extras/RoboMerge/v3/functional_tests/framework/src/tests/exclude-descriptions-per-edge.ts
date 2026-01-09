// Copyright Epic Games, Inc. All Rights Reserved.
import { EdgeProperties, FunctionalTest, P4Util, Stream } from '../framework'

const streams: Stream[] =
	[ {name: 'Main', streamType: 'mainline'}
	, {name: 'Dev-Perkin', streamType: 'development', parent: 'Main'}
	, {name: 'Dev-Pootle', streamType: 'development', parent: 'Main'}
	]

export class ExcludeDescriptionsPerEdge extends FunctionalTest {
	async setup() {
		await this.p4.depot('stream', this.depotSpec())
		await this.createStreamsAndWorkspaces(streams)

		await P4Util.addFileAndSubmit(this.getClient('Main'), 'test.txt', 'Initial content')

		const desc = 'Initial populate'

		await Promise.all([
			this.p4.populate(this.getStreamPath('Dev-Perkin'), desc),
			this.p4.populate(this.getStreamPath('Dev-Pootle'), desc)
		])
	}

	async run() {

        let client = this.getClient('Main')
		let content = 'Initial content'
		content += '\n\nsubmit1 content'
		await P4Util.editFileAndSubmit(client, 'test.txt', content, undefined, "Skipped Submit")

		content += '\n\nsubmit2 content'
		await P4Util.editFileAndSubmit(client, 'test.txt', content, undefined, "Allowed Submit")
	}

	verify() {
		return Promise.all([
			this.checkHeadRevision('Main', 'test.txt', 3),
			this.checkHeadRevision('Dev-Perkin', 'test.txt', 3),
			this.ensureBlocked('Main', 'Dev-Pootle') // skipped submit1 does try to merge submit2 creating a conflict
		])
	}

	getBranches() {
		return [ 
		    this.makeForceAllBranchDef('Main', ['Dev-Perkin', 'Dev-Pootle'])
		  , this.makeForceAllBranchDef('Dev-Perkin', [])
		  , this.makeForceAllBranchDef('Dev-Pootle', [])
		]
	}

	getEdges(): EdgeProperties[] {
		return [
		  { from: this.fullBranchName('Main'), to: this.fullBranchName('Dev-Pootle')
		  , excludeDescriptions: ['S.* S.*']
		  }
		]
	}
}
