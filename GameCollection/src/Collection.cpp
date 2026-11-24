#include "Collection.h"

using namespace std;

Collection::Collection()
    : nextId(1)
{
}

vector<Game>& Collection::getGames()
{
    return games;
}

const vector<Game>& Collection::getGames() const
{
    return games;
}

int Collection::addGame(const Game& game)
{
    Game copy(nextId, game.getAppId(), game.getTitle());
    copy.setGenre(game.getGenre());
    copy.setPlatform(game.getPlatform());
    copy.setStatus(game.getStatus());
    copy.setRating(game.getRating());
    copy.setInstalled(game.isInstalled());
    copy.setAchievements(game.getAchievements());

    games.push_back(copy);
    ++nextId;
    return copy.getId();
}

bool Collection::deleteGame(int id)
{
    for (size_t i = 0; i < games.size(); ++i)
    {
        if (games[i].getId() == id)
        {
            games.erase(games.begin() + i);
            return true;
        }
    }

    return false;
}

Game* Collection::findGame(int id)
{
    for (Game& game : games)
    {
        if (game.getId() == id)
        {
            return &game;
        }
    }

    return nullptr;
}

Game* Collection::findByTitle(const string& title)
{
    for (Game& game : games)
    {
        if (game.getTitle().find(title) != string::npos)
        {
            return &game;
        }
    }

    return nullptr;
}

string Collection::getSteamId() const
{
    return steamId;
}

string Collection::getSteamName() const
{
    return steamName;
}

void Collection::setSteamProfile(const string& id, const string& name)
{
    steamId = id;
    steamName = name;
}

int Collection::getNextId() const
{
    return nextId;
}

void Collection::setNextId(int id)
{
    nextId = id;
}
