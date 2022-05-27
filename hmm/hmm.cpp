#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#define SDL_MAIN_HANDLED
#include "SDL2/SDL.h"
#include "SDL2/SDL_image.h"
#include "SDL2/SDL_ttf.h"

typedef unsigned int uint;
typedef unsigned char uchar;

const int SCREEN_WIDTH = 1920;
const int SCREEN_HEIGHT = 1080;
const int GRID_WIDTH = 15;
const int GRID_HEIGHT = 11;

const int UNIT_ARR = 32;
const int OBST_ARR = 1;

const char UNITS_NAMES[][30] = 
{"archer", "basilisk", "battledwarf", "blackdragon",
 "blackknight", "boar", "cerberus", "crusader", 
 "crystaldragon", "demon", "firebird", "giant", 
 "gnoll", "golddragon", "harpy", "irongolem", 
 "lizardwarrior","mage", "monk", "obsidiangargoyle",
 "roc", "scorpicore", "troglodyte", "unicorn",
 "waterelemental", "wolfraider", "wraith"};
const char OBSTACLES_NAMES[][30] = {"rock"};

enum PlayerType
{
	Human,
	Computer
};

enum Entities
{
	Unvisited = 0,
	ActiveUnit = 1,
	Destination = 222,
	Unit = 223,
	Obstacle = 255
};

struct Vector2d
{
	float x = 0.0f;
	float y = 0.0f;
};

struct Vector2dInt
{
	int x = 0;
	int y = 0;
};


struct Time
{
	uint lastTime;
	uint deltaTime;

	void Init(uint t)
	{
		lastTime = t;
	}

	void CountDelta(uint t)
	{
		deltaTime = t - lastTime;
		lastTime = t;
	}
};


struct Sprite2d
{
	Vector2dInt size;
	char path[40];
	SDL_Texture* texture;
	uint pointersNum;

	bool Init(SDL_Renderer* renderer, char* path);
	void Init();
	void Destroy();

	bool IsReferenced()
	{
		return pointersNum;
	}
};

bool Sprite2d::Init(SDL_Renderer* renderer, char* path)
{
	SDL_Surface* surface = IMG_Load(path);
	if (!surface)
	{
		printf("Unable to load an image %s. Error: %s", path, IMG_GetError());
		return 0;
	}

	texture = SDL_CreateTextureFromSurface(renderer, surface);
	if (!texture)
	{
		printf("Unable to create a texture. Error: %s", SDL_GetError());
		return 0;
	}

	pointersNum = 1;
	size = { surface->w , surface->h };
	strcpy_s(this->path , path);
	SDL_FreeSurface(surface);
}

void Sprite2d::Init()
{
	size = { 0,0 };
	strcpy_s(this->path, "");
	texture = nullptr;
	pointersNum = 0;
}

void Sprite2d::Destroy()
{
	SDL_DestroyTexture(texture);
}


struct SpriteManager
{
	Sprite2d units[UNIT_ARR];
	Sprite2d obstacles[OBST_ARR];
	SDL_Renderer* renderer;

	void Init(SDL_Renderer * rend);
	Sprite2d* GetPointer(const char* unitName, Entities type);
	void DeletePointer(Sprite2d* sprite);
	Sprite2d* FindSprite(char* path, Sprite2d* spriteSet, int setLength);
	Sprite2d* CreateSprite(char* path, Sprite2d* spriteSet, int setLength);
	void Destroy();
	};

void SpriteManager::Init(SDL_Renderer* rend)
{
	renderer = rend;
	for (int i = 0; i < UNIT_ARR; i++)
		units[i].Init();
	for (int i = 0; i < OBST_ARR; i++)
		obstacles[i].Init();
}

void SpriteManager::DeletePointer(Sprite2d* sprite)
{
	if (sprite->pointersNum == 1)
	{
		sprite->Destroy();
		sprite->Init();
	}
	else
		sprite->pointersNum--;
}

Sprite2d* SpriteManager::GetPointer(const char* objName, Entities type)
{
	char fileName[40] = {"images\\"};
	strcat_s(fileName, objName);
	strcat_s(fileName, ".png");

	Sprite2d* sprite;
	switch (type)
	{
	case Unit:
		sprite = FindSprite(fileName, units, UNIT_ARR);
		if (sprite)
		{
			sprite->pointersNum++;
			return sprite;
		}
		else
		{
			sprite = CreateSprite(fileName, units, UNIT_ARR);
			if (sprite)
				return sprite;
		}
		break;

	case Obstacle:
		sprite = FindSprite(fileName, obstacles, OBST_ARR);
		if (sprite)
		{
			sprite->pointersNum++;
			return sprite;
		}
		else
		{
			sprite = CreateSprite(fileName, obstacles, OBST_ARR);
			if (sprite)
				return sprite;
		}
		break;
	default:
		break;
	}
}

Sprite2d* SpriteManager::FindSprite(char* path, Sprite2d* spriteSet, int setLength)
{
	int i = 0;
	while (i < setLength) // searching for sprite
	{
		if (spriteSet[i].IsReferenced())
			if (!strcmp(path, spriteSet[i].path))
				return &spriteSet[i];
		i++;
	}
	return nullptr;
}

Sprite2d* SpriteManager::CreateSprite(char* path, Sprite2d* spriteSet, int setLength)
{
	int i = 0;
	while (i < setLength)
	{
		if (!spriteSet[i].IsReferenced())
		{
			spriteSet[i].Init(renderer, path);
			return &spriteSet[i];
		}
		i++;
	}
	return nullptr;
}
void SpriteManager::Destroy()
{
	for (int i = 0; i < UNIT_ARR; i++)
		units[i].Destroy();
	for (int i = 0; i < OBST_ARR; i++)
		obstacles[i].Destroy();
}

struct DeWay
{
	void Init(Vector2d entityPosition);
	
	bool isExist;
	bool isBlocked;
	bool isReached;
	uchar length;
	uchar currentStep;
	Vector2d start;
	Vector2d destination;
	Vector2d step[GRID_WIDTH * GRID_HEIGHT / 2];
};

void DeWay::Init(Vector2d entityPosition)
{
	isExist = false;
	isBlocked = false;
	isReached = true;
	currentStep = 0;
	start = entityPosition;
	destination = start;
}


struct Unit2d
{
	void Init(SpriteManager* sMngr, const char* unitName);

	Sprite2d* spriteAlive = nullptr;
	Sprite2d* spriteDead = nullptr;
	const char* name = nullptr;
	uchar hp = 10;
	uchar attack = 3;
};

void Unit2d::Init(SpriteManager* sMngr, const char* unitName)
{
	name = unitName;
	char deadName[40];
	strcpy_s(deadName, name);
	strcat_s(deadName, "dead");
	spriteAlive = sMngr->GetPointer(name, Unit);
	spriteDead = sMngr->GetPointer((const char*)deadName, Unit);
}


struct Squad
{
	void Init(Vector2d pPos, SpriteManager* sMngr, const char* unitName);
	void CountVelocity();
	void Move(uint delta_time, Vector2d destPos);
	int Attack();
	void TakeDamage(Squad* enemy, int damage);

	Vector2d* GetCurrentStepCoordinates();
	
	bool isMoving;
	bool heCounterattac;
	float speed;
	int numberOfUnits;
	uchar frontUnitHP;
	Vector2d position;
	Vector2d destination;
	Vector2d velocity;
	Unit2d unit;
	DeWay way;
};

void Squad::Init(Vector2d pPos, SpriteManager* sMngr, const char* unitName)
{
	isMoving = false;
	heCounterattac = true;
	speed = 0.4;
	numberOfUnits = rand() % 10 + 10;
	frontUnitHP = unit.hp;
	position = pPos;
	destination = pPos;
	velocity = { 0,0 };
	way.Init(position);
	unit.Init(sMngr, unitName);
}

void Squad::CountVelocity()
{
	Vector2d posDifference = { (destination.x - position.x), (destination.y - position.y) };
	float distance = sqrt(posDifference.x * posDifference.x + posDifference.y * posDifference.y);
	//velocity = { (posDifference.x != 0) ? posDifference.x / distance : 0, (posDifference.y != 0) ? posDifference.y / distance : 0 };
	velocity = { posDifference.x / distance, posDifference.y / distance };
};

void Squad::Move(uint dt, Vector2d destPos)
{
	if (isMoving)
	{
		destination = destPos;
		if ( !(int)round( fabs(velocity.x) + fabs(velocity.y) ) ) 
		{ 
			CountVelocity(); 
		}
		Vector2d stepByFrame =
		{
			velocity.x * speed * dt,
			velocity.y * speed * dt
		};

		if (fabs(destination.x - position.x) > fabs(stepByFrame.x) || fabs(destination.y - position.y) > fabs(stepByFrame.y)) //unit func move
		{
			position =
			{
				position.x + stepByFrame.x,
				position.y + stepByFrame.y
			};
		}
		else
		{
			position = destination;
			velocity = {0,0};
			way.currentStep++;
		}
	}
}

int Squad::Attack()
{
	return unit.attack * numberOfUnits;
}

void Squad::TakeDamage(Squad* enemy, int damage)
{
	numberOfUnits -=  damage / unit.hp;
	
	if (frontUnitHP <= damage % unit.hp)
	{
		numberOfUnits--;
		frontUnitHP += unit.hp - damage % unit.hp;
	}
	else
		frontUnitHP -= damage % unit.hp;
	
	if (numberOfUnits < 0)
	{
		numberOfUnits = 0;
		frontUnitHP = 0;
	}

	if (numberOfUnits && heCounterattac)
	{
		heCounterattac = false;
		enemy->TakeDamage(this, Attack());
	}
}

Vector2d* Squad::GetCurrentStepCoordinates()
{
	return &way.step[way.currentStep];
}

struct Field
{
	float obstacleProb;
	Sprite2d* obstacle;
	Vector2dInt cellSize;
	Vector2dInt gridStart;
	uchar field[GRID_WIDTH][GRID_HEIGHT];

	void Init(Sprite2d* obst);
	bool InArea(Vector2dInt point);
	void AddEntity(Vector2dInt position, Entities ID);
	void GenerateField();
	void GenerateObst();
	void BurnField(DeWay* way);
	void ClearField();
	bool CheckUnitIsTrapped(Squad* squad, Vector2d** armyPos);
	void FindWay(DeWay* way);
	void DrawField(SDL_Renderer* renderer);
	void DrawUnit(SDL_Renderer* renderer, Squad* player, PlayerType ID, TTF_Font* font);
	void ShowInfo(SDL_Renderer* renderer, TTF_Font* font, Vector2dInt* mousePos, Squad** armies);
	void PrintFieldEntities();

	Vector2d ToScreen(Vector2dInt gridPosition)
	{
		Vector2d p =
		{
			(gridPosition.x * cellSize.x) + gridStart.x,
			(gridPosition.y * cellSize.y) + gridStart.y
		};
		return p;
	}
	
	Vector2dInt ToGrid(Vector2d screenPosition)
	{
		Vector2dInt p =
		{
			((int)round(screenPosition.x) - gridStart.x) / cellSize.x,
			((int)round(screenPosition.y) - gridStart.y) / cellSize.y
		};
		return p;
	}
};

void Field::Init(Sprite2d* obst)
{
	obstacleProb = .2f;
	cellSize = { 100, 80 };
	gridStart = 
	{ SCREEN_WIDTH / 2 - (int)((cellSize.x * (float)GRID_WIDTH / 2)),
	  SCREEN_HEIGHT / 2 - (int)((cellSize.y * (float)GRID_HEIGHT / 2)) };
	obstacle = obst;
	GenerateField();
}

bool Field::InArea(Vector2dInt point)
{
	if (point.x > gridStart.x && point.x < (gridStart.x + cellSize.x * GRID_WIDTH) &&
		point.y > gridStart.y && point.y < (gridStart.y + cellSize.y * GRID_HEIGHT))
		return true;
	return false;
}

void Field::AddEntity(Vector2dInt position, Entities ID)
{
	field[position.x][position.y] = ID;
}

void Field::GenerateField()
{
	for (int i = 0; i < GRID_HEIGHT; i++)
		for (int j = 0; j < GRID_WIDTH; j++)
			field[j][i] = Unvisited;
	GenerateObst();
}

void Field::GenerateObst()
{
	for (int i = 0; i < GRID_HEIGHT; i++)
		for (int j = 1; j < GRID_WIDTH-1; j++)
			if (rand() < (int)(RAND_MAX * obstacleProb))
				field[j][i] = Obstacle;
}

void Field::BurnField(DeWay* way)
{
	//burning way with grossfire algorithm
	//through the link to field grid we setting aim positions
	AddEntity(ToGrid(way->destination), Destination);
	uchar fireLayer = 2;
	Vector2dInt queue[GRID_WIDTH * GRID_HEIGHT / 2]{ ToGrid(way->start), {-1, -1}};
	Vector2dInt buffer[GRID_WIDTH * GRID_HEIGHT / 2]{};
	Vector2dInt tempCell;
	while (!way->isExist && !way->isBlocked)
	{
		int queueID = 0;
		for (int i = 0; queue[i].x > -1; i++)
			for (int k = -1; k < 2; k++)
				for (int l = -1; l < 2; l++)
					if (k * l == 0 && k != l)
					{
						tempCell = { queue[i].x + l , queue[i].y + k };
						if (tempCell.x > -1 && tempCell.x < GRID_WIDTH && tempCell.y > -1 && tempCell.y < GRID_HEIGHT)
							switch (field[tempCell.x][tempCell.y])
							{
							case Unvisited:
								field[tempCell.x][tempCell.y] = fireLayer;
								buffer[queueID] = tempCell;
								queueID++;
								break;
							case Destination:
								way->isExist = true;
								break;
							default:
								break;
							}
					}
		if (buffer[0].x == -1)
		{
			way->isBlocked = true;
			way->step[0] = { -1,-1 };
		}
		buffer[queueID] = { -1, -1 };
		for (int i = 0; i < queueID + 1; i++)
			queue[i] = buffer[i];
		fireLayer++;
	}
	way->length = fireLayer - 2;
}

	//clear field grid to origin
void Field::ClearField()
{
	for (int i = 0; i < GRID_HEIGHT; i++)
		for (int j = 0; j < GRID_WIDTH; j++)
			switch (field[j][i])
			{
			case Obstacle:
				break;
			case Unit:
				break;
			case ActiveUnit:
				break;
			default:
				field[j][i] = Unvisited;
				break;
			}
}

bool Field::CheckUnitIsTrapped(Squad* squad, Vector2d** armyPos)
{

	squad->way.isBlocked = true;
	Vector2dInt curCell = ToGrid(squad->position);
	Vector2dInt nearCell;
	for (int k = -1; k < 2; k++)
		for (int l = -1; l < 2; l++)
			if (k * l == 0 && k != l)
			{
				nearCell = { curCell.x + l , curCell.y + k };
				if (nearCell.x > -1 && nearCell.x < GRID_WIDTH && nearCell.y > -1 && nearCell.y < GRID_HEIGHT)
					switch (field[nearCell.x][nearCell.y])
					{
					case Unvisited:
						squad->way.isBlocked = false;
						break;
					case Unit:
						for (int i = 0; i < 8; i++)
						{
							Vector2dInt enemyCell = ToGrid(*armyPos[i]);
							if (enemyCell.x == nearCell.x && enemyCell.y == nearCell.y)
								squad->way.isBlocked = false;
						}
						break;
					default:
						break;
					}
			}
	return squad->way.isBlocked;
}

void Field::DrawField(SDL_Renderer* renderer)
{
	SDL_SetRenderDrawColor(renderer, 20, 150, 39, 255);
	SDL_RenderClear(renderer);
	SDL_Rect rect;
	// Here is the rectangle where the image will be on the screen
	SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
	SDL_Rect grid;
	grid.x = gridStart.x - 1; // Counting from the image's center but that's up to you
	grid.y = gridStart.y - 1; // Counting from the image's center but that's up to you
	grid.w = cellSize.x * GRID_WIDTH + 2;
	grid.h = cellSize.y * GRID_HEIGHT + 2;
	SDL_RenderDrawRect(renderer, &grid);//draw frame

	grid = { gridStart.x, gridStart.y, cellSize.x, cellSize.y }; // set rect for cells

	rect.w = obstacle->size.x;
	rect.h = obstacle->size.y;

	for (int i = 0; i < GRID_HEIGHT; i++)
	{
		for (int j = 0; j < GRID_WIDTH; j++)
		{
			SDL_RenderDrawRect(renderer, &grid);//draw one cell
			if (field[j][i] == Obstacle) //draw obstacle
			{
				rect.x = grid.x; //offset by x
				rect.y = grid.y; //offset by y
				SDL_RenderCopyEx(renderer, // Already know what is that
					obstacle->texture, // The image
					nullptr, // A rectangle to crop from the original image. Since we need the whole image that can be left empty (nullptr)
					&rect, // The destination rectangle on the screen.
					0, // An angle in degrees for rotation
					nullptr, // The center of the rotation (when nullptr, the rect center is taken)
					SDL_FLIP_NONE); // We don't want to flip the image, or want?
			}
			grid.x += cellSize.x;//draw grid
		}
		grid.x = gridStart.x;
		grid.y += cellSize.y;
	}
}

void Field::DrawUnit(SDL_Renderer* renderer, Squad* player, PlayerType ID, TTF_Font* font)
{
	SDL_Color color = { 0xff, 0xff, 0xff };
	char str[3];
	sprintf_s(str, "%i", player->numberOfUnits);
	SDL_Surface* text = TTF_RenderText_Solid(font, str, color);
	SDL_Texture* text_texture = SDL_CreateTextureFromSurface(renderer, text);
	SDL_Rect rect;
	rect.x = (int)round(player->position.x - 24); // Counting from the image's center but that's up to you
	rect.y = (int)round(player->position.y - 35); // Counting from the image's center but that's up to you
	rect.w = player->unit.spriteAlive->size.x;
	rect.h = player->unit.spriteAlive->size.y;

	SDL_RenderCopyEx(renderer, // Already know what is that
		(player->numberOfUnits > 0) ? player->unit.spriteAlive->texture : player->unit.spriteDead->texture, // The image
		nullptr, // A rectangle to crop from the original image. Since we need the whole image that can be left empty (nullptr)
		&rect, // The destination rectangle on the screen.
		0, // An angle in degrees for rotation
		nullptr, // The center of the rotation (when nullptr, the rect center is taken)
		(ID) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE); // We don't want to flip the image
	(player->isMoving) ? SDL_SetRenderDrawColor(renderer, 0x20, 0xFF, 0x20, 0x00) : SDL_SetRenderDrawColor(renderer, 0x20, 0x20, 0xFF, 0x00);
	SDL_Rect number =
	{
		rect.x + rect.w / 4 * 3,
		rect.y + rect.h / 4 * 3,
		50,
		20
	};
	SDL_RenderFillRect(renderer, &number);
	SDL_SetRenderDrawColor(renderer, 0xf0, 0xf0, 0xf0, 0x00);
	SDL_RenderDrawRect(renderer, &number);
	number = 
	{
		number.x += 25 - text->w/2,
		number.y += 10 - text->h / 2,
		text->w,
		text->h

	};
	SDL_RenderCopy(renderer, text_texture, nullptr, &number);
	SDL_FreeSurface(text);
	SDL_DestroyTexture(text_texture);
}

	//write gained way into step array
void Field::FindWay(DeWay* way)
{
	uchar stepNumber = way->length;
	if (way->isExist)
	{
		way->step[stepNumber] = { -1, -1 };
		stepNumber--;
		way->step[stepNumber] = way->destination;
		Vector2dInt nearCell;
		Vector2dInt curCell;
		while (stepNumber > 0)
		{
			curCell = ToGrid(way->step[stepNumber]);
			for (int k = -1; k < 2; k++)
				for (int l = -1; l < 2; l++)
					if (k * l == 0 && k != l)
					{
						nearCell = { curCell.x + l ,curCell.y + k };
						if (nearCell.x > -1 && nearCell.x < GRID_WIDTH && nearCell.y > -1 && nearCell.y < GRID_HEIGHT && field[nearCell.x][nearCell.y] != Unvisited)
							if (field[nearCell.x][nearCell.y] < field[curCell.x][curCell.y])
								way->step[stepNumber - 1] = ToScreen(nearCell);
					}
			stepNumber--;
		}
	}
	way->isBlocked = false;
	way->isExist = false;
	way->isReached = false;
}

void Field::ShowInfo(SDL_Renderer* renderer, TTF_Font* font, Vector2dInt* mousePos, Squad** armies)
{
	SDL_Color color = { 0xff, 0xff, 0xff };
	SDL_Surface* text = nullptr;
	SDL_Texture* text_texture = nullptr;
	char info[100];
	char str[50];
	if (InArea(*mousePos))
	{
		SDL_Rect box = { mousePos->x + 15, mousePos->y + 15, 200, 150 };
		Vector2d mouseAtGrid = ToScreen(ToGrid({ (float)mousePos->x, (float)mousePos->y }));
		for (int i = 0; i < 8; i++)
		{
			for (int j = 0; j < 2; j++)
			{
				Squad* squad = &armies[j][i];
				if (squad->position.x == (float)mouseAtGrid.x && squad->position.y == (float)mouseAtGrid.y)
				{
					strcpy_s(info, "Name:  ");
					strcat_s(info, squad->unit.name);
					sprintf_s(str, "\nHealth:  %i (%i)\nAttack:  %i", squad->unit.hp, squad->frontUnitHP, squad->unit.attack);
					strcat_s(info, str);
					
					text = TTF_RenderText_Solid_Wrapped(font, info, color, 0);
					text_texture = SDL_CreateTextureFromSurface(renderer, text);
					SDL_SetRenderDrawColor(renderer, 0x20, 0x20, 0x20, 0x00);
					SDL_RenderFillRect(renderer, &box);
					SDL_SetRenderDrawColor(renderer, 0xf0, 0xf0, 0xf0, 0x00);
					SDL_RenderDrawRect(renderer, &box);
					box = { box.x + 5, box.y + 5, text->w, text->h };
					SDL_RenderCopy(renderer, text_texture, nullptr, &box);
				}
			}
		}
	}
	SDL_FreeSurface(text);
	SDL_DestroyTexture(text_texture);
}

void Field::PrintFieldEntities()
{
	for (int i = 0; i < GRID_HEIGHT; i++)
	{
		for (int j = 0; j < GRID_WIDTH; j++)
			printf("%02x ", field[j][i]);
		printf("\n");
	}
	printf("*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\n");
}

struct Army
{
	Squad squad[8];
	Vector2d* armyPos[8];
	SpriteManager* sMngr;
	Field* f;
	uchar activeSquad;
	PlayerType owner;

	void Init(SpriteManager* manager, Field* field, PlayerType player);
	void Raise();
	Squad CreateSquad(Vector2dInt pos);
	void SetSquads();
	void NextSquad();
	Squad* GetActiveSquad();
	Squad* GetSquadFromPosition(Vector2d position);
	Vector2dInt GetActiveSquadGridPosition();
	Vector2d** GetArmyPositions();
};

void Army::Init(SpriteManager* manager, Field* field, PlayerType player)
{
	activeSquad = 0;
	f = field;
	sMngr = manager;
	owner = player;
	Raise();
	SetSquads();
}

void Army::Raise()
{
	for (int i = 0; i < 8; i++)
	{
		squad[i] = CreateSquad({ owner * (GRID_WIDTH - 1) , i });
		armyPos[i] = &squad[i].position;
	}
}

Squad Army::CreateSquad(Vector2dInt pos)
{
	Squad squad;
	const char* squadName = UNITS_NAMES[rand() % 27];
	squad.Init(f->ToScreen(pos), sMngr, squadName);
	return squad;
}

void Army::SetSquads() 
{
	for (int i = 0; i < 8; i++)
		f->AddEntity(f->ToGrid(squad[i].position), Unit);
}

void Army::NextSquad() 
{
	(activeSquad == 7) ? activeSquad = 0 : activeSquad++;
}

//Useful functions
Squad* Army::GetActiveSquad() 
{
	return &squad[activeSquad];
}

Squad* Army::GetSquadFromPosition(Vector2d position)
{
	for (int i = 0; i < 8; i++)
		if (squad[i].position.x == position.x && squad[i].position.y == position.y)
			return &squad[i];
	return nullptr;
}

Vector2dInt Army::GetActiveSquadGridPosition()
{
	return f->ToGrid(squad[activeSquad].position);
}

Vector2d** Army::GetArmyPositions() 
{
	return armyPos;
}

int main()
{
	// Init SDL libraries
	SDL_SetMainReady(); // Just leave it be
	int result = 0;
	result = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO); // Init of the main SDL library
	if (result) // SDL_Init returns 0 (false) when everything is OK
	{
		printf("Can't initialize SDL. Error: %s", SDL_GetError()); // SDL_GetError() returns a string (as const char*) which explains what went wrong with the last operation
		return -1;
	}

	result = IMG_Init(IMG_INIT_PNG); // Init of the Image SDL library. We only need to support PNG for this project
	if (!(result & IMG_INIT_PNG)) // Checking if the PNG decoder has started successfully
	{
		printf("Can't initialize SDL image. Error: %s", SDL_GetError());
		return -1;
	}

	// Creating the window 1920x1080 (could be any other size)
	SDL_Window* window = SDL_CreateWindow("FirstSDL",
		0, 0,
		SCREEN_WIDTH, SCREEN_HEIGHT,
		SDL_WINDOW_SHOWN);

	if (!window)
		return -1;

	// Creating a renderer which will draw things on the screen
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (!renderer)
		return -1;

	if (TTF_Init() < 0) {
		printf("Error intializing SDL_ttf: %s", TTF_GetError());
		return -1;
	}

	TTF_Font* font = TTF_OpenFont("fonts/OpenSans-Regular.ttf", 16);
	if (!font) {
		printf( "Error loading font: %s", TTF_GetError());
		return -1;
	}

	srand((unsigned)time(NULL));

	SDL_Event sdl_event;

	SpriteManager sMngr;
	sMngr.Init(renderer);

	Field f;
	f.Init(sMngr.GetPointer("rock", Obstacle));

	Army army[2];
	army[Human].Init(&sMngr, &f, Human);
	army[Computer].Init(&sMngr, &f, Computer);

	PlayerType activePlayer = Human;
	army[activePlayer].GetActiveSquad()->isMoving = true;
	Squad* targetSquad = nullptr;
	Squad* activeSquad = army[activePlayer].GetActiveSquad();
	Vector2d** armiesPositions[2]
	{
		army[activePlayer].GetArmyPositions(),
		army[!activePlayer].GetArmyPositions()
	};
	Squad* allSquads[2]
	{
		army[activePlayer].squad,
		army[!activePlayer].squad
	};

	Vector2dInt mousePos;
	uchar numberOfTryes = 0;
	// The main loop
	// Every iteration is a frame
	for (int i = 0; i < 32; i++)
		printf("PATH TO PICTURE [%i] %s \n", i,  sMngr.units[i].path);
	Time t;
	t.Init(SDL_GetTicks());
	bool done = false;
	while (!done)
	{
		t.CountDelta(SDL_GetTicks());
		SDL_GetMouseState(&mousePos.x, &mousePos.y);
		// Polling the messages from the OS.
		// That could be key downs, mouse movement, ALT+F4 or many others
		if (!activeSquad->numberOfUnits || f.CheckUnitIsTrapped(activeSquad, armiesPositions[!activePlayer]))
			activeSquad->isMoving = false;
		if (activePlayer == Human)
		{
			while (SDL_PollEvent(&sdl_event))
			{
				if (sdl_event.type == SDL_QUIT) // The user wants to quit
				{
					done = true;
				}
				else if (sdl_event.type == SDL_KEYDOWN) // A key was pressed
				{
					switch (sdl_event.key.keysym.sym) // Which key?
					{
					case SDLK_ESCAPE: // Posting a quit message to the OS queue so it gets processed on the next step and closes the game
						SDL_Event event;
						event.type = SDL_QUIT;
						event.quit.type = SDL_QUIT;
						event.quit.timestamp = SDL_GetTicks();
						SDL_PushEvent(&event);
						break;
						// Other keys here
					default:
						break;
					}
				}
				else if (sdl_event.type == SDL_MOUSEBUTTONDOWN)
				{
					switch (sdl_event.button.button)
					{
					case SDL_BUTTON_LEFT:
					{
						Vector2dInt choice = f.ToGrid({ (float)sdl_event.button.x, (float)sdl_event.button.y });
						if (activeSquad->way.isReached && activeSquad->isMoving && f.InArea({sdl_event.button.x, sdl_event.button.y}))
						{
							switch (f.field[choice.x][choice.y])
							{
							case Unvisited:
								
								f.AddEntity(army[activePlayer].GetActiveSquadGridPosition(), ActiveUnit);
								activeSquad->way.start = activeSquad->position;
								activeSquad->way.destination = f.ToScreen(choice);
								f.BurnField(&activeSquad->way);
								if (!activeSquad->way.isBlocked)
									f.FindWay(&activeSquad->way);
								f.ClearField();
								activeSquad->way.isBlocked = false;
								break;
							case Unit:

								bool isEnemy;
								isEnemy = false;
								for (int i = 0; i < 8; i++)
								{
									Vector2dInt enemyCell = f.ToGrid(*armiesPositions[!activePlayer][i]);
									if (enemyCell.x == choice.x && enemyCell.y == choice.y)
										isEnemy = true;
								}
								if (isEnemy)
								{
									f.AddEntity(army[activePlayer].GetActiveSquadGridPosition(), ActiveUnit);
									activeSquad->way.start = activeSquad->position;
									activeSquad->way.destination = f.ToScreen(choice);
									f.BurnField(&activeSquad->way);
									if (!activeSquad->way.isBlocked)
										f.FindWay(&activeSquad->way);
									f.ClearField();
									activeSquad->way.isBlocked = false;
									f.AddEntity(choice, Unit);
									activeSquad->way.length--;
									targetSquad = army[!activePlayer].GetSquadFromPosition(f.ToScreen(choice));
								}
								break;
							default:
								break;
							}
						}
						break;
					}
					case SDL_BUTTON_RIGHT:
						if (activeSquad->way.isReached && activeSquad->isMoving)
						{
							f.GenerateField();
							army[Human].SetSquads();
							army[Computer].SetSquads();
							f.AddEntity(army[activePlayer].GetActiveSquadGridPosition(), ActiveUnit);
						}
						break;

					default:
						break;
					}
				}
				// More events here?
			}
		}
		else
		// here code for AI way finding
		{
			mousePos = { 0,0 };
			Vector2dInt choiceAI = {-1,-1};
			Vector2d* choice = armiesPositions[!activePlayer][rand() % 8];
			numberOfTryes++;
			if (army[!activePlayer].GetSquadFromPosition(*choice)->numberOfUnits > 0)
				choiceAI = f.ToGrid(*choice);
			if (numberOfTryes > 40)
				choiceAI = { rand() % GRID_WIDTH, rand() % GRID_HEIGHT };
			if(choiceAI.x > -1 && activeSquad->way.isReached && activeSquad->isMoving)
			{
				switch (f.field[choiceAI.x][choiceAI.y])
				{
				case Unvisited:

					f.AddEntity(army[activePlayer].GetActiveSquadGridPosition(), ActiveUnit);
					activeSquad->way.start = activeSquad->position;
					activeSquad->way.destination = f.ToScreen(choiceAI);
					f.BurnField(&activeSquad->way);
					if (!activeSquad->way.isBlocked)
						f.FindWay(&activeSquad->way);
					f.ClearField();
					activeSquad->way.isBlocked = false;
					break;
				case Unit:

					bool isEnemy;
					isEnemy = false;
					for (int i = 0; i < 8; i++)
					{
						Vector2dInt enemyCell = f.ToGrid(*armiesPositions[!activePlayer][i]);

						if (enemyCell.x == choiceAI.x && enemyCell.y == choiceAI.y)
							isEnemy = true;
					}
					if (isEnemy)
					{
						f.AddEntity(army[activePlayer].GetActiveSquadGridPosition(), ActiveUnit);
						activeSquad->way.start = activeSquad->position;
						activeSquad->way.destination = f.ToScreen(choiceAI);
						f.BurnField(&activeSquad->way);
						if (!activeSquad->way.isBlocked)
							f.FindWay(&activeSquad->way);
						f.ClearField();
						activeSquad->way.isBlocked = false;
						f.AddEntity(choiceAI, Unit);
						activeSquad->way.length--;
						targetSquad = army[!activePlayer].GetSquadFromPosition(f.ToScreen(choiceAI));
					}
					break;
				default:
					break;
				}
			}
		}
		//printf("TARGET SQUAD NAME %s\n", targetSquad->unit.name);
		if (activeSquad->isMoving && !activeSquad->way.isReached)
		{
			if (activeSquad->way.length > activeSquad->way.currentStep)
			{
				activeSquad->Move(t.deltaTime, *activeSquad->GetCurrentStepCoordinates());
			}
			else
			{
				if (targetSquad)
				{
					targetSquad->TakeDamage(activeSquad, activeSquad->Attack());
					if (!targetSquad->numberOfUnits)
					{
						Vector2dInt temp = f.ToGrid(targetSquad->position);
						f.field[temp.x][temp.y] = Unvisited;
					}
				}
				activeSquad->way.currentStep = 0;
				activeSquad->way.isReached = true;
				activeSquad->isMoving = false;

				f.AddEntity(f.ToGrid(activeSquad->way.start), Unvisited);
				if (activeSquad->numberOfUnits)
					f.AddEntity(army[activePlayer].GetActiveSquadGridPosition(), Unit);
				f.PrintFieldEntities();
			}
		}

		if (!activeSquad->isMoving && activeSquad->way.isReached)
		{
			numberOfTryes = 0;
			army[activePlayer].NextSquad();
			(activePlayer == Human) ? activePlayer = Computer : activePlayer = Human;
			army[activePlayer].GetActiveSquad()->isMoving = true;
			targetSquad = nullptr;
			activeSquad = army[activePlayer].GetActiveSquad();
			Vector2d** enemyArmyPositions = army[!activePlayer].GetArmyPositions();
		}

		f.DrawField(renderer);
		for (int i = 0; i < 8; i++)
		{
			f.DrawUnit(renderer, &army[Human].squad[i], Human, font);
			f.DrawUnit(renderer, &army[Computer].squad[i], Computer, font);
		}
		f.ShowInfo(renderer, font, &mousePos, allSquads);
		SDL_RenderPresent(renderer);
		
		// next frame...
	}

	// If we reached here then the main loop stoped
	// That means the game wants to quit
	sMngr.Destroy();
	// Shutting down the renderer
	SDL_DestroyRenderer(renderer);

	// Shutting down the window
	SDL_DestroyWindow(window);

	// Quitting the Image SDL library
	IMG_Quit();
	// Quitting the main SDL library
	SDL_Quit();

	//getchar();
	// Done.
	return 0;
}

