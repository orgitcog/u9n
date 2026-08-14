// Copyright Epic Games, Inc. All Rights Reserved.
import { ChildProcess, execFile, ExecFileOptions } from 'child_process'

class ExecOpts {
	stdin?: string
}

export interface Change {
	change: number
	description: string
}

export interface DescribeResult {
	file: string
	action: string
	rev: number
}

type Type = 'string' | 'integer' | 'boolean'

type ParseOptions = {
	expected?: {[field: string]: Type}
	optional?: {[field: string]: Type}
}

export const VERBOSE = false
export class Perforce {
	static readonly P4_EXE = process.platform === 'win32' ? 'p4.exe' : 'p4'
	static readonly REGEX_NEWLINE = /\r\n|\n|\r/g

	init() {
		return this.exec(process.platform === 'darwin' ? ['-plocalhost:1666', 'login', '-s'] : ['login', '-s'])
	}

	depot(depotType: string, spec: string)  { return this.exec(['depot', '-t', depotType],  {stdin:spec}) }
	user(spec: string)                      { return this.exec(['user', '-fi'],             {stdin:spec}) }
	stream(spec: string)                    { return this.exec(['stream'],                  {stdin:spec}) }
	client(user: string, spec: string)      { return this.exec(['-u', user, 'client'],      {stdin:spec}) }
	branch(spec: string)                    { return this.exec(['branch', '-i'],            {stdin:spec}) }

	add(user: string, workspace: string, path: string, binary?: boolean)
	{
		const args = ['-u', user, '-c', workspace, 'add', '-t', binary ? 'binary+Cl' : 'text', path]
		return this.exec(args)
	}

	edit(user: string, workspace: string, path: string, opts?: string[]) { 
		return this.exec(['-u', user, '-c', workspace, 'edit', ...(opts || []), path]) 
	}
	delete(user: string, workspace: string, path: string) { return this.exec(['-u', user, '-c', workspace, 'delete', path]) }
	sync(user: string, workspace: string) { return this.exec(['-u', user, '-c', workspace, 'sync']) }
	populate(stream: string, description: string) { return this.exec(['populate', '-S', stream, '-r', '-d', description]) }
	print(client: string, path: string) { return this.exec(['-c', client, 'print', '-q', path]) }

	unshelve(user: string, workspace: string, cl: number) {
		return this.exec(['-u', user, '-c', workspace, 'unshelve', '-s', cl.toString(), '-c', cl.toString(), '//...'])
	}

	deleteShelved(user: string, workspace: string, cl: number) {
		return this.exec(['-u', user, '-c', workspace, 'shelve', '-d', '-c', cl.toString()])
	}

	resolve(user: string, workspace: string, cl: number, clobber: boolean) {
		return this.exec(['-u', user, '-c', workspace, 'resolve', clobber ? '-at' : '-ay', '-c', cl.toString(), '//...'])
	}

	fstat(p4DepotPath: string): Promise<string>;
	fstat(client: string, localPath: string): Promise<string>;
	fstat(arg0: string, arg1?: string): Promise<string> {
		if (arg1) {
			return this.exec(['-c', arg0, 'fstat', '-T', 'headRev', arg1])
		} else {
			return this.exec(['fstat', '-T', 'headRev', arg0])
		}
	}

	// submit default changelist
	submit(user: string, workspace: string, description: string): Promise<string>

	// submit specified pending changelist
	submit(user: string, workspace: string, cl: number): Promise<string>

	submit(user: string, workspace: string, arg: string | number)
	{
		let args = ['-u', user, '-c', workspace, 'submit']
		if (typeof arg === 'string') {
			args = [...args, '-d', arg]
		}
		else {
			args = [...args, '-c', arg.toString()]
		}
		return this.exec(args)
	}

	async changes(client: string, stream: string, limit: number, pending?: boolean) {
		const jsonResult = await this.execAndParse(['-c', client, 'changes', '-l', '-m', limit.toString(), '-s', pending ? 'pending' : 'submitted', stream + '/...'], undefined, {expected: {change: 'integer', desc: 'string'}})
		const result: Change[] = []
		for (const entry of jsonResult) {
			result.push({change: entry.change as number, description: entry.desc as string})
		}
		return result
	}

	private describeHeaderExpectedShape: ParseOptions = {
		expected: {change: 'integer', user: 'string', client: 'string'}
	}

	private describeEntryExpectedShape: ParseOptions = {
		expected: {action: 'string', rev: 'integer', depotFile: 'string'}
	}
	
	async describe(cl: number) {
		let result = (await this.execAndParseArray(['describe', cl.toString()], undefined, this.describeHeaderExpectedShape, this.describeEntryExpectedShape))[0]

		return result.entries as DescribeResult[];
	}

	private parseValue(key: string, value: any, parseOptions?: ParseOptions)
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

	private async execAndParse(args: string[], execOptions?: ExecOpts, parseOptions?: ParseOptions) {

		args = ['-ztag', '-Mj', ...args]
		let rawResult = await this.exec(args, execOptions)

		let result = []
		let startIndex = 0;

		let reviver = (key: string, value: any) => {
			return this.parseValue(key, value, parseOptions)
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

		if (result.length == 1 && Object.hasOwn(result[0],'data') && Object.hasOwn(result[0],'generic') && Object.hasOwn(result[0],'severity')) {
			const cmd = `p4 ${[...args, ...(execOptions && execOptions.stdin ? ['-i'] : [])].join(' ')}`
			throw [new Error(`P4 Error: ${cmd}\n${result[0]['data']}`), result[0]['data'].replace(Perforce.REGEX_NEWLINE, '\n')]
		}

		return result
	}

	private async execAndParseArray(args: string[], execOptions?: ExecOpts, headerOptions?: ParseOptions, arrayEntryOptions?: ParseOptions) {

		args = ['-ztag', '-Mj', ...args]
		let rawResult = await this.exec(args, execOptions)

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
			const cmd = `p4 ${[...args, ...(execOptions && execOptions.stdin ? ['-i'] : [])].join(' ')}`
			throw [new Error(`P4 Error: ${cmd}\n${result[0]['data']}`), result[0]['data'].replace(Perforce.REGEX_NEWLINE, '\n')]
		}

		return result
	}

	private exec(args: string[], optOpts?: ExecOpts): Promise<string> {
		if (VERBOSE) console.log('Running: ' + args.join(' '))
		let options: ExecFileOptions = {maxBuffer: 1024*1024}

		const opts: ExecOpts = optOpts || {}
		if (opts.stdin) {
			args.push('-i')
		}

		const cmd = 'p4 ' + args.join(' ')

		let child: ChildProcess
		const execPromise = new Promise<string>((done, fail) => {
			child = execFile(Perforce.P4_EXE, args, options, (err, stdout, stderr) => {

				if (stderr) {
					let errstr = `P4 Error: ${cmd}\n`
						+ `STDERR:\n${stderr}\n`
						+ `STDOUT:\n${stdout}\n`

					if (opts.stdin) {
						errstr += `STDIN:\n${opts.stdin}\n`
					}
					
					fail([new Error(errstr), stderr.toString().replace(Perforce.REGEX_NEWLINE, '\n')])
				}
				else if (err) {
					let errstr = `P4 Error: ${cmd}\n`
						+ err.toString() + '\n'

					if (stdout || stderr) {
						if (stdout) {
							errstr += `STDOUT:\n${stdout}\n`
						}
						if (stderr) {
							errstr += `STDERR:\n${stderr}\n`
						}
					}

					if (opts.stdin) {
						errstr += `STDIN:\n${opts.stdin}\n`
					}

					fail([new Error(errstr), stdout ? stdout.toString() : ''])
				}
				else {
					done(stdout.toString().replace(Perforce.REGEX_NEWLINE, '\n'))
				}
			})
		})

		// write some stdin if requested
		if (opts.stdin) {
			try {
				child!.stdin!.write(opts.stdin)
				child!.stdin!.end()
			}
			catch (ex) {
				// usually means P4 process exited immediately with an error, which should be logged above
				console.log(ex)
			}
		}

		return execPromise
	}

}
