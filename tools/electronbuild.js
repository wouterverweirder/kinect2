var path = require('path'),
	fs = require('fs'),
	async = require('async');

var argv;

function init() {
	createArgv();
	async.series([
		createTmpBindingFile,
		removePreGypInfoFromBindingFile,
		build,
		restoreBindingFile
	], function(err, result){
		console.log('all done');
	});
}

//based on https://raw.githubusercontent.com/electron/electron-rebuild/master/src/electron-locater.js
var locateElectronPrebuilt = function locateElectronPrebuilt() {
	var possibleModuleNames = ['electron', 'electron-prebuilt', 'electron-prebuilt-compile'];
  var electronPath = false;

  // Attempt to locate modules by path
  var foundModule = possibleModuleNames.some(function (moduleName) {
    electronPath = path.join(__dirname, '..', '..', moduleName);
    return fs.existsSync(electronPath);
  });

  // Return a path if we found one
  if (foundModule) return electronPath;

  // Attempt to locate modules by require
  foundModule = possibleModuleNames.some(function (moduleName) {
    try {
      electronPath = path.join(require.resolve(moduleName), '..');
    } catch (e) {
      return false;
    }
    return fs.existsSync(electronPath);
  });

  // Return a path if we found one
  if (foundModule) return electronPath;
  return null;
};

function createArgv() {
	var target = '0.30.4';
	var electron = locateElectronPrebuilt();
	console.log(electron);
	if(electron) {
		var electronPackageJSON = JSON.parse(fs.readFileSync(path.resolve(electron, 'package.json')));
		if(electronPackageJSON) {
			target = electronPackageJSON.version;
		}
	}
	console.log('build for electron ' + target + ' (' + process.arch + ')');
	argv = require('minimist')(process.argv.slice(2), {
		default: {
			target: target,
			arch: process.arch
		}
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
