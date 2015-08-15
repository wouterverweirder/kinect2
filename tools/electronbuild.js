var target = '0.30.4';
var arch = 'x64';

if(process.argv.length > 2)
{
	target = process.argv[2];
}

require('child_process')
	.spawn('cmd', ['/c', 'node-gyp', 'rebuild', '--target=' + target, '--arch=' + arch, '--dist-url=https://atom.io/download/atom-shell'], { stdio: 'inherit'});
