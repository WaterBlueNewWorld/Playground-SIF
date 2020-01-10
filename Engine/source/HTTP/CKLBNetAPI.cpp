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
#include "CKLBNetAPI.h"
#include "CKLBLuaEnv.h"
#include "CKLBUtility.h"
#include "CKLBJsonItem.h"
#include "CPFInterface.h"
#include "CKLBNetAPIKeyChain.h"

#include <time.h>
#include <ctype.h>

enum {
	// Command Values定義
	NETAPI_SEND,				// send JSON packet
	NETAPI_CANCEL,				// selected session cancel
	NETAPI_CANCEL_ALL,			// cancel all sessions
	NETAPI_BUSY,				// busy
	NETAPI_STARTUP,				// login/startUp
	NETAPI_LOGIN,				// login/login
	NETAPI_WATCH_MAINTENANCE,	// check for maintenance? [not_used]
	NETAPI_DEBUG_HDR,			// set "Debug: 1" header [not_used]
	NETAPI_GEN_CMDNUMID,		// generate commandNum string [not_used]
};

static IFactory::DEFCMD cmd[] = {
	{"NETAPI_SEND",					NETAPI_SEND					},
	{"NETAPI_CANCEL",				NETAPI_CANCEL				},
    {"NETAPI_CANCEL_ALL",			NETAPI_CANCEL_ALL			},
	{"NETAPI_BUSY",					NETAPI_BUSY					},
	{"NETAPI_STARTUP",				NETAPI_STARTUP				},
	{"NETAPI_LOGIN",				NETAPI_LOGIN				},
	{"NETAPI_WATCH_MAINTENANCE",	NETAPI_WATCH_MAINTENANCE	},
	{"NETAPI_DEBUG_HDR",			NETAPI_DEBUG_HDR			},
	{"NETAPI_GEN_CMDNUMID",			NETAPI_GEN_CMDNUMID			},

	//
	// Callback constants
	//
	{ "NETAPIMSG_CONNECTION_CANCELED",	NETAPIMSG_CONNECTION_CANCELED },
	{ "NETAPIMSG_CONNECTION_FAILED",	NETAPIMSG_CONNECTION_FAILED },
	{ "NETAPIMSG_INVITE_FAILED",		NETAPIMSG_INVITE_FAILED },
	{ "NETAPIMSG_STARTUP_FAILED",		NETAPIMSG_STARTUP_FAILED },
	{ "NETAPIMSG_LOGIN_FAILED",			NETAPIMSG_LOGIN_FAILED },
	{ "NETAPIMSG_SERVER_TIMEOUT",		NETAPIMSG_SERVER_TIMEOUT },
	{ "NETAPIMSG_REQUEST_FAILED",		NETAPIMSG_REQUEST_FAILED },
	{ "NETAPIMSG_SERVER_ERROR",			NETAPIMSG_SERVER_ERROR },
	{ "NETAPIMSG_UNKNOWN",				NETAPIMSG_UNKNOWN },
	{ "NETAPIMSG_REQUEST_SUCCESS",		NETAPIMSG_REQUEST_SUCCESS },
	{ "NETAPIMSG_LOGIN_SUCCESS",		NETAPIMSG_LOGIN_SUCCESS },
	{ "NETAPIMSG_STARTUP_SUCCESS",		NETAPIMSG_STARTUP_SUCCESS },
	{ "NETAPIMSG_INVITE_SUCCESS",		NETAPIMSG_INVITE_SUCCESS },
	{0, 0}
};

static CKLBTaskFactory<CKLBNetAPI> factory("HTTP_API", CLS_KLBNETAPI, cmd);

enum {
	ARG_CALLBACK	= 1,
	ARG_REQUIRE     = ARG_CALLBACK,
};


CKLBNetAPI::CKLBNetAPI()
: CKLBLuaTask           ()
, m_http				(NULL)
, m_timeout				(30000)
, m_timestart			(0)
, m_canceled			(false)
, m_pRoot				(NULL)
, m_callback			(NULL)
, m_http_header_array	(NULL)
, m_http_header_length	(0)
, m_nonce				(1)
, m_failTimes			(0)
{
}

CKLBNetAPI::~CKLBNetAPI() 
{
	// Done in Die()
}

u32 
CKLBNetAPI::getClassID() 
{
	return CLS_KLBNETAPI;
}

void
CKLBNetAPI::execute(u32 deltaT)
{
	if (!m_http) {
		return; // Do nothing if no active connection
	}

	m_timestart += deltaT;

	// Check cancel first
	if (m_canceled) {
		lua_callback(NETAPIMSG_CONNECTION_CANCELED, -1, NULL);

		NetworkManager::releaseConnection(m_http);
		m_http = NULL;
		// Reset flag
		m_canceled = false;
		return;
	}

	// Received data second
	if (m_http->httpRECV() || (m_http->getHttpState() != -1)) {
		// Get Data
		u8* body	= m_http->getRecvResource();
		u32 bodyLen	= body ? m_http->getSize() : 0;
		
		// Get Status Code
		int state = m_http->getHttpState();
		bool invalid = ((state >= 500) && (state <= 599)) || (state == 204);
		int msg = invalid ? NETAPIMSG_REQUEST_SUCCESS : NETAPIMSG_SERVER_ERROR;
		m_nonce++; // increment after every request

		if (m_http->isMaintenance()) {


			CKLBLuaEnv::getInstance().intoMaintenance();
			return;
		}
		// Release connection
		NetworkManager::releaseConnection(m_http);
		m_http = NULL;

		//
		// Support only JSon for callback
		// 
		freeJSonResult();
		if (bodyLen > 0) m_pRoot = getJsonTree((const char*)body, bodyLen);
		if (m_pRoot == NULL) {
			lua_callback(NETAPIMSG_SERVER_ERROR, state, NULL);

			return;
		}

		if (invalid == false) {
			if (m_lastCommand == NETAPI_STARTUP)
				return startUp(state);
			else if (m_lastCommand == NETAPI_LOGIN)
				return login(state);

			m_failTimes = 0;
		}

		lua_callback(msg, state, m_pRoot);
		return;
	}

	if ((m_http->m_threadStop == 1) && (m_http->getHttpState() == -1)) {
		lua_callback(NETAPIMSG_CONNECTION_FAILED, -1, NULL);
		NetworkManager::releaseConnection(m_http);
		m_http = NULL;
	}

	// Time out third (after check that valid has arrived)
	if (m_timestart >= m_timeout) {
		lua_callback(NETAPIMSG_SERVER_TIMEOUT, -1, NULL);
		NetworkManager::releaseConnection(m_http);
		m_http = NULL;
		return;
	}
}

void
CKLBNetAPI::die()
{
	if (m_http) {
		NetworkManager::releaseConnection(m_http);
	}
	KLBDELETEA(m_callback);
	freeHeader();
	freeJSonResult();
}

void
CKLBNetAPI::freeJSonResult() {
	KLBDELETE(m_pRoot);
}

void
CKLBNetAPI::freeHeader() {
	if (m_http_header_array) {
		for (u32 n=0; n < m_http_header_length; n++) {
			KLBDELETEA(m_http_header_array[n]);
		}
		KLBDELETEA(m_http_header_array);
		m_http_header_array = NULL;
	}
}

CKLBNetAPI* 
CKLBNetAPI::create( CKLBTask* pParentTask, 
                    const char * callback) 
{
	CKLBNetAPI* pTask = KLBNEW(CKLBNetAPI);
    if(!pTask) { return NULL; }

	if(!pTask->init(pParentTask, callback)) {
		KLBDELETE(pTask);
		return NULL;
	}
	return pTask;
}

bool 
CKLBNetAPI::init(	CKLBTask* pTask,
					const char * url) 
{
	//m_callback = (callback) ? CKLBUtility::copyString(callback) : NULL;

	// 一通り初期化値が作れたのでタスクを登録
	bool res = regist(pTask, P_INPUT);
	return res;
}

bool
CKLBNetAPI::initScript(CLuaState& lua)
{
	int argc = lua.numArgs();

    if (argc < 7) { 
		lua.retBoolean(false);
		return false; 
	}
	lua.printStack();
	CKLBNetAPIKeyChain& kc = CKLBNetAPIKeyChain::getInstance();
	kc.setUrl(lua.getString(1));
	kc.setConsumernKey(lua.getString(2));
	kc.setClient(lua.getString(3));
	kc.setAppID(lua.getString(4));
	m_callback = CKLBUtility::copyString(lua.getString(5));
	// arg 6 is max fail before restart
	kc.setRegion(lua.getString(7));

	return init(NULL, lua.getString(ARG_CALLBACK));
}

CKLBJsonItem *
CKLBNetAPI::getJsonTree(const char * json_string, u32 dataLen)
{
	CKLBJsonItem * pRoot = CKLBJsonItem::ReadJsonData((const char *)json_string, dataLen);

	return pRoot;
}

void
CKLBNetAPI::authKey()
{
	CKLBNetAPIKeyChain& kc = CKLBNetAPIKeyChain::getInstance();
	m_http = NetworkManager::createConnection();
	m_http->reuse();

	char requestData[512];
	const char* form[2];
	sprintf(requestData, "request_data={\"dummy_token\":\"%s\", \"auth_data\": \"%s\"}", "1234", "1234");
	form[0] = requestData;
	form[1] = NULL;
	setHeaders(requestData);
	m_http->setForm(form);
	
	char url[MAX_PATH];
	sprintf(url, "%s/login/authkey", kc.getUrl());
	m_http->httpPOST(url, false);
}

void
CKLBNetAPI::login(int state)
{
	CKLBNetAPIKeyChain& kc = CKLBNetAPIKeyChain::getInstance();
	m_http = NetworkManager::createConnection();
	m_http->reuse();

	char requestData[512];
	const char* form[2];
	sprintf(requestData, "request_data={\"login_key\": \"%s\",\"login_passwd\": \"%s\"}", kc.getLoginKey(), kc.getLoginPwd());
	form[0] = requestData;
	form[1] = NULL;
	setHeaders(requestData);
	m_http->setForm(form);

	char URL[MAX_PATH];
	sprintf(URL, "%s/login/login", kc.getUrl());
	m_http->httpPOST(URL, false);
	m_timestart = 0;
}

void
CKLBNetAPI::startUp(int state)
{
	CKLBNetAPIKeyChain& kc = CKLBNetAPIKeyChain::getInstance();
	m_http = NetworkManager::createConnection();
	m_http->reuse();

	char requestData[512];
	const char* form[2];
	sprintf(requestData, "request_data={\"login_key\": \"%s\",\"login_passwd\": \"%s\"}", kc.getLoginKey(), kc.getLoginPwd());
	form[0] = requestData;
	form[1] = NULL;
	setHeaders(requestData);
	m_http->setForm(form);

	char URL[MAX_PATH];
	sprintf(URL, "%s/login/startUp", kc.getUrl());
	m_http->httpPOST(URL, false);
	m_timestart = 0;
}

void
CKLBNetAPI::setHeaders(const char* data)
{
	CPFInterface& pfif = CPFInterface::getInstance();
	CKLBNetAPIKeyChain& kc = CKLBNetAPIKeyChain::getInstance();

	const char* headers[13];
#ifndef _WIN32
	char* osInfo = (char*)pfif.platform().getPlatform();
#else
	char* osInfo = "";
#endif

	// For values above
	char* alldata = new char[1280];
	char* os_data = alldata;
	char* os_version = alldata + 128;
	char* time_zone = alldata + 256;
	char* application_id = time_zone + 128;
	char* authorize = NULL; // Special
	char* bundle_version = time_zone + 256;
	char* client_version = bundle_version + 128;
	char* region = bundle_version + 256;
	char* xmc = region + 64;
	char* user_id = xmc + 128;

	// Process authorize string
	authorize = new char[1024];

	if (data != NULL) {
		char temp[41];
		pfif.platform().HMAC_SHA1(data + 13, kc.getSessionKey(), temp);
		sprintf(xmc, "X-Message-Code: %s", temp);
	}


	sprintf(authorize, "Authorize: %s", kc.getAuthorizeString(m_nonce));
	sprintf(application_id, "Application-ID: %s", kc.getAppID());
	sprintf(bundle_version, "Bundle-Version: %s", pfif.platform().getBundleVersion());
	sprintf(client_version, "Client-Version: %s", kc.getClient());
	sprintf(region, "Region: %s", kc.getRegion());

	// User-ID
	const char* uid = kc.getUserID();

	if (uid == NULL)
		user_id = NULL;
	else
		sprintf(user_id, "User-ID: %s", uid);

	// Set header
	headers[0] = "API-Model: straightforward";
	headers[1] = application_id;
	headers[2] = authorize;
	headers[3] = bundle_version;
	headers[4] = client_version;
	headers[5] = "Debug: 1";
	headers[6] = "OS: Android";
	headers[7] = "OS-Version: Nexus 5 google hammerhead 4.4.4";
	headers[8] = "Platform-Type: 2";
	headers[9] = region;
	headers[10] = xmc;
	headers[11] = user_id;
	headers[12] = NULL;

	m_http->setHeader(headers);

	delete[] authorize;
	delete[] alldata;
}

int
CKLBNetAPI::commandScript(CLuaState& lua)
{
	int argc = lua.numArgs();

	if(argc < 2) {
		lua.retBoolean(false);
		return 1;
	}

	int cmd = m_lastCommand = lua.getInt(2);
	int ret = 1;

	switch(cmd)
	{
	default:
		{
			lua.retBoolean(false);
		}
		break;
	case NETAPI_STARTUP:
		{
			// 3. login_key
			// 4. login_passwd
			// 5. nil
			// 6. timeout
			// 7. sign key
			if (argc < 6) {
				lua.retBool(false);
			}
			const char* login = lua.getString(3);
			const char* pass  = lua.getString(4);
			const char* key	  = lua.getString(7);
 
			// save cred
			CKLBNetAPIKeyChain& kc = CKLBNetAPIKeyChain::getInstance();
			kc.setLoginKey(login);
			kc.setLoginPwd(pass);
			kc.setSessionKey(key);

			m_timeout = lua.getInt(6);
			m_timestart = 0;
			authKey();
			lua.retBoolean(true);
		}
		break;
	case NETAPI_LOGIN:
		{

		}
		break;
	case NETAPI_WATCH_MAINTENANCE: 
		{

		}
		break;
	case NETAPI_DEBUG_HDR:
		{

		}
		break;
	case NETAPI_GEN_CMDNUMID:
		{

		}
		break;
	case NETAPI_CANCEL_ALL:
	case NETAPI_CANCEL:
		{
			if (m_http != NULL) {
				m_canceled = true;
			}
			lua.retBoolean(m_http != NULL);
		}
		break;
	case NETAPI_BUSY:
		{
			// If object is alive, then it is busy.
			lua.retBoolean(m_http != NULL);
		}
		break;
	case NETAPI_SEND:
		{
			//
			// 3. URL
			// 4. Header table
			// 5. POST optionnal
			// 6. Timeout
			//
			if(argc < 3 || argc > 6) {
				lua.retBoolean(false);
			}
			else
			if (m_http) {
				// Connection is still busy,you can not send
				lua.retBoolean(false);
			}
			else {
				const char * api = NULL;
				if(!lua.isNil(3)) api = lua.getString(3);

				// Header list
				const char** headers = NULL;
				const char** values  = NULL;
				freeHeader();

				if(!lua.isNil(4)) {
					// Get the asset list
					lua.retValue(4);

					// Count the number of elements
					int max = 0;
					lua.retNil();

					// Read indexes and count entries.
					while(lua.tableNext()) {
						lua.retValue(-2);
						max++;
						lua.pop(2);
					}
					max++; // Empty at the end.

					int idx=0;
					if (max) {
						m_http_header_array = KLBNEWA(const char*, max);

						if (m_http_header_array==NULL) {
							// Connection is still busy,you can not send
							lua.retBoolean(false);
							ret = 1;
							break;
						}

						// Reset all handle to NULL
						for (idx = 0; idx < max; idx++) {
							m_http_header_array[idx] = NULL;
						}

						lua.retNil();
						idx = 0;
						while(lua.tableNext()) {
							lua.retValue(-2);
							const char * key = lua.getString(-1);
							const char * value = lua.getString(-2);
							lua.pop(2);

							if (key && value) {
								int length = strlen(key) + strlen(value) + 4;
								char* string = KLBNEWA(char, length);
								// Combine string
								sprintf(string, "%s: %s", key, value);

								m_http_header_array[idx++] = string;
							}
						}
					}
					m_http_header_length = idx;
				}

				// POST JSon payload
				const char* send_json = NULL;
				if(!lua.isNil(5)) {
					// POST method
					send_json = lua.getString(5);
				} else {
					// GET method if null json.
				}

				m_http = NetworkManager::createConnection();

				if (m_http) {
					if (m_http_header_array) {
						m_http->setHeader(m_http_header_array);
					}
					if (send_json) {
						char* json;
						const char * items[2];
						const char* req = "request_data=";
						int send_json_length = strlen( send_json );
						int req_length = strlen( req );

						json = KLBNEWA( char , send_json_length+req_length+1 );
						strcpy( json , req );
						strcat( json , send_json );
						items[0] = json;
						items[1] = NULL;

						setHeaders(json);
						m_http->setForm(items);
						m_http->httpPOST(api, false);

						KLBDELETEA(json);
					} else {
						m_http->httpGET(api, false);
					}

					m_timeout	= lua.getInt(6);
					m_timestart = 0;

					lua.retBoolean(true);
				} else {
					// Connection creation failed.
					lua.retBoolean(false);
				}
			}
		}
		break;
	}
	return 1;
}

bool
CKLBNetAPI::lua_callback(int msg, int status, CKLBJsonItem * pRoot)
{
	return CKLBScriptEnv::getInstance().call_netAPI_callback(m_callback, this, m_nonce, msg, status, pRoot);
}
