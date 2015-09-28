var path = require('path'),
	fs = require('fs'),
	async = require('async');

var argv = require('minimist')(process.argv.slice(2), {
	default: {
		target: '0.30.4',
		arch: 'x64'
	}
});

function init() {
	async.series([
		createTmpBindingFile,
		removePreGypInfoFromBindingFile,
		build,
		restoreBindingFile
	], function(err, result){
		console.log('all done');
	});
}

function createTmpBindingFile(callback) {
	var srcPath = path.join(__dirname, '..', 'binding.gyp');
	var dstPath = path.join(__dirname, '..', 'binding.gyp.backup');
	copyFile(srcPath, dstPath, callback);
}

function copyFile(srcPath, dstPath, callback) {
	var cbCalled = false;
	var rd = fs.createReadStream(srcPath);
	rd.on("error", function(err) {
		done(err);
	});
	var wr = fs.createWriteStream(dstPath);
	wr.on("error", function(err) {
		done(err);
	});
	wr.on("close", function(ex) {
		done();
	});
	rd.pipe(wr);

	function done(err) {
		if (!cbCalled) {
			callback(err);
			cbCalled = true;
		}
	}
}

function removePreGypInfoFromBindingFile(callback) {
	var obj;
	var srcPath = path.join(__dirname, '..', 'binding.gyp');
	fs.readFile(srcPath, 'utf8', function (err, data) {
		if (err) {
			return callback(err);
		}
		obj = JSON.parse(data);
		obj.targets = obj.targets.filter(function(target) {
			return (target.target_name !== 'action_after_build');
		});
		fs.writeFile(srcPath, JSON.stringify(obj, null, 2), callback);
	});
}

function build(callback) {
	var nodeGypProcess = require('child_process').spawn('cmd', ['/c', 'node-gyp', 'configure', 'build', '--target=' + argv.target, '--arch=' + argv.arch, '--dist-url=https://atom.io/download/atom-shell'], { cwd: path.join(__dirname, '..') });
	nodeGypProcess.stdout.on('data', function(data) {
		data = data.toString().trim();
		console.log(data);
	});
	nodeGypProcess.stderr.on('data', function(data) {
		data = data.toString().trim();
		console.log(data);
	});
	nodeGypProcess.on('disconnect', function() {
		console.log('disconnect');
	});
	nodeGypProcess.on('close', function() {
		console.log('close');
		callback();
	});
	nodeGypProcess.on('exit', function() {
		console.log('exit');
	});
}

function restoreBindingFile(callback) {
	var srcPath = path.join(__dirname, '..', 'binding.gyp.backup');
	var dstPath = path.join(__dirname, '..', 'binding.gyp');
	copyFile(srcPath, dstPath, function(err, result){
		if(err) {
			return callback(err);
		}
		//remove the tmp file
		fs.unlink(srcPath, callback);
	});
}

init();
