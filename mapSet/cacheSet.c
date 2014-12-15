#include <stdio.h>
#include <stdlib.h>

#if 0
int network[8][8] = {
{1, 1, 1, 1, 1, 1, 1, 1},
{0, 0, 0, 0, 0, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0},
{1, 1, 1, 1, 1, 1, 1, 1} };
#endif
#if 0
int network[8][8] = {
{1, 0, 0, 0, 0, 0, 0, 1},
{1, 0, 0, 0, 0, 0, 0, 1},
{1, 0, 0, 0, 0, 0, 0, 1},
{1, 0, 0, 0, 0, 0, 0, 1},
{1, 0, 0, 0, 0, 0, 0, 1},
{1, 0, 0, 0, 0, 0, 0, 1},
{1, 0, 0, 0, 0, 0, 0, 1},
{1, 0, 0, 0, 0, 0, 0, 1} };
#endif
#if 0
int network[8][8] = {
{1, 0, 0, 0, 0, 0, 0, 1},
{0, 1, 0, 0, 0, 0, 1, 0},
{0, 0, 1, 0, 0, 1, 0, 0},
{0, 0, 0, 1, 1, 0, 0, 0},
{0, 0, 0, 1, 1, 0, 0, 0},
{0, 0, 1, 0, 0, 1, 0, 0},
{0, 1, 0, 0, 0, 0, 1, 0},
{1, 0, 0, 0, 0, 0, 0, 1} };
#endif
#if 0
int network[8][8] = {
{0, 0, 0, 1, 1, 0, 0, 0},
{0, 0, 1, 0, 0, 1, 0, 0},
{0, 1, 0, 0, 0, 0, 1, 0},
{1, 0, 0, 0, 0, 0, 0, 1},
{1, 0, 0, 0, 0, 0, 0, 1},
{0, 1, 0, 0, 0, 0, 1, 0},
{0, 0, 1, 0, 0, 1, 0, 0},
{0, 0, 0, 1, 1, 0, 0, 0} };
#endif
#if 0
int network[8][8] = {
{0, 0, 0, 0, 0, 0, 0, 0},
{1, 0, 1, 0, 1, 0, 1, 0},
{0, 1, 0, 1, 0, 1, 0, 1},
{0, 0, 0, 0, 0, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0},
{1, 0, 1, 0, 1, 0, 1, 0},
{0, 1, 0, 1, 0, 1, 0, 1},
{0, 0, 0, 0, 0, 0, 0, 0} };
#endif

// 32bit addr space, byte addressable => 4GB => 1Gword in system (each insn = 4bytes)
#define NUM_SETS 8

#define NET_WIDTH 8
int network[NET_WIDTH][NET_WIDTH] = {{0}};

int sum[NET_WIDTH][NET_WIDTH] = {{0}};
int set[NET_WIDTH][NET_WIDTH] = {{0}};
int type[NET_WIDTH*NET_WIDTH] = {0};
int count[NET_WIDTH*NET_WIDTH] = {0};
int totalCount = 0;
int size = 0;

void printMatrix(int matrix[][NET_WIDTH])
{
    for(int i = 0; i < NET_WIDTH; i++)
    {
        for(int j = 0; j < NET_WIDTH; j++)
        {
            printf("%4d ", matrix[i][j]);
        }
        printf("\n");
    }
}

#define RADIUS 1
int sameNeighbour(int curI, int curJ, int curSet)
{
    int same = 0;
    for(int i = curI-RADIUS; (i < curI+RADIUS) && (same == 0); i++)
    {
        if((i < 0) || (i >= NET_WIDTH))
            continue;
       
        for(int j=curJ-RADIUS; (j < curJ+RADIUS) && (same == 0); j++)
        {
            if((j<0) || (j>= NET_WIDTH))
                continue;

            if(set[i][j] == curSet)
                same = 1;
        }
    }
    return same;
}

#define PLACEMENT DIAMOND_FILE
#define DIAMOND_FILE "diamond64.txt"
#define CROSS_FILE "cross64.txt"
#define TOPBOTTOM_FILE "topBottom64.txt"
#define LEFTRIGHT_FILE "leftRight64.txt"

int main()
{
    FILE *netMapFile = fopen(PLACEMENT, "r");
    for(int i = 0; i < NET_WIDTH; i++)
    {
        fscanf(netMapFile, "%d %d %d %d %d %d %d %d", 
            &network[i][0], &network[i][1], &network[i][2], &network[i][3], &network[i][4], &network[i][5], &network[i][6], &network[i][7]);
    }
    printf("network:\n");
    printMatrix(network);
    printf("\n");

    for(int srcI = 0; srcI < NET_WIDTH; srcI++)
    {
        for(int srcJ = 0; srcJ < NET_WIDTH; srcJ++)
        {
            if(network[srcI][srcJ] == 1)
            {
                for(int destI = 0; destI < NET_WIDTH; destI++)
                {
                    for(int destJ = 0; destJ < NET_WIDTH; destJ++)
                    {
                        if(network[destI][destJ] == 0)
                        {
                            sum[destI][destJ] += abs(srcI-destI) + abs(srcJ-destJ);
                        }
                    }
                }
            }
            else
            {
                totalCount++;
            }
        }
    }
    printf("sum:\n");
    printMatrix(sum);
    printf("\n");

    int banksPerSet = totalCount / NUM_SETS;
    if(banksPerSet*NUM_SETS != totalCount)
        printf("!!!!!not and even number of banks per set!!!!\n");

    // copy and sort
    int sumFlat[NET_WIDTH*NET_WIDTH];
    int size = 0;
    for(int i = 0; i < NET_WIDTH; i++)
    {
        for(int j = 0; j < NET_WIDTH; j++)
        {
            if(sum[i][j] != 0)
            {
                sumFlat[size] = sum[i][j];
                size++;
            }
            //init set matrix
            set[i][j] = -1;
        }
    }
printf("copied\n");
    int tempSum = sumFlat[0];
    for(int i = 0; i < size; i++)
    {
        for(int j = i; j < size; j++)
        {
            if(sumFlat[j] > sumFlat[i])
            {
                tempSum = sumFlat[j];
                sumFlat[j] = sumFlat[i];
                sumFlat[i] = tempSum;
            }
        }
    }
printf("sorted\n");
    int assigned = 0;
    int typeIndex = 0;
    int revTypeIndex = size-1;
    int curSet = 0;
    int done = 0;
    while(assigned != totalCount)
    {
        done = 0;
        curSet = 0;
        int firstTry = 1;
        while(!done)
        {
            for(int i = 0; (i < NET_WIDTH) && !done; i++)
            {
                for(int j = 0; (j < NET_WIDTH) && !done; j++)
                {
                    if((sumFlat[typeIndex] == sum[i][j]) && (set[i][j] == -1) && (!firstTry || (0 == sameNeighbour(i, j, curSet)/* surrounding ones are not same set*/)))
                    {
                        set[i][j] = curSet;
                        assigned++;
                        curSet++;
                        typeIndex++;
                        if(curSet == NUM_SETS)
                        {
                            done = 1;
                        }
                    }
                }
            }
            firstTry = 0;
        }
printf("assigned=%d\n", assigned);
    printf("set:\n");
    printMatrix(set);
    printf("\n");
        if(assigned == totalCount)
            break;
        done = 0;
        curSet = 0;
        firstTry = 1;
        while(!done)
        {
            for(int i = 0; (i < NET_WIDTH) && !done; i++)
            {
                for(int j = 0; (j < NET_WIDTH) && !done; j++)
                {
                    if((sumFlat[revTypeIndex] == sum[i][j]) && (set[i][j] == -1) && (!firstTry || (0 == sameNeighbour(i, j, curSet)/* surrounding ones are not same set*/)))
                    {
                        set[i][j] = curSet;
                        assigned++;
                        curSet++;
                        revTypeIndex--;
                        if(curSet == NUM_SETS)
                        {
                            done = 1;
                        }
                    }
                }
            }
            firstTry = 0;
        }
printf("assigned=%d\n", assigned);
    printf("set:\n");
    printMatrix(set);
    printf("\n");
    } 

    // to file
    FILE *setFile = fopen("setMap.txt", "w");
    for(int i = 0; i < NET_WIDTH; i++)
    {
        for(int j = 0; j < NET_WIDTH; j++)
        {
            fprintf(setFile, "%4d ", set[i][j]);
        }
        fprintf(setFile, "\n");
    }
    fclose(setFile);

    //test: read in setMap again & print
    int test[NET_WIDTH][NET_WIDTH];
    FILE *setMapFile = fopen("setMap.txt", "r");
    for(int i = 0; i < NET_WIDTH; i++)
    {
        fscanf(setMapFile, "%d %d %d %d %d %d %d %d", &test[i][0], &test[i][1], &test[i][2], &test[i][3], &test[i][4], &test[i][5], &test[i][6], &test[i][7]);
    }
    printf("test:\n");
    printMatrix(test);
    printf("\n");

    // total addr range = 



    return 0;
}



