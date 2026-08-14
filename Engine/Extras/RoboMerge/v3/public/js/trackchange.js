// Copyright Epic Games, Inc. All Rights Reserved.
"use strict"

function getMergeMethodStyle(mergeMethod) {
	switch(mergeMethod) {
		case "automerge":
			return {color: "black", style: "solid"}
		case "initialSubmit":
			console.log("Unexpected mergeMethod of initialSubmit")
			return {color: "pink", style: "solid"}
		case "merge_with_conflict":
			return {color: "red", style: "dashed"}
		case "manual_merge":
			return {color: "darkgray", style: "dashed"}
		case "populate":
			return {color: "blue", style: "dashed"}
		case "transfer":
			return {color: "orange", style: "solid"}
	}
	console.log("UNKNOWN CASE: " + mergeMethod)
	return {color: "pink", style: "solid"}
}

function getStreamGraphName(streamDisplayName) {
	if (streamDisplayName.endsWith("/Main")) {
		return streamDisplayName
	}
	const match = streamDisplayName.match(/\/\/[^\/]+\/([^\/]+).*/)
	return match ? match[1] : streamDisplayName
}

async function getGitHubCommit(cl) {
	const timeout = new Promise((resolve, reject) => {
    	setTimeout(() => reject(new Error('GitSync request took longer than 3 seconds')), 3000)
    });
     const response = await Promise.race([
		$.get(`https://gitsync.devtools.epicgames.com/api/v1/changes/${cl}`),
        timeout
    ]);
    return {"commit":response.commit, "commitURL":response.commitURL};
}

async function getGitHubCommits(changes) {
	return new Map(await Promise.all(Object.keys(changes).map(async cl => {
		try {
            const ghCommit = await getGitHubCommit(cl);
            return [cl, ghCommit];
        } catch(error) {
            return [cl, null];
        }
	})));
}

function formatGitHubCommit(change) {
	if (change) {
		return `<td><a href="${change.commitURL}" target="_blank"><button class="btn btn-sm">${change.commit.slice(0,6)}</button></a></td>`
	}
	return '<td></td>'
}

async function generateChangeList(dataObj) {

	const gitHubCommits = await getGitHubCommits(dataObj.changes)
	let html = '<div style="margin: auto; width: 80%;"><table class="table"><tbody>'
	// By default the keys are going to be numerically ordered, but we want changes to appear after their parent
	// So go through and build them in a list order we prefer
	const orderedChanges = Object.keys(dataObj.changes).map(k => parseInt(k))
	const placedChanges = new Set()
	for (let i = 0; i < orderedChanges.length; i++) {
		const sourceCL = dataObj.changes[orderedChanges[i].toString()].sourceCL
		if (sourceCL && !placedChanges.has(sourceCL)) {
			let insertPoint = undefined
			for (let j = i+1; j < orderedChanges.length; j++) {
				if (orderedChanges[j] == sourceCL) {
					insertPoint = j
				}
				else if (insertPoint) {
					// Keep the children with lower CL# of a given stream in numerical order
					if (orderedChanges[j] < orderedChanges[i]) {
						insertPoint = j
					} else {
						break
					}
				}
			}
			if (insertPoint) {
				const removedKey = orderedChanges.splice(i, 1)[0]
				orderedChanges.splice(insertPoint, 0, removedKey)
				i--
				continue
			}
		}
		placedChanges.add(orderedChanges[i])
	}
	for (const cl of orderedChanges) {
		const clKey = cl.toString()
		const changeDetails = dataObj.changes[clKey]
		html += '<tr valign="middle">'
		html += `<td><b>${changeDetails.streamDisplayName}</b></td>`
		if (changeDetails.swarmLink) {
			html += `<td><a href="${changeDetails.swarmLink}" target="_blank">CL#${cl}</a></td>`
		} else {
			html += `<td>CL#${cl}</td>`
		}
		html += `${formatGitHubCommit(gitHubCommits.get(clKey))}`
		html += '</tr>'
	}
	html += "</tbody></table></div>"

	return html
}

function createGraph(changes) {
	let lines = [
		'digraph robomerge {',
		'fontname="sans-serif"; labelloc=top; fontsize=16;',
		'edge [penwidth=2]; nodesep=.7; ranksep=1.2;',
		'node [shape=box, style=filled, fontname="sans-serif", fillcolor=moccasin];'
	]
	for (let change in changes) {
		const changeDetails = changes[change]
		let attrs = [
			['label', `<<b>${getStreamGraphName(changeDetails.streamDisplayName)}</b><br/>${change}>`],
			['tooltip', `"${changeDetails.swarmLink}"` || `"CL# ${change}"`],
			['target', `"_blank"`],
			['margin', `"0.5,0.1"`],
			['fontsize', 15],
		]
		if (changeDetails.swarmLink) {
			attrs.push(['URL', `"${changeDetails.swarmLink}"`])
		}
		const attrStrs = attrs.map(([key, value]) => `${key}=${value}`)
		lines.push(`_${change} [${attrStrs.join(', ')}];`);	
	}
	for (let change in changes) {
		if (changes[change].sourceCL in changes) {
			if (changes[change].mergeMethod == "automerge") {
				lines.push(`_${changes[change].sourceCL} -> _${change};`)
			} else {
				const mergeMethodStyle = getMergeMethodStyle(changes[change].mergeMethod)
				lines.push(`_${changes[change].sourceCL} -> _${change} [color=${mergeMethodStyle.color}, style=${mergeMethodStyle.style}];`)
			}
		}
	}
	lines.push('}')

    const graphContainer = $('<div class="clearfix">');
    const flowGraph = $('<div class="flow-graph" style="display:inline-block;">').appendTo(graphContainer);
    flowGraph.append($('<div>').css('text-align', 'center').text("Building graph..."));
    renderGraph(lines.join('\n'))
        .then(svg => {
        $('#graph-key-template')
            .clone()
            .removeAttr('id')
            .css('display', 'inline-block')
            .appendTo(graphContainer);
        const span = $('<div style="margin: auto; display: flex; justify-content: center;">').html(svg);
        const svgEl = $('svg', span).addClass('branch-graph').removeAttr('width');
        // scale graph to 70% of default size
        const height = Math.round(parseInt(svgEl.attr('height')) * .7);
        svgEl.attr('height', height + 'px').css('vertical-align', 'top');
        flowGraph.empty();
        flowGraph.append(span);
    });
    return graphContainer;	
}

async function buildResults(data) {
	if (Object.keys(data.data.changes).length > 0) {
		const $successPanel = $('#success-panel');
		$('#changes', $successPanel).html(await generateChangeList(data.data));
		$successPanel.show();
		$('#graph').append(createGraph(data.data.changes))
	} else {
		const $errorPanel = $('#error-panel');
		$('pre', $errorPanel).html('No results found.')
		if (data.data.swarmURL) {
			$('.swarmURL').html(`<a href="${data.data.swarmURL}/changes/${data.originalCL.cl}" target="_blank">CL# ${data.originalCL.cl}</a>`)
		}
		$errorPanel.show();
	}
	receivedTrackingResults()
}

function doit(query) {
	$.get(query)
	.then(data => buildResults(data))
	.catch(error => {
		const $errorPanel = $('#error-panel');
		const errorMsg = error.responseText
			? error.responseText.replace(/\t/g, '    ')
			: 'Internal error: ' + error.message;
		$('pre', $errorPanel).text(errorMsg);
		$errorPanel.show();
		receivedTrackingResults()
	});
}

function receivedTrackingResults(message) {
	$('#loadingDiv').fadeOut("fast", "swing", function() { 
		$('#changes').fadeIn("fast") 
		$('#graph').fadeIn("fast")
	})
}

window.doTrackChange = doit;
