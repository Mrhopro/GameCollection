#include "Game.h"

using namespace std;

Game::Game()
    : id(0), appId(0), rating(0), installed(false)
{
}

Game::Game(int gameId, unsigned int steamAppId, const string& gameTitle)
    : id(gameId), appId(steamAppId), title(gameTitle), rating(0), installed(false)
{
}

int Game::getId() const
{
    return id;
}

unsigned int Game::getAppId() const
{
    return appId;
}

string Game::getTitle() const
{
    return title;
}

string Game::getGenre() const
{
    return genre;
}

string Game::getPlatform() const
{
    return platform;
}

string Game::getStatus() const
{
    return status;
}

int Game::getRating() const
{
    return rating;
}

bool Game::isInstalled() const
{
    return installed;
}

vector<Achievement> Game::getAchievements() const
{
    return achievements;
}

void Game::setTitle(const string& value)
{
    title = value;
}

void Game::setGenre(const string& value)
{
    genre = value;
}

void Game::setPlatform(const string& value)
{
    platform = value;
}

void Game::setStatus(const string& value)
{
    status = value;
}

void Game::setRating(int value)
{
    rating = value;
}

void Game::setInstalled(bool value)
{
    installed = value;
}

void Game::setAchievements(const vector<Achievement>& value)
{
    achievements = value;
}
