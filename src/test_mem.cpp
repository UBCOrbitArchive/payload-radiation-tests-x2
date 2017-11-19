#include "test_mem.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include "util/timing.h"
#include "util/cpu_affinity.h"

namespace orbit
{

size_t count1Bits(char value)
{
    size_t numOnes = 0;
    for(size_t i = 0; i < CHAR_BIT * sizeof value; ++i, value >>= 1)
    {
        if ((value & 1) == 1)
        {
            ++numOnes;
        }
    }
    return numOnes;
}

size_t count0Bits(char value)
{
    return (CHAR_BIT * sizeof value) - count1Bits(value);
}

/********************************** CACHE EFFICIENT ***********************************/

void cacheEffSetZeros(char *matrix, const size_t dim)
{
    size_t i, j;

    for (i = 0; i < dim * dim; i++)
    {
        matrix[i] = 0;
    }
}

size_t cacheEffCheckZerosAndFlip(char *matrix, const size_t dim)
{
    size_t i, j;
    size_t zeroToOneFlips = 0;

    for (i = 0; i < dim * dim; i++)
    {
        if(matrix[i] != 0)
        {
            zeroToOneFlips += count1Bits(matrix[i]);
        }
        matrix[i] = 1;
    }

    return zeroToOneFlips;
}

size_t cacheEffCheckOnesAndFlip(char *matrix, const size_t dim)
{
    size_t i, j;
    size_t oneToZeroFlips = 0;

    for (i = 0; i < dim * dim; i++)
    {
        if(matrix[i] != 1)
        {
            oneToZeroFlips += count0Bits(matrix[i]);
        }
        matrix[i] = 0;
    }

    return oneToZeroFlips;
}

/********************************** CACHE INEFFICIENT ***********************************/

void cacheIneffSetZeros(char *matrix, const size_t dim)
{
    size_t i, j;

    for (i = 0; i < dim; i++)
    {
        for (j = 0; j < dim; j++)
        {
            matrix[j * dim + i] = 0;
        }
    }
}

size_t cacheIneffCheckZerosAndFlip(char *matrix, const size_t dim)
{
    size_t i, j;
    size_t zeroToOneFlips = 0;

    for (i = 0; i < dim; i++)
    {
        for (j = 0; j < dim; j++)
        {
            if(matrix[j * dim + i] != 0)
            {
                zeroToOneFlips += count1Bits(matrix[j * dim + i]);
            }
            matrix[j * dim + i] = 1;
        }
    }

    return zeroToOneFlips;
}

size_t cacheIneffCheckOnesAndFlip(char *matrix, const size_t dim)
{
    size_t i, j;
    size_t zeroToOneFlips = 0;

    for (i = 0; i < dim; i++)
    {
        for (j = 0; j < dim; j++)
        {
            if(matrix[j * dim + i] != 1)
            {
                zeroToOneFlips += count0Bits(matrix[j * dim + i]);
            }
            matrix[j * dim + i] = 0;
        }
    }

    return zeroToOneFlips;
}

void sendResult(ResultQueue *resultQueue, test_id_t testId, testMemResults_t results, long double millisecs)
{
    testResult_t testResult;

    snprintf(testResult.data, sizeof(testResult.data), "0To1 = %ld, 1To0 = %ld", results.zeroToOneFlips, results.oneToZeroFlips);

    testResult.testId = testId;
    testResult.execTime = millisecs;
    testResult.timestamp = getEpochMillis();

    resultQueue->enqueueTestResult(testResult);
}

void testMemCacheEffCore(char *matrix, size_t dim, useconds_t sleepTime, test_id_t testId, ResultQueue *resultQueue)
{
    long double millisecs;
    size_t tmpResult;
    testMemResults_t totalResults = {0,0};

    timestamp_t t0 = getTimestamp();

    //------------------------------------------------------

    // Set 0's
    cacheEffSetZeros(matrix, dim);

    // Let radiation do its thing
    usleep(sleepTime);

    // Check 0's, Set 1's
    tmpResult = cacheEffCheckZerosAndFlip(matrix, dim);
    totalResults.zeroToOneFlips += tmpResult;

    // Let radiation do its thing
    usleep(sleepTime);

    // Check 1's, Set 0's
    tmpResult = cacheEffCheckOnesAndFlip(matrix, dim);
    totalResults.oneToZeroFlips += tmpResult;

    // Let radiation do its thing
    usleep(sleepTime);

    //------------------------------------------------------

    timestamp_t t1 = getTimestamp();

    millisecs = (t1 - t0) / 1000.0L;

    sendResult(resultQueue, testId, totalResults, millisecs);
}

void testMemCacheIneffCore(char *matrix, size_t dim, useconds_t sleepTime, test_id_t testId, ResultQueue *resultQueue)
{
    long double millisecs;
    size_t tmpResult;
    testMemResults_t totalResults = {0,0};

    timestamp_t t0 = getTimestamp();

    //------------------------------------------------------

    // Set 0's
    cacheIneffSetZeros(matrix, dim);

    // Let radiation do its thing
    usleep(sleepTime);

    // Check 0's, Set 1's
    tmpResult = cacheIneffCheckZerosAndFlip(matrix, dim);
    totalResults.zeroToOneFlips += tmpResult;

    // Let radiation do its thing
    usleep(sleepTime);

    // Check 1's, Set 0's
    tmpResult = cacheIneffCheckOnesAndFlip(matrix, dim);
    totalResults.oneToZeroFlips += tmpResult;

    // Let radiation do its thing
    usleep(sleepTime);

    //------------------------------------------------------

    timestamp_t t1 = getTimestamp();

    millisecs = (t1 - t0) / 1000.0L;

    sendResult(resultQueue, testId, totalResults, millisecs);
}

void testMemCacheEff(useconds_t sleepTime, size_t dim, size_t num, test_id_t testId, ResultQueue *resultQueue)
{
    char *matrix = reinterpret_cast<char*>(malloc(dim * dim * sizeof(char)));

    for (size_t i = 0; num == 0 || i < num; i++) {
        testMemCacheEffCore(matrix, dim, sleepTime, testId, resultQueue);
    }

    free(matrix);
}

void testMemL1CacheEff(useconds_t sleepTime, size_t num, int cpu, ResultQueue *resultQueue)
{
    if(cpu != -1) setCurrentThreadAffinity(cpu);
    testMemCacheEff(sleepTime, 96, num, ResultQueue::TEST_MEM_L1_CACHE_EFF, resultQueue); //  9 KB matrix < 16 KB L1 Cache
}

void testMemL2CacheEff(useconds_t sleepTime, size_t num, int cpu, ResultQueue *resultQueue)
{
    if(cpu != -1) setCurrentThreadAffinity(cpu);
    testMemCacheEff(sleepTime, 1024, num, ResultQueue::TEST_MEM_L2_CACHE_EFF, resultQueue); //  1 MB matrix < 2 MB L2 Cache
}

void testMemCacheIneff(useconds_t sleepTime, size_t num, int cpu, ResultQueue *resultQueue)
{
    if(cpu != -1) setCurrentThreadAffinity(cpu);

    const size_t dim = 2048; //  4 MB matrix > 2 MB L2 Cache

    char *matrix = reinterpret_cast<char*>(malloc(dim * dim * sizeof(char)));

    for (size_t i = 0; num == 0 || i < num; i++) {
        testMemCacheEffCore(matrix, dim, sleepTime, ResultQueue::TEST_MEM_CACHE_INEFF, resultQueue);
    }

    free(matrix);
}

/*void testMemCacheCompare(useconds_t sleepTime, size_t dim, size_t num)
{
    char *matrixEff = reinterpret_cast<char*>(malloc(dim * dim * sizeof(char)));
    char *matrixIneff = reinterpret_cast<char*>(malloc(dim * dim * sizeof(char)));

    for (size_t i = 0; i < num; i++) {
        testMemCacheEffCore(matrixEff, dim, sleepTime, logTag);
    }

    printf("\n");

    for (size_t i = 0; i < num; i++) {
        testMemCacheIneffCore(matrixIneff, dim, sleepTime, logTag);
    }

    free(matrixEff);
    free(matrixIneff);
}*/

}