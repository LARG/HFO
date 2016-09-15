/*
  This is Version 2.0 of Rich Sutton's Tile Coding Software
  available from his website at:
  http://www.richsutton.com
*/

#ifndef _TILES2_H_
#define _TILES2_H_

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>

#define MAX_NUM_VARS 20        // Maximum number of variables in a grid-tiling      
#define MAX_NUM_COORDS 100     // Maximum number of hashing coordinates      
#define MaxLONGINT 2147483647  

void GetTiles(
	int tiles[],               // provided array contains returned tiles (tile indices)
	int num_tilings,           // number of tile indices to be returned in tiles       
    int memory_size,           // total number of possible tiles
	float floats[],            // array of floating point variables
    int num_floats,            // number of floating point variables
    int ints[],				   // array of integer variables
    int num_ints);             // number of integer variables

class collision_table {
public:
    collision_table(int,int);
    ~collision_table();
    long m;
    long *data;
    int safe;
    long calls;
    long clearhits;
    long collisions;
    void reset();
    int usage();
    void save(char*, unsigned long);
    void restore(char*, unsigned long);
};
	
void GetTiles(
	int tiles[],               // provided array contains returned tiles (tile indices)
	int num_tilings,           // number of tile indices to be returned in tiles       
    collision_table *ctable,   // total number of possible tiles
	float floats[],            // array of floating point variables
    int num_floats,            // number of floating point variables
    int ints[],				   // array of integer variables
    int num_ints);             // number of integer variables

int hash_UNH(int *ints, int num_ints, long m, int increment);
int hash(int *ints, int num_ints, collision_table *ctable);

// no ints
void GetTiles(int tiles[],int nt,int memory,float floats[],int nf);
void GetTiles(int tiles[],int nt,collision_table *ct,float floats[],int nf);


// one int
void GetTiles(int tiles[],int nt,int memory,float floats[],int nf,int h1);
void GetTiles(int tiles[],int nt,collision_table *ct,float floats[],int nf,int h1);

// two ints
void GetTiles(int tiles[],int nt,int memory,float floats[],int nf,int h1,int h2);
void GetTiles(int tiles[],int nt,collision_table *ct,float floats[],int nf,int h1,int h2);

// three ints
void GetTiles(int tiles[],int nt,int memory,float floats[],int nf,int h1,int h2,int h3);
void GetTiles(int tiles[],int nt,collision_table *ct,float floats[],int nf,int h1,int h2,int h3);

// one float, no ints
void GetTiles1(int tiles[],int nt,int memory,float f1);
void GetTiles1(int tiles[],int nt,collision_table *ct,float f1);

// one float, one int
void GetTiles1(int tiles[],int nt,int memory,float f1,int h1);
void GetTiles1(int tiles[],int nt,collision_table *ct,float f1,int h1);

// one float, two ints
void GetTiles1(int tiles[],int nt,int memory,float f1,int h1,int h2);
void GetTiles1(int tiles[],int nt,collision_table *ct,float f1,int h1,int h2);

// one float, three ints
void GetTiles1(int tiles[],int nt,int memory,float f1,int h1,int h2,int h3);
void GetTiles1(int tiles[],int nt,collision_table *ct,float f1,int h1,int h2,int h3);

// two floats, no ints
void GetTiles2(int tiles[],int nt,int memory,float f1,float f2);
void GetTiles2(int tiles[],int nt,collision_table *ct,float f1,float f2);

// two floats, one int
void GetTiles2(int tiles[],int nt,int memory,float f1,float f2,int h1);
void GetTiles2(int tiles[],int nt,collision_table *ct,float f1,float f2,int h1);

// two floats, two ints
void GetTiles2(int tiles[],int nt,int memory,float f1,float f2,int h1,int h2);
void GetTiles2(int tiles[],int nt,collision_table *ct,float f1,float f2,int h1,int h2);

// two floats, three ints
void GetTiles2(int tiles[],int nt,int memory,float f1,float f2,int h1,int h2,int h3);
void GetTiles2(int tiles[],int nt,collision_table *ct,float f1,float f2,int h1,int h2,int h3);

#endif
