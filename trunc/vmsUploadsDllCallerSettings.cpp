/*
  Free Download Manager Copyright (c) 2003-2016 FreeDownloadManager.ORG
*/

#include "stdafx.h"
#include "FdmApp.h"
#include "vmsUploadsDllCallerSettings.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

vmsUploadsDllCallerSettings::vmsUploadsDllCallerSettings()
{

}

vmsUploadsDllCallerSettings::~vmsUploadsDllCallerSettings()
{

}

int vmsUploadsDllCallerSettings::InternetAccessType()
{
	return _App.InternetAccessType ();
}

DWORD vmsUploadsDllCallerSettings::FtpFlags()
{
	return _App.FtpFlags ();
}

BOOL vmsUploadsDllCallerSettings::UseHttp11()
{
	return _App.UseHttp11 ();
}

int vmsUploadsDllCallerSettings::FtpTransferType()
{
	return _App.FtpTransferType ();
}

BOOL vmsUploadsDllCallerSettings::UseCookie()
{
	return _App.UseCookie ();
}

LPCTSTR vmsUploadsDllCallerSettings::HttpAgent()
{
	static CString str;
	str = _App.Agent ();
	return str;
}

LPCTSTR vmsUploadsDllCallerSettings::FtpAsciiExts()
{
	static CString str;
	str = _App.ASCIIExts ();
	return str;
}

LPCTSTR vmsUploadsDllCallerSettings::FtpProxy_Name()
{
	static CString str;
	str = _App.FtpProxy_Name ();
	return str;
}

LPCTSTR vmsUploadsDllCallerSettings::FtpProxy_Password()
{
	static CString str;
	str = _App.FtpProxy_Password ();
	return str;
}

LPCTSTR vmsUploadsDllCallerSettings::FtpProxy_UserName()
{
	static CString str;
	str = _App.FtpProxy_UserName ();
	return str;
}

LPCTSTR vmsUploadsDllCallerSettings::HttpProxy_Name()
{
	static CString str;
	str = _App.HttpProxy_Name ();
	return str;
}

LPCTSTR vmsUploadsDllCallerSettings::HttpProxy_Password()
{
	static CString str;
	str = _App.HttpProxy_Password ();
	return str;
}

LPCTSTR vmsUploadsDllCallerSettings::HttpProxy_UserName()
{
	static CString str;
	str = _App.HttpProxy_UserName ();
	return str;
}

LPCTSTR vmsUploadsDllCallerSettings::HttpsProxy_Name()
{
	static CString str;
	str = _App.HttpsProxy_Name ();
	return str;
}

LPCTSTR vmsUploadsDllCallerSettings::HttpsProxy_Password()
{
	static CString str;
	str = _App.HttpsProxy_Password ();
	return str;
}

LPCTSTR vmsUploadsDllCallerSettings::HttpsProxy_UserName()
{
	static CString str;
	str = _App.HttpsProxy_UserName ();
	return str;
}

BOOL vmsUploadsDllCallerSettings::ShowSizesInBytes()
{
	return _pwndDownloads->IsSizesInBytes ();
}

int vmsUploadsDllCallerSettings::FirefoxSettings_Proxy_Type()
{
	return _App.FirefoxSettings_Proxy_Type ();
}

LPCTSTR vmsUploadsDllCallerSettings::FirefoxSettings_Proxy_Addr(LPCTSTR pszProtocol)
{
	static CString str;
	str = _App.FirefoxSettings_Proxy_Addr (pszProtocol);
	return str;
}

int vmsUploadsDllCallerSettings::FirefoxSettings_Proxy_Port(LPCTSTR pszProtocol)
{
	return _App.FirefoxSettings_Proxy_Port (pszProtocol);
}

BOOL vmsUploadsDllCallerSettings::GetSettingsByName(LPCTSTR pszName, LPVOID pData, DWORD dwDataSize)
{
	return FALSE;
}
