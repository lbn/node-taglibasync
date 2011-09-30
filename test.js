var taglib = require("./build/default/taglib_async.node");

var tag = new taglib.MPEG("/home/rusel/Music/Disturbed/Indestructible/Indestructible.mp3");

var changes = {v2:{}};
changes.v2.TIT2 = "TEST_Indestructible";
changes.v2.TPE1 = "TEST_Disturbed";
tag.set(changes,function(err){
	tag.get(function(err2,data) {
		console.log(JSON.stringify(data));
	});
});

