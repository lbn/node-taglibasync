# Asynchronous TagLib bindings for Node.js

## Installation
    $ node-waf configure && node-waf build

## Usage
```js

var taglib = require("./build/default/taglib_async.node");
var tag = new taglib.MPEG("./your.mp3");

tag.set({v2 : { TIT2: "New Title", TPE1: "New Artist" }},function(err) {
	tag.get(function(err,data){
		console.log(JSON.stringify(data));
	});
});
```

