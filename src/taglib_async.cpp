#include <v8.h>
#include <node.h>
#include <unistd.h>
#include <iostream>


#include <fileref.h>
#include <tag.h>
#include <mpegfile.h>
#include <id3v2tag.h>
#include <id3v2frame.h>
#include <id3v2header.h>

#include <id3v1tag.h>

using namespace v8;
using namespace node;

#define REQ_FUN_ARG(I, VAR)                                             \
  if (args.Length() <= (I) || !args[I]->IsFunction())                   \
    return ThrowException(Exception::TypeError(                         \
                  String::New("Argument " #I " must be a function")));  \
  Local<Function> VAR = Local<Function>::Cast(args[I]);

#define SET_STR_TAG(TAG) \
	v1obj->Set(String::New(#TAG),TagLibStringToString(mpeg->v1_tag->TAG())); 
#define SET_INT_TAG(TAG) \
	v1obj->Set(String::New(#TAG),Integer::New(mpeg->v1_tag->TAG())); 


class MPEG : ObjectWrap {
public:
	static Persistent<FunctionTemplate> pft;
	TagLib::MPEG::File *file;
	TagLib::ID3v2::Tag *v2_tag;
	TagLib::ID3v1::Tag *v1_tag;

	Persistent<String> path;

	
	static void Initialize(Handle<Object> target)
	{
		HandleScope scope;
	
		Local<FunctionTemplate> t = FunctionTemplate::New(New);

		pft = Persistent<FunctionTemplate>::New(t);
		pft->InstanceTemplate()->SetInternalFieldCount(2);
		pft->SetClassName(String::NewSymbol("MPEG"));

		NODE_SET_PROTOTYPE_METHOD(pft,"get",MPEG::get);
		NODE_SET_PROTOTYPE_METHOD(pft,"set",MPEG::set);

		target->Set(String::NewSymbol("MPEG"),pft->GetFunction());
	}

	static Handle<Value> New(const Arguments &args)
	{
		HandleScope scope;
		if (args.Length() < 1 || !args[0]->IsString())
			return ThrowException(String::New("Expected string 'path' as first argument"));
		MPEG *mpeg = new MPEG();
		mpeg->path = Persistent<String>::New(args[0]->ToString());
		mpeg->Wrap(args.This());
		return args.This();
	}
	
	static Handle<Value> set(const Arguments &args)
	{
		HandleScope scope;
		REQ_FUN_ARG(1,cb);
		
		MPEG* mpeg = ObjectWrap::Unwrap<MPEG>(args.This());
		MPEGTagInfoReq *req = new MPEGTagInfoReq();
		req->mpeg = mpeg;
		req->cb = Persistent<Function>::New(cb);
		req->taginfo = Persistent<Object>::New(Local<Object>::Cast(args[0]));
		req->error = Persistent<Value>::New(Null());
		mpeg->Ref();

		eio_custom(MPEG::EIO_set, EIO_PRI_DEFAULT, MPEG::EIO_setAfter, req);
		ev_ref(EV_DEFAULT_UC);

		return Undefined();
	}


	static int EIO_set(eio_req *req)
	{
		MPEGTagInfoReq *tagreq = static_cast<MPEGTagInfoReq *>(req->data);
		MPEG *mpeg = tagreq->mpeg;
		Read(mpeg);
		if (mpeg->v2_tag && tagreq->taginfo->Has(String::New("v2"))) {
			Local<Object> v2_obj = Local<Object>::Cast(tagreq->taginfo->Get(String::New("v2")));
			Local<Array> fields = v2_obj->GetPropertyNames();
			TagLib::ID3v2::FrameListMap framelistmap = mpeg->v2_tag->frameListMap();
			
			for (short i = 0; i < fields->Length(); i++)
			{
				Local<String> field = Local<String>::Cast(fields->Get(i));
				Local<Value> val = v2_obj->Get(field);
				String::AsciiValue framename(field);
				if (framelistmap.contains(*framename)) {
					TagLib::ID3v2::Frame *frame = framelistmap[*framename].front();
					frame->setText(NodeStringToTagLibString(val));
				} else {
					// TODO: create frame if not found
				}
			}
		}
		// TODO: Set v1 tags

		if (!mpeg->file->save()) {
			tagreq->error = Persistent<String>::New(String::New("Cannot save the file"));
		}	
		return 0;
	}

	static int EIO_setAfter(eio_req *req)
	{
		HandleScope scope;
		MPEGTagInfoReq *tagreq = static_cast<MPEGTagInfoReq *>(req->data);
		ev_unref(EV_DEFAULT_UC);
		tagreq->mpeg->Unref();

		Local<Value> argv[2];
		argv[0] = Local<Value>::New(tagreq->error);
		argv[1] = Local<Object>::New(tagreq->taginfo);
		TryCatch try_catch;
		tagreq->cb->Call(Context::GetCurrent()->Global(),2,argv);

		if (try_catch.HasCaught()) {
			FatalException(try_catch);
		}
		tagreq->cb.Dispose();
		delete tagreq;
		return 0;
	}

	static bool Read(MPEG *mpeg)
	{
		if (mpeg->file) {
			return true;
		}
		String::Utf8Value path(mpeg->path);
		mpeg->file = new TagLib::MPEG::File(*path);
		mpeg->v2_tag = mpeg->file->ID3v2Tag();
		mpeg->v1_tag = mpeg->file->ID3v1Tag();

		return true; // TODO: return false if file does not exist
	}
	static Handle<Value> get(const Arguments &args)
	{
		HandleScope scope;
		REQ_FUN_ARG(0,cb);
	
		MPEG* mpeg = ObjectWrap::Unwrap<MPEG>(args.This());
		MPEGTagInfoReq *req = new MPEGTagInfoReq();
		req->mpeg = mpeg;
		req->cb = Persistent<Function>::New(cb);
		req->taginfo = Persistent<Object>::New(Object::New());
		req->error = Persistent<Value>::New(Null());
		mpeg->Ref();

		eio_custom(MPEG::EIO_get, EIO_PRI_DEFAULT, MPEG::EIO_getAfter, req);
		ev_ref(EV_DEFAULT_UC);

		return Undefined();
	}

	static int EIO_get(eio_req *req)
	{
		MPEGTagInfoReq *tagreq = static_cast<MPEGTagInfoReq *>(req->data);
		MPEG *mpeg = tagreq->mpeg;
		if (!Read(mpeg)) {
			tagreq->error	= Persistent<String>::New(String::New("File does not exist"));
			return 0;
		}
		if (!mpeg->v1_tag && !mpeg->v2_tag)	{
			tagreq->error	= Persistent<String>::New(String::New("No ID3v1 or ID3v2 tags found"));
			return 0;
		}

		tagreq->taginfo->Set(String::New("v2"),Object::New());
		Local<Object> v2obj = Local<Object>::Cast(tagreq->taginfo->Get(String::New("v2")));
		
		if (mpeg->v2_tag) {
			TagLib::ID3v2::FrameList::ConstIterator it = mpeg->v2_tag->frameList().begin();
			for(; it != mpeg->v2_tag->frameList().end(); it++) {
				Handle<String> frameid = TagLibStringToString(*(new TagLib::String((*it)->frameID())));
				Local<Value> value = FrameToValue(*it);	
				v2obj->Set(frameid,value);
			}	
		}
		
		tagreq->taginfo->Set(String::New("v1"),Object::New());
		Local<Object> v1obj = Local<Object>::Cast(tagreq->taginfo->Get(String::New("v1")));
		if (mpeg->v1_tag) {
			SET_STR_TAG(title)
			SET_STR_TAG(artist)
			SET_STR_TAG(album)
			SET_STR_TAG(comment)
			SET_STR_TAG(genre)
			SET_INT_TAG(track)
			SET_INT_TAG(year)
		}
		return 0;
	}

	static int EIO_getAfter(eio_req *req)
	{
		HandleScope scope;
		MPEGTagInfoReq *tagreq = static_cast<MPEGTagInfoReq *>(req->data);
		ev_unref(EV_DEFAULT_UC);
		tagreq->mpeg->Unref();

		Local<Value> argv[2];
		argv[0] = Local<Value>::New(tagreq->error);
		argv[1] = Local<Object>::New(tagreq->taginfo);
		TryCatch try_catch;
		tagreq->cb->Call(Context::GetCurrent()->Global(),2,argv);

		if (try_catch.HasCaught()) {
			FatalException(try_catch);
		}
		tagreq->cb.Dispose();
		delete tagreq;
		return 0;
	}
	
	static Local<Value> FrameToValue(TagLib::ID3v2::Frame *frame)
	{
		const char *charval = frame->toString().to8Bit(true).c_str();
		char *end;
		int derp = (int)strtol(charval,&end,10);
		if (charval == end) { // not a number
			return Local<Value>::New(String::New(charval));
		} else { // number
			return Local<Value>::New(Integer::New(derp));
		}	
	}
	
	
	// Author: Nikhil Marathe (github/nikhilm)	
	static Local<String> TagLibStringToString( TagLib::String s )
	{
		TagLib::ByteVector str = s.data(TagLib::String::UTF16);
		// Strip the Byte Order Mark of the input to avoid node adding a UTF-8
		// Byte Order Mark
		return String::New((uint16_t *)str.mid(2,str.size()-2).data(), s.size());
	}

	// Author: Nikhil Marathe (github/nikhilm)	
	static TagLib::String NodeStringToTagLibString( Local<Value> s )
	{
		String::AsciiValue str(s->ToString());
		return TagLib::String(*str, TagLib::String::UTF8);
	}

	struct MPEGTagInfoReq
	{
		Persistent<Function> cb;
		MPEG *mpeg;
		Persistent<Object> taginfo;
		Persistent<Value> error;
	};

};



Persistent<FunctionTemplate> MPEG::pft;

extern "C" {
	static void init(Handle<Object> target)
	{
		MPEG::Initialize(target);
	}
	NODE_MODULE(taglib_async, init);
}
