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
#ifndef CKLBNetAPI_h
#define CKLBNetAPI_h

#include "CKLBLuaTask.h"
#include "CKLBHTTPInterface.h"
#include "CKLBJsonItem.h"
#include "CKLBUtility.h"
#include "MultithreadedNetwork.h"

// regionを指定しなかった場合のデフォルトregion値(ISO 3166-1)
// #define DEFAULT_REGION "840"    // 北米アメリカ合衆国

//// 指定しそうなデフォルト値をコメントアウトして書いておく。
#define DEFAULT_REGION "392"    // 日本


enum {
	// メッセージ値定義
	NETAPIMSG_CONNECTION_CANCELED = -999,	// セッションはキャンセルされた
	NETAPIMSG_CONNECTION_FAILED = -500,	// 接続に失敗した
	NETAPIMSG_INVITE_FAILED = -300,
	NETAPIMSG_STARTUP_FAILED = -200,
	NETAPIMSG_LOGIN_FAILED = -100,
	NETAPIMSG_SERVER_TIMEOUT = -3,	// サーバとの通信がタイムアウトした
	NETAPIMSG_REQUEST_FAILED = -2,
	NETAPIMSG_SERVER_ERROR = -1,
	NETAPIMSG_UNKNOWN = 0,
	NETAPIMSG_REQUEST_SUCCESS = 2,	// リクエスト成功ステータス
	NETAPIMSG_LOGIN_SUCCESS = 100,
	NETAPIMSG_STARTUP_SUCCESS = 200,
	NETAPIMSG_INVITE_SUCCESS = 300,

};

// Native側からAPIタスクにコマンドを発行するためのsingleton.
// 
class CKLBNetAPI;

/*!
* \class CKLBNetAPI
* \brief Net API class.
* 
* CKLBNetAPI is responsible Network communications.
*/
class CKLBNetAPI : public CKLBLuaTask
{
	friend class CKLBTaskFactory<CKLBNetAPI>;
private:
	CKLBNetAPI();
	virtual ~CKLBNetAPI();

	bool init(	CKLBTask* pTask, 
				const char * callback);
public:
	virtual u32 getClassID();
	static CKLBNetAPI* create(	CKLBTask* pParentTask, 
								const char * callback);
	void execute(u32 deltaT);
	void die();

	bool initScript(CLuaState& lua);
	int commandScript(CLuaState& lua);
private:
	CKLBHTTPInterface*		m_http;		// そのセッションで使用されている接続
	u32						m_timeout;	// タイムアウト時間
	u32						m_timestart;
	u32						m_http_header_length;
	u32						m_nonce;
	u32						m_failTimes;
	u32						m_lastCommand;
	bool					m_canceled;	// セッションがキャンセルされると true になる
	CKLBJsonItem*			m_pRoot;

	// スクリプトコールバック用
	const char			*	m_callback;	// Lua callback function

	// HTTP通信で追加するヘッダの配列
	const char			**	m_http_header_array;

private:
	void freeHeader();
	void freeJSonResult();
	void setHeaders(const char* data);

	bool lua_callback(int msg, int status, CKLBJsonItem * pRoot);

	CKLBJsonItem * getJsonTree(const char * json_string, u32 dataLen);
	bool get_token(CKLBJsonItem * pRoot);

	void authKey();
	void login(int state);
	void startUp(int state);

public:
	bool cancel		();
};

#endif // CKLBNetAPI_h
