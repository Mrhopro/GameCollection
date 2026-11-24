#include <windows.h>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>

#include "BinaryStorage.h"
#include "Collection.h"
#include "SteamClient.h"

using namespace std;

static string readLine(const string& prompt)
{
    cout << prompt;

    string value;
    getline(cin, value);
    return value;
}

static int readNumber(const string& prompt, int minimum, int maximum)
{
    while (true)
    {
        string input = readLine(prompt);
        istringstream inputStream(input);
        int value = 0;
        char extraCharacter = '\0';

        if (inputStream >> value &&
            !(inputStream >> extraCharacter) &&
            value >= minimum &&
            value <= maximum)
        {
            return value;
        }

        cout << "Введіть ціле число від " << minimum << " до " << maximum << ".\n";
    }
}

static void printGame(const Game& game)
{
    cout << "\nID: " << game.getId()
        << " | Назва: " << game.getTitle()
        << " | Жанр: " << game.getGenre()
        << " | Платформа: " << game.getPlatform()
        << " | Статус: " << game.getStatus()
        << " | Оцінка: " << game.getRating() << "/10";

    if (game.getAppId() != 0)
    {
        cout << " | Steam AppID: " << game.getAppId();
    }

    cout << "\nДосягнень: " << game.getAchievements().size() << "\n";
    cout << "Встановлення: "
        << (game.isInstalled() ? "встановлено" : "не встановлено")
        << "\n";
}

static void addGame(Collection& collection)
{
    string title = readLine("Назва гри: ");
    if (title.empty())
    {
        cout << "Назва не може бути порожньою.\n";
        return;
    }

    Game game(0, 0, title);
    game.setGenre(readLine("Жанр: "));
    game.setPlatform(readLine("Платформа: "));
    game.setStatus(readLine("Статус проходження: "));
    game.setRating(readNumber("Оцінка (0-10): ", 0, 10));

    int id = collection.addGame(game);
    cout << "Гру додано з ID " << id << ".\n";
}

static void showGames(const Collection& collection)
{
    if (collection.getGames().empty())
    {
        cout << "Колекція порожня.\n";
        return;
    }

    for (const Game& game : collection.getGames())
    {
        printGame(game);
    }
}

static void searchGame(const Collection& collection)
{
    string name = readLine("Частина назви: ");
    bool found = false;

    for (const Game& game : collection.getGames())
    {
        if (game.getTitle().find(name) != string::npos)
        {
            printGame(game);
            found = true;
        }
    }

    if (!found)
    {
        cout << "Ігор не знайдено.\n";
    }
}

static void editGame(Collection& collection)
{
    int id = readNumber("ID гри: ", 1, 1000000000);
    Game* game = collection.findGame(id);
    if (game == nullptr)
    {
        cout << "Гру не знайдено.\n";
        return;
    }

    string value = readLine("Нова назва (Enter — залишити): ");
    if (!value.empty())
    {
        game->setTitle(value);
    }

    value = readLine("Новий жанр (Enter — залишити): ");
    if (!value.empty())
    {
        game->setGenre(value);
    }

    value = readLine("Нова платформа (Enter — залишити): ");
    if (!value.empty())
    {
        game->setPlatform(value);
    }

    value = readLine("Новий статус (Enter — залишити): ");
    if (!value.empty())
    {
        game->setStatus(value);
    }

    value = readLine("Нова оцінка 0-10 (Enter — залишити): ");
    if (!value.empty())
    {
        istringstream inputStream(value);
        int rating = 0;
        char extraCharacter = '\0';

        if (inputStream >> rating &&
            !(inputStream >> extraCharacter) &&
            rating >= 0 &&
            rating <= 10)
        {
            game->setRating(rating);
        }
        else
        {
            cout << "Оцінка має бути цілим числом від 0 до 10. Стару оцінку залишено.\n";
        }
    }

    cout << "Дані оновлено.\n";
}

static void deleteGame(Collection& collection)
{
    int id = readNumber("ID гри для видалення: ", 1, 1000000000);
    if (collection.deleteGame(id))
    {
        cout << "Гру видалено.\n";
    }
    else
    {
        cout << "Гру не знайдено.\n";
    }
}

static void sortGames(Collection& collection)
{
    sort(
        collection.getGames().begin(),
        collection.getGames().end(),
        [](const Game& first, const Game& second)
        {
            return first.getTitle() < second.getTitle();
        });

    cout << "Колекцію відсортовано за назвою.\n";
}

static bool matchesFilter(
    const Game& game,
    const string& genre,
    const string& platform,
    const string& status,
    int minimumRating,
    int maximumRating,
    int installFilter)
{
    bool matchesGenre = genre.empty() || game.getGenre() == genre;
    bool matchesPlatform = platform.empty() || game.getPlatform() == platform;
    bool matchesStatus = status.empty() || game.getStatus() == status;
    bool matchesRating =
        game.getRating() >= minimumRating && game.getRating() <= maximumRating;

    bool matchesInstallStatus = false;
    switch (installFilter)
    {
    case 0:
        matchesInstallStatus = true;
        break;
    case 1:
        matchesInstallStatus = game.isInstalled();
        break;
    case 2:
        matchesInstallStatus = !game.isInstalled();
        break;
    }

    return matchesGenre &&
        matchesPlatform &&
        matchesStatus &&
        matchesRating &&
        matchesInstallStatus;
}

static void filterGames(const Collection& collection)
{
    string genre = readLine("Жанр для фільтра: ");
    string platform = readLine("Платформа для фільтра: ");
    string status = readLine("Статус для фільтра: ");

    int minimumRating = readNumber("Мінімальна оцінка (0-10): ", 0, 10);
    int maximumRating = readNumber(
        "Максимальна оцінка (0-10): ",
        minimumRating,
        10);
    int installFilter = readNumber(
        "Встановлення: 0 — будь-яке, 1 — встановлено, 2 — не встановлено: ",
        0,
        2);

    int shown = 0;
    for (const Game& game : collection.getGames())
    {
        if (matchesFilter(
            game,
            genre,
            platform,
            status,
            minimumRating,
            maximumRating,
            installFilter))
        {
            printGame(game);
            ++shown;
        }
    }

    if (shown == 0)
    {
        cout << "За фільтром ігор немає.\n";
    }
}

static void launchGame(Collection& collection, SteamClient& steam)
{
    int id = readNumber("ID встановленої гри: ", 1, 1000000000);
    Game* game = collection.findGame(id);
    if (game == nullptr)
    {
        cout << "Гру не знайдено.\n";
        return;
    }

    if (steam.launchInstalledGame(*game))
    {
        cout << "Команду запуску передано Steam.\n";
    }
    else
    {
        cout << steam.getError() << "\n";
    }
}

static void showStatistics(const Collection& collection)
{
    int ratedGames = 0;
    int ratingSum = 0;
    int completedGames = 0;
    int unlockedAchievements = 0;

    for (const Game& game : collection.getGames())
    {
        if (game.getRating() > 0)
        {
            ++ratedGames;
            ratingSum += game.getRating();
        }

        if (game.getStatus() == "Пройдено")
        {
            ++completedGames;
        }

        for (const Achievement& achievement : game.getAchievements())
        {
            if (achievement.unlocked)
            {
                ++unlockedAchievements;
            }
        }
    }

    cout << "Ігор: " << collection.getGames().size()
        << ", пройдено: " << completedGames
        << ", розблоковано досягнень: " << unlockedAchievements;

    if (ratedGames > 0)
    {
        double averageRating = ratingSum / static_cast<double>(ratedGames);
        cout << ", середня оцінка: " << averageRating;
    }

    cout << "\n";
}

static void connectSteamById(SteamClient& steam, Collection& collection)
{
    string input = readLine("Введіть 64-бітний SteamID або посилання на профіль: ");
    if (!steam.connectBySteamId(input, collection))
    {
        cout << steam.getError() << "\n";
        return;
    }

    cout << "Профіль SteamID підключено: " << collection.getSteamId();
    if (!collection.getSteamName().empty())
    {
        cout << " (" << collection.getSteamName() << ")";
    }

    cout << "\n" << steam.getError() << "\n";
}

static void connectSteamWithOpenId(SteamClient& steam, Collection& collection)
{
    if (steam.connectWithOpenId(collection))
    {
        cout << "Профіль Steam підключено: " << collection.getSteamId() << "\n";
        cout << steam.getError() << "\n";
    }
    else
    {
        cout << steam.getError() << "\n";
    }
}

static void importSteamLibrary(SteamClient& steam, Collection& collection)
{
    steam.importLibrary(collection);
    cout << steam.getError() << "\n";
}

static void scanSteamLibraries(SteamClient& steam, Collection& collection)
{
    int count = steam.scanInstalledGames(collection);
    cout << steam.getError() << "\n";

    if (count >= 0)
    {
        cout << "Записи колекції оновлено.\n";
    }
}

static void showMenu()
{
    cout << "\n=== ОБЛІК КОЛЕКЦІЇ ВІДЕОІГОР ===\n"
        << "1. Додати гру вручну\n"
        << "2. Показати всі ігри\n"
        << "3. Пошук\n"
        << "4. Редагувати гру\n"
        << "5. Видалити гру\n"
        << "6. Сортувати за назвою\n"
        << "7. Фільтр\n"
        << "8. Статистика\n"
        << "9. Підключити Steam за ID/посиланням\n"
        << "10. Вхід Steam OpenID\n"
        << "11. Імпортувати бібліотеку\n"
        << "12. Перевірити встановлені ігри на комп'ютері\n"
        << "13. Запустити встановлену гру\n"
        << "14. Зберегти\n"
        << "0. Вихід\n";
}

static bool saveCollection(const string& fileName, const Collection& collection)
{
    if (BinaryStorage::save(fileName, collection))
    {
        cout << "Дані збережено у бінарний файл.\n";
        return true;
    }

    cout << "Помилка запису файла.\n";
    return false;
}

int main()
{
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);

    Collection collection;
    const string dataFile = "data/collection.dat";
    CreateDirectoryA("data", nullptr);

    if (GetFileAttributesA(dataFile.c_str()) != INVALID_FILE_ATTRIBUTES &&
        !BinaryStorage::load(dataFile, collection))
    {
        cout << "Файл даних пошкоджений або має невідомий формат; "
            << "починаємо з порожньої колекції.\n";
    }

    SteamClient steam;
    bool running = true;

    while (running)
    {
        showMenu();
        int choice = readNumber("Ваш вибір: ", 0, 14);

        switch (choice)
        {
        case 1:
            addGame(collection);
            break;
        case 2:
            showGames(collection);
            break;
        case 3:
            searchGame(collection);
            break;
        case 4:
            editGame(collection);
            break;
        case 5:
            deleteGame(collection);
            break;
        case 6:
            sortGames(collection);
            break;
        case 7:
            filterGames(collection);
            break;
        case 8:
            showStatistics(collection);
            break;
        case 9:
            connectSteamById(steam, collection);
            break;
        case 10:
            connectSteamWithOpenId(steam, collection);
            break;
        case 11:
            importSteamLibrary(steam, collection);
            break;
        case 12:
            scanSteamLibraries(steam, collection);
            break;
        case 13:
            launchGame(collection, steam);
            break;
        case 14:
            saveCollection(dataFile, collection);
            break;
        case 0:
            if (!BinaryStorage::save(dataFile, collection))
            {
                cout << "Не вдалося зберегти колекцію.\n";
            }
            running = false;
            break;
        }
    }

    return 0;
}
