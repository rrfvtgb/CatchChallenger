#include "../VariableServer.h"
#include "../base/GlobalServerData.h"
#include "../base/MapServer.h"
#include "../base/Client.h"
#include "../../general/base/CommonDatapack.h"
#include "../../general/base/FacilityLib.h"
#include "../base/Client.h"
#include "../base/PreparedDBQuery.h"

using namespace CatchChallenger;

void Client::wildDrop(const uint32_t &monster)
{
    std::vector<MonsterDrops> drops=GlobalServerData::serverPrivateVariables.monsterDrops.at(monster);
    if(questsDrop.find(monster)!=questsDrop.cend())
        drops.insert(drops.end(),questsDrop.at(monster).begin(),questsDrop.at(monster).end());
    unsigned int index=0;
    bool success;
    uint32_t quantity;
    while(index<drops.size())
    {
        const uint32_t &tempLuck=drops.at(index).luck;
        const uint32_t &quantity_min=drops.at(index).quantity_min;
        const uint32_t &quantity_max=drops.at(index).quantity_max;
        if(tempLuck==100)
            success=true;
        else
        {
            if(rand()%100<(int32_t)tempLuck)
                success=true;
            else
                success=false;
        }
        if(success)
        {
            if(quantity_max==1)
                quantity=1;
            else if(quantity_min==quantity_max)
                quantity=quantity_min;
            else
                quantity=rand()%(quantity_max-quantity_min+1)+quantity_min;
            #ifdef DEBUG_MESSAGE_CLIENT_FIGHT
            normalOutput("Win "+std::to_string(quantity)+" item: "+std::to_string(drops.at(index).item));
            #endif
            addObjectAndSend(drops.at(index).item,quantity);
        }
        index++;
    }
}

uint32_t Client::catchAWild(const bool &toStorage, const PlayerMonster &newMonster)
{
    int position=999999;
    bool ok;
    const uint32_t monster_id=getMonsterId(&ok);
    if(!ok)
    {
        errorFightEngine("No more monster id: getMonsterId(&ok) failed");
        return 0;
    }

    char raw_skill_endurance[newMonster.skills.size()*(1)];
    char raw_skill[newMonster.skills.size()*(2+1)];
    {
        unsigned int sub_index=0;
        uint16_t lastSkillId=0;
        const unsigned int &sub_size=newMonster.skills.size();
        while(sub_index<sub_size)
        {
            const PlayerMonster::PlayerSkill &playerSkill=newMonster.skills.at(sub_index);
            raw_skill_endurance[sub_index]=playerSkill.endurance;

            #ifdef MAXIMIZEPERFORMANCEOVERDATABASESIZE
            //not ordened
            uint16_t skillInt;
            if(lastSkillId<=playerSkill.skill)
            {
                skillInt=playerSkill.skill-lastSkillId;
                lastSkillId=playerSkill.skill;
            }
            else
            {
                skillInt=65536-lastSkillId+playerSkill.skill;
                lastSkillId=playerSkill.skill;
            }
            #else
            //ordened
            const uint16_t &skillInt=playerSkill.skill-lastSkillId;
            lastSkillId=playerSkill.skill;
            #endif

            *reinterpret_cast<uint16_t *>(raw_skill+sub_index*(2+1))=htole16(skillInt);
            raw_skill[2+sub_index*(2+1)]=playerSkill.level;

            sub_index++;
        }
    }

    char raw_buff[(1+1+1)*newMonster.buffs.size()];
    {
        uint8_t lastBuffId=0;
        uint32_t sub_index=0;
        while(sub_index<newMonster.buffs.size())
        {
            const PlayerBuff &buff=newMonster.buffs.at(sub_index);

            #ifdef MAXIMIZEPERFORMANCEOVERDATABASESIZE
            //not ordened
            uint8_t buffInt;
            if(lastBuffId<=buff.buff)
            {
                buffInt=buff.buff-lastBuffId;
                lastBuffId=buff.buff;
            }
            else
            {
                buffInt=256-lastBuffId+buff.buff;
                lastBuffId=buff.buff;
            }
            #else
            //ordened
            const uint8_t &buffInt=buff.buff-lastBuffId;
            lastBuffId=buff.buff;
            #endif

            raw_buff[sub_index*3+0]=buffInt;
            raw_buff[sub_index*3+1]=buff.level;
            raw_buff[sub_index*3+2]=buff.remainingNumberOfTurn;
            sub_index++;
        }
    }

    if(toStorage)
    {
        public_and_private_informations.warehouse_playerMonster.push_back(newMonster);
        public_and_private_informations.warehouse_playerMonster.back().id=monster_id;
        position=public_and_private_informations.warehouse_playerMonster.size();
        const std::string &queryText=PreparedDBQueryCommon::db_query_insert_monster_full.compose(
                    std::to_string(monster_id),
                    std::to_string(character_id),
                    "2",
                    std::to_string(newMonster.hp),
                    std::to_string(newMonster.monster),
                    std::to_string(newMonster.level),
                    std::to_string(newMonster.catched_with),
                    std::to_string((uint8_t)newMonster.gender),
                    std::to_string(character_id),
                    std::to_string(position),
                    binarytoHexa(raw_buff,sizeof(raw_buff)),
                    binarytoHexa(raw_skill,sizeof(raw_skill)),
                    binarytoHexa(raw_skill_endurance,sizeof(raw_skill_endurance))
                    );
        dbQueryWriteCommon(queryText);
    }
    else
    {
        public_and_private_informations.playerMonster.push_back(newMonster);
        public_and_private_informations.playerMonster.back().id=monster_id;
        position=public_and_private_informations.playerMonster.size();
        const std::string &queryText=PreparedDBQueryCommon::db_query_insert_monster_full.compose(
                    std::to_string(monster_id),
                    std::to_string(character_id),
                    "1",
                    std::to_string(newMonster.hp),
                    std::to_string(newMonster.monster),
                    std::to_string(newMonster.level),
                    std::to_string(newMonster.catched_with),
                    std::to_string((uint8_t)newMonster.gender),
                    std::to_string(character_id),
                    std::to_string(position),
                    binarytoHexa(raw_buff,sizeof(raw_buff)),
                    binarytoHexa(raw_skill,sizeof(raw_skill)),
                    binarytoHexa(raw_skill_endurance,sizeof(raw_skill_endurance))
                    );
        dbQueryWriteCommon(queryText);
    }

    /*unsigned int index=0;
    while(index<newMonster.skills.size())
    {
        std::string queryText=PreparedDBQueryCommon::db_query_insert_monster_skill;
        stringreplaceOne(queryText,"%1",std::to_string(monster_id));
        stringreplaceOne(queryText,"%2",std::to_string(newMonster.skills.at(index).skill));
        stringreplaceOne(queryText,"%3",std::to_string(newMonster.skills.at(index).level));
        stringreplaceOne(queryText,"%4",std::to_string(newMonster.skills.at(index).endurance));
        dbQueryWriteCommon(queryText);
        index++;
    }
    index=0;
    while(index<newMonster.buffs.size())
    {
        if(CommonDatapack::commonDatapack.monsterBuffs.at(newMonster.buffs.at(index).buff).level.at(newMonster.buffs.at(index).level).duration==Buff::Duration_Always)
        {
            std::string queryText=PreparedDBQueryCommon::db_query_insert_monster_buff;
            stringreplaceOne(queryText,"%1",std::to_string(monster_id));
            stringreplaceOne(queryText,"%2",std::to_string(newMonster.buffs.at(index).buff));
            stringreplaceOne(queryText,"%3",std::to_string(newMonster.buffs.at(index).level));
            dbQueryWriteCommon(queryText);
        }
        index++;
    }*/
    wildMonsters.erase(wildMonsters.begin());
    return monster_id;
}