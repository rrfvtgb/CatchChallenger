#ifndef LOADMAPALL_H
#define LOADMAPALL_H

#include "../map-procedural-generation-terrain/VoronioForTiledMapTmx.h"
#include "../../client/tiled/tiled_map.h"
#include "../../general/base/cpp11addition.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <cstdint>

class LoadMapAll
{
public:
    enum Orientation : uint8_t
    {
        Orientation_none = 0,//where the target orientation don't matter
        Orientation_top = 1,
        Orientation_right = 2,
        Orientation_bottom = 4,
        Orientation_left = 8
    };
    struct SimplifiedMapForPathFinding
    {
        struct PathToGo
        {
            std::vector<std::pair<Orientation,uint8_t/*step number*/> > left;
            std::vector<std::pair<Orientation,uint8_t/*step number*/> > right;
            std::vector<std::pair<Orientation,uint8_t/*step number*/> > top;
            std::vector<std::pair<Orientation,uint8_t/*step number*/> > bottom;
        };
        std::unordered_map<std::pair<uint16_t,uint16_t>,PathToGo,pairhash> pathToGo;
        std::unordered_set<std::pair<uint16_t,uint16_t>,pairhash> pointQueued;
    };

    struct MapPointToParse
    {
        uint16_t x,y;
    };

    enum CityType
    {
        CityType_small,
        CityType_medium,
        CityType_big,
    };
    struct City
    {
        unsigned int x,y;
        std::string name;
        CityType type;
    };
    struct CityInternal
    {
        unsigned int x,y;
        std::string name;
        CityType type;
        std::vector<CityInternal *> citiesNeighbor;
    };
    static std::vector<City> cities;
    static std::unordered_map<uint16_t,std::unordered_map<uint16_t,unsigned int> > citiesCoordToIndex;
    static uint8_t *mapPathDirection;

    static void addDebugCity(Tiled::Map &worldMap, unsigned int mapWidth, unsigned int mapHeight);
    static void addCity(Tiled::Map &worldMap, const Grid &grid, const std::vector<std::string> &citiesNames, const unsigned int &mapXCount, const unsigned int &mapYCount);
    static bool haveCityEntryInternal(const std::unordered_map<uint32_t,std::unordered_map<uint32_t,CityInternal *> > &positionsAndIndex,
                              const unsigned int &x, const unsigned int &y);
    static bool haveCityEntry(const std::unordered_map<uint16_t, std::unordered_map<uint16_t, unsigned int> > &positionsAndIndex,
                              const unsigned int &x, const unsigned int &y);
    static bool haveCityPath(const std::unordered_map<uint32_t,std::unordered_map<uint32_t,std::unordered_map<uint32_t,std::unordered_set<uint32_t> > > > &resolvedPath,
                                  const unsigned int &x1, const unsigned int &y1,
                                  const unsigned int &x2, const unsigned int &y2);
    static Orientation reverseOrientation(const Orientation &orientation);
    static void addCityContent(Tiled::Map &worldMap, const unsigned int &mapXCount, const unsigned int &mapYCount);
};

#endif // LOADMAPALL_H