#pragma once
#include "Collection.h"
#include <string>

using namespace std;

class SteamClient
{
private:
    string apiKey;
    string errorMessage;
    string request(const string& url);
    string resolveSteamId(const string& input);
    string getSteamGenre(unsigned int appId);
public:
    SteamClient();
    string getError() const;
    bool connectBySteamId(const string& input, Collection& collection);
    bool connectWithOpenId(Collection& collection);
    bool importLibrary(Collection& collection);
    int scanInstalledGames(Collection& collection);
    bool launchInstalledGame(const Game& game);
};
