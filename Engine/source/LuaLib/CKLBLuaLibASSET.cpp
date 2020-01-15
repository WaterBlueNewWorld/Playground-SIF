﻿/* 
   Copyright 2013 KLab Inc.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#include "CKLBLuaLibASSET.h"
#include "CKLBUtility.h"
#include <dirent.h>

static ILuaFuncLib::DEFCONST luaConst[] = {
//	{ "DBG_M_SWITCH",	DBG_MENU::M_SWITCH },
	{ 0, 0 }
};

static CKLBLuaLibASSET libdef(luaConst);

CKLBLuaLibASSET::CKLBLuaLibASSET(DEFCONST * arrConstDef) : ILuaFuncLib(arrConstDef) {}
CKLBLuaLibASSET::~CKLBLuaLibASSET() {}

// Lua関数の追加
void
CKLBLuaLibASSET::addLibrary()
{
	addFunction("ASSET_getImageSize",		CKLBLuaLibASSET::luaGetImageSize);
	addFunction("ASSET_getBoundSize",		CKLBLuaLibASSET::luaGetBoundSize);
	addFunction("ASSET_getAssetInfo",		CKLBLuaLibASSET::luaGetAssetInfo);
	addFunction("ASSET_getExternalFree",	CKLBLuaLibASSET::luaGetExternalFree);
	addFunction("ASSET_getFileList",		CKLBLuaLibASSET::luaGetFileList);
	addFunction("ASSET_getAssetPathIfNotExist", CKLBLuaLibASSET::luaGetAssetPathIfNotExist);
	addFunction("ASSET_delExternal",		CKLBLuaLibASSET::luaDelExternal);
	addFunction("ASSET_registerNotFound",	CKLBLuaLibASSET::luaRegisterNotFound);
	addFunction("ASSET_setPlaceHolder",		CKLBLuaLibASSET::luaSetPlaceHolder);
	addFunction("ASSET_killDownload",		CKLBLuaLibASSET::luaKillDownload);

	addFunction("Asset_getNMAssetSize",		CKLBLuaLibASSET::luaGetNMAssetSize);
	addFunction("Asset_getNMAsset",			CKLBLuaLibASSET::luaGetNMAsset);
	addFunction("Asset_setNMAsset",			CKLBLuaLibASSET::luaSetNMAsset);
}

s32
CKLBLuaLibASSET::luaGetImageSize(lua_State * L)
{
	CLuaState lua(L);
	int argc = lua.numArgs();
	if(argc != 1) {
		lua.retNil();
		lua.retNil();
		return 2;
	}

	const char * asset_name = lua.getString(1);
	u32 handle;
	CKLBImageAsset * pAsset = (CKLBImageAsset *)CKLBUtility::loadAssetScript(asset_name, &handle);
	if(!pAsset) {
		lua.retNil();
		lua.retNil();
		return 2;
	}

	SKLBRect * rect = pAsset->getSize();

	s32 width = rect->getWidth();
	s32 height = rect->getHeight();

	CKLBDataHandler::releaseHandle(handle);

	lua.retInt(width);
	lua.retInt(height);
	return 2;	
}

s32
CKLBLuaLibASSET::luaGetBoundSize(lua_State * L)
{
	CLuaState lua(L);
	int argc = lua.numArgs();
	if(argc != 1) {
		lua.retNil();
		lua.retNil();
		return 2;
	}
	const char * asset_name = lua.getString(1);
	u32 handle;
	CKLBImageAsset * pAsset = (CKLBImageAsset *)CKLBUtility::loadAssetScript(asset_name, &handle);
	if(!pAsset) {
		lua.retNil();
		lua.retNil();
		return 2;
	}
	float width = pAsset->m_boundWidth;
	float height = pAsset->m_boundHeight;

	CKLBDataHandler::releaseHandle(handle);

	lua.retDouble(width);
	lua.retDouble(height);
	return 2;	

}

s32
CKLBLuaLibASSET::luaGetAssetInfo(lua_State * L)
{
	CLuaState lua(L);
	int argc = lua.numArgs();
	if(argc != 1) {
		lua.retNil();
		lua.retNil();
		lua.retNil();
		lua.retNil();
		return 4;
	}
	const char * asset_name = lua.getString(1);
	u32 handle;
	CKLBImageAsset * pAsset = (CKLBImageAsset *)CKLBUtility::loadAssetScript(asset_name, &handle);
	if(!pAsset) {
		lua.retNil();
		lua.retNil();
		lua.retNil();
		lua.retNil();
		return 4;
	}
	SKLBRect * rect = pAsset->getSize();
	s32 img_width = rect->getWidth();
	s32 img_height = rect->getHeight();
	float bound_width = pAsset->m_boundWidth;
	float bound_height = pAsset->m_boundHeight;

	CKLBDataHandler::releaseHandle(handle);

	lua.retInt(img_width);
	lua.retInt(img_height);
	lua.retDouble(bound_width);
	lua.retDouble(bound_height);

	return 4;
}

s32
CKLBLuaLibASSET::luaDelExternal(lua_State * L)
{
	CLuaState lua(L);
	int argc = lua.numArgs();
	if(argc != 1) {
		lua.retBool(false);
		return 1;
	}
	const char * asset_name = lua.getString(1);
	bool res = CPFInterface::getInstance().platform().removeFileOrFolder(asset_name);
	lua.retBool(res);
	return 1;
}

s32
CKLBLuaLibASSET::luaGetExternalFree(lua_State * L)
{
	CLuaState lua(L);
	int argc = lua.numArgs();
	if(argc != 0) {
		lua.retInt(0);
		return 1;
	}
	s32 res = (s32)CPFInterface::getInstance().platform().getFreeSpaceExternalKB(); // Never return more than 0xFFFFFF
	lua.retInt(res);
	return 1;
}

u32 CKLBLuaLibASSET::assetSize;

s32
CKLBLuaLibASSET::luaGetNMAssetSize(lua_State * L)
{
	CLuaState lua(L);
	IPlatformRequest& platform = CPFInterface::getInstance().platform();
	assetSize = lua.getInt(1);
	char* result = (char*)malloc(assetSize);
#ifdef _WIN32
	// not implemented for all platforms yet
	platform.getRandomBytes(result, assetSize);
#else
	for (int i = 0; i < assetSize; i++)
		result[i] = rand() % 60 + 'A';
#endif
	lua_pushlstring(L, result, assetSize);
	free(result);
	return 1;
}

s32
CKLBLuaLibASSET::luaGetNMAsset(lua_State * L) 
{
	CLuaState lua(L);
	lua.retString("12345678901234567890123456789012");
	return 1;
}

s32
CKLBLuaLibASSET::luaSetNMAsset(lua_State * L)
{
	CLuaState lua(L);
	int argc = lua.numArgs();
	lua.printStack();
	if (argc != 2) {
		lua.retBool(false);
		return 0;
	}
	const char* str1 = lua.getString(1);
	const char* str2 = lua.getString(2);
	char* result	 = (char*)malloc(assetSize);
	for (int i = 0; i < assetSize; i++) {
		result[i] = str1[i] ^ str2[i];
	}
	
	if (strlen(str1) == 16 || strlen(str2) == 16)
		lua_pushlstring(L, result, 16);
	else
		lua_pushlstring(L, result, 32);
	free(result);
	return 1;
}

s32
CKLBLuaLibASSET::luaRegisterNotFound(lua_State * L)
{
	
	CLuaState lua(L);
	CKLBAssetManager& mgr = CKLBAssetManager::getInstance();
	const char* handler = lua.getString(1);
	mgr.setRegisterNotFound(handler);
	return 1;
}

s32
CKLBLuaLibASSET::luaSetPlaceHolder(lua_State * L)
{
	CLuaState lua(L);
	CKLBAssetManager& mgr = CKLBAssetManager::getInstance();
	const char* asset = lua.getString(1);
	mgr.setPlaceholder(asset);
	return 1;
}

s32
CKLBLuaLibASSET::luaGetFileList(lua_State* L)
{
	CLuaState lua(L);
	IPlatformRequest& platform = CPFInterface::getInstance().platform();
	const char* path = platform.getFullPath(lua.getString(1));

	DIR* dir;
	struct dirent* ent;
	int i = 0;
	// create parent table
	lua.tableNew();

	if (dir = opendir(path)) {
		while (ent = readdir(dir)) {
			if (strcmp(ent->d_name, ".") && strcmp(ent->d_name, "..")) {
				// store index in table
				lua.retInt(i++);

				// create sub table
				lua.tableNew();
				lua.retString("name");
				lua.retString(ent->d_name);
				// set sub table
				lua.tableSet();
				// set parent table
				lua.tableSet();
			}
		}
		closedir(dir);
	}
	
	delete[] path;
	return 1;
}

s32
CKLBLuaLibASSET::luaGetAssetPathIfNotExist(lua_State* L)
{
	CLuaState lua(L);
	IPlatformRequest& platform = CPFInterface::getInstance().platform();
	const char* assetPath = lua.getString(1);
	char* newAssetPath = new char[strlen(assetPath) + 14];

	klb_assert(!strstr(assetPath, ".mp3") || !strstr(assetPath, ".ogg"), "Never use a .ogg or .mp3 extension. Audio Asset have none, automatically detected inside");
	sprintf(newAssetPath, "asset://%s", assetPath);

	// check path
	if (platform.getFullPath(newAssetPath)) {
		// return nil if file exists
		lua.retNil();
		delete[] newAssetPath;
		return 1;
	}

	if (!strstr(newAssetPath, ".texb") && !strstr(newAssetPath, ".imag")) {
		// check path with .mp3 audio extension
		sprintf(newAssetPath + strlen(newAssetPath), ".mp3");
		if (platform.getFullPath(newAssetPath)) {
			lua.retNil();
			delete[] newAssetPath;
			return 1;
		}

		// check path with .ogg audio extension
		sprintf(newAssetPath + strlen(newAssetPath) - 4, ".ogg");
		if (platform.getFullPath(newAssetPath)) {
			lua.retNil();
			delete[] newAssetPath;
			return 1;
		}
	}

	lua.retString(newAssetPath);
	delete[] newAssetPath;
	return 1;
}

s32
CKLBLuaLibASSET::luaKillDownload(lua_State* L)
{
	CLuaState lua(L);
	// part of Micro Download
	// TODO
	lua.printStack();
	return 1;
}

void CKLBLuaLibASSET::cmdGetImageSize(const char* asset_name, s32* pReturnWidth, s32* pReturnHeight)
{
	u32 handle;
	CKLBImageAsset * pAsset = (CKLBImageAsset *)CKLBUtility::loadAssetScript( asset_name, &handle);
	if(pAsset)
	{
		SKLBRect * rect = pAsset->getSize();

		*pReturnWidth = rect->getWidth();
		*pReturnHeight = rect->getHeight();

		CKLBDataHandler::releaseHandle(handle);
	}
}

void CKLBLuaLibASSET::cmdGetBoundSize(const char* asset_name, float* pReturnWidth, float* pReturnHeight)
{
	u32 handle;
	CKLBImageAsset * pAsset = (CKLBImageAsset *)CKLBUtility::loadAssetScript( asset_name, &handle);
	if(pAsset)
	{
		*pReturnWidth = pAsset->m_boundWidth;
		*pReturnHeight = pAsset->m_boundHeight;

		CKLBDataHandler::releaseHandle(handle);
	}
}

void CKLBLuaLibASSET::cmdGetAssetInfo(const char* asset_name, s32* pReturnImgWidth, s32* pReturnImgHeight, float* pReturnBoundWidth, float* pReturnBoundHeight)
{
	u32 handle;
	CKLBImageAsset * pAsset = (CKLBImageAsset *)CKLBUtility::loadAssetScript( asset_name, &handle);
	if(pAsset) {
		SKLBRect * rect = pAsset->getSize();
		
		*pReturnImgWidth = rect->getWidth();
		*pReturnImgHeight = rect->getHeight();
		*pReturnBoundWidth = pAsset->m_boundWidth;
		*pReturnBoundHeight = pAsset->m_boundHeight;

		CKLBDataHandler::releaseHandle(handle);
	}
}
