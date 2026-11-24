#pragma once
#include "Game.h"
#include <vector>
#include <string>

using namespace std;

class Collection
{
private:
    vector<Game> games;
    int nextId;
    string steamId;
    string steamName;
public:
    Collection();
    vector<Game>& getGames();
    const vector<Game>& getGames() const;
    int addGame(const Game& game);
    bool deleteGame(int id);
    Game* findGame(int id);
    Game* findByTitle(const string& title);
    string getSteamId() const;
    string getSteamName() const;
    void setSteamProfile(const string& id, const string& name);
    int getNextId() const;
    void setNextId(int id);
};
