/*
 * (C) 2017, Cornell University
 * All rights reserved.
 */

/* This block store module mirrors the underlying block store but contains
 * a write-through cache.
 *
 *      block_store_t *cachedisk_init(block_store_t *below,
 *                                  block_t *blocks, block_no nblocks)
 *          'below' is the underlying block store.  'blocks' points to
 *          a chunk of memory wth 'nblocks' blocks for caching.
 *          NO OTHER MEMORY MAY BE USED FOR STORING DATA.  However,
 *          malloc etc. may be used for meta-data.
 *
 *      void cachedisk_dump_stats(block_store_t *this_bs)
 *          Prints cache statistics.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "block_store.h"

/* State contains the pointer to the block module below as well as caching
 * information and caching statistics.
 */
struct cachedisk_state {
    block_store_t *below;               // block store below
    block_t *blocks;            // memory for caching blocks
    block_no nblocks;           // size of cache (not size of block store!)

    block_no *metadata;         // list of block_no, e.g metadata[i] is block_no of the cached block in blocks[i]
    unsigned int *ages;         // list of ages, e.g the cached block with smallest age is the LRU block
    

    /* Stats.
     */
    unsigned int read_hit, read_miss, write_hit, write_miss;
};

static int cachedisk_nblocks(block_store_t *this_bs){
    struct cachedisk_state *cs = this_bs->state;

    return (*cs->below->nblocks)(cs->below);
}

static int cachedisk_setsize(block_store_t *this_bs, block_no nblocks){
    struct cachedisk_state *cs = this_bs->state;

    // This function is not complete, but you do not need to 
    // implement it for this assignment.

    return (*cs->below->setsize)(cs->below, nblocks);
}



/* Helper for cachedisk_read and cachedisk_write. Return the index of LRU block in a full
 * cache, this is done by traversing the cache blocks and finding the one with smallest age.
 */
static int find_LRU_index(struct cachedisk_state *cs) {

    unsigned int i;
    unsigned int least = 0;
    for (i = 1; i < cs->nblocks; i ++) {
        if (cs->ages[i] < cs->ages[least]) {
            least = i;
        }
    }
    return least;
}



static int cachedisk_read(block_store_t *this_bs, block_no offset, block_t *block){
    struct cachedisk_state *cs = this_bs->state;

    // TODO: check the cache first.  Otherwise read from the underlying
    //       store and, if so desired, place the new block in the cache,
    //       possibly evicting a block if there is no space.


    // traverse the cache
    unsigned int ind_in_metadata;
    unsigned int hit = 0;
    for (ind_in_metadata = 0; ind_in_metadata < cs->nblocks; ind_in_metadata ++) {
        if (cs->metadata[ind_in_metadata] == offset) {
            hit = 1;
            break;
        }
    }


    // cache hit
    if (hit == 1) {

        cs->read_hit ++;

        // read data
        memcpy((void *)block, (void *)(&(cs->blocks[ind_in_metadata])), BLOCK_SIZE);

        // update ages
        cs->ages[ind_in_metadata] = cs->read_hit + cs->read_miss + cs->write_hit + cs->write_miss;
    }


    // cache miss
    else {

        cs->read_miss ++;

        // pass the read
        if ((*cs->below->read)(cs->below, offset, block) < 0) {
            panic("read error in cache_read \n");
        }

        // cache has empty slot, add it there
        unsigned int first_empty;
        unsigned int has_space = 0;
        for (first_empty = 0; first_empty < cs->nblocks; first_empty ++) {
            if (cs->metadata[first_empty] == -1) {
                cs->metadata[first_empty] = offset;
                memcpy((void *)(&(cs->blocks[first_empty])), (void *)block, BLOCK_SIZE);
                cs->ages[first_empty] = cs->read_hit + cs->read_miss + cs->write_hit + cs->write_miss;
                has_space = 1;
                break;
            }
        }

        // cache is full
        if (has_space == 0) {
            int least = find_LRU_index(cs);
            cs->metadata[least] = offset;
            memcpy((void *)(&(cs->blocks[least])), (void *)block, BLOCK_SIZE);
            cs->ages[least] = cs->read_hit + cs->read_miss + cs->write_hit + cs->write_miss;
        }
    }

    // Piazza @ 1642, should return 0.
    return 0;

}



static int cachedisk_write(block_store_t *this_bs, block_no offset, block_t *block){
    struct cachedisk_state *cs = this_bs->state;

    // TODO: this is a write-through cache, so update the layer below.
    //       However, if the block is in the cache, it should be updated
    //       as well.


    // traverse the cache
    unsigned int ind_in_metadata;
    unsigned int hit = 0;
    for (ind_in_metadata = 0; ind_in_metadata < cs->nblocks; ind_in_metadata ++) {
        if (cs->metadata[ind_in_metadata] == offset) {
            hit = 1;
            break;
        }
    }


    // cache hit
    if (hit == 1) {

        cs->write_hit ++;

        // write data to both cache and disk
        memcpy((void *)(&(cs->blocks[ind_in_metadata])), (void *)block, BLOCK_SIZE);
        if ((*cs->below->write)(cs->below, offset, block) < 0) {
            panic("write error in cache write \n");
        }

        // update ages
        cs->ages[ind_in_metadata] = cs->read_hit + cs->read_miss + cs->write_hit + cs->write_miss;

    }


    // cache miss
    else {

        cs->write_miss++;

        // pass the write
        if ((*cs->below->write)(cs->below, offset, block) < 0) {
            panic("write error in cache write \n");
        }


        // cache has empty slot, bring the block to cache
        unsigned int first_empty;
        unsigned int has_space = 0;
        for (first_empty = 0; first_empty < cs->nblocks; first_empty ++) {
            if (cs->metadata[first_empty] == -1) {
                cs->metadata[first_empty] = offset;
                memcpy((void *)(&(cs->blocks[first_empty])), (void *)block, BLOCK_SIZE);
                cs->ages[first_empty] = cs->read_hit + cs->read_miss + cs->write_hit + cs->write_miss;
                has_space = 1;
                break;
            }
        }

        // cache is full
        if (has_space == 0) {
            int least = find_LRU_index(cs);
            cs->metadata[least] = offset;
            memcpy((void *)(&(cs->blocks[least])), (void *)block, BLOCK_SIZE);
            cs->ages[least] = cs->read_hit + cs->read_miss + cs->write_hit + cs->write_miss;
        }

    }

    // Piazza @ 1642, should return 0.
    return 0;
    
}




static void cachedisk_destroy(block_store_t *this_bs){
    struct cachedisk_state *cs = this_bs->state;

    // TODO: clean up any allocated meta-data.

    free(cs->metadata);
    free(cs->ages);

    free(cs);
    free(this_bs);

}

void cachedisk_dump_stats(block_store_t *this_bs){
    struct cachedisk_state *cs = this_bs->state;

    printf("!$CACHE: #read hits:    %u\n", cs->read_hit);
    printf("!$CACHE: #read misses:  %u\n", cs->read_miss);
    printf("!$CACHE: #write hits:   %u\n", cs->write_hit);
    printf("!$CACHE: #write misses: %u\n", cs->write_miss);
}

/* Create a new block store module on top of the specified module below.
 * blocks points to a chunk of memory of nblocks blocks that can be used
 * for caching.
 */
block_store_t *cachedisk_init(block_store_t *below, block_t *blocks, block_no nblocks){
    /* Create the block store state structure.
     */
    struct cachedisk_state *cs = calloc(1, sizeof(*cs));
    cs->below = below;
    cs->blocks = blocks;
    cs->nblocks = nblocks;

    cs->metadata = (block_no *)malloc(nblocks*sizeof(block_no));        // allocate space for metadata
    cs->ages = (unsigned int *)malloc(nblocks*sizeof(unsigned int));    // allocate space for ages
    memset(cs->metadata, -1, nblocks*sizeof(block_no));                 // initialized to -1, means no block in cache
    memset(cs->ages, -1, nblocks*sizeof(unsigned int));                 // initialized to -1, means no block in cache


    /* Return a block interface to this inode.
     */
    block_store_t *this_bs = calloc(1, sizeof(*this_bs));
    this_bs->state = cs;
    this_bs->nblocks = cachedisk_nblocks;
    this_bs->setsize = cachedisk_setsize;
    this_bs->read = cachedisk_read;
    this_bs->write = cachedisk_write;
    this_bs->destroy = cachedisk_destroy;
    return this_bs;
}
