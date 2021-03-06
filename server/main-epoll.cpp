#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include "base/ServerStructures.h"
#include "base/TinyXMLSettings.h"
#include "base/GlobalServerData.h"
#include "base/ClientMapManagement/MapVisibilityAlgorithm_None.h"
#include "base/ClientMapManagement/MapVisibilityAlgorithm_Simple_StoreOnSender.h"
#include "base/ClientMapManagement/MapVisibilityAlgorithm_WithBorder_StoreOnSender.h"
#include "../general/base/tinyXML2/tinyxml2.h"
#include "../general/base/CommonSettingsCommon.h"
#include "epoll/EpollServer.h"
#include "epoll/Epoll.h"
#include "NormalServerGlobal.h"
#ifdef CATCHCHALLENGER_CLASS_ONLYGAMESERVER
#include "game-server-alone/LinkToMaster.h"
#include "epoll/EpollSocket.h"
#include "epoll/timer/TimerPurgeTokenAuthList.h"
#endif

#define MAXEVENTS 512

using namespace CatchChallenger;

#ifdef SERVERSSL
EpollSslServer *server=NULL;
#else
EpollServer *server=NULL;
#endif
TinyXMLSettings *settings=NULL;

std::string master_host;
uint16_t master_port;
uint8_t master_tryInterval;
uint8_t master_considerDownAfterNumberOfTry;

#ifndef CATCHCHALLENGER_CLASS_ONLYGAMESERVER
void generateTokenStatClient(TinyXMLSettings &settings,char * const data)
{
    FILE *fpRandomFile = fopen(RANDOMFILEDEVICE,"rb");
    if(fpRandomFile==NULL)
    {
        std::cerr << "Unable to open " << RANDOMFILEDEVICE << " to generate random token" << std::endl;
        abort();
    }
    const size_t &returnedSize=fread(data,1,TOKEN_SIZE_FOR_CLIENT_AUTH_AT_CONNECT,fpRandomFile);
    if(returnedSize!=TOKEN_SIZE_FOR_CLIENT_AUTH_AT_CONNECT)
    {
        std::cerr << "Unable to read the " << TOKEN_SIZE_FOR_CLIENT_AUTH_AT_CONNECT << " needed to do the token from " << RANDOMFILEDEVICE << std::endl;
        abort();
    }
    settings.setValue("token",binarytoHexa(reinterpret_cast<char *>(data)
                                           ,TOKEN_SIZE_FOR_CLIENT_AUTH_AT_CONNECT).c_str());
    fclose(fpRandomFile);
    settings.sync();
}
#endif

std::vector<void *> elementsToDelete[16];
size_t elementsToDeleteSize=0;
uint8_t elementsToDeleteIndex=0;

/* Catch Signal Handler functio */
void signal_callback_handler(int signum){

        printf("Caught signal SIGPIPE %d\n",signum);
}

void CatchChallenger::recordDisconnectByServer(void * client)
{
    unsigned int mainIndex=0;
    while(mainIndex<16)
    {
        const std::vector<void *> &elementsToDeleteSub=elementsToDelete[mainIndex];
        if(!elementsToDeleteSub.empty())
        {
            unsigned int index=0;
            while(index<elementsToDeleteSub.size())
            {
                if(elementsToDeleteSub.at(index)==client)
                    return;
                index++;
            }
        }
        mainIndex++;
    }
    elementsToDelete[elementsToDeleteIndex].push_back(client);
    elementsToDeleteSize++;
}

void send_settings(
        #ifdef SERVERSSL
        EpollSslServer *server
        #else
        EpollServer *server
        #endif
        ,TinyXMLSettings *settings,
        std::string &master_host,
        uint16_t &master_port,
        uint8_t &master_tryInterval,
        uint8_t &master_considerDownAfterNumberOfTry
        );

int main(int argc, char *argv[])
{
    /* Catch Signal Handler SIGPIPE */
    if(signal(SIGPIPE, signal_callback_handler)==SIG_ERR)
    {
        std::cerr << "signal(SIGPIPE, signal_callback_handler)==SIG_ERR, errno: " << std::to_string(errno) << std::endl;
        abort();
    }

    NormalServerGlobal::displayInfo();
    if(argc<1)
    {
        std::cerr << "argc<1: wrong arg count" << std::endl;
        return EXIT_FAILURE;
    }
    CatchChallenger::FacilityLibGeneral::applicationDirPath=argv[0];

    srand(static_cast<unsigned int>(time(NULL)));

    bool datapack_loaded=false;

    if(!CatchChallenger::FacilityLibGeneral::isFile(FacilityLibGeneral::getFolderFromFile(CatchChallenger::FacilityLibGeneral::applicationDirPath)+"/datapack/informations.xml"))
    {
        std::cerr << "No datapack found into: " << FacilityLibGeneral::getFolderFromFile(CatchChallenger::FacilityLibGeneral::applicationDirPath) << "/datapack/" << std::endl;
        return EXIT_FAILURE;
    }

    settings=new TinyXMLSettings(FacilityLibGeneral::getFolderFromFile(CatchChallenger::FacilityLibGeneral::applicationDirPath)+"/server-properties.xml");
    NormalServerGlobal::checkSettingsFile(settings,FacilityLibGeneral::getFolderFromFile(CatchChallenger::FacilityLibGeneral::applicationDirPath)+"/datapack/");

    if(!Epoll::epoll.init())
        return EPOLLERR;

    #ifdef SERVERSSL
    server=new EpollSslServer();
    #else
    server=new EpollServer();
    #endif

    //before linkToMaster->registerGameServer() to have the correct settings loaded
    //after server to have the settings
    send_settings(server,settings,master_host,master_port,master_tryInterval,master_considerDownAfterNumberOfTry);

    #ifdef CATCHCHALLENGER_CLASS_ONLYGAMESERVER
    {
        const int &linkfd=LinkToMaster::tryConnect(
                    master_host.c_str(),
                    master_port,
                    master_tryInterval,
                    master_considerDownAfterNumberOfTry
                    );
        if(linkfd<0)
        {
            std::cerr << "Unable to connect on master" << std::endl;
            abort();
        }
        #ifdef SERVERSSL
        ctx from what?
        LoginLinkToMaster::loginLinkToMaster=new LoginLinkToMaster(linkfd,ctx);
        #else
        LinkToMaster::linkToMaster=new LinkToMaster(linkfd);
        #endif
        LinkToMaster::linkToMaster->stat=LinkToMaster::Stat::Connected;
        LinkToMaster::linkToMaster->setSettings(settings);
        LinkToMaster::linkToMaster->readTheFirstSslHeader();
        LinkToMaster::linkToMaster->setConnexionSettings(master_tryInterval,master_considerDownAfterNumberOfTry);
        const int &s = EpollSocket::make_non_blocking(linkfd);
        if(s == -1)
            std::cerr << "unable to make to socket non blocking" << std::endl;

        LinkToMaster::linkToMaster->baseServer=server;
    }
    #endif

    bool tcpCork,tcpNodelay;
    {
        const GameServerSettings &formatedServerSettings=server->getSettings();
        const NormalServerSettings &formatedServerNormalSettings=server->getNormalSettings();
        tcpCork=CommonSettingsServer::commonSettingsServer.tcpCork;
        tcpNodelay=formatedServerNormalSettings.tcpNodelay;

        if(!formatedServerNormalSettings.proxy.empty())
        {
            std::cerr << "Proxy not supported: " << formatedServerNormalSettings.proxy << std::endl;
            return EXIT_FAILURE;
        }
        if(!formatedServerNormalSettings.proxy.empty())
        {
            std::cerr << "Proxy not supported" << std::endl;
            return EXIT_FAILURE;
        }
        #ifdef CATCHCHALLENGER_GAMESERVER_PLANTBYPLAYER
        if(!CommonSettingsServer::commonSettingsServer.plantOnlyVisibleByPlayer)
        {
            std::cerr << "plantOnlyVisibleByPlayer at false but server compiled with plantOnlyVisibleByPlayer at true, recompil to change this options" << std::endl;
            return EXIT_FAILURE;
        }
        #else
        if(CommonSettingsServer::commonSettingsServer.plantOnlyVisibleByPlayer)
        {
            qDebug() << "plantOnlyVisibleByPlayer at true but server compiled with plantOnlyVisibleByPlayer at false, recompil to change this options";
            return EXIT_FAILURE;
        }
        #endif

        {
            #ifndef CATCHCHALLENGER_CLASS_ONLYGAMESERVER
            if(
                    true
                    #ifdef CATCHCHALLENGER_DB_POSTGRESQL
                    && formatedServerSettings.database_login.tryOpenType!=DatabaseBase::DatabaseType::PostgreSQL
                    #endif
                    #ifdef CATCHCHALLENGER_DB_MYSQL
                    && formatedServerSettings.database_login.tryOpenType!=DatabaseBase::DatabaseType::Mysql
                    #endif
                    )
            {
                settings->beginGroup("db-login");
                std::cerr << "Database type not supported for now: " << settings->value("type") << std::endl;
                settings->endGroup();
                return EXIT_FAILURE;
            }
            if(
                    true
                    #ifdef CATCHCHALLENGER_DB_POSTGRESQL
                    && formatedServerSettings.database_base.tryOpenType!=DatabaseBase::DatabaseType::PostgreSQL
                    #endif
                    #ifdef CATCHCHALLENGER_DB_MYSQL
                    && formatedServerSettings.database_base.tryOpenType!=DatabaseBase::DatabaseType::Mysql
                    #endif
                    )
            {
                settings->beginGroup("db-base");
                std::cerr << "Database type not supported for now: " << settings->value("type") << std::endl;
                settings->endGroup();
                return EXIT_FAILURE;
            }
            #endif
            if(
                    true
                    #ifdef CATCHCHALLENGER_DB_POSTGRESQL
                    && formatedServerSettings.database_common.tryOpenType!=DatabaseBase::DatabaseType::PostgreSQL
                    #endif
                    #ifdef CATCHCHALLENGER_DB_MYSQL
                    && formatedServerSettings.database_common.tryOpenType!=DatabaseBase::DatabaseType::Mysql
                    #endif
                    )
            {
                settings->beginGroup("db-common");
                std::cerr << "Database type not supported for now: " << settings->value("type") << std::endl;
                settings->endGroup();
                return EXIT_FAILURE;
            }
            if(
                    true
                    #ifdef CATCHCHALLENGER_DB_POSTGRESQL
                    && formatedServerSettings.database_server.tryOpenType!=DatabaseBase::DatabaseType::PostgreSQL
                    #endif
                    #ifdef CATCHCHALLENGER_DB_MYSQL
                    && formatedServerSettings.database_server.tryOpenType!=DatabaseBase::DatabaseType::Mysql
                    #endif
                    )
            {
                settings->beginGroup("db-server");
                std::cerr << "Database type not supported for now: " << settings->value("type") << std::endl;
                settings->endGroup();
                return EXIT_FAILURE;
            }
        }


        #ifdef SERVERSSL
        if(!formatedServerNormalSettings.useSsl)
        {
            qDebug() << "Ssl connexion requested but server not compiled with ssl support!";
            return EXIT_FAILURE;
        }
        #else
        if(formatedServerNormalSettings.useSsl)
        {
            std::cerr << "Clear connexion requested but server compiled with ssl support!" << std::endl;
            return EXIT_FAILURE;
        }
        #endif
        if(CommonSettingsCommon::commonSettingsCommon.httpDatapackMirrorBase.empty())
        {
            #ifdef CATCHCHALLENGERSERVERBLOCKCLIENTTOSERVERPACKETDECOMPRESSION
            qDebug() << "Need mirror because CATCHCHALLENGERSERVERBLOCKCLIENTTOSERVERPACKETDECOMPRESSION is def, need decompression to datapack list input";
            return EXIT_FAILURE;
            #endif
            #ifdef CATCHCHALLENGER_SERVER_DATAPACK_ONLYBYMIRROR
            std::cerr << "CATCHCHALLENGER_SERVER_DATAPACK_ONLYBYMIRROR defined, httpDatapackMirrorBase can't be empty" << std::endl;
            return EXIT_FAILURE;
            #endif
        }
        else
        {
            std::vector<std::string> newMirrorList;
            std::regex httpMatch("^https?://.+$");
            const std::vector<std::string> &mirrorList=stringsplit(CommonSettingsServer::commonSettingsServer.httpDatapackMirrorServer,';');
            unsigned int index=0;
            while(index<mirrorList.size())
            {
                const std::string &mirror=mirrorList.at(index);
                if(!regex_search(mirror,httpMatch))
                {
                    std::cerr << "Mirror wrong: " << mirror << std::endl;
                    return EXIT_FAILURE;
                }
                if(stringEndsWith(mirror,'/'))
                    newMirrorList.push_back(mirror);
                else
                    newMirrorList.push_back(mirror+'/');
                index++;
            }
            CommonSettingsServer::commonSettingsServer.httpDatapackMirrorServer=stringimplode(newMirrorList,';');
            CommonSettingsCommon::commonSettingsCommon.httpDatapackMirrorBase=CommonSettingsServer::commonSettingsServer.httpDatapackMirrorServer;
        }
    }
    server->initTheDatabase();
    server->loadAndFixSettings();

    #ifndef CATCHCHALLENGER_CLASS_ONLYGAMESERVER
    switch(GlobalServerData::serverSettings.database_login.tryOpenType)
    {
        #ifdef CATCHCHALLENGER_DB_POSTGRESQL
        case DatabaseBase::DatabaseType::PostgreSQL:
            static_cast<EpollPostgresql *>(GlobalServerData::serverPrivateVariables.db_login)->setMaxDbQueries(2000000000);
        break;
        #endif
        default:
        break;
    }
    switch(GlobalServerData::serverSettings.database_base.tryOpenType)
    {
        #ifdef CATCHCHALLENGER_DB_POSTGRESQL
        case DatabaseBase::DatabaseType::PostgreSQL:
            static_cast<EpollPostgresql *>(GlobalServerData::serverPrivateVariables.db_base)->setMaxDbQueries(2000000000);
        break;
        #endif
        default:
        break;
    }
    #endif
    switch(GlobalServerData::serverSettings.database_common.tryOpenType)
    {
        #ifdef CATCHCHALLENGER_DB_POSTGRESQL
        case DatabaseBase::DatabaseType::PostgreSQL:
            static_cast<EpollPostgresql *>(GlobalServerData::serverPrivateVariables.db_common)->setMaxDbQueries(2000000000);
        break;
        #endif
        default:
        break;
    }
    switch(GlobalServerData::serverSettings.database_server.tryOpenType)
    {
        #ifdef CATCHCHALLENGER_DB_POSTGRESQL
        case DatabaseBase::DatabaseType::PostgreSQL:
            static_cast<EpollPostgresql *>(GlobalServerData::serverPrivateVariables.db_server)->setMaxDbQueries(2000000000);
        break;
        #endif
        default:
        break;
    }

    #ifndef CATCHCHALLENGER_CLASS_ONLYGAMESERVER
    if(!GlobalServerData::serverPrivateVariables.db_login->syncConnect(
                GlobalServerData::serverSettings.database_login.host,
                GlobalServerData::serverSettings.database_login.db,
                GlobalServerData::serverSettings.database_login.login,
                GlobalServerData::serverSettings.database_login.pass))
    {
        std::cerr << "Unable to connect to database login:" << GlobalServerData::serverPrivateVariables.db_login->errorMessage() << std::endl;
        return EXIT_FAILURE;
    }
    if(!GlobalServerData::serverPrivateVariables.db_base->syncConnect(
                GlobalServerData::serverSettings.database_base.host,
                GlobalServerData::serverSettings.database_base.db,
                GlobalServerData::serverSettings.database_base.login,
                GlobalServerData::serverSettings.database_base.pass))
    {
        std::cerr << "Unable to connect to database base:" << GlobalServerData::serverPrivateVariables.db_base->errorMessage() << std::endl;
        return EXIT_FAILURE;
    }
    #endif
    if(!GlobalServerData::serverPrivateVariables.db_common->syncConnect(
                GlobalServerData::serverSettings.database_common.host,
                GlobalServerData::serverSettings.database_common.db,
                GlobalServerData::serverSettings.database_common.login,
                GlobalServerData::serverSettings.database_common.pass))
    {
        std::cerr << "Unable to connect to database common:" << GlobalServerData::serverPrivateVariables.db_common->errorMessage() << std::endl;
        return EXIT_FAILURE;
    }
    if(!GlobalServerData::serverPrivateVariables.db_server->syncConnect(
                GlobalServerData::serverSettings.database_server.host,
                GlobalServerData::serverSettings.database_server.db,
                GlobalServerData::serverSettings.database_server.login,
                GlobalServerData::serverSettings.database_server.pass))
    {
        std::cerr << "Unable to connect to database server:" << GlobalServerData::serverPrivateVariables.db_server->errorMessage() << std::endl;
        return EXIT_FAILURE;
    }
    server->initialize_the_database_prepared_query();

    TimerCityCapture timerCityCapture;
    TimerDdos timerDdos;
    TimerPositionSync timerPositionSync;
    TimerSendInsertMoveRemove timerSendInsertMoveRemove;
    #ifdef CATCHCHALLENGER_CLASS_ONLYGAMESERVER
    TimerPurgeTokenAuthList timerPurgeTokenAuthList;
    #endif
    #ifndef EPOLLCATCHCHALLENGERSERVER
    {
        GlobalServerData::serverPrivateVariables.time_city_capture=FacilityLib::nextCaptureTime(GlobalServerData::serverSettings.city);
        const int64_t &time=GlobalServerData::serverPrivateVariables.time_city_capture-sFrom1970();
        timerCityCapture.setSingleShot(true);
        if(!timerCityCapture.start(time))
        {
            std::cerr << "timerCityCapture fail to set" << std::endl;
            return EXIT_FAILURE;
        }
    }
    #endif
    {
        if(!timerDdos.start(GlobalServerData::serverSettings.ddos.computeAverageValueTimeInterval*1000))
        {
            std::cerr << "timerDdos fail to set" << std::endl;
            return EXIT_FAILURE;
        }
    }
    {
        if(GlobalServerData::serverSettings.secondToPositionSync>0)
            if(!timerPositionSync.start(GlobalServerData::serverSettings.secondToPositionSync*1000))
            {
                std::cerr << "timerPositionSync fail to set" << std::endl;
                return EXIT_FAILURE;
            }
    }
    {
        if(!timerSendInsertMoveRemove.start(CATCHCHALLENGER_SERVER_MAP_TIME_TO_SEND_MOVEMENT))
        {
            std::cerr << "timerSendInsertMoveRemove fail to set" << std::endl;
            return EXIT_FAILURE;
        }
    }
    {
        if(GlobalServerData::serverSettings.sendPlayerNumber)
        {
            if(!GlobalServerData::serverPrivateVariables.player_updater.start())
            {
                std::cerr << "player_updater timer fail to set" << std::endl;
                return EXIT_FAILURE;
            }
            #ifdef CATCHCHALLENGER_CLASS_ONLYGAMESERVER
            if(!GlobalServerData::serverPrivateVariables.player_updater_to_master.start())
            {
                std::cerr << "player_updater_to_master timer fail to set" << std::endl;
                return EXIT_FAILURE;
            }
            #endif
        }
    }
    #ifdef CATCHCHALLENGER_CLASS_ONLYGAMESERVER
    {
        if(!timerPurgeTokenAuthList.start(LinkToMaster::purgeLockPeriod))
        {
            std::cerr << "timerPurgeTokenAuthList fail to set" << std::endl;
            return EXIT_FAILURE;
        }
    }
    #endif
    delete settings;

    char buf[4096];
    memset(buf,0,4096);
    /* Buffer where events are returned */
    epoll_event events[MAXEVENTS];

    char encodingBuff[1];
    #ifdef SERVERSSL
    encodingBuff[0]=0x01;
    #else
    encodingBuff[0]=0x00;
    #endif

    bool acceptSocketWarningShow=false;
    int numberOfConnectedClient=0;
    /* The event loop */
    #ifdef CATCHCHALLENGER_EXTRA_CHECK
    unsigned int clientnumberToDebug=0;
    #endif
    int number_of_events, i;
    while(1)
    {

        number_of_events = Epoll::epoll.wait(events, MAXEVENTS);
        if(elementsToDeleteSize>0 && number_of_events<MAXEVENTS)
        {
            if(elementsToDeleteIndex>=15)
                elementsToDeleteIndex=0;
            else
                ++elementsToDeleteIndex;
            const std::vector<void *> &elementsToDeleteSub=elementsToDelete[elementsToDeleteIndex];
            if(!elementsToDeleteSub.empty())
            {
                unsigned int index=0;
                while(index<elementsToDeleteSub.size())
                {
                    delete static_cast<Client *>(elementsToDeleteSub.at(index));
                    index++;
                }
            }
            elementsToDeleteSize-=elementsToDeleteSub.size();
            elementsToDelete[elementsToDeleteIndex].clear();
        }
        #ifdef SERVERBENCHMARK
        EpollUnixSocketClientFinal::start = std::chrono::high_resolution_clock::now();
        #endif
        for(i = 0; i < number_of_events; i++)
        {
            switch(static_cast<BaseClassSwitch *>(events[i].data.ptr)->getType())
            {
                case BaseClassSwitch::EpollObjectType::Server:
                {
                    if((events[i].events & EPOLLERR) ||
                    (events[i].events & EPOLLHUP) ||
                    (!(events[i].events & EPOLLIN) && !(events[i].events & EPOLLOUT)))
                    {
                        /* An error has occured on this fd, or the socket is not
                        ready for reading (why were we notified then?) */
                        std::cerr << "server epoll error" << std::endl;
                        continue;
                    }
                    /* We have a notification on the listening socket, which
                    means one or more incoming connections. */
                    while(1)
                    {
                        sockaddr in_addr;
                        socklen_t in_len = sizeof(in_addr);
                        const int &infd = server->accept(&in_addr, &in_len);
                        if(!server->isReady())
                        {
                            /// \todo dont clean error on client into this case
                            std::cerr << "client connect when the server is not ready" << std::endl;
                            ::close(infd);
                            break;
                        }
                        if(elementsToDeleteSize>64
                                #ifndef CATCHCHALLENGER_CLASS_ONLYGAMESERVER
                                || BaseServerLogin::tokenForAuthSize>=CATCHCHALLENGER_SERVER_MAXNOTLOGGEDCONNECTION
                                #else
                                || Client::tokenAuthList.size()>=CATCHCHALLENGER_SERVER_MAXNOTLOGGEDCONNECTION
                                #endif
                                )
                        {
                            /// \todo dont clean error on client into this case
                            std::cerr << "server overload" << std::endl;
                            ::close(infd);
                            break;
                        }
                        if(infd == -1)
                        {
                            if((errno == EAGAIN) ||
                            (errno == EWOULDBLOCK))
                            {
                                /* We have processed all incoming
                                connections. */
                                if(!acceptSocketWarningShow)
                                {
                                    acceptSocketWarningShow=true;
                                    std::cerr << "::accept() return -1 and errno: " << std::to_string(errno) << " event or socket ignored" << std::endl;
                                }
                                break;
                            }
                            else
                            {
                                std::cout << "connexion accepted" << std::endl;
                                break;
                            }
                        }
                        // do at the protocol negociation to send the reason
                        if(numberOfConnectedClient>=GlobalServerData::serverSettings.max_players)
                        {
                            ::close(infd);
                            break;
                        }

                        /* Make the incoming socket non-blocking and add it to the
                        list of fds to monitor. */
                        numberOfConnectedClient++;

                        //const int s = EpollSocket::make_non_blocking(infd);->do problem with large datapack from interne protocol
                        const int s = 0;
                        if(s == -1)
                            std::cerr << "unable to make to socket non blocking" << std::endl;
                        else
                        {
                            if(tcpCork)
                            {
                                //set cork for CatchChallener because don't have real time part
                                int state = 1;
                                if(setsockopt(infd, IPPROTO_TCP, TCP_CORK, &state, sizeof(state))!=0)
                                    std::cerr << "Unable to apply tcp cork" << std::endl;
                            }
                            else if(tcpNodelay)
                            {
                                //set no delay to don't try group the packet and improve the performance
                                int state = 1;
                                if(setsockopt(infd, IPPROTO_TCP, TCP_NODELAY, &state, sizeof(state))!=0)
                                    std::cerr << "Unable to apply tcp no delay" << std::endl;
                            }

                            Client *client;
                            switch(GlobalServerData::serverSettings.mapVisibility.mapVisibilityAlgorithm)
                            {
                                case MapVisibilityAlgorithmSelection_Simple:
                                    client=new MapVisibilityAlgorithm_Simple_StoreOnSender(infd
                                                       #ifdef SERVERSSL
                                                       ,server->getCtx()
                                                       #endif
                                                                                           );
                                break;
                                case MapVisibilityAlgorithmSelection_WithBorder:
                                    client=new MapVisibilityAlgorithm_WithBorder_StoreOnSender(infd
                                                           #ifdef SERVERSSL
                                                           ,server->getCtx()
                                                           #endif
                                                                                               );
                                break;
                                default:
                                case MapVisibilityAlgorithmSelection_None:
                                    client=new MapVisibilityAlgorithm_None(infd
                                       #ifdef SERVERSSL
                                       ,server->getCtx()
                                       #endif
                                                                           );
                                break;
                            }
                            #ifdef CATCHCHALLENGER_EXTRA_CHECK
                            ++clientnumberToDebug;
                            #endif
                            //just for informations
                            {
                                char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
                                const int &s = getnameinfo(&in_addr, in_len,
                                hbuf, sizeof hbuf,
                                sbuf, sizeof sbuf,
                                NI_NUMERICHOST | NI_NUMERICSERV);
                                if(s == 0)
                                {
                                    #ifdef CATCHCHALLENGER_EXTRA_CHECK
                                    //std::cout << "Accepted connection on descriptor " << infd << "(host=" << hbuf << ", port=" << sbuf << "), client: " << client << ", clientnumberToDebug: " << clientnumberToDebug << std::endl;
                                    #else
                                    //std::cout << "Accepted connection on descriptor " << infd << "(host=" << hbuf << ", port=" << sbuf << "), client: " << client << std::endl;
                                    #endif
                                    client->socketStringSize=static_cast<int>(strlen(hbuf))+static_cast<int>(strlen(sbuf))+1+1;
                                    client->socketString=new char[client->socketStringSize];
                                    strcpy(client->socketString,hbuf);
                                    strcat(client->socketString,":");
                                    strcat(client->socketString,sbuf);
                                    client->socketString[client->socketStringSize-1]='\0';
                                }
                                /*else
                                    std::cout << "Accepted connection on descriptor " << infd << ", client: " << client << std::endl;*/
                            }
                            epoll_event event;
                            event.data.ptr = client;
                            event.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET | EPOLLRDHUP | EPOLLET | EPOLLOUT;
                            const int s = Epoll::epoll.ctl(EPOLL_CTL_ADD, infd, &event);
                            if(s == -1)
                            {
                                std::cerr << "epoll_ctl on socket error" << std::endl;
                                delete client;
                            }
                            else
                            {
                                if(::write(infd,encodingBuff,sizeof(encodingBuff))!=sizeof(encodingBuff))
                                {
                                    std::cerr << "socket first byte write error" << std::endl;
                                    delete client;
                                }
                            }

                        }
                    }
                }
                break;
                case BaseClassSwitch::EpollObjectType::Client:
                {
                    Client * const client=static_cast<Client *>(events[i].data.ptr);
                    if((events[i].events & EPOLLERR) ||
                    (events[i].events & EPOLLHUP) ||
                    (!(events[i].events & EPOLLIN) && !(events[i].events & EPOLLOUT)))
                    {
                        /* An error has occured on this fd, or the socket is not
                        ready for reading (why were we notified then?) */
                        if(!(events[i].events & EPOLLHUP))
                            std::cerr << "client epoll error: " << events[i].events << std::endl;
                        numberOfConnectedClient--;

                        client->disconnectClient();

                        continue;
                    }
                    //ready to read
                    if(events[i].events & EPOLLIN)
                        client->parseIncommingData();
                    if(events[i].events & EPOLLRDHUP || events[i].events & EPOLLHUP || client->socketIsClosed())
                    {
                        // Crash at 51th: /usr/bin/php -f loginserver-json-generator.php 127.0.0.1 39034
                        numberOfConnectedClient--;
                        client->disconnectClient();
                    }
                }
                break;
                case BaseClassSwitch::EpollObjectType::Timer:
                {
                    static_cast<EpollTimer *>(events[i].data.ptr)->exec();
                    static_cast<EpollTimer *>(events[i].data.ptr)->validateTheTimer();
                }
                break;
                case BaseClassSwitch::EpollObjectType::Database:
                {
                    switch(static_cast<CatchChallenger::DatabaseBase *>(events[i].data.ptr)->databaseType())
                    {
                        #ifdef CATCHCHALLENGER_DB_POSTGRESQL
                        case DatabaseBase::DatabaseType::PostgreSQL:
                        {
                            EpollPostgresql * const db=static_cast<EpollPostgresql *>(events[i].data.ptr);
                            db->epollEvent(events[i].events);
                            if(!datapack_loaded)
                            {
                                if(db->isConnected())
                                {
                                    std::cout << "datapack_loaded not loaded: start preload data " << std::endl;
                                    server->preload_the_data();
                                    datapack_loaded=true;
                                }
                                else
                                    std::cerr << "datapack_loaded not loaded: but database seam don't be connected" << std::endl;
                            }
                            if(!db->isConnected())
                            {
                                std::cerr << "database disconnect, quit now" << std::endl;
                                return EXIT_FAILURE;
                            }
                        }
                        break;
                        #endif
                        #ifdef CATCHCHALLENGER_DB_MYSQL
                        case DatabaseBase::DatabaseType::Mysql:
                        {
                            EpollMySQL * const db=static_cast<EpollMySQL *>(events[i].data.ptr);
                            db->epollEvent(events[i].events);
                            if(!datapack_loaded)
                            {
                                if(db->isConnected())
                                {
                                    std::cout << "datapack_loaded not loaded: start preload data " << std::endl;
                                    server->preload_the_data();
                                    datapack_loaded=true;
                                }
                                else
                                    std::cerr << "datapack_loaded not loaded: but database seam don't be connected" << std::endl;
                            }
                            if(!db->isConnected())
                            {
                                std::cerr << "database disconnect, quit now" << std::endl;
                                return EXIT_FAILURE;
                            }
                        }
                        break;
                        #endif
                        default:
                        std::cerr << "epoll database type return not supported" << std::endl;
                        abort();
                    }
                }
                break;
                #ifdef CATCHCHALLENGER_CLASS_ONLYGAMESERVER
                case BaseClassSwitch::EpollObjectType::MasterLink:
                {
                    LinkToMaster * const client=static_cast<LinkToMaster *>(events[i].data.ptr);
                    if((events[i].events & EPOLLERR) ||
                    (events[i].events & EPOLLHUP) ||
                    (!(events[i].events & EPOLLIN) && !(events[i].events & EPOLLOUT)))
                    {
                        /* An error has occured on this fd, or the socket is not
                        ready for reading (why were we notified then?) */
                        if(!(events[i].events & EPOLLHUP))
                            std::cerr << "client epoll error: " << events[i].events << std::endl;
                        client->tryReconnect();
                        continue;
                    }
                    //ready to read
                    if(events[i].events & EPOLLIN)
                        client->parseIncommingData();
                    if(events[i].events & EPOLLHUP || events[i].events & EPOLLRDHUP)
                        client->tryReconnect();
                }
                break;
                #endif
                default:
                    std::cerr << "unknown event" << std::endl;
                break;
            }
        }
        #ifdef SERVERBENCHMARK
        std::chrono::duration<unsigned long long int,std::nano> elapsed_seconds = std::chrono::high_resolution_clock::now()-EpollUnixSocketClientFinal::start;
        EpollUnixSocketClientFinal::timeUsed+=elapsed_seconds.count();
        #endif
    }
    server->close();
    server->unload_the_data();
    #ifdef CATCHCHALLENGER_CLASS_ONLYGAMESERVER
    LinkToMaster::linkToMaster->closeSocket();
    #endif
    delete server;
    #ifdef CATCHCHALLENGER_CLASS_ONLYGAMESERVER
    delete LinkToMaster::linkToMaster;
    #endif
    return EXIT_SUCCESS;
}
