#!/usr/bin/env node
// Copyright Epic Games, Inc. All Rights Reserved.

const fs = require('fs')

Promise.all([
	fs.promises.copyFile('node_modules/@viz-js/viz/lib/viz-standalone.js', 'public/js/viz-standalone.js'),
])
.then(() => {
	console.log('Installed GraphViz for web app')
})