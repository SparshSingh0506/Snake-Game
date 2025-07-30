#include <raylib.h>
#include <raymath.h>
#include <string>
#include <deque>

constexpr int gridWidth = 800;
constexpr int gridHeight = 800;
constexpr int offset = 50;

constexpr int cellSize = 50;
constexpr float rows = gridHeight / cellSize;
constexpr float cols = gridWidth / cellSize;

class GameSettings {
    friend class Snake;
    friend class GameCore;

private:
    enum class Difficulty {Easy, Medium, Hard};

    static double getInterval(Difficulty difficulty) {//return difficulty interval
        switch(difficulty) {
            case Difficulty::Easy : return 0.5; break;
            case Difficulty::Medium : return 0.3; break;
            case Difficulty::Hard : return 0.1; break;
            default : return 0.6;
        }
    }

    static Difficulty setDifficulty(std::string sDifficulty) {
        if (sDifficulty == "Easy") return Difficulty::Easy;
        else if (sDifficulty == "Medium") return Difficulty::Medium;
        else if (sDifficulty == "Hard") return Difficulty::Hard;
        return Difficulty::Easy; //default
    }

public:
    static void gameInit() {
        InitWindow(900, 950, "Snake");
        SetTargetFPS(60);
    }
};

class Background {
private:
    //all static as no object of this class will be made

    static inline constexpr Color BackgroundColor = {73, 98, 58, 191};
    static inline constexpr Color GridColor = {200, 200, 200, 50};

    static inline constexpr int borderCorrection = 8;

    static inline const Rectangle borderRectangle = {
        offset - borderCorrection, 
        offset - borderCorrection, 
        gridWidth + 2 * borderCorrection,
        gridHeight + 2 * borderCorrection
    };
    //not constexpr as value is being defined at runtime

public:
    static void Draw() {
        ClearBackground(BackgroundColor);

        DrawRectangleLinesEx(borderRectangle, 8, BLACK); 
        //Ex gives more control over regular function

        for (int i = 0; i <= rows; i++) {
            DrawLine(offset, offset + i * cellSize, offset + cols * cellSize, offset + i * cellSize, GridColor);
        }

        for (int j = 0; j <= cols; j++) {
            DrawLine(offset + j * cellSize, offset, offset + j * cellSize, offset + rows * cellSize, GridColor);
        }

    }
};

class Food {
    friend class CollisionHandler;

private:
    Color FoodColor = {255, 50, 50, 191};
    Texture2D appleTexture; //to access it both in constructor and destructor
    Vector2 applePos;
    std::deque<Vector2> snakeBody;

    void handleTexture() {
        Image appleImg = LoadImage("Graphics/Apple.png"); //loads image to RAM
        ImageResize(&appleImg, cellSize, cellSize);

        appleTexture = LoadTextureFromImage(appleImg); //load from RAM to GPU VRAM for performance
        UnloadImage(appleImg); //remove image from RAM after VRAM stores it as texture
    }

    bool shouldGenerateAgain(const std::deque<Vector2>& snakeBody, const Vector2& pos) const {
        for (auto body : snakeBody) if (pos == body) return true; //if random pos aligns on snake body, generate pos again 
        return false;
    }

    Vector2 generateRandomPos(const std::deque<Vector2>& snakeBody) {
        Vector2 pos;

        do {
        float randX = GetRandomValue(0, rows - 1);//both inclusive
        float randY = GetRandomValue(0, cols - 1);

        pos = {randX * cellSize + offset, randY * cellSize + offset};

        } while (shouldGenerateAgain(snakeBody, pos)); //do while loop instead of recursion avoids stack overflow as snake grows

        return pos;
    }

public:
    Food(const std::deque<Vector2>& snakeBody) {
        handleTexture();
        this -> snakeBody = snakeBody;
        applePos = generateRandomPos(snakeBody); 
    }

    ~Food() {
        UnloadTexture(appleTexture); //remove from VRAM as well once finished
    }

    void Draw() const {
        DrawTexture(appleTexture, applePos.x, applePos.y, WHITE);//white means no tint on image
    }
};

class Snake {
    friend class CollisionHandler;
    friend class GameCore;

private:
   GameSettings::Difficulty difficulty;

   double interval = GameSettings::getInterval(difficulty);

   bool addSegment = false;

    std::deque<Vector2> snakeBody = { //O(1) operations for deque!
        Vector2{((cols * cellSize) / 2 - cellSize), ((rows * cellSize) / 2 - cellSize)}, 
        Vector2{((cols * cellSize) / 2), ((rows * cellSize) / 2 - cellSize)}
    };

    Vector2 lastDirection = {-1, 0};//first time for default value later to be updated

    Vector2 getMoveDirection() {
        if (IsKeyDown(KEY_W) && lastDirection.y != 1) lastDirection = {0, -1};
        else if (IsKeyDown(KEY_S) && lastDirection.y != -1) lastDirection = {0, 1};
        else if (IsKeyDown(KEY_A) && lastDirection.x != 1) lastDirection = {-1, 0};
        else if (IsKeyDown(KEY_D) && lastDirection.x != -1) lastDirection = {1, 0};

        return (lastDirection * cellSize);
    }

    void moveSnake(const Vector2& direction){
        snakeBody.push_front(Vector2Add(snakeBody[0], direction));

        if (addSegment) addSegment = false;
        else snakeBody.pop_back();
        //when addSegment is true we just dont pop back for that frame, adding a segment
    }

public:
    Snake(GameSettings::Difficulty difficulty) : difficulty(difficulty) {}

    void Draw() const {
        for (const Vector2 segment : snakeBody) {
            Rectangle snakeRect = {segment.x, segment.y, cellSize, cellSize};
            DrawRectangleRounded(snakeRect, 0.5, 10, PURPLE);
        }
    }

    void Update() {
        static double lastUpdateTime = 0;
        double currentTime = GetTime();

        Vector2 direction = getMoveDirection();

        if (currentTime - lastUpdateTime >= interval) {
            moveSnake(direction);
            lastUpdateTime = currentTime;
        }
    }
};

class CollisionHandler {
    friend class GameCore;
    friend class ScoreHandler;

private:
    Food* apple;
    Snake* playerSnake; //pointer to directly access snake and food objects instantiated in GameCore class

    CollisionHandler(Food* apple, Snake* playerSnake) : apple(apple), playerSnake(playerSnake) {}
    //private constructor will be called as GameCore is a friend

    bool foodEaten = false;

    void foodCollisionHandle() {
        if (Vector2Equals(playerSnake->snakeBody[0], apple->applePos)) {
            apple->applePos = apple->generateRandomPos(playerSnake->snakeBody);
            playerSnake->addSegment = true;

            foodEaten = true;
        }
    }

    bool gameOver = false;

    void borderCollisionHandle(const Vector2& snakeHead) {
        if (
            snakeHead.x < offset || snakeHead.x >= gridWidth + offset || 
            snakeHead.y < offset || snakeHead.y >= gridHeight + offset 
        ) gameOver = true;
    }

    void selfCollisionHandle (const Vector2& snakeHead) {
        for (unsigned short int i = 1; i < playerSnake->snakeBody.size(); i++) { 
            if (Vector2Equals(snakeHead, playerSnake->snakeBody[i])) {
                gameOver = true;
                break;
            }
        }
    }

    bool handle() { 
        Vector2 snakeHead = playerSnake->snakeBody[0]; //not at class level else it wont update every update call

        foodCollisionHandle();
        borderCollisionHandle(snakeHead);
        selfCollisionHandle(snakeHead);

        return gameOver;
    }
};

class ScoreHandler {
    friend class GameCore;

private:
    static inline int score = 0;
    static inline int length = 2;

    const int scoreMultiplier = 10;

    CollisionHandler* foodCollision;

    void Update() {
        if (foodCollision->foodEaten) {
            score += scoreMultiplier;
            length++;

            foodCollision->foodEaten = false;
        }
    }

    void Draw() const {
        DrawText(TextFormat("Score : %d", score), offset, gridHeight + 1.5 * offset, 50, ORANGE);
        DrawText(TextFormat("Length : %d", length), gridWidth - 4 * offset, gridHeight + 1.5 * offset, 50, ORANGE);
    }

public:
    ScoreHandler(CollisionHandler* foodCollision) : foodCollision(foodCollision) {}
};

class GameCore {
private:
    GameSettings::Difficulty difficulty;
    Snake playerSnake; //initialize first as apple.generateRandomPos() needs snake reference
    Food apple;
    CollisionHandler collision;
    ScoreHandler scoreBoard;

    bool gameOver = false;

    void gameOverDraw() const {
        if (gameOver) {
            unsigned char alpha = ((sinf(GetTime() * 3) + 1) * 0.5) * 255;
            //sin(x) + 1 -> range shifts from -1 -> 1 to 0 -> 2 (mx + c), * 0.5 -> makes range 0 to 1, GetTime() * 4 is speed, * 255 for alpha
            Color FlashingRed = {255, 0, 0, alpha};
            DrawText("GAME OVER!", (gridWidth / 2) - 4 * cellSize, (gridHeight / 2) , 80, FlashingRed);
        }
    }

    void Update() {
        if (!gameOver) {
            playerSnake.Update(); //stopping this also stops random apple pos generation
            scoreBoard.Update();
            gameOver = collision.handle();
        }
    }

    void Draw() const {
        Background::Draw();
        apple.Draw();
        playerSnake.Draw();
        scoreBoard.Draw();

        gameOverDraw();
    }

public:
    GameCore(std::string sDifficulty) :
        playerSnake(GameSettings::setDifficulty(sDifficulty)), 
        apple(playerSnake.snakeBody),
        collision(&apple, &playerSnake),
        scoreBoard(&collision)
    {}

    void exec() {
        while (!WindowShouldClose()) {
            BeginDrawing();

            Update();
            Draw();

            EndDrawing();
        }

        CloseWindow();
    }
};

int main() {
    GameSettings::gameInit();

    GameCore game("Medium");
        
    game.exec();

    return 0;
}