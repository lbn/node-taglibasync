var taglib = require("./build/default/taglib_async.node");

var tag = new taglib.Tag("/home/rusel/Music/Disturbed/Indestructible/Deceiver.mp3");
tag.get(function(err,data) {
	console.log(JSON.stringify(data));
	tag.set({artist:"test"},function(err) {
		console.log(err);
	});
});

