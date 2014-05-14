#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include "defs.h"
#include "mem.h"
#include "common.h"
#include "context.h"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static HCCounters zeroCounters = {0x00, 0x00, 0x00, 0x00};
static HCCounters auxCounters;

static void InitHashTable(CModel *cModel)
  { 
  cModel->hTable.entries = (Entry **) Calloc(HASH_SIZE, sizeof(Entry *));
  cModel->hTable.counters = (HCCounters **) Calloc(HASH_SIZE,
  sizeof(HCCounters *));
  cModel->hTable.entrySize = (unsigned short *) Calloc(HASH_SIZE, 
  sizeof(unsigned short));
  }

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void FreeCModel(CModel *cModel)
  {
  uint32_t k;

  if(cModel->mode == HASH_TABLE_MODE)
    {
    for(k = 0 ; k < HASH_SIZE ; ++k)
      {
      if(cModel->hTable.entrySize[k] != 0)
        Free(cModel->hTable.entries[k]);
      if(cModel->hTable.counters[k] != NULL)
        Free(cModel->hTable.counters[k]);
      }
    Free(cModel->hTable.entries);
    Free(cModel->hTable.counters);
    Free(cModel->hTable.entrySize);
    }
  else // TABLE_MODE
    Free(cModel->array.counters);

  Free(cModel);
  }

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static void InitArray(CModel *cModel)
  {
  cModel->array.counters = (ACCounter *) Calloc(cModel->nPModels << 2, 
  sizeof(ACCounter));
  }

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static void InsertKey(HashTable *hTable, unsigned hIndex, uint64_t idx)
  {
  hTable->entries[hIndex] = (Entry *) Realloc(hTable->entries[hIndex],
  (hTable->entrySize[hIndex] + 1) * sizeof(Entry), sizeof(Entry));

  hTable->entries[hIndex][hTable->entrySize[hIndex]].key = (uint32_t) 
  (idx >> HASH_SHIFT);
  hTable->entrySize[hIndex]++;
  }

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static void InsertCounters(HashTable *hTable, unsigned hIndex, unsigned 
nHCCounters, unsigned k, unsigned smallCounters)
  {
  hTable->counters[hIndex] = (HCCounters *) Realloc(hTable->counters[hIndex], 
  (nHCCounters + 1) * sizeof(HCCounters), sizeof(HCCounters));

  if(k < nHCCounters)
    memmove(hTable->counters[hIndex][k + 1], hTable->counters[hIndex][k],
      (nHCCounters - k) * sizeof(HCCounters));

  hTable->counters[hIndex][k][0] =  smallCounters &  0x03;
  hTable->counters[hIndex][k][1] = (smallCounters & (0x03 << 2)) >> 2;
  hTable->counters[hIndex][k][2] = (smallCounters & (0x03 << 4)) >> 4;
  hTable->counters[hIndex][k][3] = (smallCounters & (0x03 << 6)) >> 6;
  }

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static HCCounter *GetHCCounters(HashTable *hTable, uint64_t key)
  {
  unsigned k = 0, n;
  unsigned hIndex = key % HASH_SIZE;

  for(n = 0 ; n < hTable->entrySize[hIndex] ; n++)
    {
    if(((uint64_t) hTable->entries[hIndex][n].key << HASH_SHIFT) + hIndex == 
    key)
      {
      switch(hTable->entries[hIndex][n].counters)
        {
        case 0:
        return hTable->counters[hIndex][k];

        default:
        auxCounters[0] =  hTable->entries[hIndex][n].counters &  0x03;
        auxCounters[1] = (hTable->entries[hIndex][n].counters & (0x03<<2))>>2;
        auxCounters[2] = (hTable->entries[hIndex][n].counters & (0x03<<4))>>4;
        auxCounters[3] = (hTable->entries[hIndex][n].counters & (0x03<<6))>>6;
        return auxCounters;
        }
      }

    if(hTable->entries[hIndex][n].counters == 0)
      k++;
    }

  return NULL;
  }

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

PModel *CreatePModel(unsigned nSymbols)
  {
  PModel *pModel;
  pModel = (PModel *) Malloc(sizeof(PModel));
  pModel->freqs = (unsigned *) Malloc(nSymbols * sizeof(unsigned));

  return pModel;
  }

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

FloatPModel *CreateFloatPModel(unsigned nSymbols)
  {
  FloatPModel *floatPModel;
  floatPModel = (FloatPModel *) Malloc(sizeof(FloatPModel));
  floatPModel->freqs = (double *) Malloc(nSymbols * sizeof(double));
        
  return floatPModel;
  }

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void UpdateCModelCounterIr(CModel *cModel, unsigned symbol)
  {
  unsigned  n;
  ACCounter *aCounters;
  uint64_t  pModelIdx = cModel->pModelIdxIR;

  if(cModel->mode == HASH_TABLE_MODE)
    {
    unsigned char smallCounter;
    unsigned i, k = 0;
    unsigned nHCCounters;            // The number of HCCounters in this entry
    unsigned hIndex = pModelIdx % HASH_SIZE;                 // The hash index

    for(n = 0 ; n < cModel->hTable.entrySize[hIndex] ; n++)
      {
      if(((uint64_t) cModel->hTable.entries[hIndex][n].key << HASH_SHIFT) + 
      hIndex == pModelIdx)
        {
        // If "counters" is zero, then update the "large" counters.
        if(cModel->hTable.entries[hIndex][n].counters == 0)
          {
          if(++cModel->hTable.counters[hIndex][k][symbol] == 255)
            {
            cModel->hTable.counters[hIndex][k][0] >>= 1;
            cModel->hTable.counters[hIndex][k][1] >>= 1;
            cModel->hTable.counters[hIndex][k][2] >>= 1;
            cModel->hTable.counters[hIndex][k][3] >>= 1;
            }
          return;
          }
        
        smallCounter = (cModel->hTable.entries[hIndex][n].counters >> (symbol 
        << 1)) & 0x03;
         // If "counters" is non-zero, then this is at least the
         // second time that this key is generated. Therefore,
         // if the "small" counter of the symbol if full (i.e.,
         // is equal to 3), then the "large" counters have to be
         // inserted into the right position.
        if(smallCounter == 3)
          {
          nHCCounters = k;
          for(i = n + 1 ; i < cModel->hTable.entrySize[hIndex] ; i++)
            if(cModel->hTable.entries[hIndex][i].counters == 0)
              nHCCounters++;

          InsertCounters(&cModel->hTable, hIndex, nHCCounters, k,
          cModel->hTable.entries[hIndex][n].counters);
          cModel->hTable.entries[hIndex][n].counters = 0;
          cModel->hTable.counters[hIndex][k][symbol]++;
          return;
          }
        else // There is still room for incrementing the "small" counter.
          {
          smallCounter++;
          cModel->hTable.entries[hIndex][n].counters &= ~(0x03<<(symbol<<1));
          cModel->hTable.entries[hIndex][n].counters |= (smallCounter<<(symbol
          <<1));
          return;
          }
        }

      // Keeps counting the number of HCCounters in this entry
      if(!cModel->hTable.entries[hIndex][n].counters)
        k++;
      }

    // If key not found
    InsertKey(&cModel->hTable, hIndex, pModelIdx);
    cModel->hTable.entries[hIndex][cModel->hTable.entrySize[hIndex]-1].
    counters = (0x01 << (symbol << 1));
    }
  else
    {
    aCounters = &cModel->array.counters[pModelIdx << 2];
    aCounters[symbol]++;
    if(aCounters[symbol] == cModel->maxCount && cModel->maxCount != 0)
      {    
      aCounters[0] >>= 1;
      aCounters[1] >>= 1;
      aCounters[2] >>= 1;
      aCounters[3] >>= 1;
      }
    }
  }

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void UpdateCModelCounter(CModel *cModel, unsigned symbol)
  {
  unsigned  n;
  ACCounter *aCounters;
  uint64_t  pModelIdx = cModel->pModelIdx;

  if(cModel->mode == HASH_TABLE_MODE)
    {
    unsigned char smallCounter;
    unsigned i, k = 0;
    unsigned nHCCounters;            // The number of HCCounters in this entry
    unsigned hIndex = pModelIdx % HASH_SIZE;                 // The hash index

    for(n = 0 ; n < cModel->hTable.entrySize[hIndex] ; n++)
      {
      if(((uint64_t) cModel->hTable.entries[hIndex][n].key << HASH_SHIFT) + 
      hIndex == pModelIdx)
        {
        // If "counters" is zero, then update the "large" counters.
        if(cModel->hTable.entries[hIndex][n].counters == 0)
          {
          if(++cModel->hTable.counters[hIndex][k][symbol] == 255)
            {
            cModel->hTable.counters[hIndex][k][0] >>= 1;
            cModel->hTable.counters[hIndex][k][1] >>= 1;
            cModel->hTable.counters[hIndex][k][2] >>= 1;
            cModel->hTable.counters[hIndex][k][3] >>= 1;
            }
          return;
          }
        
        smallCounter = (cModel->hTable.entries[hIndex][n].counters >> (symbol 
        << 1)) & 0x03;
         // If "counters" is non-zero, then this is at least the
         // second time that this key is generated. Therefore,
         // if the "small" counter of the symbol if full (i.e.,
         // is equal to 3), then the "large" counters have to be
         // inserted into the right position.
        if(smallCounter == 3)
          {
          nHCCounters = k;
          for(i = n + 1 ; i < cModel->hTable.entrySize[hIndex] ; i++)
            if(cModel->hTable.entries[hIndex][i].counters == 0)
              nHCCounters++;

          InsertCounters(&cModel->hTable, hIndex, nHCCounters, k,
          cModel->hTable.entries[hIndex][n].counters);
          cModel->hTable.entries[hIndex][n].counters = 0;
          cModel->hTable.counters[hIndex][k][symbol]++;
          return;
          }
        else // There is still room for incrementing the "small" counter.
          {
          smallCounter++;
          cModel->hTable.entries[hIndex][n].counters &= ~(0x03<<(symbol<<1));
          cModel->hTable.entries[hIndex][n].counters |= (smallCounter<<(symbol
          <<1));
          return;
          }
        }

      // Keeps counting the number of HCCounters in this entry
      if(!cModel->hTable.entries[hIndex][n].counters)
        k++;
      }

    // If key not found
    InsertKey(&cModel->hTable, hIndex, pModelIdx);
    cModel->hTable.entries[hIndex][cModel->hTable.entrySize[hIndex]-1].
    counters = (0x01 << (symbol << 1));
    }
  else
    {
    aCounters = &cModel->array.counters[pModelIdx << 2];
    aCounters[symbol]++;
    if(aCounters[symbol] == cModel->maxCount && cModel->maxCount != 0)
      {    
      aCounters[0] >>= 1;
      aCounters[1] >>= 1;
      aCounters[2] >>= 1;
      aCounters[3] >>= 1;
      }
    }
  }

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

CModel *CreateCModel(uint32_t ctx, uint32_t aDen, uint32_t ir, uint8_t ref) 
  {
  CModel    *cModel;
  uint64_t  prod = 1, *multipliers;
  uint32_t  n;

  cModel = (CModel *) Calloc(1, sizeof(CModel));

  if(ctx > MAX_HASH_CTX)
    {
    fprintf(stderr, "Error: context size cannot be greater than %d\n", 
    MAX_HASH_CTX);
    exit(1);
    }
  
  multipliers           = (uint64_t *) Calloc(ctx, sizeof(uint64_t));
  cModel->nPModels      = (uint64_t) pow(ALPHABET_SIZE, ctx);
  cModel->ctx           = ctx;
  cModel->alphaDen      = aDen;
  cModel->pModelIdx     = 0;
  cModel->pModelIdxIR   = cModel->nPModels - 1;
  cModel->ir            = ir  == 0 ? 0 : 1;
  cModel->ref           = ref == 0 ? 0 : 1;

  if(ctx >= HASH_TABLE_BEGIN_CTX)
    {
    cModel->mode     = HASH_TABLE_MODE;
    cModel->maxCount = DEFAULT_MAX_COUNT >> 8;
    InitHashTable(cModel);
    }
  else
    {
    cModel->mode     = ARRAY_MODE;
    cModel->maxCount = DEFAULT_MAX_COUNT;
    InitArray(cModel);
    }

  for(n = 0 ; n != cModel->ctx ; ++n)
    {
    multipliers[n] = prod;
    prod <<= 2;
    }

  cModel->multiplier = multipliers[cModel->ctx-1];

  return cModel;
  }

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ResetCModelIdx(CModel *cModel)
  {
  cModel->pModelIdx   = 0;
  cModel->pModelIdxIR = cModel->nPModels - 1;
  }

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

inline uint8_t GetPModelIdxIR(uint8_t *p, CModel *M)
  {
  M->pModelIdxIR = (M->pModelIdxIR >> 2) + GetCompNum(*p) * M->multiplier;
  return GetCompNum(*(p - M->ctx));
  }

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

inline void GetPModelIdx(uint8_t *p, CModel *M)
  {
  M->pModelIdx = ((M->pModelIdx - *(p - M->ctx) * M->multiplier) << 2) + *p;
  }

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ComputePModel(CModel *cModel, PModel *pModel)
  {
  HCCounter *hCounters;
  ACCounter *aCounters;

  if(cModel->mode == HASH_TABLE_MODE)
    {
    if(!(hCounters = GetHCCounters(&cModel->hTable, cModel->pModelIdx)))
      hCounters = zeroCounters;
    pModel->freqs[0] = 1 + cModel->alphaDen * hCounters[0];
    pModel->freqs[1] = 1 + cModel->alphaDen * hCounters[1];
    pModel->freqs[2] = 1 + cModel->alphaDen * hCounters[2];
    pModel->freqs[3] = 1 + cModel->alphaDen * hCounters[3];
    }
  else
    {
    aCounters = &cModel->array.counters[cModel->pModelIdx << 2];
    pModel->freqs[0] = 1 + cModel->alphaDen * aCounters[0];
    pModel->freqs[1] = 1 + cModel->alphaDen * aCounters[1];
    pModel->freqs[2] = 1 + cModel->alphaDen * aCounters[2];
    pModel->freqs[3] = 1 + cModel->alphaDen * aCounters[3];
    }

  pModel->sum = pModel->freqs[0] + pModel->freqs[1] + pModel->freqs[2] + 
  pModel->freqs[3];
  }

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

double PModelSymbolNats(PModel *pModel, unsigned symbol)
  {
  return log((double) pModel->sum / pModel->freqs[symbol]);
  }

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
