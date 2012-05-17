// automatically generated by admSerialization.py, do not edit!
#include "ADM_default.h"
#include "ADM_paramList.h"
#include "ADM_coreJson.h"
#include "msharpen.h"
bool  msharpen_jserialize(const char *file, const msharpen *key){
admJson json;
json.addBool("mask",key->mask);
json.addBool("highq",key->highq);
json.addUint32("threshold",key->threshold);
json.addUint32("strength",key->strength);
return json.dumpToFile(file);
};
bool  msharpen_jdeserialize(const char *file, const ADM_paramList *tmpl,msharpen *key){
admJsonToCouple json;
CONFcouple *c=json.readFromFile(file);
if(!c) {ADM_error("Cannot read json file");return false;}
bool r= ADM_paramLoadPartial(c,tmpl,key);
delete c;
return r;
};