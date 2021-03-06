
#include "StdAfx.h"
#include "GameBoard.h"

GameBoard::GameBoard(){
    xSize = XSIZE;
    ySize = YSIZE;
	//I tried it with larger variations of density but the game becomes impossible with too many or too few rocks
	density = 2; //from 1 to 3 is reasonable corresponding to 3/7,4/8 or 5/9
	difficulty = 1; //Must be at least 1
	moveCounter = 0;
	deaths = 0;
    board = new int[XSIZE*YSIZE];
    ResetBoard();
	if (!rock.loadFromFile("resources/Rock.png"))
		throw "Rock texture load failure";
	if (!player.loadFromFile("resources/Player.png"))
		throw "Player texture load failure";
	if (!enemy.loadFromFile("resources/Enemy.png"))
		throw "Enemy texture load failure";
	if (!goal.loadFromFile("resources/End.png"))
		throw "Goal textur load failure";
	if (!background.loadFromFile("resources/Background.png"))
		throw "Background texture load failure";
	background.setRepeated(true);
	if (!shade.loadFromFile("resources/Shade.png"))
		throw "Shade texture load failure";
	shade.setRepeated(true);
	if (!font.loadFromFile("resources/arial.ttf"))
		throw "ERROR font load error";
	if (!deathSound.loadFromFile("resources/screammono.wav"))
        throw "ERROR sound load error";
	dsound.setBuffer(deathSound);
	if (!music.openFromFile("resources/scream.wav")) //Yes this is the wilhelm scream
		throw "ERROR sound load error";
	
}

int* GameBoard::SpotRef(int x, int y){
    return board + (x+y*(xSize));
}
int GameBoard::SpotVal(int x, int y){
    return *SpotRef(x,y);
}
int* GameBoard::SpotRef(Location l){
    return board + (l.x+l.y*(xSize));
}
int GameBoard::SpotVal(Location l){
    return *SpotRef(l.x,l.y);
} 
void GameBoard::SetSpot(int x, int y, int newVal){
    board[x+y*(xSize)] = newVal;
}
bool GameBoard::ValidLocation(Location l){
    return (l.x >= 0 && l.x < xSize && l.y>=0 && l.y < ySize);
}

void GameBoard::ResetBoard(){
	for(int i = 0; i<xSize; i++){
        for(int j = 0; j < ySize; j++){
			if(rand()%(density+6) < 4){
				 *SpotRef(i,j) = EMPTY;
			}else{
				*SpotRef(i,j) = WALL;
			}
        }
    }
	*SpotRef(0,1) = EMPTY;
	*SpotRef(1,0) = EMPTY;
	*SpotRef(1,1) = EMPTY; //hopefully avoids starting off stuck
	for (list<Entity *>::iterator iterator = entityList.begin(); iterator != entityList.end(); ++iterator){
		(*iterator)->Reset();
		*SpotRef((*iterator)->loc) = (*iterator)->symbol;
		//cout << "set loc " << (*iterator)->loc.x << "," << (*iterator)->loc.y << " symbol " << (*iterator)->symbol << endl;
	}
}

//allows overlap but doesn't handle it well, game is assumed to end immediately when overlap occurs so should be ok
//TODO: allow for entity overlap to properly replicate
bool GameBoard::TryMoveBlock(Location fromLoc, dirVec dir, bool entity){ 
    Location newLoc = fromLoc.AddVec(dir);
    if(!ValidLocation(fromLoc) || !ValidLocation(newLoc)){
        return false;
    }
    if(SpotVal(fromLoc) == EMPTY){  //is there a block at location?
        return false;
    }

	if(SpotVal(newLoc) == SpotVal(fromLoc)){
		return false; //cant move stuff onto itself.  
	}
	
	if(SpotVal(fromLoc) != WALL){
		if(SpotVal(newLoc) == WALL){ //trying to move into a block
			return false;
		}
	}else{
		if(SpotVal(newLoc) != EMPTY){ //is new location empty
			return false;
		}
	}
    //TODO: check if block is an entity (via value)
    //If yes then update entity's location
    int blockVal = SpotVal(fromLoc);
    *SpotRef(newLoc) = blockVal;//update new location
    *SpotRef(fromLoc) = EMPTY;//update old location 
    return true;
}

bool GameBoard::TryMoveEntity(Entity *ent, dirVec dir, bool canPush, bool canPull){
	//cout << "move try" << endl;
    Location pullFrom = ent->loc.SubVec(dir);
    Location pushFrom = ent->loc.AddVec(dir);
	//cout << "-from" << pushFrom.x << "," << pushFrom.y << endl;
    if(canPush){//try moving pushed block
		if(SpotVal(pushFrom) == WALL){ //only try moving rocks, not entities
			TryMoveBlock(pushFrom, dir, false); //does't matter if successful
		}
    }
    bool success = true;
	//cout << "move entity" << endl;
    success = TryMoveBlock(ent->loc, dir, true);  //try moving self
    if(!success){ 
		//cout << "fail";
        return false;
    }
    ent->loc = pushFrom; //might be done in move sec later
	//cout << "ent" << ent->loc.x << "," << ent->loc.y << endl;
    if(canPull){//is pulling?
		if(SpotVal(pullFrom) == WALL){
			TryMoveBlock(pullFrom,dir, false); //try moving pulled block
		}
    }
	//cout << "\n" << "Move success";
    return true;
}

bool GameBoard::MoveMonster(Entity *monster, Entity *player){
	if(monster->loc == player->loc){
		PlayerDied();
		return false;
	}else{
		//monster has 1 or 2 directions that are toward the player.  
		bool oneDirection = (monster->loc.x == player->loc.x || monster->loc.y == player->loc.y);
		int i = 0;
		dirVec possibleMoves[2];
		int weight[2];
		int absx = abs(monster->loc.x-player->loc.x);
		if(monster->loc.x < player->loc.x){
			possibleMoves[i] = dirVec(1,0);
			weight[i] = absx;
			i++;
		}else if(monster->loc.x > player->loc.x){
			possibleMoves[i] = dirVec(-1,0);
			weight[i] = absx;
			i++;
		}
		int absy = abs(monster->loc.y-player->loc.y);
		if(monster->loc.y < player->loc.y){
			possibleMoves[i] = dirVec(0,1);
			weight[i] = absy;
			i++;
		}else if(monster->loc.y > player->loc.y){
			possibleMoves[i] = dirVec(0,-1);
			weight[i] = absy;
			i++;
		}
		int choice = 0;
		
		if(!oneDirection){
			//weight is porportial to distance in that direction
			if (rand()%(weight[0]+weight[1]) >= weight[0]){
				//for example monster is more likely to go left if player is more left than down
				choice = 1;
			}
		}
		//cout << "M";
		//try moving the monster one way
		bool success = TryMoveEntity(monster, possibleMoves[choice], true, false);
		if(!oneDirection && ! success){ //try the other way if first direction didn't work
			//cout << "M";
			return TryMoveEntity(monster, possibleMoves[1-choice], true, false); //try other way
		}
		return success;
	}
}

void GameBoard::PlayerDied(){
	//cout << "SCREAM" << endl;
	dsound.play();
	while (dsound.getStatus() == sf::Sound::Playing)
    {
        // Leave some CPU time for other processes
		//cout << "playing sound ";
        sf::sleep(sf::milliseconds(100));
	}
	deaths++;
	ResetBoard();
	//music.play();
}

void GameBoard::ShadowText(sf::RenderWindow &window, sf::Text text){
	sf::Color color = text.getColor();
	text.setColor(sf::Color::Black);
	window.draw(text);
	text.move(1,-1);
	text.setColor(color);
	window.draw(text);
}

void GameBoard::DisplayTopBar(sf::RenderWindow &window){
	sf::RectangleShape rectangle(sf::Vector2f(120, 50));
	rectangle.setSize(sf::Vector2f(XSIZE*TILE_SIZE, HEADER_SIZE));
	rectangle.setFillColor(sf::Color::Blue);
	window.draw(rectangle);
	std::stringstream sstm;
	sstm << "deaths: " << deaths;
	sf::Text text;
	text.setFont(font);
	text.setString(sstm.str());
	text.setColor(sf::Color::Black);
	text.setCharacterSize(HEADER_SIZE-2);
	text.setColor(sf::Color::Red);
	ShadowText(window, text);
	
	std::stringstream sstm2;
	sstm2 << "level: " << difficulty;
	text.setString(sstm2.str());
	text.setPosition(XSIZE*TILE_SIZE-text.getGlobalBounds().width-1,0);
	ShadowText(window, text);
}

void GameBoard::DisplayButton(sf::RenderWindow &window, sf::RectangleShape rectangle, string buttonString){
	sf::Text text;
	text.setFont(font);
	text.setString(buttonString);
	text.setColor(sf::Color::White);
	text.setCharacterSize(22);
	text.setPosition(rectangle.getPosition().x+4,rectangle.getPosition().y+1);
	rectangle.setFillColor(sf::Color::Green);
	window.draw(rectangle);
	ShadowText(window, text);
}

void GameBoard::DisplayMenu(sf::RenderWindow &window){
	sf::Sprite shadeSprite;
	shadeSprite.setTexture(shade);
	shadeSprite.setTextureRect(sf::IntRect(0, 0, XSIZE*TILE_SIZE, YSIZE*TILE_SIZE));
	shadeSprite.setPosition(0,HEADER_SIZE);
	window.draw(shadeSprite);

}

void GameBoard::DisplayBoard(sf::RenderWindow &window){
	window.clear();
	sf::Sprite backgroundSprite;
	backgroundSprite.setTexture(background);
	backgroundSprite.setTextureRect(sf::IntRect(0, 0, XSIZE*TILE_SIZE, YSIZE*TILE_SIZE+HEADER_SIZE));
	window.draw(backgroundSprite);
	DisplayTopBar(window);
	sf::Sprite sprite;
    for(int i = 0; i < YSIZE; i++){
        for(int j = 0; j < XSIZE; j++){
			bool draw = true;
			switch(SpotVal(j,i)){
				case EMPTY:
					draw = false;
					break;
				case WALL:
					sprite.setTexture(rock);
					break;
				case PLAYER:
					sprite.setTexture(player);
					break;
				case ENEMY:	
					sprite.setTexture(enemy);
					break;
				case GOAL:
					sprite.setTexture(goal);
					break;
				default:
					cout << "error bad case"; //not used
					break;
			}
			if(draw){
				sprite.setPosition(sf::Vector2f((float) j*TILE_SIZE, (float) HEADER_SIZE + TILE_SIZE * (YSIZE-1) - i*TILE_SIZE));
				window.draw(sprite);
			}
        }
    }
}

void GameBoard::AddEntity(Entity *ent){
	entityList.insert(entityList.end(), ent);
	*SpotRef(ent->loc) = ent->symbol;
}

void GameBoard::RemoveEntity(Entity *ent){
	*SpotRef(ent->loc) = EMPTY;
	entityList.remove(ent);
}