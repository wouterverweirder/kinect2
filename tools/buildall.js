//build versions (32bit & 64bit) for all supported node versions
//make sure to have latest version of node-gyp installed aswell (npm install -g node-gyp)
//use --publish to upload binaries to S3
var jsdom = require('jsdom'),
	https = require('https'),
	fs = require('fs'),
	path = require('path'),
	async = require('async');

var argv = require('minimist')(process.argv.slice(2));

var nodeExe = process.argv[0];
var nodeGyp = path.join(nodeExe, '..', 'node_modules', 'npm', 'node_modules', 'node-gyp', 'bin', 'node-gyp.js');
var nodePreGyp = path.join(nodeExe, '..', 'node_modules', 'node-pre-gyp', 'bin', 'node-pre-gyp');

var buildDir = path.join(__dirname, '..', 'build');
var nodeBinariesDir = path.join(buildDir, 'node_binaries');

var nodeListUrl = 'https://nodejs.org/dist/';
var iojsListUrl = 'https://iojs.org/dist/';

var publish = argv.publish;

var nodeVersions = {};

function init() {
	async.series([
		function(callback) { mkDirIfNotExists(buildDir, callback); },
		function(callback) { mkDirIfNotExists(nodeBinariesDir, callback); },
		addNodeJsVersionsToNodeVersionsObject,
		addIoJsVersionsToNodeVersionsObject,
		loadNodeAbis,
		reduceNodeVersionsToUniqueNodeAbis,
		function(callback) { buildAll(nodeVersions, callback); }
	], function(err, results){
		console.log('all done');
	});
}

function mkDirIfNotExists(dir, callback) {
	fs.stat(dir, function(err, stats){
		if(!err) {
			return callback();
		}
		//folder does not exist, create it
		fs.mkdir(dir, callback);
	});
}

function downloadIfNotExists(file, remoteUrl, callback) {
	fs.stat(file, function(err, stats){
		if(!err) {
			return callback();
		}
		//file does not exist, download it
		console.log("download " + remoteUrl + " to " + file);
		https.get(remoteUrl, function (response) {
			if (response.statusCode !== 200) {
				return callback('HTTP response status code ' + response.statusCode);
			}
			response.on("end", function() {
				console.log("completed download of " + remoteUrl);
				setTimeout(callback, 1000);
			});
			var stream = require('fs').createWriteStream(file);
			response.pipe(stream);
		});
	});
}

function loadNodeAbis(callback) {
	https.get('https://raw.githubusercontent.com/mapbox/node-pre-gyp/master/lib/util/abi_crosswalk.json', function(response) {
		var body = '';
		response.on('data', function(d) {
			body += d;
		});
		response.on('end', function() {
			var parsed = JSON.parse(body);
			for(var version in parsed) {
				if(nodeVersions[version]) {
					nodeVersions[version].node_abi = parsed[version].node_abi;
					nodeVersions[version].v8 = parsed[version].v8;
				}
			}
			callback();
		});
	});
}

function reduceNodeVersionsToUniqueNodeAbis(callback) {
	var nodeVersionsByNodeAbi = {};
	var reducedNodeVersions = {};
	for(var version in nodeVersions) {
		var versionInfo = nodeVersions[version];
		if(versionInfo.node_abi && !nodeVersionsByNodeAbi[versionInfo.node_abi]) {
			nodeVersionsByNodeAbi[versionInfo.node_abi] = versionInfo;
			reducedNodeVersions[version] = versionInfo;
		}
	}
	nodeVersions = reducedNodeVersions;
	callback();
}

function addNodeJsVersionsToNodeVersionsObject(callback) {
	jsdom.env({
		url: nodeListUrl,
		scripts: ["http://code.jquery.com/jquery.js"],
		done: function (err, window) {
			var $ = window.$;
			//get the version numbers
			$("a").each(function() {
				var href = $(this).attr('href');
				if(href.charAt(0) === 'v') {
					var versionSplitted = href.substring(1, href.length - 1).split('.');
					versionSplitted.forEach(function(version, index){
						versionSplitted[index] = parseInt(version);
					});
					//version check: only do node 0.12+
					if(versionSplitted[0] === 0) {
						if(versionSplitted[1] < 12) {
							//dont handle this one
							return;
						}
					}
					var version = versionSplitted.join('.');
					var versionInfo = {
						string: version,
						flavor: 'node',
						major: versionSplitted[0],
						minor: versionSplitted[1],
						revision: versionSplitted[2],
						ia32: '',
						x64: ''
					};
					//before node 4.x - links to binaries were different
					if(versionSplitted[0] === 0) {
						versionInfo.ia32 = nodeListUrl + href + 'node.exe';
						versionInfo.x64 = nodeListUrl + href + 'x64/node.exe';
					} else {
						versionInfo.ia32 = nodeListUrl + href + 'win-x86/node.exe';
						versionInfo.x64 = nodeListUrl + href + 'win-x64/node.exe';
					}
					nodeVersions[version] = versionInfo;
				}
			});

			callback();
		}
	});
}

function addIoJsVersionsToNodeVersionsObject(callback) {
	jsdom.env({
		url: iojsListUrl,
		scripts: ["http://code.jquery.com/jquery.js"],
		done: function (err, window) {
			var $ = window.$;
			//get the version numbers
			$("a").each(function() {
				var href = $(this).attr('href');
				if(href.charAt(0) === 'v') {
					var versionSplitted = href.substring(1, href.length - 1).split('.');
					versionSplitted.forEach(function(version, index){
						versionSplitted[index] = parseInt(version);
					});
					var version = versionSplitted.join('.');
					var versionInfo = {
						string: version,
						flavor: 'iojs',
						major: versionSplitted[0],
						minor: versionSplitted[1],
						revision: versionSplitted[2],
						ia32: '',
						x64: ''
					};
					versionInfo.ia32 = iojsListUrl + href + 'win-x86/iojs.exe';
					versionInfo.x64 = iojsListUrl + href + 'win-x64/iojs.exe';
					nodeVersions[version] = versionInfo;
				}
			});

			callback();
		}
	});
}

function executePreGyp(nodeExe, callback) {
	var nodePreGypProcess;
	if(publish) {
		nodePreGypProcess = require('child_process').spawn(nodeExe, [nodePreGyp, 'package', 'publish'], { cwd: path.join(__dirname, '..') });
	} else {
		nodePreGypProcess = require('child_process').spawn(nodeExe, [nodePreGyp, 'configure', 'build'], { cwd: path.join(__dirname, '..') });
	}
	nodePreGypProcess.stdout.on('data', function(data) {
		data = data.toString().trim();
		console.log(data);
	});
	nodePreGypProcess.stderr.on('data', function(data) {
		data = data.toString().trim();
		console.log(data);
	});
	nodePreGypProcess.on('disconnect', function() {
		console.log('disconnect');
	});
	nodePreGypProcess.on('close', function() {
		console.log('close');
		callback();
	});
	nodePreGypProcess.on('exit', function() {
		console.log('exit');
	});
}

function buildAll(versions, callback) {
	async.forEachOfSeries(versions, function(versionInfo, version, versionsCallback){
		async.series([
			function(versionSerieCallback) { mkDirIfNotExists(path.join(nodeBinariesDir, versionInfo.string), versionSerieCallback); },
			function(versionSerieCallback) { mkDirIfNotExists(path.join(nodeBinariesDir, versionInfo.string, 'ia32'), versionSerieCallback); },
			function(versionSerieCallback) { downloadIfNotExists(path.join(nodeBinariesDir, versionInfo.string, 'ia32', versionInfo.flavor + '.exe'), versionInfo.ia32, versionSerieCallback); },
			function(versionSerieCallback) { executePreGyp(path.join(nodeBinariesDir, versionInfo.string, 'ia32', versionInfo.flavor + '.exe'), versionSerieCallback); },
			function(versionSerieCallback) { mkDirIfNotExists(path.join(nodeBinariesDir, versionInfo.string, 'x64'), versionSerieCallback); },
			function(versionSerieCallback) { downloadIfNotExists(path.join(nodeBinariesDir, versionInfo.string, 'x64', versionInfo.flavor + '.exe'), versionInfo.x64, versionSerieCallback); },
			function(versionSerieCallback) { executePreGyp(path.join(nodeBinariesDir, versionInfo.string, 'x64', versionInfo.flavor + '.exe'), versionSerieCallback); }
		], function(err, results){
			versionsCallback(err, results)
		});
	}, function(err, results) {
		callback(err, results);
	});
}

init();
