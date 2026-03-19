// Copyright Epic Games, Inc. All Rights Reserved.

import { Args } from '../common/args';

let args: Args
export function roboArgsInit(inArgs: Args) {
	args = inArgs
}

export function getExclusiveLockOpenedsToRun() {
	return args.exclusiveLockOpenedsToRun
} 