var fs = require('fs');

function getRuntimeInfo() {
	var execPath = fs.realpathSync(process.execPath);

	var runtime = execPath
		.split(/[\\/]+/).pop()
		.split('.').shift();

	runtime = runtime === 'nodejs' ? 'node' : runtime;

	return {
		name: runtime,
		execPath: execPath
	};
}

var runtime = getRuntimeInfo();

if(runtime.name === 'node') {
	//just do node-gyp
	require('child_process')
		.spawn('cmd', ['/c', 'node-gyp', 'configure', 'build'], { stdio: 'inherit'});
} else {
	//use pangyp as long as node-gyp for iojs is broken :-(
	require('child_process')
		.spawn('cmd', ['/c', 'pangyp', 'configure', 'build'], { stdio: 'inherit'});
}