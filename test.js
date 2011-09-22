var taglib = require("./build/default/taglib_async.node");

var tag = new taglib.MPEG("/home/rusel/Music/Disturbed/Indestructible/Deceiver.mp3");

var changes = {v2:{}};
changes.v2.TIT2 = "TEST_Deceiver";
changes.v2.TPE1 = "TEST_Disturbed";
tag.set(changes,function(err){
	tag.get(function(err2,data) {
		console.log(JSON.stringify(data));
	});
});

