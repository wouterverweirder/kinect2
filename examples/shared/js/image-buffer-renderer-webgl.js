// WEBGL implementation from Kinect2 SDK JS Examples. Copyright (c) Microsoft Corporation.  All rights reserved.
function ImageBufferRendererWebgl(imageCanvas) {

	//////////////////////////////////////////////////////////////
	// ImageBufferRendererWebgl private constants

	// vertices representing entire viewport as two triangles which make up the whole
	// rectangle, in post-projection/clipspace coordinates
	var VIEWPORT_VERTICES = new Float32Array([
			-1.0, -1.0,
			1.0, -1.0,
			-1.0, 1.0,
			-1.0, 1.0,
			1.0, -1.0,
			1.0, 1.0]);
	var NUM_VIEWPORT_VERTICES = VIEWPORT_VERTICES.length / 2;

	// Texture coordinates corresponding to each viewport vertex
	var VERTEX_TEXTURE_COORDS = new Float32Array([
			0.0, 1.0,
			1.0, 1.0,
			0.0, 0.0,
			0.0, 0.0,
			1.0, 1.0,
			1.0, 0.0]);

	//////////////////////////////////////////////////////////////
	// ImageBufferRendererWebgl private properties
	var metadata = this;
	var contextAttributes = { premultipliedAlpha: true, depth: false };
	var glContext = imageCanvas.getContext('webgl', contextAttributes) || imageCanvas.getContext('experimental-webgl', contextAttributes);
	glContext.clearColor(0.0, 0.0, 0.0, 0.0);      // Set clear color to black, fully transparent

	var vertexShader = createShaderFromSource(glContext.VERTEX_SHADER,
			"\
			attribute vec2 aPosition;\
			attribute vec2 aTextureCoord;\
			\
			varying highp vec2 vTextureCoord;\
			\
			void main() {\
					gl_Position = vec4(aPosition, 0, 1);\
					vTextureCoord = aTextureCoord;\
			}");
	var fragmentShader = createShaderFromSource(glContext.FRAGMENT_SHADER,
			"\
			precision mediump float;\
			\
			varying highp vec2 vTextureCoord;\
			\
			uniform sampler2D uSampler;\
			\
			void main() {\
					gl_FragColor = texture2D(uSampler, vTextureCoord);\
			}");
	var program = createProgram([vertexShader, fragmentShader]);
	glContext.useProgram(program);

	var positionAttribute = glContext.getAttribLocation(program, "aPosition");
	glContext.enableVertexAttribArray(positionAttribute);

	var textureCoordAttribute = glContext.getAttribLocation(program, "aTextureCoord");
	glContext.enableVertexAttribArray(textureCoordAttribute);

	// Associate the uniform texture sampler with TEXTURE0 slot
	var textureSamplerUniform = glContext.getUniformLocation(program, "uSampler");
	glContext.uniform1i(textureSamplerUniform, 0);

	// Create a buffer used to represent whole set of viewport vertices
	var vertexBuffer = glContext.createBuffer();
	glContext.bindBuffer(glContext.ARRAY_BUFFER, vertexBuffer);
	glContext.bufferData(glContext.ARRAY_BUFFER, VIEWPORT_VERTICES, glContext.STATIC_DRAW);
	glContext.vertexAttribPointer(positionAttribute, 2, glContext.FLOAT, false, 0, 0);

	// Create a buffer used to represent whole set of vertex texture coordinates
	var textureCoordinateBuffer = glContext.createBuffer();
	glContext.bindBuffer(glContext.ARRAY_BUFFER, textureCoordinateBuffer);
	glContext.bufferData(glContext.ARRAY_BUFFER, VERTEX_TEXTURE_COORDS, glContext.STATIC_DRAW);
	glContext.vertexAttribPointer(textureCoordAttribute, 2, glContext.FLOAT, false, 0, 0);

	// Create a texture to contain images from Kinect server
	// Note: TEXTURE_MIN_FILTER, TEXTURE_WRAP_S and TEXTURE_WRAP_T parameters need to be set
	//       so we can handle textures whose width and height are not a power of 2.
	var texture = glContext.createTexture();
	glContext.bindTexture(glContext.TEXTURE_2D, texture);
	glContext.texParameteri(glContext.TEXTURE_2D, glContext.TEXTURE_MAG_FILTER, glContext.LINEAR);
	glContext.texParameteri(glContext.TEXTURE_2D, glContext.TEXTURE_MIN_FILTER, glContext.LINEAR);
	glContext.texParameteri(glContext.TEXTURE_2D, glContext.TEXTURE_WRAP_S, glContext.CLAMP_TO_EDGE);
	glContext.texParameteri(glContext.TEXTURE_2D, glContext.TEXTURE_WRAP_T, glContext.CLAMP_TO_EDGE);
	glContext.bindTexture(glContext.TEXTURE_2D, null);

	// Since we're only using one single texture, we just make TEXTURE0 the active one
	// at all times
	glContext.activeTexture(glContext.TEXTURE0);

	//////////////////////////////////////////////////////////////
	// ImageBufferRendererWebgl private methods

	// Create a shader of specified type, with the specified source, and compile it.
	//     .createShaderFromSource(shaderType, shaderSource)
	//
	// shaderType: Type of shader to create (fragment or vertex shader)
	// shaderSource: Source for shader to create (string)
	function createShaderFromSource(shaderType, shaderSource) {
			var shader = glContext.createShader(shaderType);
			glContext.shaderSource(shader, shaderSource);
			glContext.compileShader(shader);

			// Check for errors during compilation
			var status = glContext.getShaderParameter(shader, glContext.COMPILE_STATUS);
			if (!status) {
					var infoLog = glContext.getShaderInfoLog(shader);
					console.log("Unable to compile Kinect '" + shaderType + "' shader. Error:" + infoLog);
					glContext.deleteShader(shader);
					return null;
			}

			return shader;
	}

	// Create a WebGL program attached to the specified shaders.
	//     .createProgram(shaderArray)
	//
	// shaderArray: Array of shaders to attach to program
	function createProgram(shaderArray) {
			var newProgram = glContext.createProgram();

			for (var shaderIndex = 0; shaderIndex < shaderArray.length; ++shaderIndex) {
					glContext.attachShader(newProgram, shaderArray[shaderIndex]);
			}
			glContext.linkProgram(newProgram);

			// Check for errors during linking
			var status = glContext.getProgramParameter(newProgram, glContext.LINK_STATUS);
			if (!status) {
					var infoLog = glContext.getProgramInfoLog(newProgram);
					console.log("Unable to link Kinect WebGL program. Error:" + infoLog);
					glContext.deleteProgram(newProgram);
					return null;
			}

			return newProgram;
	}

	//////////////////////////////////////////////////////////////
	// ImageBufferRendererWebgl public properties
	this.isProcessing = false;
	this.canvas = imageCanvas;
	this.width = 0;
	this.height = 0;
	this.gl = glContext;

	//////////////////////////////////////////////////////////////
	// ImageBufferRendererWebgl public functions

	// Draw image data into WebGL canvas context
	//     .processImageData(imageBuffer, width, height)
	//
	// imageBuffer: ArrayBuffer containing image data
	// width: width of image corresponding to imageBuffer data
	// height: height of image corresponding to imageBuffer data
	this.processImageData = function(imageBuffer, width, height) {
			if ((width != metadata.width) || (height != metadata.height)) {
					// Whenever the image width or height changes, update tracked metadata and canvas
					// viewport dimensions.
					this.width = width;
					this.height = height;
					this.canvas.width = width;
					this.canvas.height = height;
					glContext.viewport(0, 0, width, height);
			}
			//Associate the specified image data with a WebGL texture so that shaders can use it
			glContext.bindTexture(glContext.TEXTURE_2D, texture);
			glContext.texImage2D(glContext.TEXTURE_2D, 0, glContext.RGBA, width, height, 0, glContext.RGBA, glContext.UNSIGNED_BYTE, new Uint8Array(imageBuffer));

			//Draw vertices so that shaders get invoked and we can render the texture
			glContext.drawArrays(glContext.TRIANGLES, 0, NUM_VIEWPORT_VERTICES);
			glContext.bindTexture(glContext.TEXTURE_2D, null);
	};

	// Clear all image data from WebGL canvas
	//     .clear()
	this.clear = function() {
			glContext.clear(glContext.COLOR_BUFFER_BIT);
	};
}

if(module && module.exports) {
	module.exports = ImageBufferRendererWebgl;
}