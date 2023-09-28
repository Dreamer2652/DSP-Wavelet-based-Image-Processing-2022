#include <stdio.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#pragma warning(disable: 4996)  // For VS environment, if not please ignore this code

#define ROUND_FIRST 1  // For quant-mapping value comparison

/* ########## Please change the variable below for the test ########## */

// # quantization bits for each levels
#define QUANT_BITS_LOW 7   // LL3 : # of bits for non-loss quantization = 14
#define QUANT_BITS_3 7   // {HL3,LH3,HH3} : # of bits for non-loss quantization = 14
#define QUANT_BITS_2 6   // {HL2,LH2,HH2} : # of bits for non-loss quantization = 12
#define QUANT_BITS_1 5   // {HL1,LH1,HH1} : # of bits for non-loss quantization = 10

#define input "Airplane_256x256_yuv400_8bit.raw"  // Change file name including filename extension
#define height 256        // Change img height
#define width 256         // Change img width

/* ########## Please change the variable above for the test ########## */

#define SIZE height*width
#define depth 3           // Max transform depth

typedef short DATA;  // signed data type greater than 14-bits (minimum : short)

// Image buffer
unsigned char origin[height][width], result[height][width];
DATA temp[height][width];  // Temporary array

// Haar wavelet transform
void H_wavelet(DATA coeff[][width], int level)
{
  int i, j, h, w, H = height>>level, W = width>>level;  // H, W : Maximum idx

  // Calculate coefficient to temp array (stride = 2)
  for(i = 0; i < (H<<1); i+=2)
    for(j = 0, h = i>>1; j < (W<<1); j+=2)
    {
      w = j>>1;
      temp[h][w] = (coeff[i][j]+coeff[i][j+1])+(coeff[i+1][j]+coeff[i+1][j+1]);  // LL : ++++
      temp[h][w+W] = (coeff[i][j]-coeff[i][j+1])+(coeff[i+1][j]-coeff[i+1][j+1]);  // HL : +-+-
      temp[h+H][w] = (coeff[i][j]+coeff[i][j+1])-(coeff[i+1][j]+coeff[i+1][j+1]);  // LH : ++--
      temp[h+H][w+W] = (coeff[i][j]-coeff[i][j+1])-(coeff[i+1][j]-coeff[i+1][j+1]);  // HH : +--+
    }

  // Copy data
  H<<=1, W<<=1;
  for(i = 0; i < H; i++)
    for(j = 0; j < W; j++)
    {
      coeff[i][j] = temp[i][j];
      
      // Get unsigned 8-bit format coefficient
      h = coeff[i][j]<0 ? -coeff[i][j] : coeff[i][j];  // ABS(coeff)
      if(i<(H>>1) && j<(W>>1))  // LL area
        result[i][j] = h+(1<<level*2-1)>>(level*2);
      else  // Other area
        result[i][j] = h+(1<<level*2-2)>>(level*2-1);
    }
}

// Haar inverse transform
void H_inverse(DATA coeff[][width], int level)
{
  int i, j, h, w, H = height>>level, W = width>>level;  // H, W : Maximum idx

  // Reconstruct coefficient to temp array (stride = 2)
  for(i = 0; i < H; i++)
    for(j = 0, h = i<<1; j < W; j++)
    {
      w = j<<1;
      temp[h][w] = (coeff[i][j]+coeff[i][j+W])+(coeff[i+H][j]+coeff[i+H][j+W])>>2;  // LL : ++++
      temp[h][w+1] = (coeff[i][j]-coeff[i][j+W])+(coeff[i+H][j]-coeff[i+H][j+W])>>2;  // HL : +-+-
      temp[h+1][w] = (coeff[i][j]+coeff[i][j+W])-(coeff[i+H][j]+coeff[i+H][j+W])>>2;  // LH : ++--
      temp[h+1][w+1] = (coeff[i][j]-coeff[i][j+W])-(coeff[i+H][j]-coeff[i+H][j+W])>>2;  // HH : +--+
    }

  // Copy data
  H<<=1, W<<=1;
  for(i = 0; i < H; i++)
    for(j = 0; j < W; j++)
      coeff[i][j] = temp[i][j];
}

int main()
{
  int i, j, t;
  FILE* file;
  DATA coeff[height][width];  // LL3 area treat as unsigned
  char name[128];

  /* ########## 0. Data initialize ########## */
  // Read original img
  file = fopen(input, "rb");
  if (!file)
  {
    printf("FILE OPEN ERROR\n");
    return -1;
  }
  printf("Input file name : %s\n", input);
  fread(origin, sizeof(char), SIZE, file);
  fclose(file);

  // Copy original img to base coeff
  for(i = 0; i < height; i++)
    for(j = 0; j < width; j++)
      coeff[i][j] = origin[i][j];

  /* ########## 1. Haar wavelet transform (x3) ########## */
  for(t = 0; t++ < depth;)
  {
    H_wavelet(coeff, t);  // 3x Haar wavelet transform
    
    // Write coeff to file format
    sprintf(name, "Coeff_level_%d_%s", t, input);
    file = fopen(name, "wb");
    if (!file)
    {
      printf("FILE OPEN ERROR\n");
      return -1;
    }
    printf("Level %d coeff file name : %s\n", t, name);
    fwrite(result, sizeof(char), SIZE, file);
    fclose(file);
  }

  /* ########## 2. Quantization - Uniform ########## */
  for(i = 0; i < height; i++)
    for(j = 0; j < width; j++)
    {
      t = coeff[i][j];

      #if ROUND_FIRST == 0  // Quantize immediately
        if(i<(height>>3) && j<(width>>3)) // LL3
          coeff[i][j] = QUANT_BITS_LOW > 13 ? t : t>>(14-QUANT_BITS_LOW);  // 14-bits unsigned data
        else if(i<(height>>2) && j<(width>>2)) // {HL3,LH3,HH3}
          coeff[i][j] = QUANT_BITS_3 > 13 ? t : t>>(14-QUANT_BITS_3);  // 14-bits data
        else if(i<(height>>1) && j<(width>>1)) // {HL2,LH2,HH2}
          coeff[i][j] = QUANT_BITS_2 > 11 ? t : t>>(12-QUANT_BITS_2);  // 12-bits data
        else // {HL1,LH1,HH1}
          coeff[i][j] = QUANT_BITS_1 > 9 ? t : t>>(10-QUANT_BITS_1);  // 10-bits data
      #else  // Quantize by adding rounding value
        if(i<(height>>3) && j<(width>>3)) // LL3
          coeff[i][j] = QUANT_BITS_LOW > 13 ? t : t+(1<<(13-QUANT_BITS_LOW))>>(14-QUANT_BITS_LOW);  // 14-bits unsigned data
        else if(i<(height>>2) && j<(width>>2)) // {HL3,LH3,HH3}
          coeff[i][j] = QUANT_BITS_3 > 13 ? t : t+(1<<(13-QUANT_BITS_3))>>(14-QUANT_BITS_3);  // 14-bits data
        else if(i<(height>>1) && j<(width>>1)) // {HL2,LH2,HH2}
          coeff[i][j] = QUANT_BITS_2 > 11 ? t : t+(1<<(11-QUANT_BITS_2))>>(12-QUANT_BITS_2);  // 12-bits data
        else // {HL1,LH1,HH1}
          coeff[i][j] = QUANT_BITS_1 > 9 ? t : t+(1<<(9-QUANT_BITS_1))>>(10-QUANT_BITS_1);  // 10-bits data
      #endif
    }
  
  /* ########## 3. Inverse Quantization - Uniform ########## */
  for(i = 0; i < height; i++)
    for(j = 0; j < width; j++)
    {
      t = coeff[i][j];
      
      #if ROUND_FIRST == 0  // Return to median
        if(i<(height>>3) && j<(width>>3)) // LL3
          coeff[i][j] = QUANT_BITS_LOW > 13 ? t : (t<<14-QUANT_BITS_LOW)+(1<<(13-QUANT_BITS_LOW));
        else if(i<(height>>2) && j<(width>>2)) // {HL3,LH3,HH3}
          coeff[i][j] = QUANT_BITS_3 > 13 ? t : (t<<14-QUANT_BITS_3)+(1<<(13-QUANT_BITS_3));
        else if(i<(height>>1) && j<(width>>1)) // {HL2,LH2,HH2}
          coeff[i][j] = QUANT_BITS_2 > 11 ? t : (t<<12-QUANT_BITS_2)+(1<<(11-QUANT_BITS_2));
        else // {HL1,LH1,HH1}
          coeff[i][j] = QUANT_BITS_1 > 9 ? t : (t<<10-QUANT_BITS_1)+(1<<(9-QUANT_BITS_1));
      #else  // Return to both ends
        if(i<(height>>3) && j<(width>>3)) // LL3
          coeff[i][j] = QUANT_BITS_LOW > 13 ? t : (t<<14-QUANT_BITS_LOW);
        else if(i<(height>>2) && j<(width>>2)) // {HL3,LH3,HH3}
          coeff[i][j] = QUANT_BITS_3 > 13 ? t : (t<<14-QUANT_BITS_3);
        else if(i<(height>>1) && j<(width>>1)) // {HL2,LH2,HH2}
          coeff[i][j] = QUANT_BITS_2 > 11 ? t : (t<<12-QUANT_BITS_2);
        else // {HL1,LH1,HH1}
          coeff[i][j] = QUANT_BITS_1 > 9 ? t : (t<<10-QUANT_BITS_1);
      #endif
    }

  /* ########## 4. Haar inverse transform (x3) ########## */
  for(i = depth; i > 0 ;)
    H_inverse(coeff, i--);  // 3x Haar inverse transform
  
  // Get 8-bit format result
  for(i = 0; i < height; i++)
    for(j = 0; j < width; j++)
    {
      t = coeff[i][j];
      result[i][j] = t<0 ? 0 : t>255 ? 255 : t;  // Convert to 8bit
    }
  
  // Write reconstruct img
  sprintf(name,"Reconstrct_%s", input);
  file = fopen(name, "wb");
  if (!file)
  {
    printf("FILE OPEN ERROR\n");
    return -1;
  }
  printf("Reconstruct file name : %s\n", name);
  fwrite(result, sizeof(char), SIZE, file);
  fclose(file);
  
  /* ########## 5. Evaluate RMSE ########## */
  unsigned long long sum = 0;
  for (i = 0; i < height; i++)
    for (j = 0; j < width; j++)
      sum += (origin[i][j] - result[i][j]) * (origin[i][j] - result[i][j]);
  double MSE = sum;
  MSE /= SIZE;
  
  printf("MSE = %lf, RMSE = %lf\n", MSE, sqrt(MSE));
  return 0;
}
