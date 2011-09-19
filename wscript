def set_options(opt):
	opt.tool_options("compiler_cxx")

def configure(conf):
	conf.check_tool("compiler_cxx")
	conf.check_tool("node_addon")
	conf.check_cfg(package="taglib",args="--cflags --libs",
		uselib_store="TAGLIB")

def build(bld):
	obj = bld.new_task_gen("cxx", "shlib", "node_addon")
	obj.target = "taglib_async"
	obj.source = "src/taglib_async.cpp"
	obj.cxxflags = ["-g","-D_FILE_OFFSET_BITS=64", "-D_LARGEFILE_SOURCE"]
	obj.uselib = "TAGLIB"


