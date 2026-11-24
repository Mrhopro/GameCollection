#pragma once
#include <string>
#include <vector>

using namespace std;

struct Achievement
{
    string name;
    string description;
    bool unlocked = false;
};

class Game
{
private:
    int id;
    unsigned int appId;
    string title;
    string genre;
    string platform;
    string status;
    int rating;
    bool installed;
    vector<Achievement> achievements;

public:
    Game();
    Game(int gameId, unsigned int steamAppId, const string& gameTitle);
    int getId() const;
    unsigned int getAppId() const;
    string getTitle() const;
    string getGenre() const;
    string getPlatform() const;
    string getStatus() const;
    int getRating() const;
    bool isInstalled() const;
    vector<Achievement> getAchievements() const;
    void setTitle(const string& value);
    void setGenre(const string& value);
    void setPlatform(const string& value);
    void setStatus(const string& value);
    void setRating(int value);
    void setInstalled(bool value);
    void setAchievements(const vector<Achievement>& value);
};
