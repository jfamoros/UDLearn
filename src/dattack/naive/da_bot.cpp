#include <iostream>
#include <cmath>
#include "ale_interface.hpp"
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <vector>

#include "SDL.h"

using namespace std;

#define twK 20000

// Global vars
const int maxSteps(twK);
float totalReward;
ALEInterface alei;
bool manualInput(false);
int lastLives;
time_t lastTimeChangedMode(std::time(0));

struct RAM_Info {
    int value;
    int appearances;
    RAM_Info() : value(0), appearances(0) {}
};

vector<RAM_Info> lastRAM(128);
ofstream csv;
bool toCSV = false;
vector<unsigned char> grayscale;
// non modificable
const int LINE_WIDTH(160);          // Amount of pixels in a line
const int SHIP_WIDTH(7);            // Ship width in pixels
const int LEFT_THRESOLD(21);        // MIN X non-pixel coordinate the ship can move 25
const int RIGHT_THRESOLD(138);      // MAX X non-pixel coordinate the ship can move 135
const int P1_BULLETS_COLOR(174);
const int P1_SHIP_COLOR(115);
const int EN_BULLETS_COLOR(176);


// modifiable params
const int VISION_THRESOLD(10);
const int SECOND_VISION_LINE_THRESOLD(168);
const bool bRandomisePlayerGPos(false);
const bool bRandomiseEnemyGPos(false);

enum BlockingHit {
    EMoveRight,
    EMoveLeft,
    ENotBlocking
};

void write(double d)
{
    if (toCSV) csv << to_string(d);
}

void write(string s)
{
    if (toCSV) csv << s;
}

void dumpLine(int TARGET_LINE) {
    int const FILTER_LINE(LINE_WIDTH*TARGET_LINE);
    for(int i = FILTER_LINE; i < FILTER_LINE + LINE_WIDTH; ++i) {
        int a(grayscale[i]);
        cout << (a==0 ? "." : ",") << ((i%160==0) ? "\n" : "");
    }
}

///////////////////////////////////////////////////////////////////////////////
/// Print usage and exit
///////////////////////////////////////////////////////////////////////////////
void usage(char* pname)
{
   std::cerr
      << "\nUSAGE:\n" 
      << "   " << pname << " <romfile> "<< "<display media?([0],1)> <Print ram?([0],1)> <Dump output?([0],1)>\n";
   exit(-1);
}

void printRAM()
{
    //system("clear");

    for (int i = 0; i < 128; i++)
    {
        if (i % 8 == 0)
        {
            //std::cout << std::endl;
        }

        //cout << "[";

        if (i < 10)
        {
            //cout << "  ";
        }
        else if (i < 100)
        {
            //cout << " ";
        }

        //cout << i << "]";

        //stringstream ramValue;
        RAM_Info aux;
        aux.value = (int)alei.getRAM().get(i);
        //int intRamValue = (int)alei.getRAM().get(i);
        //ramValue << hex << aux.value;

        // ANSI colour codes
        if (aux.value == lastRAM[i].value)
        {
            // Red color
            //cout << "\033[1;31m";
        }
        else
        {
            // Green color
            //cout << "\033[1;32m";
            lastRAM[i].appearances++;
        }

        // Same length for al cases
        if (aux.value < 0x10)
        {
            //cout << "0";
        }

        //cout << ramValue.str() << "\t";

        // ANSI colour code close
        //cout << "\033[0m";

        lastRAM[i].value = aux.value;
    }
}

// Keys: https://wiki.libsdl.org/SDL_Keycode
void checkKeys()
{
    Uint8* keystate = SDL_GetKeyState(NULL);

    // It checks if key 'e' has been pressed to change to manual mode
    if(keystate[SDLK_e] && (time(0) - lastTimeChangedMode) >= 1)
    {
        if (manualInput)
        {
            cout << "[UDL] Bot Control" << endl;
        }
        else
        {
            cout << "[UDL] Manual Control" << endl;
        }

        manualInput = !manualInput;

        lastTimeChangedMode = time(0);
    }
}

///////////////////////////////////////////////////////////////////////////////
/// Get info from RAM
///////////////////////////////////////////////////////////////////////////////
int getChar_X(int char_id)
{
    // player position hex nibbles are inverted so we turn it around, same with the first nibble which seems to be desync with the second nibble
    int const val(alei.getRAM().get(char_id));
    int const rawFirstNibble((7-((val & 0xF0)>>4)) & 0x0F);
    int const rawSecondNibble(val & 0x0F);
    return (rawSecondNibble*16) + rawFirstNibble; 
}

int getP1_X(bool pixel=false)
{
    if(pixel) {
        // 185 is the ship position line we are tracking
        int const FILTER_LINE(LINE_WIDTH*185);
        for(int i = FILTER_LINE; i < FILTER_LINE + LINE_WIDTH; ++i) {
            int a(grayscale[i]);
            if(a == P1_SHIP_COLOR){
                return (i - FILTER_LINE);
            }
        }
    }

    return getChar_X(16) + (bRandomisePlayerGPos ? rand()%2-1 : 0);
}

int firing_frames(0);
int last_firing_pos(0);
int getFiringEnemy_X() { 
    const int current_pos = getChar_X(15);
    if(last_firing_pos == current_pos) firing_frames++;
    else firing_frames = 0;
    last_firing_pos = current_pos;
    return current_pos + (bRandomiseEnemyGPos ? rand()%2-1 : 0); 
}

int mid_frames(0);
int last_mid_pos(0);
int getMidEnemy_X() { 
    const int current_pos = getChar_X(14);
    if(last_mid_pos == current_pos) mid_frames++;
    else mid_frames = 0;
    last_mid_pos = current_pos;
    return current_pos + (bRandomiseEnemyGPos ? rand()%2-1 : 0); 
}

int further_frames(0);
int last_further_pos(0);
int getFurtherEnemy_X() { 
    const int current_pos = getChar_X(13);
    if(last_further_pos == current_pos) further_frames++;
    else further_frames = 0;
    last_further_pos = current_pos;
    return current_pos + (bRandomiseEnemyGPos ? rand()%2-1 : 0); 
}


int EnemyHandler() {
    int a = getFurtherEnemy_X();
    int b = getMidEnemy_X();
    int c = getFiringEnemy_X();

    if(firing_frames > 4) {
        if(mid_frames > 4) {
            return a;
        } else {
            return b;
        }
    } else {
        return c;
    }
}

///////////////////////////////////////////////////////////////////////////////
/// 1D Collision check given a screen line
///////////////////////////////////////////////////////////////////////////////

bool ImpactValIsAnEnemy(int impact_pixel_val){
    return (impact_pixel_val != EN_BULLETS_COLOR && 
    impact_pixel_val != P1_SHIP_COLOR && 
    impact_pixel_val != P1_BULLETS_COLOR &&
    impact_pixel_val != 0);
}

BlockingHit isBlockingHit(int line) {
    int const FILTER_LINE(LINE_WIDTH*line);
    int const VISION_X_AREA(SHIP_WIDTH + (VISION_THRESOLD*2));
    for(int i = 0; i < VISION_X_AREA; ++i) {
        const int impact_pixel_val(grayscale[FILTER_LINE + (getP1_X(true)-VISION_THRESOLD) + i]);
        // Avoid flies
        if (line > SECOND_VISION_LINE_THRESOLD && ImpactValIsAnEnemy(impact_pixel_val)) {
            if(i < (VISION_X_AREA/2)) return EMoveRight;
            else if(i >= (VISION_X_AREA/2)) return EMoveLeft;
        }
        // Avoid pew pew's
        if(impact_pixel_val == EN_BULLETS_COLOR) {
            // Blocking hit, now we have to determine the direction we want to  based on the impact point
            if(i < (VISION_X_AREA/2)) return EMoveRight;
            else if(i >= (VISION_X_AREA/2)) return EMoveLeft;
        }
    }
    return ENotBlocking;
}

float manualMode()
{   
    alei.getScreenGrayscale(grayscale);
    BlockingHit bs = ENotBlocking;
    for(int line = 185; line > 60; --line) {
        bs = isBlockingHit(line); 
        if(bs != ENotBlocking) break;
    } 

    if(bs != ENotBlocking) {
        cout << "BlockingHit = true" << endl;
    } else {
        cout << "BlockingHit = false" << endl;
    }



    Uint8* keystate = SDL_GetKeyState(NULL);
    float reward = 0;

    if(keystate[SDLK_SPACE])
    {
        alei.act(PLAYER_A_FIRE);
    }

    if(keystate[SDLK_LEFT])
    {
        reward += alei.act(PLAYER_A_LEFT);
    }
    if(keystate[SDLK_RIGHT])
    {
        reward += alei.act(PLAYER_A_RIGHT);
    }
    if(keystate[SDLK_UP])
    {
        reward += alei.act(PLAYER_A_UP);
    }
    if(keystate[SDLK_DOWN])
    {
        reward += alei.act(PLAYER_A_DOWN);
    }


    return (reward + alei.act(PLAYER_A_NOOP));
}




///////////////////////////////////////////////////////////////////////////////
/// Do Next Agent Step
///////////////////////////////////////////////////////////////////////////////
struct DirtyState {
    bool Dirty;
    BlockingHit Direction;
    DirtyState(bool bDirty, BlockingHit bDirection) 
    : Dirty(bDirty), Direction(bDirection) { }
};

DirtyState ms(false, ENotBlocking);
DirtyState ds(false, ENotBlocking);
float agentStep()
{
    // get screen information
    alei.getScreenGrayscale(grayscale);
    
    // Reseting variables
    float reward = 0;    
    BlockingHit eBH = ENotBlocking;


    // Iterating from bottom line to top line which defines a vision rectangle (see is BlockingHit)
    for(int line = 185; line > 60; --line) {
        eBH = isBlockingHit(line); 
        if (eBH != ENotBlocking && !ds.Dirty)
        {   
            /**
            * Dirtying the warning state, now we have to avoid every enemy and undesired object
            * Random direction for the bot to pick when the dirty state is set
            **/
            ds.Dirty = true; ds.Direction = eBH; 
            break;
        } else if (eBH != ENotBlocking) {
            /**
            * Theorically at this point the direction is already set so we don't have to re-set it here
            * we just mark the dirty state to true
            **/
            ds.Dirty = true; 
            break;
        }
    }


    /***************************************************
    * Blocking hit detected at this point
    ***************************************************/
    if(eBH != ENotBlocking) 
    {
        // Left thresold
        if(getP1_X() == LEFT_THRESOLD) {
            // If we cannot move more to the left we change direction
            ds.Direction = EMoveRight;
        }
        // Right thresold
        else if(getP1_X() == RIGHT_THRESOLD) {
            // If we cannot move more to the right we change direction
            ds.Direction = EMoveLeft;   
        }
        switch(ds.Direction){
            case EMoveLeft:
                reward+= alei.act(PLAYER_A_LEFT);
                ms.Direction = EMoveLeft;       
                break;
            case EMoveRight:
                reward+= alei.act(PLAYER_A_RIGHT);
                ms.Direction = EMoveRight;
                break;
            default:
                reward+= reward+=alei.act(PLAYER_A_FIRE);
                ms.Direction = ENotBlocking;                //action fire
        }
        
    } else {
        // Here we are safe, don't move but keep shooting
        reward+=alei.act(PLAYER_A_FIRE);
        ds.Dirty = false;
        const int mypos = getP1_X();
        const int en_pos = EnemyHandler();
        if(mypos < en_pos) {
            reward+= alei.act(PLAYER_A_RIGHT);
            ms.Direction = EMoveRight;
        } else if (mypos > en_pos) {
            reward+= alei.act(PLAYER_A_LEFT);
            ms.Direction = EMoveLeft;    
        } else {
            reward+=alei.act(PLAYER_A_FIRE);
            ms.Direction = ENotBlocking;
        }
    }   
    return (reward + alei.act(PLAYER_A_NOOP));
}

void ramToCsv() {
    //starting at 5 since the first 5 positions contain redundant information
    for (unsigned i = 5; i < 127; ++i) {
        write( ((double)alei.getRAM().get(i)) / 255);
        write(",");
    }
    write((double)alei.getRAM().get(127));
    write(",");

    //ONE HOT ENCODING FOR THE OUTPUTS
    if(ms.Direction == ENotBlocking)
        write(1);
    else
        write(0);

    write(",");

    if(ms.Direction == EMoveLeft)
        write(1);
    else
        write(0);

    write(",");

    if(ms.Direction == EMoveRight)
        write(1);
    else
        write(0);

    write("\n");
}

///////////////////////////////////////////////////////////////////////////////
/// MAIN PROGRAM
///////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
    // Check input parameter
    if (argc < 2)
        usage(argv[0]);

    /** 
    * argv[1] : rom
    * argv[2] : media? true/>false<
    * argv[3] : print_ram? true/>false<
    **/
    const bool display_media(argc >= 3 ? atoi(argv[2])==1 : false);
    const bool printRam(argc >= 4 ? atoi(argv[3])==1 : false);
    toCSV = argc == 5 ? atoi(argv[4])==1 : false;

	if (toCSV)
    {
        csv.open("../demon_attack.csv");
        
        for (unsigned i = 5; i < 127; ++i) {
            write(i);
            write(",");
        }

        write(127);
        write(",");

        write("Fire");
        write(",");

        write("MoveLeft");
        write(",");

        write("MoveRight");
        write("\n");
    }
	
    // Init rand seed
    srand(time(NULL));

    // Create alei object.
    alei.setInt("random_seed", rand()%1000);
    alei.setFloat("repeat_action_probability", 0);
    alei.setBool("sound", false);
    alei.setBool("display_screen", display_media);
    alei.loadROM(argv[1]);


    // Init
    lastLives = alei.lives();
    totalReward = .0f;

    // Main loop
    int step;

    /*
    * Bot expl: 
    **/
    for (step = 0; !alei.game_over() && step < maxSteps; ++step) 
    {
        // Debug mode ***********************************
        if(printRam) printRAM();
        if(display_media) checkKeys();
        if(toCSV) ramToCsv();
        // **********************************************

        // Total reward summation
        totalReward += manualInput ? manualMode() : agentStep();
   }

   for (unsigned i = 0; i < lastRAM.size(); ++i) {
        if(lastRAM[i].appearances > 100 && i != 61)
            cout << "[" << i << "]" << lastRAM[i].appearances << endl;
            //cout << i << ", ";
   }

    std::cout << "Steps: " << step << std::endl;
    std::cout << "Reward: " << totalReward << std::endl;
    
    csv.close();
   
   return 0;
}
