/* 
 * Name: Wang Tao
 * ID: 515030910083
 *
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
	int i,j,k;
	int t1, t2, t3, t4, t5, t6, t7, t8;   //temp register on stack
	
	if(M == 32 && N == 32){ //case 1
		//partition it into 8 * 8 matirx
		for(i = 0; i < M; i += 8){
			for(j = 0; j < N; j += 8){
				for(k = 0; k < 8; k++){
					t1 = A[i][j];
					t2 = A[i][j + 1];
					t3 = A[i][j + 2];
					t4 = A[i][j + 3];
					t5 = A[i][j + 4];
					t6 = A[i][j + 5];
					t7 = A[i][j + 6];
					t8 = A[i][j + 7];
				
					B[j][i] = t1;
					B[j + 1][i] = t2;
					B[j + 2][i] = t3;
					B[j + 3][i] = t4;
					B[j + 4][i] = t5;
					B[j + 5][i] = t6;
					B[j + 6][i] = t7;
					B[j + 7][i] = t8;

					i++;
				}
				i -= 8;
			}
		}

	}

	else if (M == 64 && N == 64){
	// use a trick skill to decrease the miss rate of B
		// A: 11 12 ->  B: A11 x -> A11 A21  -> A11 A21
		//    21 22        A21 x    A12   x     A12 A22

		for(i = 0; i < M ; i += 8){
			for(j = 0; j < N; j += 8){
				for(k = 0; k < 4; k++){
					t1 = A[i + k][j];
					t2 = A[i + k][j + 1];
					t3 = A[i + k][j + 2];
					t4 = A[i + k][j + 3];
					t5 = A[i + k][j + 4];
					t6 = A[i + k][j + 5];
					t7 = A[i + k][j + 6];
					t8 = A[i + k ][j + 7];
		
					B[j][i+k] = t1;
					B[j + 1][i+k] = t2;
					B[j + 2][i+k] = t3;
					B[j + 3][i+k] = t4;
				
					B[j][i +k+ 4] = t5;
					B[j + 1][i +k+ 4] = t6;
					B[j + 2][i +k+ 4] = t7;
					B[j + 3][i +k+ 4] = t8;
				}
				for(k = 0; k<4; k++){
					t5 = A[i+4][j+k+4];
      		        t6 = A[i+5][j+k+4];
                  	t7 = A[i+6][j+k+4];
                    t8 = A[i+7][j+k+4];
					
					t1 = B[j+k][i+4];
                 	t2 = B[j+k][i+5];
                   	t3 = B[j+k][i+6];
                   	t4 = B[j+k][i+7];

                    B[j+k][i+4] = A[i+4][j+k];
                    B[j+k][i+5] = A[i+5][j+k];
                    B[j+k][i+6] = A[i+6][j+k];
                    B[j+k][i+7] = A[i+7][j+k];
             	    
                    B[j+k+4][i] = t1;
                    B[j+k+4][i+1] = t2;
                    B[j+k+4][i+2] = t3;
                    B[j+k+4][i+3] = t4;
                    B[j+k+4][i+4] = t5;
                    B[j+k+4][i+5] = t6;
                    B[j+k+4][i+6] = t7;
                    B[j+k+4][i+7] = t8;
				}
			}
		}
	}

	else if(M == 61 && N == 67){  
	//split A into 64*61 and 3*61
		for(i = 0; i<64; i+=8){
			for(j = 0; j < 61; j++){
				t1 = A[i][j];
				t2 = A[i+1][j];
				t3 = A[i+2][j];
				t4 = A[i+3][j];
				t5 = A[i+4][j];
				t6 = A[i+5][j];
				t7 = A[i+6][j];
				t8 = A[i+7][j];
				
				B[j][i] = t1;
				B[j][i+1] = t2;
				B[j][i+2] = t3;
				B[j][i+3] = t4;
				B[j][i+4] = t5;
				B[j][i+5] = t6;
				B[j][i+6] = t7;
				B[j][i+7] = t8;
			
			}
		}
	//	 for 3*61
		for( j = 0; j< M; j++){
			t1 = A[64][j];
			t2 = A[65][j];
			t3 = A[66][j];
			
			B[j][64] = t1;
			B[j][65] = t2;
			B[j][66] = t3;
		}
	}

	else{ //use sample
		
		for (i = 0; i < N; i++) {
			for (j = 0; j < M; j++) {
				k = A[i][j];
				B[j][i] = k;
			}
		}    
	}



}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

