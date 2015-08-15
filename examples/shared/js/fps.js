(function(){
	//browser fps calc http://jsfiddle.net/krnlde/u1fL1cs5/
	var fpsEl = document.createElement('h1');
	fpsEl.style.position = 'absolute';
	fpsEl.style.top = 0;
	fpsEl.style.left = 0;
	fpsEl.style.background = 'white';
	document.body.appendChild(fpsEl);
	window.countFPS = (function () {
	  var lastLoop = (new Date()).getMilliseconds();
	  var count = 1;
	  var fps = 0;

	  return function () {
	    var currentLoop = (new Date()).getMilliseconds();
	    if (lastLoop > currentLoop) {
	      fps = count;
	      count = 1;
	    } else {
	      count += 1;
	    }
	    lastLoop = currentLoop;
	    return fps;
	  };
	}());
	(function loop() {
		requestAnimationFrame(function () {
			fpsEl.innerHTML = countFPS() + ' fps';
			loop();
		});
	}());
})();
