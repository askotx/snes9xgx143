/****************************************************************************
 * GC Config and Information Screens 
 *
 * Most of this code is reused from FCEU-GC
 ****************************************************************************/

#include <gccore.h>
#include <string.h>
#include <memmap.h>
#include <snes9x.h>
#include <soundux.h>
#include "iplfont.h"

extern unsigned int copynow;
extern GXRModeObj *vmode;
extern unsigned int *xfb[2];
extern int whichfb;
extern int font_size[256];
extern int font_height;
extern unsigned int blit_lookup[4];
extern unsigned int blit_lookup_inv[4];
extern void GetISODirectory();
extern void AnimFrame();
extern int SaveSRAM( int mode, int slot, int type);
extern int OpenDVD();
extern int OpenSD();
extern void dvd_motor_off();
extern int timerstyle;
extern int PADCAL;

extern int showspinner;

extern int showcontroller;

extern int UseSDCARD;

extern void uselessinquiry ();

extern int allowupdown;

extern unsigned char isWii;

void credits();
int ConfigMenu();
void SetScreen();
void ClearScreen();

char *title = "Snes9x 1.43 - GX Edition 0.1.0";
extern int CARDSLOT;

#define SOFTRESET_ADR ((volatile u32*)0xCC003024)

/****************************************************************************
 * Font drawing support
 ****************************************************************************/
int GetTextWidth( char *text )
{
    unsigned int i, w = 0;

    for ( i = 0; i < strlen(text); i++ )
        w += font_size[(int)text[i]];

    return w;
}

int CentreTextPosition( char *text )
{
    return ( ( 640 - GetTextWidth(text) ) >> 1 );
}

void WriteCentre( int y, char *text )
{
    write_font( CentreTextPosition(text), y, text);
}

/**
 * Wait for user to press A or B. Returns 0 = B; 1 = A
 */
int WaitButtonAB ()
{
    int btns;

    while ( (PAD_ButtonsDown (0) & (PAD_BUTTON_A | PAD_BUTTON_B)) );

    while ( TRUE )
    {
        btns = PAD_ButtonsDown (0);
        if ( btns & PAD_BUTTON_A )
            return 1;
        else if ( btns & PAD_BUTTON_B )
            return 0;
    }
}

/**
 * Show a prompt with choice of two options. Returns 1 if A button was pressed
 and 0 if B button was pressed.
 */
int WaitPromptChoice (char *msg, char *bmsg, char *amsg)
{
    char choiceOption[80];  
    sprintf (choiceOption, "B = %s   :   A = %s", bmsg, amsg);

    ClearScreen ();  
    WriteCentre(220, msg);
    WriteCentre(220 + font_height, choiceOption);  
    SetScreen ();

    return WaitButtonAB ();
}

void WaitPrompt( char *msg )
{
    int quit = 0;

    while ( PAD_ButtonsDown(0) & PAD_BUTTON_A ) {} ;

    while( !(PAD_ButtonsDown(0) & PAD_BUTTON_A ) && (quit == 0 ))
    {
        ClearScreen();
        WriteCentre( 220, msg);
        WriteCentre( 220 + font_height, "Press A to Continue");

        if ( PAD_ButtonsDown(0) & PAD_BUTTON_A )
            quit = 1;

        SetScreen();
    }
}

void ShowAction( char *msg )
{
    ClearScreen();
    WriteCentre( 220 + ( font_height >> 1), msg);
    SetScreen();
}

/****************************************************************************
 * SetScreen
 ****************************************************************************/
void SetScreen()
{
    VIDEO_SetNextFramebuffer( xfb[whichfb] );
    VIDEO_Flush();
    VIDEO_WaitVSync();
}

/****************************************************************************
 * ClearScreen
 ****************************************************************************/
void ClearScreen()
{

    copynow = GX_FALSE;
    whichfb ^= 1;
    VIDEO_ClearFrameBuffer(vmode, xfb[whichfb], COLOR_WHITE);
    AnimFrame();
}

/****************************************************************************
 * Welcome and ROM Information
 ****************************************************************************/
void Welcome()
{

    char work[1024];
    char titles[9][10] = {
        { "ROM" }, { "ROM ID" }, { "Company" },
        { "Size" }, { "SRAM" }, { "Type" },
        { "Checksum" }, { "TV Type" }, { "Frames" } };
    int p = 96;
    int i;
    int quit = 0;

    showspinner = 0;

    while ( quit == 0 )
    {
        p = 96;
        ClearScreen();

        /*** Title ***/
        write_font(CentreTextPosition(title) , ( 480 - ( 16 * font_height )) >> 1 , title);

        /*** Print titles ***/
        for ( i = 0; i < 9; i++ )
            write_font( 48, p += font_height, titles[i]);

        /*** Show some basic ROM information ***/
        p = 96;
        write_font( 592 - GetTextWidth(Memory.ROMName), p += font_height, Memory.ROMName);
        write_font( 592 - GetTextWidth(Memory.ROMId), p += font_height, Memory.ROMId);
        write_font( 592 - GetTextWidth(Memory.CompanyId), p += font_height, Memory.CompanyId);
        sprintf(work, "%d", Memory.ROMSize);
        write_font( 592 - GetTextWidth(work), p += font_height, work);
        sprintf(work, "%d", Memory.SRAMSize);
        write_font( 592 - GetTextWidth(work), p += font_height, work);
        sprintf(work, "%d", Memory.ROMType);
        write_font( 592 - GetTextWidth(work), p += font_height, work);
        sprintf(work, "%04x / %04x", Memory.ROMChecksum, Memory.ROMComplementChecksum);
        write_font( 592 - GetTextWidth(work), p += font_height, work);
        sprintf(work, "%s", Settings.PAL ? "PAL" : "NTSC");
        write_font( 592 - GetTextWidth(work), p += font_height, work);
        sprintf(work, "%d", Memory.ROMFramesPerSecond);
        write_font( 592 - GetTextWidth(work), p += font_height, work);

        strcpy(work,"Enjoy the past!");
        write_font( CentreTextPosition(work), 400, work);

        SetScreen();

        if (PAD_ButtonsDown(0) & (PAD_BUTTON_A | PAD_BUTTON_B) ) quit = 1;
    }

    showspinner = 1;
}

/****************************************************************************
 * Credits section
 ****************************************************************************/
void credits()
{
    int quit = 0;

    showspinner = 0;

    while (quit == 0)
    {
        ClearScreen();

        /*** Title ***/
        write_font(CentreTextPosition(title) , ( 480 - ( 16 * font_height )) >> 1 , title);

        int p = 80;

        WriteCentre( p += font_height, "Snes9x 1.43 - Snes9x Team");
        WriteCentre( p += font_height, "GC port - softdev");
        WriteCentre( p += font_height, "GX info - http://www.gc-linux.org");
        WriteCentre( p += font_height, "Font - Qoob/or9");
        WriteCentre( p += font_height, "libogc - shagkur/wntrmute");
        WriteCentre( p += font_height, "DVD lib - Ninjamod Team");

        p += font_height;
        WriteCentre( p += font_height, "Testing");
        WriteCentre( p += font_height, "Mithos / luciddream / softdev");

        p += font_height;
        WriteCentre( p += font_height, "Greets : brakken, HonkeyKong");
        WriteCentre( p += font_height, "cedy_nl, raz, scognito");

        //p += font_height;
        WriteCentre( p += font_height, "Extras: KruLLo, Askot, dsbomb");
        WriteCentre( p += font_height, "Support - http://www.tehskeen.com");

        SetScreen();

        if (PAD_ButtonsDown(0) & (PAD_BUTTON_A | PAD_BUTTON_B) ) quit = 1;
    }

    showspinner = 1;
}

/****************************************************************************
 * DrawMenu
 *
 * Display the requested menu
 ****************************************************************************/
void DrawMenu(char *title, char items[][20], int maxitems, int select)
{
    int i,w,p,h;

    ClearScreen();

    /*** Draw Title Centred ***/
    h = (480 - (( maxitems + 3 ) * font_height)) / 2;
    write_font( CentreTextPosition(title), h, title);

    p = h + ( font_height << 1 );

    for( i = 0; i < maxitems; i++ )
    {
        w = CentreTextPosition(items[i]);
        h = GetTextWidth(items[i]);

        if ( i == select )
            writex( w, p, h, font_height, (char *)items[i], blit_lookup_inv );
        else
            writex( w, p, h, font_height, (char *)items[i], blit_lookup );

        p += font_height;
    }

    SetScreen();
}

/****************************************************************************
 * PADMap
 *
 * Remap a pad to the correct key
 ****************************************************************************/
extern unsigned short gcpadmap[12];
char PADMap( int padvalue, int padnum )
{
    char padkey;

    switch( padvalue )
    {
        case 0: gcpadmap[padnum] = PAD_BUTTON_A; padkey = 'A'; break;
        case 1: gcpadmap[padnum] = PAD_BUTTON_B; padkey = 'B'; break;
        case 2: gcpadmap[padnum] = PAD_BUTTON_X; padkey = 'X'; break;
        case 3: gcpadmap[padnum] = PAD_BUTTON_Y; padkey = 'Y'; break;
    }

    return padkey;
}

/****************************************************************************
 * PAD Configuration
 *
 * This screen simply let's the user swap A/B/X/Y around.
 ****************************************************************************/
int configpadcount = 8;
char padmenu[8][20] = { 
    { "SETUP A" }, 
    { "SNES BUTTON A - X" }, { "SNES BUTTON B - A" },
    { "SNES BUTTON X - B" }, { "SNES BUTTON Y - Y" },
    { "ANALOG CLIP   - 70"}, { "ALLOW U+D / L+R OFF"}, 
    { "Return to previous" } 
};
/*int padmap[4] = { 0,1,2,3};*/
int conmap[5][4] = { 
    { 2,0,1,3 }, { 2,0,3,1 }, 
    { 2,3,1,0 }, { 2,1,3,0 }, 
    { 0,1,2,3} 
};
char mpads[4];

int PADCON = 0;

void ConfigPAD()
{

    int menu = 0;
    short j;
    int redraw = 1;
    int quit = 0;
    int i;

    showcontroller = 1;

    for ( i = 0; i < 4; i++ )
    {
        mpads[i] = padmenu[i+1][16] == 'A' ? 0 :
            padmenu[i+1][16] == 'B' ? 1 :
            padmenu[i+1][16] == 'X' ? 2 :
            padmenu[i+1][16] == 'Y' ? 3 : 0;
    }

    while ( quit == 0 )
    {
        if ( redraw ) {
            sprintf(padmenu[0],"SETUP %c", PADCON + 65);
            strcpy(padmenu[6], allowupdown == 1 ? "ALLOW U+D / L+R ON" : "ALLOW U+D / L+R OFF");
            sprintf(padmenu[5],"ANALOG CLIP   - %d", PADCAL);
            DrawMenu("Gamecube Pad Configuration", &padmenu[0], configpadcount, menu);
        }

        if (j & PAD_BUTTON_B) {
            quit = 1;
        }

        redraw = 1;

        j = PAD_ButtonsDown(0);

        if ( j & PAD_BUTTON_DOWN ) {
            menu++;
            redraw = 1;
        }

        if ( j & PAD_BUTTON_UP ) {
            menu--;
            redraw = 1;
        }

        if ( j & PAD_BUTTON_A ) {
            redraw = 1;
            switch( menu ) {

                case 0: PADCON++;
                        if ( PADCON == 5 )
                            PADCON = 0;
                        for ( i = 0; i < 4; i++ ) {
                            mpads[i] = conmap[PADCON][i];
                            padmenu[i+1][16] = PADMap( mpads[i], i );
                        }
                        i = -1;
                        break;

                case 1: i = 0; break;

                case 2: i = 1; break;

                case 3: i = 2; break;

                case 4: i = 3; break;

                case 5: i = -1;
                        PADCAL += 5;
                        if ( PADCAL > 90 )
                            PADCAL = 40;

                        sprintf(padmenu[5],"ANALOG CLIP   - %d", PADCAL);
                        break;

                case 6: i = -1;
                        allowupdown ^= 1;
                        break;

                case 7: quit = 1; break;
                default: break;
            }

            if ( ( quit == 0 ) && ( i >= 0)) {
                mpads[i]++;
                if ( mpads[i] == 4 )
                    mpads[i] = 0;

                padmenu[i+1][16] = PADMap( mpads[i], i );
            }
        }

        if ( j & PAD_BUTTON_B ) quit = 1;

        if ( menu < 0 )
            menu = configpadcount - 1;

        if ( menu == configpadcount )
            menu = 0;
    }

    showcontroller = 0;

    return;
}

int sgmcount = 5;
char sgmenu[5][20] = { 
    { "Use: SLOT A" }, { "Device:  MCARD" },
    { "Save SRAM" }, { "Load SRAM" },	
    { "Return to previous" } 
};

int slot = 0;
int device = 0;

void savegame()
{
    int menu = 0;
    short j;
    int redraw = 1;
    int quit = 0;

    while ( quit == 0 )
    {
        if ( redraw ){
            sprintf(sgmenu[0], (slot == 0) ? "Use: SLOT A" : "Use: SLOT B");
            sprintf(sgmenu[1], (device == 0) ? "Device:  MCARD" : "Device: SDCARD");
            DrawMenu("Save Game Manager", &sgmenu[0], sgmcount, menu);
        } 

        redraw = 1;

        j = PAD_ButtonsDown(0);

        if ( j & PAD_BUTTON_DOWN ) {
            menu++;
            redraw = 1;
        }

        if ( j & PAD_BUTTON_UP ) {
            menu--;
            redraw = 1;
        }

        if ( j & PAD_BUTTON_A ) {
            redraw = 1;

            while( PAD_ButtonsDown(0) & PAD_BUTTON_A ) {};
            switch( menu ) {
                case 0 :
                    slot ^= 1;
                    break;
                case 1 : 
                    device ^= 1;
                    break;
                case 2 : 
                    SaveSRAM(1,slot,device); //Save
                    break;
                case 3 :
                    SaveSRAM(0,slot,device);  //Load
                    break;
                case 4 :
                    quit = 1; 
                    break;
            }
        } 

        if (j & PAD_BUTTON_B) quit = 1;

        if ( menu < 0 ) menu = sgmcount - 1;

        if ( menu == sgmcount ) menu = 0;
    }
}

/****************************************************************************
 * Emulator Options
 *
 * Moved to new standalone menu for 0.0.4
 ****************************************************************************/
int emumenucount = 8;
char emumenu[8][20] = { 
    {"Sound Echo ON"}, {"Reverse Stereo OFF"},
    {"Transparency ON"}, {"FPS Display OFF"},
    {"Interpolate OFF"}, {"Timer VBLANK"},
    {"Multitap ON"},{"Return to Previous"} 
};
void EmuMenu()
{
    int menu = 0;
    short j;
    int redraw = 1;
    int quit = 0;

    while ( quit == 0 )
    {
        if ( redraw ) {
            /*** Update the settings where needed ***/
            strcpy(emumenu[0], Settings.DisableSoundEcho == FALSE ? "Sound Echo ON" : "Sound Echo OFF");
            strcpy(emumenu[1], Settings.ReverseStereo == FALSE ? "Reverse Stereo OFF" : "Reverse Stereo ON");
            strcpy(emumenu[2], Settings.Transparency == TRUE ? "Transparency ON" : "Transparency OFF" );
            strcpy(emumenu[3], Settings.DisplayFrameRate == TRUE ? "FPS Display ON" : "FPS Display OFF");
            strcpy(emumenu[4], Settings.InterpolatedSound == TRUE ? "Interpolate ON" : "Interpolate OFF");
            strcpy(emumenu[5], timerstyle == 0 ? "Timer VBLANK" : "Timer CLOCK");
            strcpy(emumenu[6], Settings.MultiPlayer5 == true ? "Multitap ON" : "Multitap OFF");
            DrawMenu("Emulator Options", &emumenu[0], emumenucount, menu);
        }

        redraw = 1;

        j = PAD_ButtonsDown(0);

        if ( j & PAD_BUTTON_DOWN ) {
            menu++;
            redraw = 1;
        }

        if ( j & PAD_BUTTON_UP ) {
            menu--;
            redraw = 1;
        }

        if ( j & PAD_BUTTON_A ) {
            redraw = 1;
            switch(menu)
            {
                case 0 :
                    Settings.DisableSoundEcho ^= 1;
                    break;
                case 1 :
                    Settings.ReverseStereo ^= 1;
                    break;
                case 2 :
                    Settings.Transparency ^= 1;
                    break;
                case 3 :
                    Settings.DisplayFrameRate ^= 1;
                    break;
                case 4 :	
                    Settings.InterpolatedSound ^= 1;
                    break;
                case 5 :
                    timerstyle ^= 1;
                    break;
                case 6 :
                    Settings.MultiPlayer5 ^= 1;
                    if (Settings.MultiPlayer5)
                        Settings.ControllerOption = SNES_MULTIPLAYER5;
                    else
                        Settings.ControllerOption = SNES_JOYPAD;
                    Settings.MultiPlayer5Master = Settings.MultiPlayer5;
                    break;	
                case 7 : 	
                    quit = 1;
                    break;

            }			
        }
        if (j & PAD_BUTTON_B) {
            quit = 1;
        }

        if (j & PAD_BUTTON_B) quit = 1;

        if ( menu < 0 )
            menu = emumenucount - 1;

        if ( menu == emumenucount )
            menu = 0;
    }
}

/****************************************************************************
 * Media Select Screen
 ****************************************************************************/

int choosenSDSlot = 0;
int mediacount = 4;

char mediamenu[4][20] = { 
    { "Load from DVD" }, { "Load from SDCARD"}, 
    { "SDGecko: Slot A" }, { "Return to previous" } 
};

int numsdslots = 2;
char sdslots[2][10] = {
        { "Slot A" }, { "Slot B" }
};

int MediaSelect(){

    int menu = 0;
    int quit = 0;
    short j;
    int redraw = 1;

    while ( quit == 0 )
    {
        if ( redraw ) //DrawMenu(&mediamenu[0], mediacount, menu );			
            DrawMenu("Load a Game", &mediamenu[0], mediacount, menu);

        redraw = 0;

        j = PAD_ButtonsDown(0);

        if ( j & PAD_BUTTON_DOWN ) {
            menu++;
            redraw = 1;
        }

        if ( j & PAD_BUTTON_UP ) {
            menu--;
            redraw = 1;
        }

        if ( j & PAD_BUTTON_A ) {
            redraw = 1;
            switch ( menu ) {
                case 0:	UseSDCARD = 0;
                        OpenDVD();
                        return 1;
                        break;

                case 1:	UseSDCARD = 1;
                        OpenSD();
                        return 1;
                        break;
                case 2:
                        choosenSDSlot++;
                        if (choosenSDSlot >= numsdslots) choosenSDSlot = 0;
                        sprintf(mediamenu[2], "SDGecko: %s", sdslots[choosenSDSlot]);
                        break;
                case 3: quit = 1;
                        break;

                default: break ;
            }
        }

        if ( j & PAD_BUTTON_B ) quit = 1;

        if ( menu == mediacount  )
            menu = 0;		

        if ( menu < 0 )
            menu = mediacount - 1;

    }

    return 0;
}

/****************************************************************************
 * Configuration Screen
 *
 * This is the main screen the user sees when they press Z-RIGHT_SHOULDER
 * The available options are:
 *
 *	1.1 Sound Echo (ON/OFF)
 *	1.2 Transparency (ON/OFF)
 *	1.3 FPS Display (ON/OFF)
 *	1.4 Save Game Manager
 *	1.5 Load New ROM 	(DVD Only!)
 *	1.6 Pad Configuration
 *	1.7 PSO/SDReload
 *	1.8 View Credits
 *	1.9 Return to Game
 ****************************************************************************/
int configmenucount = 11;
char configmenu[11][20] = {
    { "Play Game" }, 
    { "Reset Emulator" },
    { "Load New Game" }, 
    { "SRAM Manager" }, 
    { "ROM Information" }, 
    { "Configure Joypads" },
    { "Emulator Options" }, 
    { "Stop DVD Motor" }, 
    { "PSO Reload" },
    { "Reboot Gamecube" }, 
    { "View Credits" }  
};

int ConfigMenu()
{
    int menu = 0;
    short j;
    int redraw = 1;
    int quit = 0;

    void (*PSOReload)() = (void(*)())0x80001800;

    copynow = GX_FALSE;
    Settings.Paused =TRUE;
    S9xSetSoundMute(TRUE);

    if (isWii) {
        strcpy(configmenu[9], "Reboot Wii");
        strcpy(configmenu[8], "SD Reload");
    }

    while ( quit == 0 )
    {
        if ( redraw ) {
            DrawMenu("Snes9x GX Configuration", &configmenu[0], configmenucount, menu);
        }

        redraw = 1;

        j = PAD_ButtonsDown(0);

        if ( j & PAD_BUTTON_DOWN ) {
            menu++;
            redraw = 1;
        }

        if ( j & PAD_BUTTON_UP ) {
            menu--;
            redraw = 1;
        }

        if ( j & PAD_BUTTON_A ) {
            redraw = 1;
            switch( menu ) {

                case 0 : // Play Game
                    quit = 1;
                    break;
                case 1 : // Reset Emulator
                    S9xSoftReset();
                    quit = 1;
                    break;
                case 2 : // Load New Game
                    MediaSelect();
                    break;
                case 3 : // SRAM Manager
                    savegame();
                    break;
                case 4 : // ROM Information
                    Welcome();
                    break;
                case 5 : // Configure Joypads
                    ConfigPAD();
                    break;
                case 6 : // Emulator Options
                    EmuMenu();
                    break;
                case 7: // Stop DVD Motor
                    ShowAction("Stopping DVD ... Wait");
                    dvd_motor_off();
                    WaitPrompt("DVD Motor Stopped");
                    break;
                case 8 : // PSO Reload
                    PSOReload();
                    break;
                case 9 : // Reboot
                    *SOFTRESET_ADR = 0x00000000;
                    break;
                case 10 : // View Credits
                    credits();
                    break;
                default :
                    break;
            }
        }

        if ( j & PAD_BUTTON_B ) quit = 1;
        if ( menu < 0 )
            menu = configmenucount - 1;

        if ( menu == configmenucount )
            menu = 0;
    }

    /*** Remove any still held buttons ***/
    while(PAD_ButtonsHeld(0)) VIDEO_WaitVSync();

    uselessinquiry ();		/*** Stop the DVD from causing clicks while playing ***/

    Settings.Paused = FALSE;
    return 0;
}
