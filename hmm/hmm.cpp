#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#define SDL_MAIN_HANDLED
#include "SDL2/SDL.h"
#include "SDL2/SDL_image.h"


const int SCREEN_WIDTH = 1920;
const int SCREEN_HEIGHT = 1080;
const int GRID_WIDTH = 15;
const int GRID_HEIGHT = 11;

enum Entities
{
	Unvisited = 0,
	Player = 1,
	Destination = 222,
	Obstacle = 255
};

struct Vector2d
{
	double x;
	double y;
};

struct Vector2dInt
{
	int x;
	int y;
};


struct Time
{
	unsigned int lastTime, deltaTime;

	void Init(unsigned int t)
	{
		lastTime = t;
	}

	void CountDelta(unsigned int t)
	{
		deltaTime = t - lastTime;
		lastTime = t;
	}
};


struct Sprite2d
{
	Vector2dInt size;
	const char* path;
	SDL_Texture* texture;

	bool Init(SDL_Renderer* renderer, const char* path);
};


struct DeWay
{
	bool isExist, isBlocked, isReached;
	unsigned char length;
	unsigned char currentStep;
	Vector2d start;
	Vector2d destination;
	Vector2d step[GRID_WIDTH * GRID_HEIGHT / 2];

	void Init(Vector2d entityPosition);
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
	bool isMoving;
	double speed;
	Vector2d position;
	Vector2d destination;
	Vector2d velocity;
	DeWay way;
	Sprite2d* sprite;

	void Init(Vector2d pPos, Sprite2d* sprt);
	void CountVelocity();
	void Move(unsigned int delta_time, Vector2d destPos);
};

void Unit2d::Init(Vector2d pPos, Sprite2d* sprt)
{
	isMoving = false;
	speed = 0.4;
	position = pPos;
	destination = pPos;
	velocity = { 0,0 };
	sprite = sprt;
	way.Init(position);
}

void Unit2d::CountVelocity()
{
	Vector2d posDifference = { (destination.x - position.x), (destination.y - position.y) };
	double distance = sqrt(posDifference.x * posDifference.x + posDifference.y * posDifference.y);
	//velocity = { (posDifference.x != 0) ? posDifference.x / distance : 0, (posDifference.y != 0) ? posDifference.y / distance : 0 };
	velocity = { posDifference.x / distance, posDifference.y / distance };
};

void Unit2d::Move(unsigned int dt, Vector2d destPos)
{
	if (isMoving)
	{
		destination = destPos;
		if ( !(int)( fabs(velocity.x) + fabs(velocity.y) ) ) 
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

struct Field
{
	float obstacleProb;
	Sprite2d* obstacle;
	Vector2dInt cellSize;
	Vector2dInt gridStart;
	unsigned char field[GRID_WIDTH][GRID_HEIGHT];

	void Init(Sprite2d* obst);
	bool InArea(Vector2dInt point);
	void AddEntity(Vector2dInt position, Entities ID);
	void GenerateField();
	void BurnField(DeWay* way);
	void ClearField();
	void FindWay(DeWay* way);
	void DrawField(SDL_Renderer* renderer, Unit2d* player);


	Vector2d ToScreen(Vector2dInt gridPosition)
	{
		Vector2d p =
		{
			(gridPosition.x * cellSize.x) + gridStart.x,
			(gridPosition.y * cellSize.y) + gridStart.y
		};
		return p;
	}
	/*
	Vector2dInt RoundToGrid(Vector2dInt screenPosition)
	{
		Vector2dInt p =
		{
			(screenPosition.x - gridStart.x) / cellSize.x,
			(screenPosition.y - gridStart.y) / cellSize.y
		};
		return p;
	}*/
	Vector2dInt ToGrid(Vector2d screenPosition)
	{
		Vector2dInt p =
		{
			((int)screenPosition.x - gridStart.x) / cellSize.x,
			((int)screenPosition.y - gridStart.y) / cellSize.y
		};
		return p;
	}
};

void Field::Init(Sprite2d* obst)
{
	obstacleProb = .3f;
	cellSize = { 100, 80 };
	gridStart = { SCREEN_WIDTH / 2 - (int)(cellSize.x * (float)GRID_WIDTH / 2), SCREEN_HEIGHT / 2 - (int)(cellSize.y * (float)GRID_HEIGHT / 2) };
	obstacle = obst;
	GenerateField();
}

bool Field::InArea(Vector2dInt point)
{
	if (point.x > gridStart.x && point.x < (gridStart.x + cellSize.x * GRID_WIDTH) &&
		point.y > gridStart.y && point.y < (gridStart.y + cellSize.y * GRID_HEIGHT) &&
		field[(point.x - gridStart.x) / cellSize.x][(point.y - gridStart.y) / cellSize.y] == Unvisited)
	{ return true; }
	else { return false; }

}

void Field::AddEntity(Vector2dInt position, Entities ID)
{
	field[position.x][position.y] = ID;
}

void Field::GenerateField()
{
	for (int i = 0; i < GRID_HEIGHT; i++)
	{
		for (int j = 0; j < GRID_WIDTH; j++)
		{
			int r = rand();
			if (r < (int)(RAND_MAX * obstacleProb))
			{
				field[j][i] = Obstacle;
			}
			else
			{
				field[j][i] = Unvisited;
			}
		}
	}
}

void Field::BurnField(DeWay* way)
{
	//through the link to field grid we setting entity and aim positions
	//burning way with grossfire algorithm
	//write gained way into step array
	//clear linked field grid to origin
	AddEntity(ToGrid(way->destination), Destination);
	unsigned char fireLayer = 2;
	Vector2dInt queue[GRID_WIDTH * GRID_HEIGHT / 2]{ ToGrid((*way).start), {-1, -1}};
	Vector2dInt buffer[GRID_WIDTH * GRID_HEIGHT / 2]{};
	Vector2dInt tempCell;
	while (!(*way).isExist && !(*way).isBlocked)
	{
		int queueID = 0;
		for (int i = 0; queue[i].x > -1; i++)
		{
			for (int k = -1; k < 2; k++)
			{
				for (int l = -1; l < 2; l++)
				{
					if (k * l == 0 && k != l)
					{
						tempCell = { queue[i].x + l , queue[i].y + k };
						if (tempCell.x > -1 && tempCell.x < GRID_WIDTH && tempCell.y > -1 && tempCell.y < GRID_HEIGHT)
						{
							switch (field[tempCell.x][tempCell.y])
							{
							case Unvisited:
								field[tempCell.x][tempCell.y] = fireLayer;
								buffer[queueID] = tempCell;
								queueID++;
								break;
							case Destination:
								(*way).isExist = true;
								break;
							}
						}
					}
				}
			}


		}
		if (buffer[0].x == -1)
		{
			(*way).isBlocked = true;
			(*way).step[0] = { -1,-1 };
		}
		buffer[queueID] = { -1, -1 };
		for (int i = 0; i < queueID + 1; i++)
		{
			queue[i] = buffer[i];
		}
		fireLayer++;
	}
	(*way).length = fireLayer - 2;
}

void Field::ClearField()
{
	for (int i = 0; i < GRID_HEIGHT; i++)
	{
		for (int j = 0; j < GRID_WIDTH; j++)
		{
			switch (field[j][i])
			{
			case Obstacle:
				break;

			default:
				field[j][i] = Unvisited;
				break;

			}
		}
	}
}

void Field::DrawField(SDL_Renderer* renderer, Unit2d* player)
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

	rect.x = (int)round(player->position.x); // Counting from the image's center but that's up to you
	rect.y = (int)round(player->position.y - 20); // Counting from the image's center but that's up to you
	rect.w = player->sprite->size.x;
	rect.h = player->sprite->size.y;

	SDL_RenderCopyEx(renderer, // Already know what is that
		player->sprite->texture, // The image
		nullptr, // A rectangle to crop from the original image. Since we need the whole image that can be left empty (nullptr)
		&rect, // The destination rectangle on the screen.
		0, // An angle in degrees for rotation
		nullptr, // The center of the rotation (when nullptr, the rect center is taken)
		(player->velocity.x >= 0) ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL); // We don't want to flip the image

	// Showing the screen to the player
	SDL_RenderPresent(renderer);

}

void Field::FindWay(DeWay* way)
{
	if (way->isExist)
	{
		way->step[(way->length)] = { -1, -1 };
		way->length--;
		way->step[way->length] = way->destination;
		Vector2dInt nearCell;
		Vector2dInt curCell;
		while (way->length > 0)
		{
			curCell = ToGrid(way->step[way->length]);
			for (int k = -1; k < 2; k++)
			{
				for (int l = -1; l < 2; l++)
				{
					if (k * l == 0 && k != l)
					{
						nearCell = { curCell.x + l ,curCell.y + k };
						if (nearCell.x > -1 && nearCell.x < GRID_WIDTH && nearCell.y > -1 && nearCell.y < GRID_HEIGHT && field[nearCell.x][nearCell.y] != Unvisited)
						{
							if (field[nearCell.x][nearCell.y] < field[curCell.x][curCell.y])
							{
								way->step[(way->length) - 1] = ToScreen(nearCell);
							}
						}
					}
				}
			}
			(way->length)--;
		}
	}
	way->isBlocked = false;
	way->isExist = false;
	way->isReached = false;
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

	// Setting the color of an empty window (RGBA). You are free to adjust it.
	SDL_SetRenderDrawColor(renderer, 20, 150, 39, 255);

	srand((unsigned)time(NULL));

	// Loading an image
	Sprite2d sprite{};
	if (!sprite.Init(renderer, "image.png"))
		return -1;
	Sprite2d obstacle{};
	if (!obstacle.Init(renderer, "rock.png"))
		return -1;

	SDL_Event sdl_event;

	Field f{};
	f.Init(&obstacle);
	Unit2d player{};
	f.AddEntity( {0,0}, Player);
	player.Init(f.ToScreen({ 0,0 }), &sprite);

	// The main loop
	// Every iteration is a frame
	Time t;
	t.Init(SDL_GetTicks());
	bool done = false;
	while (!done)
	{
		t.CountDelta(SDL_GetTicks());
		// Polling the messages from the OS.
		// That could be key downs, mouse movement, ALT+F4 or many others
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
						if (player.way.isReached && f.InArea({ sdl_event.button.x, sdl_event.button.y }))
						{
							player.way.start = player.position;
							player.way.destination = f.ToScreen( f.ToGrid({ (double)sdl_event.button.x, (double)sdl_event.button.y }) );
							f.BurnField(&player.way);
							f.FindWay(&player.way);
							f.ClearField();
							player.isMoving = true;
						}
					}
					break;

				case SDL_BUTTON_RIGHT:
				if (player.way.isReached)
				{
					f.GenerateField();
					f.AddEntity(f.ToGrid(player.position), Player);
				}
				break;
				
				default:
					break;
				}
			}
			// More events here?
		}

		if ((player.way.step[player.way.currentStep].x >= 0) && !player.way.isReached)
		{
			player.Move(t.deltaTime, player.way.step[player.way.currentStep]);
		}
		else 
		{
			player.isMoving = false;
			player.way.isReached = true;
			player.way.currentStep = 0;
		}

		f.DrawField(renderer, &player);

		// next frame...
	}

	// If we reached here then the main loop stoped
	// That means the game wants to quit

	// Shutting down the renderer
	SDL_DestroyRenderer(renderer);

	// Shutting down the window
	SDL_DestroyWindow(window);

	// Quitting the Image SDL library
	IMG_Quit();
	// Quitting the main SDL library
	SDL_Quit();

	// Done.
	return 0;
}

bool Sprite2d::Init(SDL_Renderer* renderer, const char* path)
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

	size = { surface->w , surface->h };
	this->path = path;

	SDL_FreeSurface(surface);
}