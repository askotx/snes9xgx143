/****************************************************************************
 * DVD.CPP
 *
 * This module manages all dvd i/o etc.
 * There is also a simple ISO9660 parser included.
 ****************************************************************************/
#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memmap.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <unistd.h>
#include "gcconfig.h"

/*** Simplified Directory Entry Record I only care about a couple of values ***/
#define RECLEN 0
#define EXTENT 6
#define FILE_LENGTH 14
#define FILE_FLAGS 25
#define FILENAME_LENGTH 32
#define FILENAME 33
#define MAXJOLIET 256

/* GUI Definition */
#define MAXFILES 1000
#define PAGESIZE 15
extern int PADCAL;

#define SNESDIR "snes9x"
#define SAVEDIR "saves"
#define ROMSDIR "roms"

/* SDCARD reading & browsing */
int UseSDCARD = 0;
FILE * filehandle;
char rootSDdir[MAXPATHLEN];
int haveSDdir = 0;

/* File selection */
typedef struct {
    u64 offset;
    unsigned int length;	
    char flags;
    char filename[MAXJOLIET+1];
}FILEENTRIES;
int maxfiles = 0;
int offset = 0;
int selection = 0;
FILEENTRIES filelist[MAXFILES];

/*** DVD Read Buffer ***/
int LoadFromDVD = 0;
unsigned char readbuffer[2048] ATTRIBUTE_ALIGN(32);
//unsigned char savebuffer[0x22000] ATTRIBUTE_ALIGN (32);
volatile long *dvd=(volatile long *)0xCC006000;
extern void SendDriveCode( int model );
extern int font_height;
extern unsigned int blit_lookup[4];
extern unsigned int blit_lookup_inv[4];
extern void writex(int x, int y, int sx, int sy, char *string, unsigned int *lookup);
extern bool isZipFile();
extern int unzipDVDFile( unsigned char *outbuffer, unsigned int discoffset, unsigned int length);
extern void mdelay(unsigned int us);
extern int IsXenoGCImage( char *buffer );
void GetSDInfo();

extern int SaveTheSRAM( int mode, int slot, int type);
extern int autoSaveLoad;
extern int ChosenSlot;
extern int ChosenDevice;
extern unsigned char isWii;
int sdslot = 0;

/****************************************************************************
 * DVD Lowlevel Functions
 *
 * These are here simply because the same functions in libogc are not
 * exposed to the user
 ****************************************************************************/

void dvd_inquiry()
{

    dvd[0] = 0x2e;
    dvd[1] = 0;
    dvd[2] = 0x12000000;
    dvd[3] = 0;
    dvd[4] = 0x20;
    dvd[5] = 0x80000000;
    dvd[6] = 0x20;
    dvd[7] = 3;

    while( dvd[7] & 1 );
    DCFlushRange((void *)0x80000000, 32);
}

/****************************************************************************
 * uselessinquiry
 *
 * As the name suggests, this function is quite useless.
 * It's only purpose is to stop any pending DVD interrupts while we use the
 * memcard interface.
 *
 * libOGC tends to foul up if you don't, and sometimes does if you do!
 ****************************************************************************/
void uselessinquiry ()
{

    dvd[0] = 0;
    dvd[1] = 0;
    dvd[2] = 0x12000000;
    dvd[3] = 0;
    dvd[4] = 0x20;
    dvd[5] = 0x80000000;
    dvd[6] = 0x20;
    dvd[7] = 1;

    while (dvd[7] & 1);

}

void dvd_unlock()
{
    dvd[0] |= 0x00000014;
    dvd[1] = 0x00000000;
    dvd[2] = 0xFF014D41;
    dvd[3] = 0x54534849;
    dvd[4] = 0x54410200;
    dvd[7] = 1;
    while ((dvd[0] & 0x14) == 0) { }
    dvd[0] |= 0x00000014;
    dvd[1] = 0x00000000;
    dvd[2] = 0xFF004456;
    dvd[3] = 0x442D4741;
    dvd[4] = 0x4D450300;
    dvd[7] = 1;
    while ((dvd[0] & 0x14) == 0) { }
}

void dvd_extension()
{
    dvd[0] = 0x2E;
    dvd[1] = 0;

    dvd[2] = 0x55010000;
    dvd[3] = 0;
    dvd[4] = 0;
    dvd[5] = 0;
    dvd[6] = 0;
    dvd[7] = 1; // enable reading!
    while (dvd[7] & 1);
}

#define DEBUG_STOP_DRIVE 0
#define DEBUG_START_DRIVE 0x100
#define DEBUG_ACCEPT_COPY 0x4000
#define DEBUG_DISC_CHECK 0x8000

void dvd_motor_on_extra()
{

    dvd[0] = 0x2e;
    dvd[1] = 0;
    dvd[2] = 0xfe110000 | DEBUG_START_DRIVE | DEBUG_ACCEPT_COPY | DEBUG_DISC_CHECK;
    dvd[3] = 0;
    dvd[4] = 0;
    dvd[5] = 0;
    dvd[6] = 0;
    dvd[7] = 1;
    while ( dvd[7] & 1 );

}

void dvd_motor_off( )
{
    dvd[0] = 0x2e;
    dvd[1] = 0;
    dvd[2] = 0xe3000000;
    dvd[3] = 0;
    dvd[4] = 0;
    dvd[5] = 0;
    dvd[6] = 0;
    dvd[7] = 1; // Do immediate
    while (dvd[7] & 1);

    /*** PSO Stops blackscreen at reload ***/
    dvd[0] = 0x14;
    dvd[1] = 0;
}

void dvd_setstatus()
{
    dvd[0] = 0x2E;
    dvd[1] = 0;

    dvd[2] = 0xee060300;
    dvd[3] = 0;
    dvd[4] = 0;
    dvd[5] = 0;
    dvd[6] = 0;
    dvd[7] = 1; // enable reading!
    while (dvd[7] & 1);
}

unsigned int dvd_read_id(void *dst)
{
    if ((((int)dst) & 0xC0000000) == 0x80000000) // cached?
        DCInvalidateRange((void *)dst, 0x20);

    dvd[0] = 0x2E;
    dvd[1] = 0;

    dvd[2] = 0xA8000040;
    dvd[3] = 0;
    dvd[4] = 0x20;
    dvd[5] = (unsigned long)dst;
    dvd[6] = 0x20;
    dvd[7] = 3; // enable reading!

    while (dvd[7] & 1);

    if (dvd[0] & 0x4)
        return 1;
    return 0;
}

unsigned int dvd_read(void *dst, unsigned int len, u64 offset) {
    unsigned char* buffer = (unsigned char*)(unsigned int)readbuffer;

    if (len > 2048 )
        return 1; // We only allow 2k reads

    DCInvalidateRange ((void *) buffer, len);
    if ( (offset<0x57057C00) || (isWii && offset < 0x1FD3E0000LL) ) { // Don't read past 8,543,666,176 DVD9
        offset >>= 2;
        dvd[0] = 0x2E;
        dvd[1] = 0;
        dvd[2] = 0xA8000000;
        dvd[3] = (u32)offset;
        dvd[4] = len;
        dvd[5] = (unsigned long)buffer;
        dvd[6] = len;
        dvd[7] = 3; // enable reading with DMA
        while (dvd[7] & 1);
        memcpy (dst, buffer, len);
    } else // Let's not read past end of DVD
        return 0;

    if (dvd[0] & 0x4)               /* Ensure it has completed */
        return 0;

    return 1;
}

void dvd_reset(void) {

    *(unsigned long*)0xcc006004 = 2;
    unsigned long v = *(unsigned long*)0xcc003024;
    *(unsigned long*)0xcc003024 = (v &~4) | 1;

    mdelay(1);

    *(unsigned long*)0xcc003024 = v | 5;
}


/****************************************************************************
 * ISO Parsing Functions
 ****************************************************************************/
#define PVDROOT 0x9c
static int IsJoliet = 0;
static u64 rootdir = 0;
static int rootdirlength = 0;
int IsPVD() {
    int sector = 16;
    u32 rootdir32;

    rootdir = rootdirlength = 0;
    IsJoliet = -1;

    /*** Read the ISO section looking for a valid
      Primary Volume Decriptor.
      Spec says first 8 characters are id ***/
    // Look for Joliet PVD first
    while ( sector < 32 ) {
        int res = dvd_read( &readbuffer, 2048, sector << 11 );
        if (res) {
            if ( memcmp( &readbuffer, "\2CD001\1", 8 ) == 0 ) {
                memcpy(&rootdir32, &readbuffer[PVDROOT + EXTENT], 4);
                rootdir = (u64)rootdir32;
                rootdir <<= 11;
                memcpy(&rootdirlength, &readbuffer[PVDROOT + FILE_LENGTH], 4);
                IsJoliet = 1;
                break;
            }
        } else
            return 0;
        sector++;
    }

    if (IsJoliet > 0)
        return 1;

    sector = 16;
    // Look for ISO9660 PVD next
    while ( sector < 32 ) {
        int res = dvd_read( &readbuffer, 2048, sector << 11 );
        if (res) {
            if ( memcmp( &readbuffer, "\1CD001\1", 8 ) == 0 ) {
                memcpy(&rootdir32, &readbuffer[PVDROOT + EXTENT], 4);
                rootdir = (u64)rootdir32;
                rootdir <<= 11;
                memcpy(&rootdirlength, &readbuffer[PVDROOT + FILE_LENGTH], 4);
                IsJoliet = 0;
                break;
            }
        } else
            return 0;
        sector++;
    }

    return (IsJoliet == 0);	
}

/****************************************************************************
 * getfiles
 *
 * Retrieve the current file directory entry
 ****************************************************************************/
static int diroffset = 0;
int getfiles( int filecount ) {
    char fname[MAXJOLIET];
    char *ptr;
    char *filename;
    char *filenamelength;
    char *rr;
    int j;
    u32 offset32;

    /*** Do some basic checks ***/
    if ( filecount >= MAXFILES ) return 0;
    if ( diroffset >= 2048 ) return 0;

    /*** Now decode this entry ***/
    if ( readbuffer[diroffset] != 0 ) {
        /* Update offsets into sector buffer */
        ptr = (char *)&readbuffer[0];
        ptr += diroffset;
        filename = ptr + FILENAME;
        filenamelength = ptr + FILENAME_LENGTH;

        /* Check for wrap round - illegal in ISO spec,
         * but certain crap writers do it! */
        if ( diroffset + readbuffer[diroffset] > 2048 ) return 0;

        if ( *filenamelength ) {
            memset(&fname, 0, 512);

            /*** Return the values needed ***/
            if (!IsJoliet)
                strcpy(fname, filename);
            else {			
                for ( j = 0; j < ( *filenamelength >> 1 ); j++ ) {
                    fname[j] = filename[j*2+1];
                }

                fname[j] = 0;

                if ( strlen(fname) >= MAXJOLIET ) fname[MAXJOLIET] = 0;
                if ( strlen(fname) == 0 ) fname[0] = filename[0];
            }

            if ( strlen(fname) == 0 ) strcpy(fname,"ROOT");
            else {
                if ( fname[0] == 1 ) strcpy(fname,"..");
                else{
                    //fname[ *filenamelength ] = 0;
                    /*
                     * Move *filenamelength to t,
                     * Only to stop gcc warning for noobs :)
                     */
                    int t = *filenamelength;
                    fname[t] = 0;
                }
            }

            /** Rockridge Check **/ /*** Remove any trailing ;1 from ISO name ***/
            rr = strstr (fname, ";"); //if ( fname[ strlen(fname) - 2 ] == ';' )
            if (rr != NULL) *rr = 0;  //fname[ strlen(fname) - 2 ] = 0;*/

            strcpy(filelist[filecount].filename, fname);
            memcpy(&offset32, &readbuffer[diroffset + EXTENT], 4);
            filelist[filecount].offset = (u64)offset32;
            memcpy(&filelist[filecount].length, &readbuffer[diroffset + FILE_LENGTH], 4);
            memcpy(&filelist[filecount].flags, &readbuffer[diroffset + FILE_FLAGS], 1);
char msg[1024];
sprintf(msg, "Adding file#%d: %s", filecount, fname);
ShowAction(msg);

            filelist[filecount].offset <<= 11;
            filelist[filecount].flags = filelist[filecount].flags & 2;

            /*** Prepare for next entry ***/
            diroffset += readbuffer[diroffset];

            return 1;
        } 		
    }
    return 0;
}

 #ifdef __gamecube__
/****************************************************************************
 * ParseDirectory
 *
 * Parse the isodirectory, returning the number of files found
 ****************************************************************************/
int parsedir() {
    int pdlength;
    u64 pdoffset;
    u64 rdoffset;
    int len = 0;
    int filecount = 0;

    pdoffset = rdoffset = rootdir;
    pdlength = rootdirlength;
    filecount = 0;

    /*** Clear any existing values ***/
    memset(&filelist, 0, sizeof(FILEENTRIES) * MAXFILES);
    /*** Get as many files as possible ***/			
    while ( len < pdlength ) {
        int r = dvd_read(&readbuffer, 2048, pdoffset);
        //if (dvd_read (&readbuffer, 2048, pdoffset) == 0)
        if (r == 0)
            return 0;
        diroffset = 0;

        while ( getfiles( filecount ) ) {
            if ( filecount < MAXFILES )
                filecount++;
        }

        len += 2048;
        pdoffset = rdoffset + len;
    }
    return filecount;
}
#endif
/***************************************************************************
 * Update SDCARD curent directory name 
 ***************************************************************************/ 
int updateSDdirname()
{
    int size=0;
    char *test;
    char temp[1024];

    /* current directory doesn't change */
    if (strcmp(filelist[selection].filename,".") == 0) return 0; 

    /* go up to parent directory */
    else if (strcmp(filelist[selection].filename,"..") == 0) {
        /* determine last subdirectory namelength */
        sprintf(temp,"%s",rootSDdir);
        test = strtok(temp,"/");
        while (test != NULL) { 
            size = strlen(test);
            test = strtok(NULL,"/");
        }

        /* remove last subdirectory name */
        size = strlen(rootSDdir) - size - 1;
        rootSDdir[size] = 0;

        /* handles root name */
        if (strcmp(rootSDdir, "/") == 0)
            sprintf(rootSDdir,"fat:/");

        return 1;
    } else {
        /* test new directory namelength */
        if ((strlen(rootSDdir)+1+strlen(filelist[selection].filename)) < MAXPATHLEN) 
        {
            /* handles root name */
            sprintf(temp, "/%s/..", SNESDIR);
            if (strcmp(rootSDdir, temp) == 0) 
                sprintf(rootSDdir,"fat:/");

            /* update current directory name */
            sprintf(rootSDdir, "%s/%s",rootSDdir, filelist[selection].filename);
            return 1;
        }
        else
        {
            WaitPrompt((char*)"Dirname is too long !"); 
            return -1;
        }
    } 
}

/***************************************************************************
 * Browse SDCARD subdirectories 
 ***************************************************************************/ 
int parseSDdirectory() {
    int numstored = 0;
    DIR_ITER *sddir;
    char filename[MAXPATHLEN];
    struct stat filestat;
	char msg[128];
    
    /* initialize selection */
    selection = offset = 0;

    /* open the directory */ 
    sddir = diropen(rootSDdir);
	if (sddir == NULL) {
        strcpy(rootSDdir, "fat:/");
        sddir = diropen(rootSDdir);
        WaitPrompt(msg);
        if (sddir == NULL) {
            sprintf(msg, "Error opening %s", rootSDdir);
            WaitPrompt(msg);
            return 0;
        }
    }

    /* Move to DVD structure - this is required for the file selector */ 
    while(dirnext(sddir,filename,&filestat) == 0) {
        if(strcmp(filename,".") != 0) {
            memset(&filelist[numstored], 0, sizeof(FILEENTRIES));
            strncpy(filelist[numstored].filename, filename, MAXPATHLEN);
            filelist[numstored].length = filestat.st_size;
            filelist[numstored].flags = (filestat.st_mode & _IFDIR) == 0 ? 0 : 1;
            numstored++;
        }
    }

    /*** close directory ***/
    dirclose(sddir);

    return numstored;
}

/****************************************************************************
 * ShowFiles
 *
 * Support function for FileSelector
 ****************************************************************************/
void ShowFiles( int offset, int selection ) {
    int i,j;
    char text[80];

    ClearScreen();

    j = 0;
    for ( i = offset; i < ( offset + PAGESIZE ) && ( i < maxfiles ); i++ ) {
        if ( filelist[i].flags ) {
            strcpy(text,"[");
            strcat(text, filelist[i].filename);
            strcat(text,"]");
        } else
            strcpy(text, filelist[i].filename);

        char dir[1024];
        if (UseSDCARD)
            strcpy(dir, rootSDdir);
        else
            dir[0] = 0;

        writex(CentreTextPosition(dir), 32, GetTextWidth(dir), font_height, dir, blit_lookup);
        while (GetTextWidth(text) > 620)
            text[strlen(text)-2] = 0;
        if ( j == ( selection - offset ) )
            writex( CentreTextPosition(text), ( j * font_height ) + 64,	GetTextWidth(text), font_height, text, blit_lookup_inv );
        else
            writex( CentreTextPosition(text), ( j * font_height ) + 64, GetTextWidth(text), font_height, text, blit_lookup );

        j++;		
    }

    SetScreen();
}

/****************************************************************************
 * FileSelector
 *
 * Let user select another ROM to load
 ****************************************************************************/
/*int selection = 0;*/
extern int showspinner;

void FileSelector() {
    short p=0;
    signed char a;
    int haverom = 0;
    int redraw = 1;

    showspinner = 0;
    while ( haverom == 0 ) {
        if ( redraw ) ShowFiles( offset, selection );

        redraw = 0;
        p = PAD_ButtonsDown(0);
        a = PAD_StickY(0);

        if (p & PAD_BUTTON_B) return;

        if ( ( p & PAD_BUTTON_DOWN ) || ( a < -PADCAL ) ) {
            selection++;
            if ( selection == maxfiles ) selection = offset = 0;		
            if ( ( selection - offset ) >= PAGESIZE ) offset += PAGESIZE;
            redraw = 1;
        } // End of down
        if ( ( p & PAD_BUTTON_UP ) || ( a > PADCAL ) ) {
            selection--;
            if ( selection < 0 ){
                selection = maxfiles - 1;
                offset = selection - PAGESIZE + 1;
            }
            if ( selection < offset ) offset -= PAGESIZE;
            if ( offset < 0 ) offset = 0;			
            redraw = 1;
        } // End of Up

        if (( p & PAD_BUTTON_LEFT ) || (p & PAD_TRIGGER_L)) {
            /*** Go back a page ***/
            selection -= PAGESIZE;
            if ( selection < 0 ) {
                selection = maxfiles - 1;
                offset = selection-PAGESIZE + 1;
            }
            if ( selection < offset ) offset -= PAGESIZE;
            if ( offset < 0 ) offset = 0;	
            redraw = 1;
        }

        if (( p & PAD_BUTTON_RIGHT ) || (p & PAD_TRIGGER_R)) {
            /*** Go forward a page ***/
            selection += PAGESIZE;
            if ( selection > maxfiles - 1 ) selection = offset = 0;
            if ( ( selection - offset ) >= PAGESIZE ) offset += PAGESIZE;
            redraw = 1;
        }

        if ( p & PAD_BUTTON_A ) {
            if ( filelist[selection].flags ) { /*** This is directory ***/
                if (UseSDCARD) {
                    /* update current directory and set new entry list if directory has changed */
                    int status = updateSDdirname();
                    if (status == 1) {							
                        maxfiles = parseSDdirectory();
                        if (!maxfiles) {
                            WaitPrompt ((char*)"Error reading directory!");
                            haverom   = 1; // quit SD menu
                            haveSDdir = 0; // reset everything at next access
                        }
                    } else if (status == -1) {
                        haverom   = 1; // quit SD menu
                        haveSDdir = 0; // reset everything at next access
                    }
                } 
#ifdef __gamecube__
                else {
                    rootdir = filelist[selection].offset;
                    rootdirlength = filelist[selection].length;
                    offset = selection = 0;
                    maxfiles = parsedir();
                }
#endif
            } else {
                rootdir = filelist[selection].offset;
                rootdirlength = filelist[selection].length;
                /*** Put ROM Load Routine Here :) ***/
                LoadFromDVD = 1;
                Memory.LoadROM( "DVD" );
                Memory.LoadSRAM( "DVD" ); // doesn't do anything
                if (autoSaveLoad && Memory.SRAMSize)
                    if (SaveTheSRAM(0, ChosenSlot, ChosenDevice))
                        S9xSoftReset(); // Reset emu
                haverom = 1;
            }
            redraw = 1;
        }
    }
    showspinner = 1;
}

/****************************************************************************
 * LoadDVDFile
 ****************************************************************************/
int LoadDVDFile( unsigned char *buffer ) {
    int offset;
    int blocks;
    int i;
    u64 discoffset;

    /*** SDCard Addition ***/
    if (UseSDCARD) GetSDInfo ();
    if (rootdirlength == 0)  return 0;

    /*** How many 2k blocks to read ***/
    blocks = rootdirlength / 2048;
    offset = 0;
    discoffset = rootdir;

    ShowAction((char*)"Loading ... Wait");
	
    if (UseSDCARD) fread(readbuffer, 1, 2048, filehandle);
    else dvd_read(&readbuffer, 2048, discoffset);

    if ( isZipFile() == false ) {
    
        if (UseSDCARD) fseek(filehandle, 0, SEEK_SET);
        
        for ( i = 0; i < blocks; i++ ) {
            if (UseSDCARD) fread(readbuffer, 1, 2048, filehandle);
            else dvd_read(&readbuffer, 2048, discoffset);
            memcpy(&buffer[offset], &readbuffer, 2048);
            offset += 2048;
            discoffset += 2048;
        }

        /*** And final cleanup ***/
        if( rootdirlength % 2048 ) {
            i = rootdirlength % 2048;
            if (UseSDCARD) fread(readbuffer, 1, i, filehandle);
            else dvd_read(&readbuffer, 2048, discoffset);
            memcpy(&buffer[offset], &readbuffer, i);
        }
    } else {		
        return unzipDVDFile( buffer, discoffset, rootdirlength);
    }
    if (UseSDCARD) fclose(filehandle);

    return rootdirlength;
}

#ifdef __gamecube__
/****************************************************************************
 * OpenDVD
 *
 * This function performs the swap task for softmodders.
 * For Viper/Qoob users, sector 0 is read, and if it contains all nulls
 * an ISO disc is assumed.
 ****************************************************************************/
static int havedir = 0;

int OpenDVD() {
    haveSDdir = 0;

    // Mount the DVD if necessary
    if (!IsPVD()) {
        ShowAction((char*)"Mounting DVD");
        DVD_Mount();
        havedir = 0;
        if (!IsPVD()) {
            WaitPrompt((char*)"Error reading DVD");
            return 0; // No correct ISO9660 DVD
        }
    }

    /*** At this point I should have an unlocked DVD ... so let's do the ISO ***/
    if ( havedir != 1 ) {
        if ( IsPVD() ) {
            /*** Have a valid PVD, so start reading directory entries ***/
            maxfiles = parsedir();	
            if ( maxfiles ) {
                offset = selection = 0;
                FileSelector();
                havedir = 1;
            }
        } else {
            return 0;
        }
    } else  {
        FileSelector();
    }

    return 1;
}
#endif

int OpenSD () {
    UseSDCARD = 1;
    char msg[128];

    if (ChosenSlot != sdslot) haveSDdir = 0;

    if (haveSDdir == 0) {
#ifdef __gamecube__
        /* don't mess with DVD entries */
        havedir = 0;
#endif
        /* Reset SDCARD root directory */
        sprintf(rootSDdir,"/%s/%s", SNESDIR, ROMSDIR);
        sdslot = ChosenSlot;

        /* Parse initial root directory and get entries list */
        ShowAction((char*)"Reading Directory ...");
        if ((maxfiles = parseSDdirectory ())) {
            sprintf (msg, "Found %d entries", maxfiles);
            ShowAction(msg);
            /* Select an entry */
            FileSelector ();

            /* memorize last entries list, actual root directory and selection for next access */
            haveSDdir = 1;
        } else {
            /* no entries found */
            sprintf (msg, "Error reading %s", rootSDdir);
            ShowAction (msg);
            return 0;
        }
    }
    /* Retrieve previous entries list and made a new selection */
    else  FileSelector ();

    return 1;
}


/****************************************************************************
 * SDCard Get Info
 ****************************************************************************/ 
void GetSDInfo () 
{
    char fname[MAXPATHLEN];
    rootdirlength = 0;

    /* Check filename length */
    if ((strlen(rootSDdir)+1+strlen(filelist[selection].filename)) < MAXPATHLEN)
        sprintf(fname, "%s/%s",rootSDdir,filelist[selection].filename); 

    else
    {
        WaitPrompt ((char*)"Maximum Filename Length reached !"); 
        haveSDdir = 0; // reset everything before next access
    }

    filehandle = fopen(fname, "rb");
    if (filehandle == NULL)
    {
        WaitPrompt ((char*)"Unable to open file!");
        return;
    }
    fseek(filehandle, 0, SEEK_END);
    rootdirlength = ftell(filehandle);
    fseek(filehandle, 0, SEEK_SET);
}
