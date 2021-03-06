#include "MapController.h"
#include "../../general/base/MoveOnTheMap.h"

#include <QMessageBox>

MapController::MapController(const bool &centerOnPlayer,const bool &debugTags,const bool &useCache,const bool &OpenGL) :
    MapVisualiserPlayer(centerOnPlayer,debugTags,useCache,OpenGL)
{
    setWindowIcon(QIcon(":/icon.png"));

    botTileset = new Tiled::Tileset("bot",16,24);
    botTileset->loadFromImage(QImage(":/bot_skin.png"),":/bot_skin.png");
    botNumber = 0;

    //the bot management
    if(!connect(&timerBotMove,SIGNAL(timeout()),this,SLOT(botMove())))
        abort();
    if(!connect(&timerBotManagement,SIGNAL(timeout()),this,SLOT(botManagement())))
        abort();
    timerBotMove.start(66);
    timerBotManagement.start(3000);

    playerTileset = new Tiled::Tileset("player",16,24);
    QString externalFile=QCoreApplication::applicationDirPath()+"/player_skin.png";
    if(QFile::exists(externalFile))
    {
        QImage externalImage(externalFile);
        if(!externalImage.isNull() && externalImage.width()==48 && externalImage.height()==96)
            playerTileset->loadFromImage(externalImage,externalFile);
        else
            playerTileset->loadFromImage(QImage(":/player_skin.png"),":/player_skin.png");
    }
    else
        playerTileset->loadFromImage(QImage(":/player_skin.png"),":/player_skin.png");
    playerMapObject = new Tiled::MapObject();

    //the direction
    direction=CatchChallenger::Direction_look_at_bottom;
    playerMapObject->setTile(playerTileset->tileAt(7));
}

MapController::~MapController()
{
    delete playerTileset;
    delete botTileset;
}

void MapController::setScale(int scaleSize)
{
    scale(scaleSize,scaleSize);
}

void MapController::setBotNumber(quint16 botNumber)
{
    this->botNumber=botNumber;
    botManagement();
}

bool MapController::botMoveStepSlot(Bot *bot)
{
    int baseTile=1;
    //move the player for intermediate step and define the base tile (define the stopped step with direction)
    switch(bot->direction)
    {
        case CatchChallenger::Direction_move_at_left:
        baseTile=10;
        switch(bot->moveStep)
        {
            case 1:
            case 2:
            bot->mapObject->setX(bot->mapObject->x()-0.33);
            break;
        }
        break;
        case CatchChallenger::Direction_move_at_right:
        baseTile=4;
        switch(bot->moveStep)
        {
            case 1:
            case 2:
            bot->mapObject->setX(bot->mapObject->x()+0.33);
            break;
        }
        break;
        case CatchChallenger::Direction_move_at_top:
        baseTile=1;
        switch(bot->moveStep)
        {
            case 1:
            case 2:
            bot->mapObject->setY(bot->mapObject->y()-0.33);
            break;
        }
        break;
        case CatchChallenger::Direction_move_at_bottom:
        baseTile=7;
        switch(bot->moveStep)
        {
            case 1:
            case 2:
            bot->mapObject->setY(bot->mapObject->y()+0.33);
            break;
        }
        break;
        default:
        qDebug() << QStringLiteral("botMoveStepSlot(): moveStep: %1, wrong direction").arg(bot->moveStep);
        return false;
    }

    //apply the right step of the base step defined previously by the direction
    switch(bot->moveStep)
    {
        //stopped step
        case 0:
        bot->mapObject->setTile(botTileset->tileAt(baseTile+0));
        break;
        //transition step
        case 1:
        bot->mapObject->setTile(botTileset->tileAt(baseTile-1));
        break;
        case 2:
        bot->mapObject->setTile(botTileset->tileAt(baseTile+1));
        break;
        //stopped step
        case 3:
        bot->mapObject->setTile(botTileset->tileAt(baseTile+0));
        break;
    }

    bot->moveStep++;

    //if have finish the step
    if(bot->moveStep>3)
    {
        CatchChallenger::Map * old_map=&all_map[bot->map]->logicalMap;
        CatchChallenger::Map * map=&all_map[bot->map]->logicalMap;
        //set the final value (direction, position, ...)
        switch(bot->direction)
        {
            case CatchChallenger::Direction_move_at_left:
            bot->direction=CatchChallenger::Direction_look_at_left;
            CatchChallenger::MoveOnTheMap::move(CatchChallenger::Direction_move_at_left,&map,&bot->x,&bot->y);
            break;
            case CatchChallenger::Direction_move_at_right:
            bot->direction=CatchChallenger::Direction_look_at_right;
            CatchChallenger::MoveOnTheMap::move(CatchChallenger::Direction_move_at_right,&map,&bot->x,&bot->y);
            break;
            case CatchChallenger::Direction_move_at_top:
            bot->direction=CatchChallenger::Direction_look_at_top;
            CatchChallenger::MoveOnTheMap::move(CatchChallenger::Direction_move_at_top,&map,&bot->x,&bot->y);
            break;
            case CatchChallenger::Direction_move_at_bottom:
            bot->direction=CatchChallenger::Direction_look_at_bottom;
            CatchChallenger::MoveOnTheMap::move(CatchChallenger::Direction_move_at_bottom,&map,&bot->x,&bot->y);
            break;
            default:
            qDebug() << QStringLiteral("botMoveStepSlot(): bot->moveStep: %1, wrong direction when bot->moveStep>2").arg(bot->moveStep);
            return false;
        }
        //if the map have changed
        if(old_map!=map)
        {
            //remove bot
            if(ObjectGroupItem::objectGroupLink.contains(all_map[old_map->map_file]->objectGroup))
                ObjectGroupItem::objectGroupLink[all_map[old_map->map_file]->objectGroup]->removeObject(bot->mapObject);
            else
                qDebug() << QStringLiteral("botMoveStepSlot(), ObjectGroupItem::objectGroupLink not contains bot->mapObject at remove to change the map");
            //add bot
            if(ObjectGroupItem::objectGroupLink.contains(all_map[map->map_file]->objectGroup))
                ObjectGroupItem::objectGroupLink[all_map[map->map_file]->objectGroup]->addObject(bot->mapObject);
            else
                return false;
            bot->map=map->map_file;
        }
        //move to the final position (integer), y+1 because the tile lib start y to 1, not 0
        bot->mapObject->setPosition(QPoint(bot->x,bot->y+1));

        bot->inMove=false;
    }

    return true;
}

void MapController::botMove()
{
    int index;
    QSet<int> continuedMove;
    //continue the move
    index=0;
    while(index<botList.size())
    {
        if(botList.at(index).inMove)
        {
            if(!botMoveStepSlot(&botList[index]))
            {
                delete botList.at(index).mapObject;
                botList.removeAt(index);
                index--;
            }
            continuedMove << index;
        }
        index++;
    }
    //start move
    index=0;
    while(index<botList.size())
    {
        if(!botList.at(index).inMove && !continuedMove.contains(index))
        {
            QList<CatchChallenger::Direction> directions_allowed;
            if(CatchChallenger::MoveOnTheMap::canGoTo(CatchChallenger::Direction_move_at_left,all_map[botList.at(index).map]->logicalMap,botList.at(index).x,botList.at(index).y,true))
                directions_allowed << CatchChallenger::Direction_move_at_left;
            if(CatchChallenger::MoveOnTheMap::canGoTo(CatchChallenger::Direction_move_at_right,all_map[botList.at(index).map]->logicalMap,botList.at(index).x,botList.at(index).y,true))
                directions_allowed << CatchChallenger::Direction_move_at_right;
            if(CatchChallenger::MoveOnTheMap::canGoTo(CatchChallenger::Direction_move_at_top,all_map[botList.at(index).map]->logicalMap,botList.at(index).x,botList.at(index).y,true))
                directions_allowed << CatchChallenger::Direction_move_at_top;
            if(CatchChallenger::MoveOnTheMap::canGoTo(CatchChallenger::Direction_move_at_bottom,all_map[botList.at(index).map]->logicalMap,botList.at(index).x,botList.at(index).y,true))
                directions_allowed << CatchChallenger::Direction_move_at_bottom;
            if(directions_allowed.size()>0)
            {
                int random = rand()%directions_allowed.size();
                CatchChallenger::Direction final_direction=directions_allowed.at(random);

                botList[index].direction=final_direction;
                botList[index].inMove=true;
                botList[index].moveStep=1;
                switch(final_direction)
                {
                    case CatchChallenger::Direction_move_at_left:
                        botMoveStepSlot(&botList[index]);
                    break;
                    case CatchChallenger::Direction_move_at_right:
                        botMoveStepSlot(&botList[index]);
                    break;
                    case CatchChallenger::Direction_move_at_top:
                        botMoveStepSlot(&botList[index]);
                    break;
                    case CatchChallenger::Direction_move_at_bottom:
                        botMoveStepSlot(&botList[index]);
                    break;
                    default:
                    qDebug() << QStringLiteral("transformLookToMove(): wrong direction");
                    return;
                }
            }
        }
        index++;
    }
}

void MapController::botManagement()
{
    int index;
    //remove bot where the map is not displayed
    index=0;
    while(index<botList.size())
    {
        if(!displayed_map.contains(botList.at(index).map))
        {
            if(all_map.contains(botList.at(index).map))
            {
                ObjectGroupItem::objectGroupLink[all_map[botList.at(index).map]->objectGroup]->removeObject(botList.at(index).mapObject);
                delete botList.at(index).mapObject;
            }
            else
            {
                //the map have been removed then the map object too
            }
            botList.removeAt(index);
        }
        else
            index++;
    }
    //remove random bot
    index=0;
    while(index<botList.size())
    {
        if(index>=botNumber /* if to much bot */ || rand()%100<20)
        {
            if(all_map.contains(botList.at(index).map))
            {
                if(ObjectGroupItem::objectGroupLink.contains(all_map[botList.at(index).map]->objectGroup))
                    ObjectGroupItem::objectGroupLink[all_map[botList.at(index).map]->objectGroup]->removeObject(botList.at(index).mapObject);
                else
                    qDebug() << QStringLiteral("botManagement(), ObjectGroupItem::objectGroupLink not contains botList.at(index).mapObject at remove random bot");
                delete botList.at(index).mapObject;
            }
            else
            {
                //the map have been removed then the map object too
            }
            botList.removeAt(index);
        }
        else
            index++;
    }
    //add bot
    if(!botSpawnPointList.isEmpty())
    {
        index=botList.size();
        while(index<botNumber)//do botNumber bot
        {
            BotSpawnPoint point=botSpawnPointList[rand()%botSpawnPointList.size()];

            Bot bot;
            bot.map=point.map->logicalMap.map_file;
            bot.mapObject=new Tiled::MapObject();
            bot.x=point.x;
            bot.y=point.y;
            bot.direction=CatchChallenger::Direction_look_at_bottom;
            bot.inMove=false;
            bot.moveStep=0;

            if(ObjectGroupItem::objectGroupLink.contains(all_map[bot.map]->objectGroup))
                ObjectGroupItem::objectGroupLink[all_map[bot.map]->objectGroup]->addObject(bot.mapObject);
            else
                qDebug() << QStringLiteral("botManagement(), ObjectGroupItem::objectGroupLink not contains bot.map->objectGroup");

            //move to the final position (integer), y+1 because the tile lib start y to 1, not 0
            bot.mapObject->setPosition(QPoint(bot.x,bot.y+1));

            bot.mapObject->setTile(botTileset->tileAt(7));
            botList << bot;
            index++;
        }
    }
    else
    {
        //qDebug() << "Bot spawn list is empty";
    }
}

//call after enter on new map
void MapController::loadPlayerFromCurrentMap()
{
    MapVisualiserPlayer::loadPlayerFromCurrentMap();

    //list bot spawn point
    botSpawnPointList.clear();
    {
        QSet<QString>::const_iterator i = displayed_map.constBegin();
        while (i != displayed_map.constEnd()) {
            MapVisualiserThread::Map_full * map=getMap(*i);
            if(map!=NULL)
            {
                int index=0;
                while(index<map->logicalMap.bot_spawn_points.size())
                {
                    BotSpawnPoint point;
                    point.map=map;
                    point.x=map->logicalMap.bot_spawn_points.at(index).x;
                    point.y=map->logicalMap.bot_spawn_points.at(index).y;
                    botSpawnPointList << point;
                    index++;
                }
            }
            ++i;
        }
    }

    botManagement();
}

//call before leave the old map (and before loadPlayerFromCurrentMap())
void MapController::unloadPlayerFromCurrentMap()
{
    MapVisualiserPlayer::unloadPlayerFromCurrentMap();

    botSpawnPointList.clear();
}

bool MapController::viewMap(const QString &fileName)
{
    current_map=NULL;

    QString current_map_fileName=loadOtherMap(fileName);
    if(current_map_fileName.isEmpty())
    {
        QMessageBox::critical(this,"Error",mLastError);
        return false;
    }
    current_map=all_map[current_map_fileName];

    render();

    //position
    if(!current_map->logicalMap.rescue_points.empty())
    {
        x=current_map->logicalMap.rescue_points.first().x;
        y=current_map->logicalMap.rescue_points.first().y;
    }
    else if(!current_map->logicalMap.bot_spawn_points.empty())
    {
        x=current_map->logicalMap.bot_spawn_points.first().x;
        y=current_map->logicalMap.bot_spawn_points.first().y;
    }
    else
    {
        x=current_map->logicalMap.width/2;
        y=current_map->logicalMap.height/2;
    }

    mapUsed=loadMap(current_map,true);
    removeUnusedMap();
    loadPlayerFromCurrentMap();

    show();

    return true;
}
