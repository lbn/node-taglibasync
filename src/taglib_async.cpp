#include <v8.h>
#include <node.h>
#include <unistd.h>
#include <iostream>


#include <fileref.h>
#include <tag.h>

using namespace v8;
using namespace node;

#define REQ_FUN_ARG(I, VAR)                                             \
  if (args.Length() <= (I) || !args[I]->IsFunction())                   \
    return ThrowException(Exception::TypeError(                         \
                  String::New("Argument " #I " must be a function")));  \
  Local<Function> VAR = Local<Function>::Cast(args[I]);

#define GET_STR_TAG(TAG) \
	tagreq->taginfo->Set(String::New(#TAG),TagLibStringToString(tag->fileref->tag()->TAG())); 
#define GET_INT_TAG(TAG) \
	tagreq->taginfo->Set(String::New(#TAG),Integer::New(tag->fileref->tag()->TAG())); 

class Tag : ObjectWrap {
public:
	static Persistent<FunctionTemplate> pft;
	TagLib::FileRef *fileref;
	Persistent<String> path;

	static void Initialize(Handle<Object> target)
	{
		HandleScope scope;
	
		Local<FunctionTemplate> t = FunctionTemplate::New(New);

		pft = Persistent<FunctionTemplate>::New(t);
		pft->InstanceTemplate()->SetInternalFieldCount(2);
		pft->SetClassName(String::NewSymbol("Tag"));

		NODE_SET_PROTOTYPE_METHOD(pft,"get",Tag::get);
		NODE_SET_PROTOTYPE_METHOD(pft,"set",Tag::set);

		target->Set(String::NewSymbol("Tag"),pft->GetFunction());
	}

	static Handle<Value> New(const Arguments &args)
	{
		HandleScope scope;
		if (args.Length() < 1 || !args[0]->IsString())
			return ThrowException(String::New("Expected string 'path' as first argument"));
		Tag *tag = new Tag();
		tag->path = Persistent<String>::New(args[0]->ToString());
		tag->Wrap(args.This());
		return args.This();
	}

	static Handle<Value> set(const Arguments &args)
	{
		HandleScope scope;
		REQ_FUN_ARG(1,cb);
		
		Tag* tag = ObjectWrap::Unwrap<Tag>(args.This());
		TagInfoReq *req = new TagInfoReq();
		req->tag = tag;
		req->cb = Persistent<Function>::New(cb);
		req->taginfo = Persistent<Object>::New(Local<Object>::Cast(args[0]));
		req->error = Persistent<Value>::New(Null());
		tag->Ref();

		eio_custom(Tag::EIO_set, EIO_PRI_DEFAULT, Tag::EIO_setAfter, req);
		ev_ref(EV_DEFAULT_UC);

		return Undefined();
	}


	static int EIO_set(eio_req *req)
	{
		TagInfoReq *tagreq = static_cast<TagInfoReq *>(req->data);
		Tag *tag = tagreq->tag;
		if (tag->fileref == NULL) {
			String::Utf8Value path(tag->path);
			tag->fileref = new TagLib::FileRef(*path);
		}
		if (tag->fileref->isNull() || !tag->fileref->tag()) {
			tagreq->error	= Persistent<String>::New(String::New("Error while reading data from the file"));
		} else {
			Local<Array> fields = tagreq->taginfo->GetPropertyNames();
			tagreq->error	= Persistent<Integer>::New(Integer::New(fields->Length()));
			for (short i = 0; i < fields->Length(); i++)
			{
				Local<Value> field = capitalise(fields->Get(i));
				Local<Value> val = tagreq->taginfo->Get(field);
				// TODO: save changes
			}
		}
		return 0;
	}

	static int EIO_setAfter(eio_req *req)
	{
		HandleScope scope;
		TagInfoReq *tagreq = static_cast<TagInfoReq *>(req->data);
		ev_unref(EV_DEFAULT_UC);
		tagreq->tag->Unref();

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

	static Handle<Value> get(const Arguments &args)
	{
		HandleScope scope;
		REQ_FUN_ARG(0,cb);
	
		Tag* tag = ObjectWrap::Unwrap<Tag>(args.This());
		TagInfoReq *req = new TagInfoReq();
		req->tag = tag;
		req->cb = Persistent<Function>::New(cb);
		req->taginfo = Persistent<Object>::New(Object::New());
		req->error = Persistent<Value>::New(Null());
		tag->Ref();

		eio_custom(Tag::EIO_get, EIO_PRI_DEFAULT, Tag::EIO_getAfter, req);
		ev_ref(EV_DEFAULT_UC);

		return Undefined();
	}
	static int EIO_get(eio_req *req)
	{
		TagInfoReq *tagreq = static_cast<TagInfoReq *>(req->data);
		Tag *tag = tagreq->tag;
		if (tag->fileref == NULL) {
			String::Utf8Value path(tag->path);
			tag->fileref = new TagLib::FileRef(*path);
		}
		if (tag->fileref->isNull() || !tag->fileref->tag()) {
			tagreq->error	= Persistent<String>::New(String::New("Error while reading data from derp.ini"));
		} else {
			GET_STR_TAG(artist)
			GET_STR_TAG(album)
			GET_STR_TAG(title)
			GET_STR_TAG(genre)
			GET_INT_TAG(year)
			GET_INT_TAG(track)
		}	
		return 0;
	}

	static int EIO_getAfter(eio_req *req)
	{
		HandleScope scope;
		TagInfoReq *tagreq = static_cast<TagInfoReq *>(req->data);
		ev_unref(EV_DEFAULT_UC);
		tagreq->tag->Unref();

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
		String::Utf8Value str(s->ToString());
		return TagLib::String(*str, TagLib::String::UTF8);
	}

	struct TagInfoReq
	{
		Persistent<Function> cb;
		Tag *tag;
		Persistent<Object> taginfo;
		Persistent<Value> error;
	};

	static Local<String> capitalise(Local<Value> val)
	{
		String::Utf8Value str(val);
		std::string a = *str;
		a[0] = toupper(a[0]);
		return Local<String>::New(String::New(a.c_str()));
	}
};



Persistent<FunctionTemplate> Tag::pft;

extern "C" {
	static void init(Handle<Object> target)
	{
		Tag::Initialize(target);
	}
	NODE_MODULE(taglib_async, init);
}
