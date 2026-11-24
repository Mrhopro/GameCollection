#include "BinaryStorage.h"
#include <cstdint>
#include <fstream>

using namespace std;

namespace
{
    const uint32_t maximumStringLength = 1000000;
    const uint32_t maximumRecordCount = 100000;
}

static bool writeString(ofstream& file, const string& value)
{
	uint32_t length = static_cast<uint32_t>(value.size()); // size_t to uint32_t 
    file.write(reinterpret_cast<const char*>(&length), sizeof(length)); // uint32_t like mass char

    if (length > 0)
    {
        file.write(value.data(), length);
    }

    return static_cast<bool>(file);
}

static bool readString(ifstream& file, string& value)
{
    uint32_t length = 0;
    file.read(reinterpret_cast<char*>(&length), sizeof(length));

    if (!file || length > maximumStringLength)
    {
        return false;
    }

    value.assign(length, '\0');
    if (length > 0)
    {
        file.read(&value[0], length);
    }

    return static_cast<bool>(file);
}

bool BinaryStorage::save(const string& fileName, const Collection& collection)
{
    ofstream file(fileName, ios::binary | ios::trunc);
    if (!file)
    {
        return false;
    }

    const char signature[4] = { 'G', 'C', '0', '2' };
    file.write(signature, sizeof(signature));

    uint32_t gameCount = static_cast<uint32_t>(collection.getGames().size());
    int nextId = collection.getNextId();

    file.write(reinterpret_cast<const char*>(&gameCount), sizeof(gameCount));
    file.write(reinterpret_cast<const char*>(&nextId), sizeof(nextId));

    if (!writeString(file, collection.getSteamId()))
    {
        return false;
    }
    if (!writeString(file, collection.getSteamName()))
    {
        return false;
    }

    for (const Game& game : collection.getGames())
    {
        int id = game.getId();
        int rating = game.getRating();
        unsigned int appId = game.getAppId();
        uint8_t installed = game.isInstalled() ? 1 : 0;

        file.write(reinterpret_cast<const char*>(&id), sizeof(id));
        file.write(reinterpret_cast<const char*>(&appId), sizeof(appId));
        file.write(reinterpret_cast<const char*>(&rating), sizeof(rating));
        file.write(reinterpret_cast<const char*>(&installed), sizeof(installed));

        if (!writeString(file, game.getTitle()))
        {
            return false;
        }
        if (!writeString(file, game.getGenre()))
        {
            return false;
        }
        if (!writeString(file, game.getPlatform()))
        {
            return false;
        }
        if (!writeString(file, game.getStatus()))
        {
            return false;
        }

        vector<Achievement> achievements = game.getAchievements();
        uint32_t achievementCount = static_cast<uint32_t>(achievements.size());
        file.write(reinterpret_cast<const char*>(&achievementCount), sizeof(achievementCount));

        for (const Achievement& achievement : achievements)
        {
            if (!writeString(file, achievement.name))
            {
                return false;
            }
            if (!writeString(file, achievement.description))
            {
                return false;
            }

            uint8_t unlocked = achievement.unlocked ? 1 : 0;
            file.write(reinterpret_cast<const char*>(&unlocked), sizeof(unlocked));
        }
    }

    return static_cast<bool>(file);
}

bool BinaryStorage::load(const string& fileName, Collection& collection)
{
    ifstream file(fileName, ios::binary);
    if (!file)
    {
        return false;
    }

    char signature[4] = {};
    file.read(signature, sizeof(signature));
    if (!file)
    {
        return false;
    }

    bool isOldFormat = signature[0] == 'G' && signature[1] == 'C' &&
        signature[2] == '0' && signature[3] == '1';
    bool isCurrentFormat = signature[0] == 'G' && signature[1] == 'C' &&
        signature[2] == '0' && signature[3] == '2';

    if (!isOldFormat && !isCurrentFormat)
    {
        return false;
    }

    uint32_t gameCount = 0;
    int nextId = 1;
    file.read(reinterpret_cast<char*>(&gameCount), sizeof(gameCount));
    file.read(reinterpret_cast<char*>(&nextId), sizeof(nextId));

    if (!file || gameCount > maximumRecordCount || nextId < 1)
    {
        return false;
    }

    string steamId;
    string steamName;
    if (!readString(file, steamId) || !readString(file, steamName))
    {
        return false;
    }

    Collection loaded;
    loaded.setSteamProfile(steamId, steamName);
    loaded.setNextId(nextId);

    for (uint32_t i = 0; i < gameCount; ++i)
    {
        int id = 0;
        int rating = 0;
        unsigned int appId = 0;
        uint8_t installed = 0;

        file.read(reinterpret_cast<char*>(&id), sizeof(id));
        file.read(reinterpret_cast<char*>(&appId), sizeof(appId));
        file.read(reinterpret_cast<char*>(&rating), sizeof(rating));

        if (isCurrentFormat)
        {
            file.read(reinterpret_cast<char*>(&installed), sizeof(installed));
        }

        if (!file || installed > 1)
        {
            return false;
        }

        Game game(id, appId, "");
        string title;
        string genre;
        string platform;
        string status;

        if (!readString(file, title) || !readString(file, genre) ||
            !readString(file, platform) || !readString(file, status))
        {
            return false;
        }

        game.setTitle(title);
        game.setGenre(genre);
        game.setPlatform(platform);
        game.setStatus(status);
        game.setRating(rating);
        game.setInstalled(installed != 0);

        uint32_t achievementCount = 0;
        file.read(reinterpret_cast<char*>(&achievementCount), sizeof(achievementCount));
        if (!file || achievementCount > maximumRecordCount)
        {
            return false;
        }

        vector<Achievement> achievements;
        for (uint32_t j = 0; j < achievementCount; ++j)
        {
            Achievement achievement;
            uint8_t unlocked = 0;

            if (!readString(file, achievement.name) ||
                !readString(file, achievement.description))
            {
                return false;
            }

            file.read(reinterpret_cast<char*>(&unlocked), sizeof(unlocked));
            if (!file || unlocked > 1)
            {
                return false;
            }

            achievement.unlocked = unlocked != 0;
            achievements.push_back(achievement);
        }

        game.setAchievements(achievements);
        loaded.getGames().push_back(game);
    }

    collection = loaded;
    return true;
}
