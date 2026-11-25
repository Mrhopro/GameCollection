#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include "SteamClient.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <shellapi.h>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cctype>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Shell32.lib")
using namespace std;
using json = nlohmann::json;

static size_t receiveData(void* data, size_t size, size_t count, void* user)
{
    string* response = static_cast<string*>(user);
    response->append(static_cast<char*>(data), size * count);
    return size * count;
}

static bool isSteamId(const string& value)
{
    return value.size() == 17 &&
        value.find_first_not_of("0123456789") == string::npos;
}

static string urlDecode(const string& value)
{
    string result;
    for (size_t i = 0; i < value.size(); ++i)
    {
        if (value[i] == '+')
        {
            result += ' ';
            continue;
        }

        if (value[i] == '%' && i + 2 < value.size())
        {
            string hexadecimal = value.substr(i + 1, 2);
            result += static_cast<char>(strtol(hexadecimal.c_str(), nullptr, 16));
            i += 2;
            continue;
        }

        result += value[i];
    }

    return result;
}

static vector<string> readQuotedValues(const string& line)
{
    vector<string> values;
    size_t position = 0;
    while (position < line.size())
    {
        size_t start = line.find('"', position);
        if (start == string::npos) break;
        size_t end = start + 1;
        string value;
        while (end < line.size())
        {
            if (line[end] == '\\' && end + 1 < line.size() &&
                (line[end + 1] == '\\' || line[end + 1] == '"'))
            {
                value += line[end + 1];
                end += 2;
            }
            else if (line[end] == '"') break;
            else value += line[end++];
        }
        if (end == line.size()) break;
        values.push_back(value);
        position = end + 1;
    }

    return values;
}

static unsigned int readUnsignedNumber(const string& value)
{
    try
    {
        return static_cast<unsigned int>(stoul(value));
    }
    catch (...)
    {
        return 0;
    }
}

static void readManifestField(
    const string& line,
    unsigned int& appId,
    unsigned int& stateFlags,
    string& name,
    string& installDir)
{
    vector<string> values = readQuotedValues(line);
    if (values.size() < 2 || values[0].empty())
    {
        return;
    }

    switch (values[0][0])
    {
    case 'a':
        if (values[0] == "appid")
        {
            appId = readUnsignedNumber(values[1]);
        }
        break;
    case 'n':
        if (values[0] == "name")
        {
            name = values[1];
        }
        break;
    case 'i':
        if (values[0] == "installdir")
        {
            installDir = values[1];
        }
        break;
    case 'S':
        if (values[0] == "StateFlags")
        {
            stateFlags = readUnsignedNumber(values[1]);
        }
        break;
    }
}

static bool waitForOpenIdResponse(string& requestText, string& errorMessage)
{
    const int callbackPort = 43821;
    WSADATA winsock;

    if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0)
    {
        errorMessage = "Не вдалося запустити мережеву бібліотеку Windows.";
        return false;
    }

	SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // socket(AF_INET - IPv4, SOCK_STREAM надійне потокове з’єднання, IPPROTO_TCP - протокол TCP)
    if (server == INVALID_SOCKET)
    {
        WSACleanup();
        errorMessage = "Не вдалося відкрити локальний callback-порт 43821.";
        return false;
    }

    sockaddr_in address{}; // socket address internet
	address.sin_family = AF_INET; // soccket IPv4
	address.sin_port = htons(callbackPort); // коневртація порту в мережевий порядок байтів
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    bool serverReady =
		::bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != SOCKET_ERROR && // sockaddr_in* to sockaddr*
        listen(server, 1) != SOCKET_ERROR;

    if (!serverReady)
    {
        closesocket(server);
        WSACleanup();
        errorMessage = "Не вдалося відкрити локальний callback-порт 43821.";
        return false;
    }

    string callbackUrl = "http://127.0.0.1:43821/";
	string loginUrl = // OpenID parmeters 
        "https://steamcommunity.com/openid/login?openid.ns=http%3A%2F%2Fspecs.openid.net%2Fauth%2F2.0"
        "&openid.mode=checkid_setup&openid.return_to=" + callbackUrl +
        "&openid.realm=http%3A%2F%2F127.0.0.1%3A43821%2F"
        "&openid.identity=http%3A%2F%2Fspecs.openid.net%2Fauth%2F2.0%2Fidentifier_select"
        "&openid.claimed_id=http%3A%2F%2Fspecs.openid.net%2Fauth%2F2.0%2Fidentifier_select";

	ShellExecuteA(nullptr, "open", loginUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL); // browser opening 
    cout << "Підтвердіть вхід Steam у браузері. Очікується відповідь...\n";

    fd_set readableSockets;
    FD_ZERO(&readableSockets);
    FD_SET(server, &readableSockets);

    timeval waitTime{};
    waitTime.tv_sec = 120;
    waitTime.tv_usec = 0;

    int socketReady = select(0, &readableSockets, nullptr, nullptr, &waitTime);
    if (socketReady == 0)
    {
        closesocket(server);
        WSACleanup();
        errorMessage = "Час очікування входу Steam минув. Спробуйте ще раз.";
        return false;
    }
    if (socketReady == SOCKET_ERROR)
    {
        closesocket(server);
        WSACleanup();
        errorMessage = "Помилка під час очікування відповіді Steam.";
        return false;
    }

    SOCKET client = accept(server, nullptr, nullptr);
    if (client == INVALID_SOCKET)
    {
        closesocket(server);
        WSACleanup();
        errorMessage = "Steam не повернув відповідь OpenID.";
        return false;
    }

    char buffer[8192];
    int receivedBytes = recv(client, buffer, sizeof(buffer), 0);
    if (receivedBytes > 0)
    {
        requestText.assign(buffer, receivedBytes);
    }

    const char reply[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Connection: close\r\n\r\n"
        "Вхід завершено. Поверніться до програми.";
    send(client, reply, sizeof(reply) - 1, 0);

    closesocket(client);
    closesocket(server);
    WSACleanup();
    return true;
}

static bool parseOpenIdResponse(
    const string& requestText,
    string& steamId,
    string& postData,
    string& errorMessage) // GET /?openid.ns=...&openid.claimed_id=... HTTP/1.1
{
    size_t queryStart = requestText.find("GET /?");
    if (queryStart == string::npos)
    {
        errorMessage = "Steam не повернув відповідь OpenID.";
        return false;
    }

	size_t queryEnd = requestText.find(' ', queryStart + 5);
    if (queryEnd == string::npos)
    {
        errorMessage = "Steam не повернув відповідь OpenID.";
        return false;
    }

    string query = requestText.substr(
        queryStart + 6,
        queryEnd - (queryStart + 6));

    const string keys[9] =
    {
        "openid.ns",
        "openid.op_endpoint",
        "openid.claimed_id",
        "openid.identity",
        "openid.return_to",
        "openid.response_nonce",
        "openid.assoc_handle",
        "openid.signed",
        "openid.sig"
    };
    string values[9];

    for (int i = 0; i < 9; ++i)
    {
        string marker = keys[i] + "=";
        size_t valueStart = query.find(marker);
        if (valueStart == string::npos)
        {
            continue;
        }

        valueStart += marker.size();
		size_t valueEnd = query.find('&', valueStart); // & separator between key-value
        string encodedValue = query.substr(
            valueStart,
			valueEnd == string::npos ? string::npos : valueEnd - valueStart); // how many char to copy
        values[i] = urlDecode(encodedValue);
    }

    const string steamIdPrefix = "http://steamcommunity.com/openid/id/";
    if (values[2].find(steamIdPrefix) != 0)
    {
        errorMessage = "Steam повернув неправильний ідентифікатор профілю.";
        return false;
    }

    steamId = values[2].substr(steamIdPrefix.size());
    if (!isSteamId(steamId))
    {
        errorMessage = "Steam повернув некоректний SteamID.";
        return false;
    }

    CURL* curl = curl_easy_init();
    if (curl == nullptr)
    {
        errorMessage = "Не вдалося перевірити відповідь Steam.";
        return false;
    }

	postData = "openid.mode=check_authentication";
    for (int i = 0; i < 9; ++i)
    {
		char* encodedValue = curl_easy_escape( // encode value for HTTP POST bc steam OpenID response may contain special characters
            curl,
            values[i].c_str(),
			static_cast<int>(values[i].size())); // size_t to int

        if (encodedValue != nullptr)
        {
			postData += "&" + keys[i] + "=" + encodedValue;
            curl_free(encodedValue);
        }
    }

    curl_easy_cleanup(curl);
    return true;
}

static bool verifyOpenIdResponse(
    const string& postData,
    string& errorMessage)
{
    CURL* curl = curl_easy_init();
    if (curl == nullptr)
    {
        errorMessage = "Не вдалося перевірити відповідь Steam.";
        return false;
    }

    string response;
    curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/openid/login");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receiveData);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);

    CURLcode result = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK || response.find("is_valid:true") == string::npos) // ns:http://specs.openid.net/auth/2.0 is_valid:true
    {
        errorMessage = "Steam не підтвердив вхід OpenID.";
        return false;
    }

    return true;
}
SteamClient::SteamClient()
{
    char* key = nullptr;
	size_t keySize = 0; // api key size and key string
    errno_t result = _dupenv_s(&key, &keySize, "STEAM_WEB_API_KEY");
    if (result == 0 && key != nullptr)
    {
        apiKey = key;
    }

    free(key);
	curl_global_init(CURL_GLOBAL_DEFAULT); // Initialize libcurl globally
}

string SteamClient::getError() const
{
    return errorMessage;
}

string SteamClient::request(const string& url)
{
    CURL* curl = curl_easy_init();
    if (curl == nullptr)
    {
        errorMessage = "Не вдалося створити HTTP-запит.";
        return "";
    }

    string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receiveData);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "GameCollectionStudentProject/1.0");

    CURLcode result = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    if (result != CURLE_OK || status < 200 || status >= 300)
    {
        errorMessage = "Помилка мережі Steam: " + string(curl_easy_strerror(result));
        if (status != 0)
        {
            errorMessage += " (HTTP " + to_string(status) + ")";
        }

        return "";
    }

    return response;
}

string SteamClient::resolveSteamId(const string& input) // https://steamcommunity.com/profiles/76561199231911456
{
    if (isSteamId(input))
    {
        return input;
    }

    string name = input;
    size_t slash = name.find_last_of('/');

    if (slash != string::npos)
    {
        name = name.substr(slash + 1);
    }
    if (name.find("id/") == 0)
    {
        name = name.substr(3);
    }
    if (isSteamId(name))
    {
        return name;
    }

    if (apiKey.empty())
    {
        errorMessage =
            "Для пошуку vanity-посилання потрібна змінна STEAM_WEB_API_KEY; "
            "введіть 64-бітний SteamID або скористайтеся OpenID.";
        return "";
    }
    if (name.empty() || name.find("profiles/") == 0)
    {
        errorMessage = "Перевірте посилання або SteamID.";
        return "";
    }

    string url =
        "https://api.steampowered.com/ISteamUser/ResolveVanityURL/v1/?key=" +
        apiKey + "&vanityurl=" + name;
    json data = json::parse(request(url), nullptr, false); // response: { "steamid": "76561199231911456", "success": 1 }
    if (data.is_discarded() || 
		!data.contains("response") || 
        data["response"].value("success", 0) != 1)
    {
        errorMessage = "Steam не зміг визначити SteamID за введеним посиланням.";
        return "";
    }

    return data["response"].value("steamid", "");
}

bool SteamClient::connectBySteamId(const string& input, Collection& collection)
{
    errorMessage.clear();
    string id = resolveSteamId(input);
    if (id.empty())
    {
        return false;
    }

    if (apiKey.empty())
    {
        collection.setSteamProfile(id, "");
        errorMessage = "SteamID збережено. Для профілю та імпорту бібліотеки потрібен STEAM_WEB_API_KEY.";
        return true;
    }

    string url =
        "https://api.steampowered.com/ISteamUser/GetPlayerSummaries/v2/?key=" +
        apiKey + "&steamids=" + id;
    json data = json::parse(request(url), nullptr, false); // response: { "players": [ { "personaname": "User Name" } ] }
    if (data.is_discarded() ||
        !data.contains("response") ||
        data["response"]["players"].empty())
    {
        errorMessage = "Профіль Steam недоступний або не знайдений.";
        return false;
    }

    string name = data["response"]["players"][0].value("personaname", "Steam користувач");
    collection.setSteamProfile(id, name);
    return true;
}
bool SteamClient::connectWithOpenId(Collection& collection)
{
    errorMessage.clear();
    string requestText;
    if (!waitForOpenIdResponse(requestText, errorMessage))
    {
        return false;
    }

    string steamId;
    string postData;
    if (!parseOpenIdResponse(requestText, steamId, postData, errorMessage))
    {
        return false;
    }

    if (!verifyOpenIdResponse(postData, errorMessage))
    {
        return false;
    }

    collection.setSteamProfile(steamId, "");
    if (apiKey.empty())
    {
        errorMessage = "Вхід OpenID підтверджено. Для отримання назви профілю та імпорту потрібен STEAM_WEB_API_KEY.";
        return true;
    }

    return connectBySteamId(steamId, collection);
}
string SteamClient::getSteamGenre(unsigned int appId)
{
    string url =
        "https://store.steampowered.com/api/appdetails?appids=" +
        to_string(appId) + "&l=uk";
    json data = json::parse(request(url), nullptr, false);
	string key = to_string(appId); // app id is the key in the json response { "123456": { "success": true, "data": { "genres": [ { "id": "1", "description": "Action" } ] } } }
    if (data.is_discarded() ||
        !data.contains(key) ||
        !data[key].value("success", false) ||
        !data[key].contains("data") ||
        !data[key]["data"].contains("genres"))
    {
        return "";
    }

    string genre;
    for (const auto& item : data[key]["data"]["genres"])
    {
        string name = item.value("description", "");
        if (!name.empty())
        {
            if (!genre.empty())
            {
                genre += ", ";
            }

            genre += name;
        }
    }

    return genre;
}

bool SteamClient::importLibrary(Collection& collection)
{
    if (apiKey.empty())
    {
        errorMessage = "Для Steam Web API задайте змінну STEAM_WEB_API_KEY.";
        return false;
    }
    if (collection.getSteamId().empty())
    {
        errorMessage = "Спочатку підключіть профіль Steam.";
        return false;
    }

    string url =
        "https://api.steampowered.com/IPlayerService/GetOwnedGames/v1/?key=" +
        apiKey + "&steamid=" + collection.getSteamId() +
        "&include_appinfo=1&include_played_free_games=1";
    json data = json::parse(request(url), nullptr, false);
    if (data.is_discarded() ||
        !data.contains("response") ||
        !data["response"].contains("games"))
    {
        errorMessage = "Бібліотека недоступна. Перевірте приватність профілю Steam.";
        return false;
    }

    int added = 0;
    int updated = 0;
    for (const auto& steamGame : data["response"]["games"])
    {
        unsigned int appId = steamGame.value("appid", 0u);
        string name = steamGame.value("name", "Невідома гра");
        Game* existing = nullptr;

        for (Game& game : collection.getGames())
        {
            if (game.getAppId() == appId && appId != 0)
            {
                existing = &game;
                break;
            }
        }

        if (existing)
        {
            existing->setTitle(name);
            existing->setPlatform("Steam");
            if (existing->getGenre().empty()) existing->setGenre(getSteamGenre(appId));
            ++updated;
        }
        else
        {
            Game game(0, appId, name);
            game.setPlatform("Steam");
            game.setStatus("Не розпочато");
            game.setGenre(getSteamGenre(appId));
            collection.addGame(game);
            ++added;
        }
    }

    int achievementGames = 0;
    for (Game& game : collection.getGames())
    {
        if (game.getAppId() == 0)
        {
            continue;
        }

        string achievementUrl =
            "https://api.steampowered.com/ISteamUserStats/GetPlayerAchievements/v1/?key=" +
            apiKey + "&steamid=" + collection.getSteamId() +
            "&appid=" + to_string(game.getAppId());
        json achievementData = json::parse(request(achievementUrl), nullptr, false);
        if (achievementData.is_discarded() ||
            !achievementData.contains("playerstats") ||
            !achievementData["playerstats"].value("success", false) ||
            !achievementData["playerstats"].contains("achievements"))
        {
            continue;
        }

        vector<Achievement> achievements;
        for (const auto& item : achievementData["playerstats"]["achievements"])
        {
            Achievement achievement;
            achievement.name = item.value("apiname", "Досягнення");
            achievement.description = item.value("name", "");
            achievement.unlocked = item.value("achieved", 0) != 0;
            achievements.push_back(achievement);
        }

        game.setAchievements(achievements);
        ++achievementGames;
    }

    errorMessage =
        "Імпортовано ігор: " + to_string(added) +
        ", оновлено: " + to_string(updated) +
        ". Отримано досягнення для ігор: " + to_string(achievementGames) +
        ". Інші ігри могли не повернути дані або не підтримувати досягнення.";
    return true;
}
int SteamClient::scanInstalledGames(Collection& collection)
{
    errorMessage.clear();
    vector<string> libraries;

    char steamPath[MAX_PATH] = {};
    DWORD pathSize = sizeof(steamPath);
    LONG result = RegGetValueA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", "SteamPath",
        RRF_RT_REG_SZ, nullptr, steamPath, &pathSize);

    if (result == ERROR_SUCCESS)
    {
        libraries.push_back(steamPath);
    }

    if (libraries.empty())
    {
        char programFiles[MAX_PATH] = {};
        DWORD length = GetEnvironmentVariableA("ProgramFiles(x86)", programFiles, MAX_PATH);
        if (length > 0 && length < MAX_PATH)
        {
            string defaultPath = string(programFiles) + "\\Steam";
            if (filesystem::exists(filesystem::u8path(defaultPath)))
            {
                libraries.push_back(defaultPath);
            }
        }
    }

    if (libraries.empty())
    {
        errorMessage = "Не знайдено шлях до Steam у реєстрі Windows.";
        return -1;
    }

    string foldersFile = libraries[0] + "\\steamapps\\libraryfolders.vdf";
    ifstream folders(foldersFile);
    if (folders)
    {
        string line;
        while (getline(folders, line))
        {
            vector<string> values = readQuotedValues(line);
            if (values.size() < 2)
            {
                continue;
            }

            bool isPath = values[0] == "path";
            bool isLibraryNumber = !values[0].empty() &&
                values[0].find_first_not_of("0123456789") == string::npos;

            if (isPath || isLibraryNumber)
            {
                bool isAbsolutePath =
                    values[1].find(":\\") != string::npos ||
                    values[1].find("\\\\") == 0;

                if (isAbsolutePath)
                {
                    libraries.push_back(values[1]);
                }
            }
        }
    }

    struct LocalGameInfo
    {
        unsigned int appId;
        string name;
    };

    vector<LocalGameInfo> installedGames;
    bool libraryFound = false;

    try
    {
        for (const string& library : libraries)
        {
            filesystem::path steamApps = filesystem::u8path(library) / "steamapps";
            if (!filesystem::exists(steamApps))
            {
                continue;
            }

            libraryFound = true;
            for (const auto& entry : filesystem::directory_iterator(steamApps))
            {
                string fileName = entry.path().filename().string();
                bool isManifest =
                    fileName.find("appmanifest_") == 0 &&
                    entry.path().extension() == ".acf";
                if (!isManifest)
                {
                    continue;
                }

                ifstream manifest(entry.path());
                if (!manifest)
                {
                    continue;
                }

                string line;
                string name;
                string installDir;
                unsigned int appId = 0;
                unsigned int stateFlags = 0;

                while (getline(manifest, line))
                {
                    readManifestField(line, appId, stateFlags, name, installDir);
                }
                bool isMarkedInstalled = (stateFlags & 4) != 0;
                if (appId == 0 || !isMarkedInstalled || installDir.empty())
                {
                    continue;
                }

                filesystem::path installedPath =
                    steamApps / "common" / filesystem::u8path(installDir);
                if (!filesystem::exists(installedPath))
                {
                    continue;
                }

                installedGames.push_back({ appId, name });
            }
        }
    }
    catch (const filesystem::filesystem_error&)
    {
        errorMessage = "Помилка під час читання папок Steam.";
        return -1;
    }

    if (!libraryFound)
    {
        errorMessage = "Не вдалося знайти папки steamapps у налаштованих бібліотеках Steam.";
        return -1;
    }

    for (Game& game : collection.getGames())
    {
        if (game.getAppId() != 0)
        {
            game.setInstalled(false);
        }
    }

    for (const LocalGameInfo& localGame : installedGames)
    {
        Game* game = nullptr;

        for (Game& item : collection.getGames())
        {
            if (item.getAppId() == localGame.appId)
            {
                game = &item;
                break;
            }
        }

        if (game)
        {
            game->setPlatform("Steam");
            game->setInstalled(true);
            if (!localGame.name.empty())
            {
                game->setTitle(localGame.name);
            }
        }
        else
        {
            string title = localGame.name.empty()
                ? "Невідома гра"
                : localGame.name;
            Game newGame(0, localGame.appId, title);
            newGame.setPlatform("Steam");
            newGame.setStatus("Не розпочато");
            newGame.setInstalled(true);
            collection.addGame(newGame);
        }
    }

    errorMessage = "Знайдено встановлених ігор Steam: " + to_string(installedGames.size()) + ".";
    return static_cast<int>(installedGames.size());
}

bool SteamClient::launchInstalledGame(const Game& game)
{
    errorMessage.clear();
    if (!game.isInstalled() || game.getAppId() == 0)
    {
        errorMessage =
            "Цю гру не позначено як встановлену. Спочатку перевірте бібліотеку Steam.";
        return false;
    }

    string command = "steam://run/" + to_string(game.getAppId());
    HINSTANCE result = ShellExecuteA(
        nullptr,
        "open",
        command.c_str(),
        nullptr,
        nullptr,
        SW_SHOWNORMAL);

    if (reinterpret_cast<INT_PTR>(result) <= 32)
    {
        errorMessage = "Не вдалося відкрити Steam для запуску гри.";
        return false;
    }

    return true;
}
