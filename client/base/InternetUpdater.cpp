#include "InternetUpdater.h"
#include "PlatformMacro.h"
#include "../../general/base/GeneralVariable.h"
#include "../../general/base/Version.h"
#include "../../general/base/cpp11addition.h"
#include "ClientVariable.h"
#include "Ultimate.h"

#include <QNetworkRequest>
#include <QUrl>
#include <QRegularExpression>

#if defined(__unix__) || defined(Q_OS_UNIX) || !defined(Q_OS_WIN32)
    #include <unistd.h>
    #include <sys/types.h>
#endif
#if defined(_WIN32) || defined(Q_OS_WIN32)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #include <tchar.h>
    #include <stdio.h>
    #include <strsafe.h>
    typedef void (WINAPI *PGNSI) (LPSYSTEM_INFO);
    typedef BOOL (WINAPI *PGPI) (DWORD, DWORD, DWORD, DWORD, PDWORD);
#endif
#ifdef Q_OS_MAC
#include <QStringList>
#include <QFile>
#endif

InternetUpdater *InternetUpdater::internetUpdater=NULL;

InternetUpdater::InternetUpdater()
{
    if(!connect(&newUpdateTimer,&QTimer::timeout,this,&InternetUpdater::downloadFile))
        abort();
    if(!connect(&firstUpdateTimer,&QTimer::timeout,this,&InternetUpdater::downloadFile))
        abort();
    newUpdateTimer.start(60*60*1000);
    firstUpdateTimer.setSingleShot(true);
    firstUpdateTimer.start(5);
}

void InternetUpdater::downloadFile()
{
    QString catchChallengerVersion;
    if(Ultimate::ultimate.isUltimate())
        catchChallengerVersion=QStringLiteral("CatchChallenger Ultimate/%1").arg(CATCHCHALLENGER_VERSION);
    else
        catchChallengerVersion=QStringLiteral("CatchChallenger/%1").arg(CATCHCHALLENGER_VERSION);
    #if defined(_WIN32) || defined(Q_OS_MAC)
    catchChallengerVersion+=QStringLiteral(" (OS: %1)").arg(QString::fromStdString(GetOSDisplayString()));
    #endif
    catchChallengerVersion+=QStringLiteral(" ")+CATCHCHALLENGER_PLATFORM_CODE;
    QNetworkRequest networkRequest(QStringLiteral(CATCHCHALLENGER_UPDATER_URL));
    networkRequest.setHeader(QNetworkRequest::UserAgentHeader,catchChallengerVersion);
    networkRequest.setRawHeader("Connection", "Close");
    reply = qnam.get(networkRequest);
    if(!connect(reply, &QNetworkReply::finished, this, &InternetUpdater::httpFinished))
        abort();
}

void InternetUpdater::httpFinished()
{
    QVariant redirectionTarget = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    if (!reply->isFinished())
    {
        qDebug() << (QStringLiteral("get the new update failed: not finished"));
        reply->deleteLater();
        return;
    }
    else if(reply->error())
    {
        newUpdateTimer.stop();
        newUpdateTimer.start(24*60*60*1000);
        qDebug() << (QStringLiteral("get the new update failed: %1").arg(reply->errorString()));
        reply->deleteLater();
        return;
    } else if (!redirectionTarget.isNull()) {
        qDebug() << (QStringLiteral("redirection denied to: %1").arg(redirectionTarget.toUrl().toString()));
        reply->deleteLater();
        return;
    }
    std::string newVersion=QString::fromUtf8(reply->readAll()).toStdString();
    if(newVersion.empty())
    {
        qDebug() << (QStringLiteral("version string is empty"));
        reply->deleteLater();
        return;
    }
    stringreplaceAll(newVersion,"\n","");
    if(!QString::fromStdString(newVersion).contains(QRegularExpression(QStringLiteral("^[0-9]+(\\.[0-9]+)+$"))))
    {
        qDebug() << (QStringLiteral("version string don't match: %1").arg(QString::fromStdString(newVersion)));
        reply->deleteLater();
        return;
    }
    if(newVersion==CATCHCHALLENGER_VERSION)
    {
        reply->deleteLater();
        return;
    }
    if(!versionIsNewer(newVersion))
    {
        reply->deleteLater();
        return;
    }
    newUpdateTimer.stop();
    emit newUpdate(newVersion);
    reply->deleteLater();
}

bool InternetUpdater::versionIsNewer(const std::string &version)
{
    std::vector<std::string> versionANumber=stringsplit(version,'.');
    std::vector<std::string> versionBNumber=stringsplit(std::string(CATCHCHALLENGER_VERSION),'.');
    unsigned int index=0;
    int defaultReturnValue=true;
    while(index<versionANumber.size() && index<versionBNumber.size())
    {
        unsigned int reaNumberA=stringtoint32(versionANumber.at(index));
        unsigned int reaNumberB=stringtoint32(versionBNumber.at(index));
        if(reaNumberA>reaNumberB)
            return true;
        if(reaNumberA<reaNumberB)
            return false;
        index++;
    }
    return defaultReturnValue;
}

std::string InternetUpdater::getText(const std::string &version)
{
    std::string url="http://catchchallenger.first-world.info/download.html";
    return QStringLiteral("<a href=\"%1\" style=\"text-decoration:none;color:#100;\">%2</a>")
            .arg(QString::fromStdString(url))
            .arg(tr("New version: %1").arg("<b>"+QString::fromStdString(version)+"</b>"))
            .toStdString()+"<br />"+tr("Click here to go on download page").toStdString();
}

#ifdef Q_OS_WIN32
std::string InternetUpdater::GetOSDisplayString()
{
   QString Os;
   OSVERSIONINFOEX osvi;
   SYSTEM_INFO si;
   PGNSI pGNSI;
   PGPI pGPI;
   BOOL bOsVersionInfoEx;
   DWORD dwType;

   ZeroMemory(&si, sizeof(SYSTEM_INFO));
   ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));

   osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
   bOsVersionInfoEx = GetVersionEx((OSVERSIONINFO*) &osvi);

   if(bOsVersionInfoEx == 0)
        return "Os detection blocked";

   // Call GetNativeSystemInfo if supported or GetSystemInfo otherwise.

   pGNSI = (PGNSI) GetProcAddress(
      GetModuleHandle(TEXT("kernel32.dll")),
      "GetNativeSystemInfo");
   if(NULL != pGNSI)
      pGNSI(&si);
   else GetSystemInfo(&si);

   if(VER_PLATFORM_WIN32_NT==osvi.dwPlatformId && osvi.dwMajorVersion>4)
   {
      if(osvi.dwMajorVersion==6)
      {
          switch(osvi.dwMinorVersion)
          {
            case 0:
                if(osvi.wProductType==VER_NT_WORKSTATION)
                    Os+=QStringLiteral("Windows Vista ");
                else Os+=QStringLiteral("Windows Server 2008 ");
            break;
            case 1:
                if(osvi.wProductType==VER_NT_WORKSTATION)
                    Os+=QStringLiteral("Windows 7 ");
                else Os+=QStringLiteral("Windows Server 2008 R2 ");
            break;
            case 2:
                if(osvi.wProductType==VER_NT_WORKSTATION)
                    Os+=QStringLiteral("Windows 8 ");
                else Os+=QStringLiteral("Windows Server 2012 ");
            break;
            default:
                 if(osvi.wProductType==VER_NT_WORKSTATION)
                    Os+=QStringLiteral("Windows (dwMajorVersion: %1, dwMinorVersion: %2)").arg(osvi.dwMinorVersion).arg(osvi.dwMinorVersion);
                 else Os+=QStringLiteral("Windows Server (dwMajorVersion: %1, dwMinorVersion: %2)").arg(osvi.dwMinorVersion).arg(osvi.dwMinorVersion);
            break;
          }

         pGPI = (PGPI) GetProcAddress(
            GetModuleHandle(TEXT("kernel32.dll")),
            "GetProductInfo");

         pGPI(osvi.dwMajorVersion, osvi.dwMinorVersion, 0, 0, &dwType);

         switch(dwType)
         {
            case PRODUCT_ULTIMATE:
               Os+=QStringLiteral("Ultimate Edition");
               break;
            case PRODUCT_PROFESSIONAL:
               Os+=QStringLiteral("Professional");
               break;
            case PRODUCT_HOME_PREMIUM:
               Os+=QStringLiteral("Home Premium Edition");
               break;
            case PRODUCT_HOME_BASIC:
               Os+=QStringLiteral("Home Basic Edition");
               break;
            case PRODUCT_ENTERPRISE:
               Os+=QStringLiteral("Enterprise Edition");
               break;
            case PRODUCT_BUSINESS:
               Os+=QStringLiteral("Business Edition");
               break;
            case PRODUCT_STARTER:
               Os+=QStringLiteral("Starter Edition");
               break;
            case PRODUCT_CLUSTER_SERVER:
               Os+=QStringLiteral("Cluster Server Edition");
               break;
            case PRODUCT_DATACENTER_SERVER:
               Os+=QStringLiteral("Datacenter Edition");
               break;
            case PRODUCT_DATACENTER_SERVER_CORE:
               Os+=QStringLiteral("Datacenter Edition (core installation)");
               break;
            case PRODUCT_ENTERPRISE_SERVER:
               Os+=QStringLiteral("Enterprise Edition");
               break;
            case PRODUCT_ENTERPRISE_SERVER_CORE:
               Os+=QStringLiteral("Enterprise Edition (core installation)");
               break;
            case PRODUCT_ENTERPRISE_SERVER_IA64:
               Os+=QStringLiteral("Enterprise Edition for Itanium-based Systems");
               break;
            case PRODUCT_SMALLBUSINESS_SERVER:
               Os+=QStringLiteral("Small Business Server");
               break;
            case PRODUCT_SMALLBUSINESS_SERVER_PREMIUM:
               Os+=QStringLiteral("Small Business Server Premium Edition");
               break;
            case PRODUCT_STANDARD_SERVER:
               Os+=QStringLiteral("Standard Edition");
               break;
            case PRODUCT_STANDARD_SERVER_CORE:
               Os+=QStringLiteral("Standard Edition (core installation)");
               break;
            case PRODUCT_WEB_SERVER:
               Os+=QStringLiteral("Web Server Edition");
               break;
         }
      }
      else if(osvi.dwMajorVersion==5)
      {
            switch(osvi.dwMinorVersion)
            {
                case 0:
                    Os+=QStringLiteral("Windows 2000 ");
                    if(osvi.wProductType==VER_NT_WORKSTATION)
                       Os+=QStringLiteral("Professional");
                    else
                    {
                       if(osvi.wSuiteMask & VER_SUITE_DATACENTER)
                          Os+=QStringLiteral("Datacenter Server");
                       else if(osvi.wSuiteMask & VER_SUITE_ENTERPRISE)
                          Os+=QStringLiteral("Advanced Server");
                       else Os+=QStringLiteral("Server");
                    }
                break;
                case 1:
                    Os+=QStringLiteral("Windows XP ");
                    if(osvi.wSuiteMask & VER_SUITE_PERSONAL)
                       Os+=QStringLiteral("Home Edition");
                    else Os+=QStringLiteral("Professional");
                break;
                case 2:
                    if(GetSystemMetrics(SM_SERVERR2))
                        Os+=QStringLiteral("Windows Server 2003 R2, ");
                    else if(osvi.wSuiteMask & VER_SUITE_STORAGE_SERVER )
                        Os+=QStringLiteral("Windows Storage Server 2003");
                    else if(osvi.wSuiteMask & VER_SUITE_WH_SERVER )
                        Os+=QStringLiteral("Windows Home Server");
                    else if(osvi.wProductType==VER_NT_WORKSTATION && si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64)
                        Os+=QStringLiteral("Windows XP Professional x64 Edition");
                    else Os+=QStringLiteral("Windows Server 2003, ");
                    // Test for the server type.
                    if(osvi.wProductType!=VER_NT_WORKSTATION )
                    {
                        if(si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_IA64)
                        {
                            if( osvi.wSuiteMask & VER_SUITE_DATACENTER )
                                Os+=QStringLiteral("Datacenter Edition for Itanium-based Systems");
                            else if( osvi.wSuiteMask & VER_SUITE_ENTERPRISE )
                                Os+=QStringLiteral("Enterprise Edition for Itanium-based Systems");
                        }
                        else if(si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64)
                        {
                            if(osvi.wSuiteMask & VER_SUITE_DATACENTER)
                                Os+=QStringLiteral("Datacenter x64 Edition");
                            else if(osvi.wSuiteMask & VER_SUITE_ENTERPRISE)
                                Os+=QStringLiteral("Enterprise x64 Edition");
                            else Os+=QStringLiteral("Standard x64 Edition");
                        }
                        else
                        {
                            if(osvi.wSuiteMask & VER_SUITE_COMPUTE_SERVER)
                                Os+=QStringLiteral("Compute Cluster Edition");
                            else if( osvi.wSuiteMask & VER_SUITE_DATACENTER)
                                Os+=QStringLiteral("Datacenter Edition");
                            else if(osvi.wSuiteMask & VER_SUITE_ENTERPRISE)
                                Os+=QStringLiteral("Enterprise Edition");
                            else if(osvi.wSuiteMask & VER_SUITE_BLADE)
                                Os+=QStringLiteral("Web Edition");
                            else Os+=QStringLiteral("Standard Edition");
                        }
                    }
                break;
            }
        }
        else
        {
            if(osvi.wProductType==VER_NT_WORKSTATION)
                Os+=QStringLiteral("Windows (dwMajorVersion: %1, dwMinorVersion: %2)").arg(osvi.dwMinorVersion).arg(osvi.dwMinorVersion);
            else Os+=QStringLiteral("Windows Server (dwMajorVersion: %1, dwMinorVersion: %2)").arg(osvi.dwMinorVersion).arg(osvi.dwMinorVersion);
        }

        // Include service pack (if any) and build number.
        QString QszCSDVersion=QString::fromUtf16((ushort*)osvi.szCSDVersion);
        if(!QszCSDVersion.isEmpty())
            Os+=QStringLiteral(" %1").arg(QszCSDVersion);
        Os+=QStringLiteral(" (build %1)").arg(osvi.dwBuildNumber);
        if(osvi.dwMajorVersion >= 6)
        {
            if(si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64)
                Os+=QStringLiteral(", 64-bit");
            else if(si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_INTEL)
                Os+=QStringLiteral(", 32-bit");
        }
    }
    else
    {
       if(osvi.wProductType==VER_NT_WORKSTATION)
           Os+=QStringLiteral("Windows (dwMajorVersion: %1, dwMinorVersion: %2)").arg(osvi.dwMinorVersion).arg(osvi.dwMinorVersion);
       else Os+=QStringLiteral("Windows Server (dwMajorVersion: %1, dwMinorVersion: %2)").arg(osvi.dwMinorVersion).arg(osvi.dwMinorVersion);
    }
    return Os.toStdString();
}
#endif

#ifdef Q_OS_MAC
std::string InternetUpdater::GetOSDisplayString()
{
        QStringList key;
    QStringList string;
    QFile xmlFile(QStringLiteral("/System/Library/CoreServices/SystemVersion.plist"));
    if(xmlFile.open(QIODevice::ReadOnly))
    {
        QString content=xmlFile.readAll();
        xmlFile.close();
        QString errorStr;
        int errorLine;
        int errorColumn;
        QDomDocument domDocument;
        if (!domDocument.setContent(content, false, &errorStr,&errorLine,&errorColumn))
            return "Mac OS X";
        else
        {
            QDomElement root = domDocument.documentElement();
            if(root.tagName()!=QStringLiteral("plist"))
                return "Mac OS X";
            else
            {
                if(root.isElement())
                {
                    QDomElement SubChild=root.firstChildElement(QStringLiteral("dict"));
                    while(!SubChild.isNull())
                    {
                        if(SubChild.isElement())
                        {
                            QDomElement SubChild2=SubChild.firstChildElement(QStringLiteral("key"));
                            while(!SubChild2.isNull())
                            {
                                if(SubChild2.isElement())
                                    key << SubChild2.text();
                                else
                                    return "Mac OS X";
                                SubChild2 = SubChild2.nextSiblingElement(QStringLiteral("key"));
                            }
                            SubChild2=SubChild.firstChildElement(QStringLiteral("string"));
                            while(!SubChild2.isNull())
                            {
                                if(SubChild2.isElement())
                                    string << SubChild2.text();
                                else
                                    return "Mac OS X";
                                SubChild2 = SubChild2.nextSiblingElement(QStringLiteral("string"));
                            }
                        }
                        else
                            return "Mac OS X";
                        SubChild = SubChild.nextSiblingElement(QStringLiteral("property"));
                    }
                }
                else
                    return "Mac OS X";
            }
        }
    }
    if(key.size()!=string.size())
        return "Mac OS X";
    int index=0;
    while(index<key.size())
    {
        if(key.at(index)==QStringLiteral("ProductVersion"))
            return "Mac OS X "+string.at(index).toStdString();
        index++;
    }
    return "Mac OS X";
}
#endif
