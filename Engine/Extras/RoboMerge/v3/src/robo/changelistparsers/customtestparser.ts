// Copyright Epic Games, Inc. All Rights Reserved.

import { DescriptionParser, descriptionParsers } from '../targets'

export class TestCustomDescriptionParser extends DescriptionParser {

	static {
		descriptionParsers.set("testcustomparser", TestCustomDescriptionParser)
	}

	override parse(desc: string, lineEnd: string = '\n') {
		// If the first line is first.last that will be the author tag
		const authorMatch = desc.match(/^([^ ]*?\.[^ ]*?)\n(.*)/s)
		if (authorMatch) {
			this.authorTag = authorMatch[1]
			this.descFinal = authorMatch[2].split(lineEnd)
		}
		else {
			this.descFinal = desc.split(lineEnd)
		}
	}
}